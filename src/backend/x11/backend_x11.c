#include "backend_x11.h"
#include "../../plugin.h"
#include "../../utils.h"
#include "context.h"
#include "dock.h"
#include "popup.h"
#include "toplevel.h"

#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>

static void
dispatch_node_configure(Node *n, uint32_t width, uint32_t height)
{
    switch (n->kind) {
    case NODE_LAYER: {
        X11DockNode *dn = n->priv;
        dn->width = width;
        dn->height = height;
        if (dn->nc && dn->nc->post_configure)
            dn->nc->post_configure(dn->nc, width, height);
        break;
    }
    case NODE_POPUP: {
        X11PopupNode *pn = n->priv;
        pn->width = width;
        pn->height = height;
        if (pn->nc && pn->nc->post_popup_configure)
            pn->nc->post_popup_configure(pn->nc, pn->x, pn->y, width, height);
        break;
    }
    case NODE_TOPLEVEL: {
        X11ToplevelNode *tn = n->priv;
        tn->width = width;
        tn->height = height;
        if (tn->nc && tn->nc->post_toplevel_configure)
            tn->nc->post_toplevel_configure(tn->nc, width, height);
        break;
    }
    }
}

static void
dispatch_node_close(Node *n)
{
    switch (n->kind) {
    case NODE_LAYER: {
        X11DockNode *dn = n->priv;
        if (dn->nc && dn->nc->post_close) dn->nc->post_close(dn->nc);
        break;
    }
    case NODE_POPUP: {
        X11PopupNode *pn = n->priv;
        if (pn->nc && pn->nc->post_popup_done) pn->nc->post_popup_done(pn->nc);
        break;
    }
    case NODE_TOPLEVEL: {
        X11ToplevelNode *tn = n->priv;
        if (tn->nc && tn->nc->post_toplevel_close)
            tn->nc->post_toplevel_close(tn->nc);
        break;
    }
    }
}

static int
x11_backend_dispatch(BackendContext *bc)
{
    X11Context *ctx = bc->priv;
    xcb_generic_event_t *ev;

    while ((ev = xcb_poll_for_event(ctx->conn))) {
        uint8_t type = ev->response_type & ~0x80;

        switch (type) {
        case XCB_EXPOSE: {
            xcb_expose_event_t *e = (xcb_expose_event_t *)ev;
            if (e->count != 0) break; /* only final expose */
            Node *n = x11_win_lookup(ctx, e->window);
            if (n && n->priv)
                dispatch_node_configure(n, ((X11DockNode *)n->priv)->width,
                        ((X11DockNode *)n->priv)->height);
            break;
        }
        case XCB_CONFIGURE_NOTIFY: {
            xcb_configure_notify_event_t *e =
                    (xcb_configure_notify_event_t *)ev;
            Node *n = x11_win_lookup(ctx, e->window);
            if (n && n->priv) {
                /* Update stored position for popups */
                if (n->kind == NODE_POPUP) {
                    X11PopupNode *pn = n->priv;
                    pn->x = e->x;
                    pn->y = e->y;
                } else if (n->kind == NODE_TOPLEVEL) {
                    X11ToplevelNode *tn = n->priv;
                    tn->x = e->x;
                    tn->y = e->y;
                } else {
                    X11DockNode *dn = n->priv;
                    dn->x = e->x;
                    dn->y = e->y;
                }
                dispatch_node_configure(
                        n, (uint32_t)e->width, (uint32_t)e->height);
            }
            break;
        }
        case XCB_CLIENT_MESSAGE: {
            xcb_client_message_event_t *e = (xcb_client_message_event_t *)ev;
            if (e->data.data32[0] == ctx->wm_delete_window) {
                Node *n = x11_win_lookup(ctx, e->window);
                if (n && n->priv) dispatch_node_close(n);
            }
            break;
        }
        case XCB_UNMAP_NOTIFY: {
            xcb_unmap_notify_event_t *e = (xcb_unmap_notify_event_t *)ev;
            Node *n = x11_win_lookup(ctx, e->window);
            /* Treat unmap of a popup as popup_done */
            if (n && n->priv && n->kind == NODE_POPUP) dispatch_node_close(n);
            break;
        }
        default:
            break;
        }
        free(ev);
    }

    return xcb_connection_has_error(ctx->conn) ? -1 : 0;
}

static bool
x11_backend_init(BackendContext *bc)
{
    (void)bc;
    return true;
}

static void
x11_backend_destroy(BackendContext *bc)
{ (void)bc; }

static int
x11_backend_get_fd(BackendContext *bc)
{
    X11Context *ctx = bc->priv;
    return xcb_get_file_descriptor(ctx->conn);
}

static void
x11_backend_flush(BackendContext *bc)
{
    X11Context *ctx = bc->priv;
    xcb_flush(ctx->conn);
}

static bool
x11_layer_create(
        BackendContext *bc, Node *n, const LayerCreateArgs *a, NotifyCtx *nc)
{
    X11Context *ctx = bc->priv;
    X11DockNode *dn = calloc(1, sizeof(*dn));
    if (!dn) return false;

    dn->nc = nc;

    if (!x11_dock_node_init(dn, ctx, a->x, a->y, a->width, a->height, a->anchor,
                a->exclusive_zone, a->namespace)) {
        free(dn);
        return false;
    }

    x11_win_register(ctx, dn->window, n);
    n->priv = dn;

    if (nc && nc->post_configure) nc->post_configure(nc, a->width, a->height);

    log_debug("x11_layer_create: win=0x%x size=%ux%u", dn->window, a->width,
            a->height);
    return true;
}

static bool
x11_popup_create(BackendContext *bc, Node *n, const PopupCreateArgs *a,
        Node *parent, NotifyCtx *nc)
{
    X11Context *ctx = bc->priv;
    X11PopupNode *pn = calloc(1, sizeof(*pn));
    if (!pn) return false;

    pn->nc = nc;

    int32_t px = 0, py = 0;
    if (parent && parent->priv) {
        switch (parent->kind) {
        case NODE_LAYER: {
            X11DockNode *pd = parent->priv;
            px = pd->x;
            py = pd->y;
            break;
        }
        case NODE_POPUP: {
            X11PopupNode *pp = parent->priv;
            px = pp->x;
            py = pp->y;
            break;
        }
        case NODE_TOPLEVEL: {
            X11ToplevelNode *pt = parent->priv;
            px = pt->x;
            py = pt->y;
            break;
        }
        }
    }

    int32_t rx, ry;
    x11_popup_compute_position(px, py, a->anchor_x, a->anchor_y,
            a->anchor_width, a->anchor_height, a->width, a->height, a->anchor,
            a->gravity, &rx, &ry);

    if (!x11_popup_node_init(pn, ctx, rx, ry, a->width, a->height)) {
        free(pn);
        return false;
    }

    x11_win_register(ctx, pn->window, n);
    n->priv = pn;

    if (nc && nc->post_popup_configure)
        nc->post_popup_configure(nc, rx, ry, a->width, a->height);

    log_debug("x11_popup_create: win=0x%x pos=(%d,%d) size=%ux%u", pn->window,
            rx, ry, a->width, a->height);
    return true;
}

static bool
x11_toplevel_create(
        BackendContext *bc, Node *n, const ToplevelCreateArgs *a, NotifyCtx *nc)
{
    X11Context *ctx = bc->priv;
    X11ToplevelNode *tn = calloc(1, sizeof(*tn));
    if (!tn) return false;

    tn->nc = nc;

    if (!x11_toplevel_node_init(
                tn, ctx, a->width, a->height, a->title, a->app_id)) {
        free(tn);
        return false;
    }

    x11_win_register(ctx, tn->window, n);
    n->priv = tn;

    if (nc && nc->post_toplevel_configure)
        nc->post_toplevel_configure(nc, a->width, a->height);

    log_debug("x11_toplevel_create: win=0x%x size=%ux%u", tn->window, a->width,
            a->height);
    return true;
}

static void
x11_node_destroy(BackendContext *bc, Node *n)
{
    X11Context *ctx = bc->priv;
    if (!n->priv) return;

    switch (n->kind) {
    case NODE_LAYER: {
        X11DockNode *dn = n->priv;
        x11_win_unregister(ctx, dn->window);
        x11_dock_node_destroy(dn, ctx);
        free(dn);
        break;
    }
    case NODE_POPUP: {
        X11PopupNode *pn = n->priv;
        x11_win_unregister(ctx, pn->window);
        x11_popup_node_destroy(pn, ctx);
        free(pn);
        break;
    }
    case NODE_TOPLEVEL: {
        X11ToplevelNode *tn = n->priv;
        x11_win_unregister(ctx, tn->window);
        x11_toplevel_node_destroy(tn, ctx);
        free(tn);
        break;
    }
    }
    n->priv = NULL;
}

static bool
x11_ewmh_get_active_window(BackendContext *bc, uint32_t *out_wid)
{
    X11Context *ctx = bc->priv;
    xcb_get_property_cookie_t cookie =
            xcb_ewmh_get_active_window(&ctx->ewmh, ctx->screen_nbr);
    xcb_window_t active;
    if (!xcb_ewmh_get_active_window_reply(&ctx->ewmh, cookie, &active, NULL))
        return false;
    *out_wid = (uint32_t)active;
    return true;
}

static bool
x11_ewmh_get_client_list(
        BackendContext *bc, uint32_t **out_wids, uint32_t *out_count)
{
    X11Context *ctx = bc->priv;
    xcb_get_property_cookie_t cookie =
            xcb_ewmh_get_client_list(&ctx->ewmh, ctx->screen_nbr);
    xcb_ewmh_get_windows_reply_t reply;
    if (!xcb_ewmh_get_client_list_reply(&ctx->ewmh, cookie, &reply, NULL))
        return false;

    *out_count = reply.windows_len;
    *out_wids = malloc(reply.windows_len * sizeof(uint32_t));
    if (*out_wids)
        memcpy(*out_wids, reply.windows,
                reply.windows_len * sizeof(xcb_window_t));
    xcb_ewmh_get_windows_reply_wipe(&reply);
    return *out_wids != NULL;
}

static const BackendOps x11_ops = {
        .init = x11_backend_init,
        .destroy = x11_backend_destroy,
        .get_fd = x11_backend_get_fd,
        .dispatch = x11_backend_dispatch,
        .flush = x11_backend_flush,
        .layer_create = x11_layer_create,
        .popup_create = x11_popup_create,
        .toplevel_create = x11_toplevel_create,
        .node_destroy = x11_node_destroy,
        .ewmh_get_active_window = x11_ewmh_get_active_window,
        .ewmh_get_client_list = x11_ewmh_get_client_list,
};

BackendContext *
x11_backend_create(X11Context *ctx)
{
    BackendContext *bc = calloc(1, sizeof(*bc));
    if (!bc) return NULL;
    bc->ops = &x11_ops;
    bc->priv = ctx;
    return bc;
}

void
x11_backend_free(BackendContext *bc)
{ free(bc); }