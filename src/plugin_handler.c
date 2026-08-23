#include "plugin.h"
#include "utils.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define INITIAL_NODE_CAPACITY 64

static SurfaceHandle
node_alloc(PluginHandler *ph, NodeKind kind, uint32_t owner)
{
    for (uint32_t i = 0; i < ph->node_capacity; i++) {
        if (!ph->nodes[i].in_use) {
            ph->nodes[i].in_use = true;
            ph->nodes[i].generation++;
            ph->nodes[i].kind = kind;
            ph->nodes[i].owner = owner;
            ph->nodes[i].parent = SURFACE_HANDLE_INVALID;
            ph->nodes[i].priv = NULL;
            ph->nodes[i].nc = NULL;
            memset(&ph->nodes[i].callbacks, 0, sizeof(ph->nodes[i].callbacks));
            if (i >= ph->node_count) ph->node_count = i + 1;
            return (SurfaceHandle){i, ph->nodes[i].generation};
        }
    }

    uint32_t old_cap = ph->node_capacity;
    uint32_t new_cap = old_cap * 2;
    Node *new_nodes = realloc(ph->nodes, new_cap * sizeof(Node));
    if (!new_nodes) return SURFACE_HANDLE_INVALID;
    memset(new_nodes + old_cap, 0, (new_cap - old_cap) * sizeof(Node));
    ph->nodes = new_nodes;
    ph->node_capacity = new_cap;

    uint32_t i = old_cap;
    ph->nodes[i].in_use = true;
    ph->nodes[i].generation = 1;
    ph->nodes[i].kind = kind;
    ph->nodes[i].owner = owner;
    ph->nodes[i].parent = SURFACE_HANDLE_INVALID;
    ph->node_count = i + 1;
    return (SurfaceHandle){i, 1};
}

Node *
node_lookup(PluginHandler *ph, SurfaceHandle h)
{
    if (h.index >= ph->node_count) return NULL;
    Node *n = &ph->nodes[h.index];
    if (!n->in_use || n->generation != h.generation) return NULL;
    return n;
}

static void
node_destroy_impl(PluginHandler *ph, Node *n)
{
    if (!n || !n->in_use) return;

    // kill children
    SurfaceHandle self = {(uint32_t)(n - ph->nodes), n->generation};
    for (uint32_t i = 0; i < ph->node_count; i++) {
        if (ph->nodes[i].in_use && surface_handle_eq(ph->nodes[i].parent, self))
            node_destroy_impl(ph, &ph->nodes[i]);
    }

    // empty nc
    if (n->nc) {
        n->nc->post_configure = NULL;
        n->nc->post_close = NULL;
        n->nc->post_popup_configure = NULL;
        n->nc->post_popup_done = NULL;
        n->nc->post_toplevel_configure = NULL;
        n->nc->post_toplevel_close = NULL;
    }

    // destroy backend
    ph->backend->ops->node_destroy(ph->backend, n);

    free(n->nc);
    n->nc = NULL;
    n->priv = NULL;

    log_debug("node destroyed: index=%u gen=%u kind=%d",
            (uint32_t)(n - ph->nodes), n->generation, n->kind);
    n->in_use = false;
}

void
node_destroy_all_for(PluginHandler *ph, uint32_t owner)
{
    for (uint32_t i = 0; i < ph->node_count; i++) {
        if (ph->nodes[i].in_use && ph->nodes[i].owner == owner)
            node_destroy_impl(ph, &ph->nodes[i]);
    }
}

void
plugin_handler_post_event(PluginHandler *ph, uint32_t owner, SurfaceEvent *ev)
{
    if (owner >= ph->plugin_count) return;
    LoadedPlugin *lp = &ph->plugins[owner];

    pthread_mutex_lock(&lp->event_lock);
    ev->next = NULL;
    if (lp->event_tail)
        lp->event_tail->next = ev;
    else
        lp->event_head = ev;
    lp->event_tail = ev;
    pthread_cond_signal(&lp->event_cond);
    pthread_mutex_unlock(&lp->event_lock);
}

// notify ctx callbacks (from main)
static void
nc_post_configure(NotifyCtx *nc, uint32_t w, uint32_t h)
{
    SurfaceEvent *ev = calloc(1, sizeof(*ev));
    if (!ev) return;
    ev->kind = SURFACE_EVENT_CONFIGURE;
    ev->handle = nc->handle;
    ev->configure.width = w;
    ev->configure.height = h;
    plugin_handler_post_event(nc->ph, nc->owner, ev);
}

static void
nc_post_close(NotifyCtx *nc)
{
    SurfaceEvent *ev = calloc(1, sizeof(*ev));
    if (!ev) return;
    ev->kind = SURFACE_EVENT_CLOSE;
    ev->handle = nc->handle;
    plugin_handler_post_event(nc->ph, nc->owner, ev);
}

static void
nc_post_popup_configure(
        NotifyCtx *nc, int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    SurfaceEvent *ev = calloc(1, sizeof(*ev));
    if (!ev) return;
    ev->kind = SURFACE_EVENT_POPUP_CONFIGURE;
    ev->handle = nc->handle;
    ev->popup_configure.x = x;
    ev->popup_configure.y = y;
    ev->popup_configure.width = w;
    ev->popup_configure.height = h;
    plugin_handler_post_event(nc->ph, nc->owner, ev);
}

static void
nc_post_popup_done(NotifyCtx *nc)
{
    SurfaceEvent *ev = calloc(1, sizeof(*ev));
    if (!ev) return;
    ev->kind = SURFACE_EVENT_POPUP_DONE;
    ev->handle = nc->handle;
    plugin_handler_post_event(nc->ph, nc->owner, ev);
}

static void
nc_post_toplevel_configure(NotifyCtx *nc, uint32_t w, uint32_t h)
{
    SurfaceEvent *ev = calloc(1, sizeof(*ev));
    if (!ev) return;
    ev->kind = SURFACE_EVENT_TOPLEVEL_CONFIGURE;
    ev->handle = nc->handle;
    ev->configure.width = w;
    ev->configure.height = h;
    plugin_handler_post_event(nc->ph, nc->owner, ev);
}

static void
nc_post_toplevel_close(NotifyCtx *nc)
{
    SurfaceEvent *ev = calloc(1, sizeof(*ev));
    if (!ev) return;
    ev->kind = SURFACE_EVENT_TOPLEVEL_CLOSE;
    ev->handle = nc->handle;
    plugin_handler_post_event(nc->ph, nc->owner, ev);
}

static NotifyCtx *
make_nc_layer(PluginHandler *ph, SurfaceHandle h, uint32_t owner)
{
    NotifyCtx *nc = calloc(1, sizeof(*nc));
    if (!nc) return NULL;
    nc->ph = ph;
    nc->handle = h;
    nc->owner = owner;
    nc->post_configure = nc_post_configure;
    nc->post_close = nc_post_close;
    return nc;
}

static NotifyCtx *
make_nc_popup(PluginHandler *ph, SurfaceHandle h, uint32_t owner)
{
    NotifyCtx *nc = calloc(1, sizeof(*nc));
    if (!nc) return NULL;
    nc->ph = ph;
    nc->handle = h;
    nc->owner = owner;
    nc->post_popup_configure = nc_post_popup_configure;
    nc->post_popup_done = nc_post_popup_done;
    return nc;
}

static NotifyCtx *
make_nc_toplevel(PluginHandler *ph, SurfaceHandle h, uint32_t owner)
{
    NotifyCtx *nc = calloc(1, sizeof(*nc));
    if (!nc) return NULL;
    nc->ph = ph;
    nc->handle = h;
    nc->owner = owner;
    nc->post_toplevel_configure = nc_post_toplevel_configure;
    nc->post_toplevel_close = nc_post_toplevel_close;
    return nc;
}

// request processing (from main)
static SurfaceHandle
process_layer_create(PluginHandler *ph, Request *req)
{
    LayerCreateArgs *a = &req->args.layer;
    SurfaceHandle h = node_alloc(ph, NODE_LAYER, req->owner);
    if (!surface_handle_valid(h)) return SURFACE_HANDLE_INVALID;

    Node *n = node_lookup(ph, h);

    NotifyCtx *nc = make_nc_layer(ph, h, req->owner);
    if (!nc) {
        n->in_use = false;
        return SURFACE_HANDLE_INVALID;
    }

    n->nc = nc;
    n->callbacks = req->callbacks;

    if (!ph->backend->ops->layer_create(ph->backend, n, a, nc)) {
        free(nc);
        n->nc = NULL;
        n->in_use = false;
        return SURFACE_HANDLE_INVALID;
    }

    log_debug("process_layer_create: index=%u gen=%u", h.index, h.generation);
    return h;
}

static SurfaceHandle
process_popup_create(PluginHandler *ph, Request *req)
{
    PopupCreateArgs *a = &req->args.popup;
    SurfaceHandle h = node_alloc(ph, NODE_POPUP, req->owner);
    if (!surface_handle_valid(h)) return SURFACE_HANDLE_INVALID;

    Node *n = node_lookup(ph, h);

    Node *parent = node_lookup(ph, a->parent);
    if (!parent) {
        n->in_use = false;
        return SURFACE_HANDLE_INVALID;
    }

    NotifyCtx *nc = make_nc_popup(ph, h, req->owner);
    if (!nc) {
        n->in_use = false;
        return SURFACE_HANDLE_INVALID;
    }

    n->nc = nc;
    n->parent = a->parent;
    n->callbacks = req->callbacks;

    if (!ph->backend->ops->popup_create(ph->backend, n, a, parent, nc)) {
        free(nc);
        n->nc = NULL;
        n->in_use = false;
        return SURFACE_HANDLE_INVALID;
    }

    log_debug("process_popup_create: index=%u gen=%u", h.index, h.generation);
    return h;
}

static SurfaceHandle
process_toplevel_create(PluginHandler *ph, Request *req)
{
    ToplevelCreateArgs *a = &req->args.toplevel;
    SurfaceHandle h = node_alloc(ph, NODE_TOPLEVEL, req->owner);
    if (!surface_handle_valid(h)) return SURFACE_HANDLE_INVALID;

    Node *n = node_lookup(ph, h);

    NotifyCtx *nc = make_nc_toplevel(ph, h, req->owner);
    if (!nc) {
        n->in_use = false;
        return SURFACE_HANDLE_INVALID;
    }

    n->nc = nc;
    n->callbacks = req->callbacks;

    if (!ph->backend->ops->toplevel_create(ph->backend, n, a, nc)) {
        free(nc);
        n->nc = NULL;
        n->in_use = false;
        return SURFACE_HANDLE_INVALID;
    }

    log_debug(
            "process_toplevel_create: index=%u gen=%u", h.index, h.generation);
    return h;
}

static void
process_destroy(PluginHandler *ph, Request *req)
{
    Node *n = node_lookup(ph, req->args.destroy_handle);
    if (!n) return;
    if (n->owner != req->owner) {
        log_debug("destroy rejected: owner mismatch");
        return;
    }
    node_destroy_impl(ph, n);
}

void
plugin_handler_process_requests(PluginHandler *ph)
{
    pthread_mutex_lock(&ph->request_lock);
    Request *req = ph->request_head;
    ph->request_head = NULL;
    ph->request_tail = NULL;
    pthread_mutex_unlock(&ph->request_lock);

    while (req) {
        Request *next = req->next;
        SurfaceHandle r = SURFACE_HANDLE_INVALID;
        log_debug("processing request kind=%d from owner=%u", req->kind,
                req->owner);

        switch (req->kind) {
        case REQ_LAYER_CREATE:
            r = process_layer_create(ph, req);
            break;
        case REQ_POPUP_CREATE:
            r = process_popup_create(ph, req);
            break;
        case REQ_TOPLEVEL_CREATE:
            r = process_toplevel_create(ph, req);
            break;
        case REQ_DESTROY:
            process_destroy(ph, req);
            break;
        }

        pthread_mutex_lock(req->done_lock);
        if (req->out) *req->out = r;
        *req->done = true;
        pthread_cond_signal(req->done_cond);
        pthread_mutex_unlock(req->done_lock);

        req = next;
    }
}

// wake pipe
void
plugin_handler_wake(PluginHandler *ph)
{
    char c = 1;
    (void)write(ph->wake_fds[1], &c, 1);
}

void
plugin_handler_drain_wake(PluginHandler *ph)
{
    char buf[64];
    while (read(ph->wake_fds[0], buf, sizeof(buf)) > 0)
        ;
}

// API callbacks (from plugin)
static SurfaceHandle
submit_request_and_wait(PluginHandler *ph, Request *req)
{
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    bool done = false;
    SurfaceHandle out = SURFACE_HANDLE_INVALID;

    req->done_lock = &lock;
    req->done_cond = &cond;
    req->done = &done;
    req->out = &out;
    req->next = NULL;

    log_debug(
            "submitting request kind=%d from owner=%u", req->kind, req->owner);

    pthread_mutex_lock(&ph->request_lock);
    if (ph->request_tail)
        ph->request_tail->next = req;
    else
        ph->request_head = req;
    ph->request_tail = req;
    pthread_mutex_unlock(&ph->request_lock);

    plugin_handler_wake(ph);

    pthread_mutex_lock(&lock);
    while (!done)
        pthread_cond_wait(&cond, &lock);
    pthread_mutex_unlock(&lock);

    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);
    log_debug("request kind=%d completed for owner=%u: handle=%u/%u", req->kind,
            req->owner, out.index, out.generation);
    return out;
}

static SurfaceHandle
api_layer_create(
        PluginHandle *self, LayerCreateArgs args, SurfaceCallbacks callbacks)
{
    Request req = {
            .kind = REQ_LAYER_CREATE,
            .owner = self->plugin_index,
            .args.layer = args,
            .callbacks = callbacks,
    };
    return submit_request_and_wait(self->handler, &req);
}

static SurfaceHandle
api_popup_create(
        PluginHandle *self, PopupCreateArgs args, SurfaceCallbacks callbacks)
{
    Request req = {
            .kind = REQ_POPUP_CREATE,
            .owner = self->plugin_index,
            .args.popup = args,
            .callbacks = callbacks,
    };
    return submit_request_and_wait(self->handler, &req);
}

static SurfaceHandle
api_toplevel_create(
        PluginHandle *self, ToplevelCreateArgs args, SurfaceCallbacks callbacks)
{
    Request req = {
            .kind = REQ_TOPLEVEL_CREATE,
            .owner = self->plugin_index,
            .args.toplevel = args,
            .callbacks = callbacks,
    };
    return submit_request_and_wait(self->handler, &req);
}

static void
api_surface_destroy(PluginHandle *self, SurfaceHandle handle)
{
    Request req = {
            .kind = REQ_DESTROY,
            .owner = self->plugin_index,
            .args.destroy_handle = handle,
    };
    submit_request_and_wait(self->handler, &req);
}

static void
api_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[plugin] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

static void
api_quit(PluginHandle *self)
{
    log_debug("api: quit requested by plugin %u", self->plugin_index);
    self->handler->running = false;
    plugin_handler_wake(self->handler);
}

static void
api_set_data(PluginHandle *self, void *data)
{ self->plugin_data = data; }

static void *
api_get_data(PluginHandle *self)
{ return self->plugin_data; }

static bool
api_ewmh_get_active_window(PluginHandle *self, uint32_t *out_wid)
{
    PluginHandler *ph = self->handler;
    if (!ph->backend->ops->ewmh_get_active_window) return false;
    return ph->backend->ops->ewmh_get_active_window(ph->backend, out_wid);
}

static bool
api_ewmh_get_client_list(
        PluginHandle *self, uint32_t **out_wids, uint32_t *out_count)
{
    PluginHandler *ph = self->handler;
    if (!ph->backend->ops->ewmh_get_client_list) return false;
    return ph->backend->ops->ewmh_get_client_list(
            ph->backend, out_wids, out_count);
}

// handler init / teardown
bool
plugin_handler_init(PluginHandler *ph, BackendContext *backend)
{
    memset(ph, 0, sizeof(*ph));
    ph->backend = backend;
    ph->running = true;

    ph->nodes = calloc(INITIAL_NODE_CAPACITY, sizeof(Node));
    if (!ph->nodes) return false;
    ph->node_capacity = INITIAL_NODE_CAPACITY;

    pthread_mutex_init(&ph->request_lock, NULL);
    pthread_mutex_init(&ph->exit_lock, NULL);

    if (pipe(ph->wake_fds) < 0) {
        free(ph->nodes);
        return false;
    }
    fcntl(ph->wake_fds[0], F_SETFL, O_NONBLOCK);

    ph->core = (CoreAPI){
            .version = 1,
            .size = sizeof(CoreAPI),
            .log = api_log,
            .quit = api_quit,
            .set_data = api_set_data,
            .get_data = api_get_data,
    };
    ph->surfaces = (SurfaceAPI){
            .version = 1,
            .size = sizeof(SurfaceAPI),
            .layer_create = api_layer_create,
            .popup_create = api_popup_create,
            .toplevel_create = api_toplevel_create,
            .destroy = api_surface_destroy,
    };
    ph->api = (ShellAPI){
            .abi_version = 1,
            .core = &ph->core,
            .surfaces = &ph->surfaces,
            .ewmh = NULL,
    };

    // check ewmh api
    if (backend->ops->ewmh_get_active_window ||
            backend->ops->ewmh_get_client_list) {
        ph->ewmh_api = (EWMHAPI){
                .version = 1,
                .size = sizeof(EWMHAPI),
                .get_active_window = api_ewmh_get_active_window,
                .get_client_list = api_ewmh_get_client_list,
        };
        ph->api.ewmh = &ph->ewmh_api;
    }

    log_debug("plugin handler initialized");
    return true;
}

void
plugin_handler_teardown(PluginHandler *ph)
{
    plugin_handler_unload_all(ph);

    free(ph->nodes);
    pthread_mutex_destroy(&ph->request_lock);
    pthread_mutex_destroy(&ph->exit_lock);

    // drain pending requests
    Request *req = ph->request_head;
    while (req) {
        Request *next = req->next;
        pthread_mutex_lock(req->done_lock);
        *req->done = true;
        pthread_cond_signal(req->done_cond);
        pthread_mutex_unlock(req->done_lock);
        req = next;
    }

    close(ph->wake_fds[0]);
    close(ph->wake_fds[1]);

    log_debug("plugin handler torn down");
}
