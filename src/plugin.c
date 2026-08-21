#include "plugin.h"
#include "utils.h"

#include <dlfcn.h>
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
            ph->nodes[i].wl_surface = NULL;
            ph->nodes[i].impl = NULL;
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

    switch (n->kind) {
    case NODE_LAYER:
        if (n->impl) {
            layer_surface_destroy(n->impl);
            free(n->impl);
        }
        break;
    case NODE_POPUP:
        if (n->impl) {
            popup_surface_destroy(n->impl);
            free(n->impl);
        }
        break;
    case NODE_TOPLEVEL:
        if (n->impl) {
            window_surface_destroy(n->impl);
            free(n->impl);
        }
        break;
    }
    if (n->wl_surface) {
        surface_destroy(n->wl_surface);
        free(n->wl_surface);
    }

    log_debug("node destroyed: index=%u gen=%u kind=%d",
            (uint32_t)(n - ph->nodes), n->generation, n->kind);
    n->in_use = false;
    n->impl = NULL;
    n->wl_surface = NULL;
}

static void
node_destroy_all_for(PluginHandler *ph, uint32_t owner)
{
    for (uint32_t i = 0; i < ph->node_count; i++) {
        if (ph->nodes[i].in_use && ph->nodes[i].owner == owner)
            node_destroy_impl(ph, &ph->nodes[i]);
    }
}

static void
post_event(PluginHandler *ph, uint32_t owner, SurfaceEvent *ev)
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

// called by main thread

typedef struct {
    PluginHandler *ph;
    SurfaceHandle handle;
    uint32_t owner;
} NotifyCtx;

static void
notify_layer_configure(void *data, uint32_t width, uint32_t height)
{
    NotifyCtx *nc = data;
    SurfaceEvent *ev = calloc(1, sizeof(*ev));
    if (!ev) return;
    ev->kind = SURFACE_EVENT_CONFIGURE;
    ev->handle = nc->handle;
    ev->configure.width = width;
    ev->configure.height = height;
    post_event(nc->ph, nc->owner, ev);
}

static void
notify_layer_closed(void *data)
{
    NotifyCtx *nc = data;
    SurfaceEvent *ev = calloc(1, sizeof(*ev));
    if (!ev) return;
    ev->kind = SURFACE_EVENT_CLOSE;
    ev->handle = nc->handle;
    post_event(nc->ph, nc->owner, ev);
}

static void
notify_popup_configure(
        void *data, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    NotifyCtx *nc = data;
    SurfaceEvent *ev = calloc(1, sizeof(*ev));
    if (!ev) return;
    ev->kind = SURFACE_EVENT_POPUP_CONFIGURE;
    ev->handle = nc->handle;
    ev->popup_configure.x = x;
    ev->popup_configure.y = y;
    ev->popup_configure.width = width;
    ev->popup_configure.height = height;
    post_event(nc->ph, nc->owner, ev);
}

static void
notify_popup_closed(void *data)
{
    NotifyCtx *nc = data;
    SurfaceEvent *ev = calloc(1, sizeof(*ev));
    if (!ev) return;
    ev->kind = SURFACE_EVENT_POPUP_DONE;
    ev->handle = nc->handle;
    post_event(nc->ph, nc->owner, ev);
}

static void
notify_toplevel_configure(void *data, uint32_t width, uint32_t height)
{
    NotifyCtx *nc = data;
    SurfaceEvent *ev = calloc(1, sizeof(*ev));
    if (!ev) return;
    ev->kind = SURFACE_EVENT_TOPLEVEL_CONFIGURE;
    ev->handle = nc->handle;
    ev->configure.width = width;
    ev->configure.height = height;
    post_event(nc->ph, nc->owner, ev);
}

static void
notify_toplevel_closed(void *data)
{
    NotifyCtx *nc = data;
    SurfaceEvent *ev = calloc(1, sizeof(*ev));
    if (!ev) return;
    ev->kind = SURFACE_EVENT_TOPLEVEL_CLOSE;
    ev->handle = nc->handle;
    post_event(nc->ph, nc->owner, ev);
}

static SurfaceHandle
process_layer_create(PluginHandler *ph, Request *req)
{
    LayerCreateArgs *a = &req->args.layer;
    SurfaceHandle h = node_alloc(ph, NODE_LAYER, req->owner);
    if (!surface_handle_valid(h)) return SURFACE_HANDLE_INVALID;

    Node *n = node_lookup(ph, h);

    WaylandSurface *ws = calloc(1, sizeof(*ws));
    if (!ws || !surface_init(ws, ph->ctx)) {
        free(ws);
        n->in_use = false;
        return SURFACE_HANDLE_INVALID;
    }
    n->wl_surface = ws;

    LayerSurface *ls = calloc(1, sizeof(*ls));
    if (!ls) {
        node_destroy_impl(ph, n);
        return SURFACE_HANDLE_INVALID;
    }

    NotifyCtx *nc = calloc(1, sizeof(*nc));
    if (!nc) {
        free(ls);
        node_destroy_impl(ph, n);
        return SURFACE_HANDLE_INVALID;
    }
    nc->ph = ph;
    nc->handle = h;
    nc->owner = req->owner;
    ls->on_configure = notify_layer_configure;
    ls->on_closed = notify_layer_closed;
    ls->notify_data = nc;

    if (!layer_surface_init(ls, ph->ctx, ws, a->layer, a->namespace, a->anchor,
                a->width, a->height, a->exclusive_zone)) {
        free(nc);
        free(ls);
        node_destroy_impl(ph, n);
        return SURFACE_HANDLE_INVALID;
    }
    n->impl = ls;
    n->callbacks = req->callbacks;

    log_debug("created layer node: index=%u gen=%u size=%ux%u", h.index,
            h.generation, ls->width, ls->height);
    return h;
}

static SurfaceHandle
process_popup_create(PluginHandler *ph, Request *req)
{
    PopupCreateArgs *a = &req->args.popup;
    SurfaceHandle h = node_alloc(ph, NODE_POPUP, req->owner);
    if (!surface_handle_valid(h)) return SURFACE_HANDLE_INVALID;

    Node *n = node_lookup(ph, h);

    Node *pn = node_lookup(ph, a->parent);
    if (!pn) {
        n->in_use = false;
        return SURFACE_HANDLE_INVALID;
    }

    PopupParent parent;
    switch (pn->kind) {
    case NODE_LAYER: {
        LayerSurface *pls = pn->impl;
        parent.type = POPUP_PARENT_LAYER;
        parent.layer_surface = pls->layer_surface;
        break;
    }
    case NODE_POPUP: {
        PopupSurface *pps = pn->impl;
        parent.type = POPUP_PARENT_XDG;
        parent.xdg_surface = pps->xdg_surface;
        break;
    }
    case NODE_TOPLEVEL: {
        WindowSurface *pws = pn->impl;
        parent.type = POPUP_PARENT_XDG;
        parent.xdg_surface = pws->xdg_surface;
        break;
    }
    }

    WaylandSurface *ws = calloc(1, sizeof(*ws));
    if (!ws || !surface_init(ws, ph->ctx)) {
        free(ws);
        n->in_use = false;
        return SURFACE_HANDLE_INVALID;
    }
    n->wl_surface = ws;

    PopupSurface *ps = calloc(1, sizeof(*ps));
    if (!ps) {
        node_destroy_impl(ph, n);
        return SURFACE_HANDLE_INVALID;
    }

    NotifyCtx *nc = calloc(1, sizeof(*nc));
    if (!nc) {
        free(ps);
        node_destroy_impl(ph, n);
        return SURFACE_HANDLE_INVALID;
    }
    nc->ph = ph;
    nc->handle = h;
    nc->owner = req->owner;
    ps->on_configure = notify_popup_configure;
    ps->on_closed = notify_popup_closed;
    ps->notify_data = nc;

    if (!popup_surface_init(ps, ph->ctx, ws, &parent, a->anchor_x, a->anchor_y,
                a->anchor_width, a->anchor_height, a->width, a->height,
                a->anchor, a->gravity)) {
        free(nc);
        free(ps);
        node_destroy_impl(ph, n);
        return SURFACE_HANDLE_INVALID;
    }
    n->impl = ps;
    n->parent = a->parent;
    n->callbacks = req->callbacks;

    log_debug("created popup node: index=%u gen=%u at (%d,%d) size=%ux%u",
            h.index, h.generation, ps->x, ps->y, ps->width, ps->height);
    return h;
}

static SurfaceHandle
process_toplevel_create(PluginHandler *ph, Request *req)
{
    ToplevelCreateArgs *a = &req->args.toplevel;
    SurfaceHandle h = node_alloc(ph, NODE_TOPLEVEL, req->owner);
    if (!surface_handle_valid(h)) return SURFACE_HANDLE_INVALID;

    Node *n = node_lookup(ph, h);

    WaylandSurface *ws = calloc(1, sizeof(*ws));
    if (!ws || !surface_init(ws, ph->ctx)) {
        free(ws);
        n->in_use = false;
        return SURFACE_HANDLE_INVALID;
    }
    n->wl_surface = ws;

    WindowSurface *wsurf = calloc(1, sizeof(*wsurf));
    if (!wsurf) {
        node_destroy_impl(ph, n);
        return SURFACE_HANDLE_INVALID;
    }

    NotifyCtx *nc = calloc(1, sizeof(*nc));
    if (!nc) {
        free(wsurf);
        node_destroy_impl(ph, n);
        return SURFACE_HANDLE_INVALID;
    }
    nc->ph = ph;
    nc->handle = h;
    nc->owner = req->owner;
    wsurf->on_configure = notify_toplevel_configure;
    wsurf->on_closed = notify_toplevel_closed;
    wsurf->notify_data = nc;

    if (!window_surface_init(
                wsurf, ph->ctx, ws, a->width, a->height, a->title, a->app_id)) {
        free(nc);
        free(wsurf);
        node_destroy_impl(ph, n);
        return SURFACE_HANDLE_INVALID;
    }
    n->impl = wsurf;
    n->callbacks = req->callbacks;

    log_debug("created toplevel node: index=%u gen=%u size=%ux%u", h.index,
            h.generation, wsurf->width, wsurf->height);
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

    /* free notify context */
    switch (n->kind) {
    case NODE_LAYER:
        if (n->impl) {
            LayerSurface *ls = n->impl;
            free(ls->notify_data);
            ls->notify_data = NULL;
            ls->on_configure = NULL;
            ls->on_closed = NULL;
        }
        break;
    case NODE_POPUP:
        if (n->impl) {
            PopupSurface *ps = n->impl;
            free(ps->notify_data);
            ps->notify_data = NULL;
            ps->on_configure = NULL;
            ps->on_closed = NULL;
        }
        break;
    case NODE_TOPLEVEL:
        if (n->impl) {
            WindowSurface *ws = n->impl;
            free(ws->notify_data);
            ws->notify_data = NULL;
            ws->on_configure = NULL;
            ws->on_closed = NULL;
        }
        break;
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
        SurfaceHandle result = SURFACE_HANDLE_INVALID;

        switch (req->kind) {
        case REQ_LAYER_CREATE:
            result = process_layer_create(ph, req);
            break;
        case REQ_POPUP_CREATE:
            result = process_popup_create(ph, req);
            break;
        case REQ_TOPLEVEL_CREATE:
            result = process_toplevel_create(ph, req);
            break;
        case REQ_DESTROY:
            process_destroy(ph, req);
            break;
        }

        // signal requesting thread
        pthread_mutex_lock(req->done_lock);
        if (req->out) *req->out = result;
        *req->done = true;
        pthread_cond_signal(req->done_cond);
        pthread_mutex_unlock(req->done_lock);

        req = next;
    }
}

// functions called by plugin thread

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

// plugin thread

static void
dispatch_event(LoadedPlugin *lp, SurfaceEvent *ev)
{
    PluginHandler *ph = lp->handle.handler;
    Node *n = node_lookup(ph, ev->handle);
    if (!n || !n->callbacks.on_event) return;
    n->callbacks.on_event(ev->handle, ev, n->callbacks.user);
}

static void *
plugin_thread_main(void *arg)
{
    LoadedPlugin *lp = arg;
    PluginHandler *ph = lp->handle.handler;

    log_debug("plugin thread started: %s", lp->plugin->name);

    if (lp->plugin->init(&ph->api, &lp->handle, NULL) != 0) {
        log_debug("plugin init failed: %s", lp->plugin->name);
        return NULL;
    }
    log_debug("plugin initialized: %s", lp->plugin->name);

    while (!lp->shutdown) {
        pthread_mutex_lock(&lp->event_lock);
        while (!lp->event_head && !lp->shutdown)
            pthread_cond_wait(&lp->event_cond, &lp->event_lock);
        SurfaceEvent *ev = lp->event_head;
        if (ev) {
            lp->event_head = ev->next;
            if (!lp->event_head) lp->event_tail = NULL;
        }
        pthread_mutex_unlock(&lp->event_lock);

        if (ev) {
            dispatch_event(lp, ev);
            free(ev);
        }
    }

    log_debug("plugin thread shutting down: %s", lp->plugin->name);
    if (lp->plugin->destroy) lp->plugin->destroy(&ph->api, &lp->handle);

    return NULL;
}

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

bool
plugin_handler_init(PluginHandler *ph, WaylandContext *ctx)
{
    memset(ph, 0, sizeof(*ph));
    ph->ctx = ctx;
    ph->running = true;

    ph->nodes = calloc(INITIAL_NODE_CAPACITY, sizeof(Node));
    if (!ph->nodes) return false;
    ph->node_capacity = INITIAL_NODE_CAPACITY;

    pthread_mutex_init(&ph->request_lock, NULL);

    if (pipe(ph->wake_fds) < 0) {
        free(ph->nodes);
        return false;
    }

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
    };

    log_debug("plugin handler initialized");
    return true;
}

bool
plugin_handler_load(PluginHandler *ph, const char *path)
{
    void *dl = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!dl) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return false;
    }

    ShellPlugin *sp = dlsym(dl, "shell_plugin");
    if (!sp) {
        fprintf(stderr, "dlsym: %s\n", dlerror());
        dlclose(dl);
        return false;
    }

    uint32_t idx = ph->plugin_count;
    LoadedPlugin *new_arr =
            realloc(ph->plugins, (idx + 1) * sizeof(LoadedPlugin));
    if (!new_arr) {
        dlclose(dl);
        return false;
    }
    ph->plugins = new_arr;
    ph->plugin_count = idx + 1;

    LoadedPlugin *lp = &ph->plugins[idx];
    memset(lp, 0, sizeof(*lp));
    lp->plugin = sp;
    lp->dl_handle = dl;
    lp->handle.handler = ph;
    lp->handle.plugin_index = idx;
    lp->handle.plugin_data = NULL;
    pthread_mutex_init(&lp->event_lock, NULL);
    pthread_cond_init(&lp->event_cond, NULL);

    log_debug("loaded plugin: %s (%s)", sp->name, path);

    if (pthread_create(&lp->thread, NULL, plugin_thread_main, lp) != 0) {
        fprintf(stderr, "pthread_create failed for plugin %s\n", sp->name);
        dlclose(dl);
        ph->plugin_count--;
        return false;
    }

    return true;
}

void
plugin_handler_unload_all(PluginHandler *ph)
{
    for (uint32_t i = 0; i < ph->plugin_count; i++) {
        LoadedPlugin *lp = &ph->plugins[i];

        /* signal shutdown */
        pthread_mutex_lock(&lp->event_lock);
        lp->shutdown = true;
        pthread_cond_signal(&lp->event_cond);
        pthread_mutex_unlock(&lp->event_lock);
    }

    /* destroy all plugin-owned nodes before join */
    for (uint32_t i = 0; i < ph->plugin_count; i++)
        node_destroy_all_for(ph, i);

    for (uint32_t i = 0; i < ph->plugin_count; i++) {
        LoadedPlugin *lp = &ph->plugins[i];
        pthread_join(lp->thread, NULL);

        /* drain event queue */
        SurfaceEvent *ev = lp->event_head;
        while (ev) {
            SurfaceEvent *next = ev->next;
            free(ev);
            ev = next;
        }

        pthread_mutex_destroy(&lp->event_lock);
        pthread_cond_destroy(&lp->event_cond);
        dlclose(lp->dl_handle);
        log_debug("unloaded plugin: %s", lp->plugin->name);
    }
    free(ph->plugins);
    ph->plugins = NULL;
    ph->plugin_count = 0;
}

void
plugin_handler_teardown(PluginHandler *ph)
{
    plugin_handler_unload_all(ph);

    // free remaining notify contexts
    for (uint32_t i = 0; i < ph->node_count; i++) {
        Node *n = &ph->nodes[i];
        if (!n->in_use) continue;
        switch (n->kind) {
        case NODE_LAYER:
            if (n->impl) free(((LayerSurface *)n->impl)->notify_data);
            break;
        case NODE_POPUP:
            if (n->impl) free(((PopupSurface *)n->impl)->notify_data);
            break;
        case NODE_TOPLEVEL:
            if (n->impl) free(((WindowSurface *)n->impl)->notify_data);
            break;
        }
    }

    free(ph->nodes);
    pthread_mutex_destroy(&ph->request_lock);

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
