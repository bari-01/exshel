#include "backend_wl.h"
#include "../plugin.h"
#include "../utils.h"
#include "context.h"
#include "layer.h"
#include "popup.h"
#include "toplevel.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

// enum maps
static enum zwlr_layer_shell_v1_layer
map_shell_layer(ShellLayer l)
{
    switch (l) {
    case SHELL_LAYER_BACKGROUND:
        return ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
    case SHELL_LAYER_BOTTOM:
        return ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
    case SHELL_LAYER_TOP:
        return ZWLR_LAYER_SHELL_V1_LAYER_TOP;
    case SHELL_LAYER_OVERLAY:
        return ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
    }
    return ZWLR_LAYER_SHELL_V1_LAYER_TOP;
}

static uint32_t
map_shell_anchor(uint32_t anchor)
{
    uint32_t r = 0;
    if (anchor & SHELL_ANCHOR_TOP) r |= ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
    if (anchor & SHELL_ANCHOR_BOTTOM) r |= ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
    if (anchor & SHELL_ANCHOR_LEFT) r |= ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
    if (anchor & SHELL_ANCHOR_RIGHT) r |= ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
    return r;
}

static enum xdg_positioner_anchor
map_popup_anchor(PopupAnchor a)
{
    switch (a) {
    case POPUP_ANCHOR_NONE:
        return XDG_POSITIONER_ANCHOR_NONE;
    case POPUP_ANCHOR_TOP:
        return XDG_POSITIONER_ANCHOR_TOP;
    case POPUP_ANCHOR_BOTTOM:
        return XDG_POSITIONER_ANCHOR_BOTTOM;
    case POPUP_ANCHOR_LEFT:
        return XDG_POSITIONER_ANCHOR_LEFT;
    case POPUP_ANCHOR_RIGHT:
        return XDG_POSITIONER_ANCHOR_RIGHT;
    case POPUP_ANCHOR_TOP_LEFT:
        return XDG_POSITIONER_ANCHOR_TOP_LEFT;
    case POPUP_ANCHOR_BOTTOM_LEFT:
        return XDG_POSITIONER_ANCHOR_BOTTOM_LEFT;
    case POPUP_ANCHOR_TOP_RIGHT:
        return XDG_POSITIONER_ANCHOR_TOP_RIGHT;
    case POPUP_ANCHOR_BOTTOM_RIGHT:
        return XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT;
    }
    return XDG_POSITIONER_ANCHOR_NONE;
}

static enum xdg_positioner_gravity
map_popup_gravity(PopupGravity g)
{
    switch (g) {
    case POPUP_GRAVITY_NONE:
        return XDG_POSITIONER_GRAVITY_NONE;
    case POPUP_GRAVITY_TOP:
        return XDG_POSITIONER_GRAVITY_TOP;
    case POPUP_GRAVITY_BOTTOM:
        return XDG_POSITIONER_GRAVITY_BOTTOM;
    case POPUP_GRAVITY_LEFT:
        return XDG_POSITIONER_GRAVITY_LEFT;
    case POPUP_GRAVITY_RIGHT:
        return XDG_POSITIONER_GRAVITY_RIGHT;
    case POPUP_GRAVITY_TOP_LEFT:
        return XDG_POSITIONER_GRAVITY_TOP_LEFT;
    case POPUP_GRAVITY_BOTTOM_LEFT:
        return XDG_POSITIONER_GRAVITY_BOTTOM_LEFT;
    case POPUP_GRAVITY_TOP_RIGHT:
        return XDG_POSITIONER_GRAVITY_TOP_RIGHT;
    case POPUP_GRAVITY_BOTTOM_RIGHT:
        return XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT;
    }
    return XDG_POSITIONER_GRAVITY_NONE;
}

static bool
wl_backend_init(Context *bc)
{
    (void)bc;
    return true;
}

static void
wl_backend_destroy(Context *bc)
{ (void)bc; }

static int
wl_backend_get_fd(Context *bc)
{
    WlContext *ctx = bc->priv;
    return wl_display_get_fd(ctx->display);
}

static int
wl_backend_prepare(Context *bc)
{
    WlContext *ctx = bc->priv;
    while (wl_display_prepare_read(ctx->display) != 0) {
        if (wl_display_dispatch_pending(ctx->display) < 0) return -1;
    }
    return 0;
}

static int
wl_backend_dispatch(Context *bc)
{
    WlContext *ctx = bc->priv;
    if (wl_display_read_events(ctx->display) < 0) {
        if (errno != EAGAIN) return -1;
    }
    if (wl_display_dispatch_pending(ctx->display) < 0) return -1;
    return 0;
}

static void
wl_backend_cancel(Context *bc)
{
    WlContext *ctx = bc->priv;
    wl_display_cancel_read(ctx->display);
}

static void
wl_backend_flush(Context *bc)
{
    WlContext *ctx = bc->priv;
    wl_display_flush(ctx->display);
}

static bool
wl_layer_create(Context *bc, Node *n, const LayerCreateArgs *a, NotifyCtx *nc)
{
    WlContext *ctx = bc->priv;
    WlLayerNode *wln = calloc(1, sizeof(*wln));
    if (!wln) return false;

    if (!wl_surface_init(&wln->surface, ctx)) {
        free(wln);
        return false;
    }

    wln->nc = nc;

    if (!wl_layer_node_init(wln, ctx, map_shell_layer(a->layer), a->namespace,
            map_shell_anchor(a->anchor), a->width, a->height,
            a->exclusive_zone)) {
        wl_surface_fini(&wln->surface);
        free(wln);
        return false;
    }

    n->priv = wln;
    log_debug("wl_layer_create: node priv=%p size=%ux%u", (void *)wln,
        wln->width, wln->height);
    return true;
}

static bool
wl_popup_create(
    Context *bc, Node *n, const PopupCreateArgs *a, Node *parent, NotifyCtx *nc)
{
    WlContext *ctx = bc->priv;
    WlPopupNode *wpn = calloc(1, sizeof(*wpn));
    if (!wpn) return false;

    if (!wl_surface_init(&wpn->surface, ctx)) {
        free(wpn);
        return false;
    }

    wpn->nc = nc;

    WlPopupParent pp;
    switch (parent->kind) {
    case NODE_LAYER: {
        WlLayerNode *pln = parent->priv;
        pp.type = WL_POPUP_PARENT_LAYER;
        pp.layer_surface = pln->layer_surface;
        break;
    }
    case NODE_POPUP: {
        WlPopupNode *ppn = parent->priv;
        pp.type = WL_POPUP_PARENT_XDG;
        pp.xdg_surface = ppn->xdg_surface;
        break;
    }
    case NODE_TOPLEVEL: {
        WlToplevelNode *ptn = parent->priv;
        pp.type = WL_POPUP_PARENT_XDG;
        pp.xdg_surface = ptn->xdg_surface;
        break;
    }
    default:
        wl_surface_fini(&wpn->surface);
        free(wpn);
        return false;
    }

    if (!wl_popup_node_init(wpn, ctx, &pp, a->anchor_x, a->anchor_y,
            a->anchor_width, a->anchor_height, a->width, a->height,
            map_popup_anchor(a->anchor), map_popup_gravity(a->gravity))) {
        wl_surface_fini(&wpn->surface);
        free(wpn);
        return false;
    }

    n->priv = wpn;
    log_debug("wl_popup_create: node priv=%p pos=(%d,%d) size=%ux%u",
        (void *)wpn, wpn->x, wpn->y, wpn->width, wpn->height);
    return true;
}

static bool
wl_toplevel_create(
    Context *bc, Node *n, const ToplevelCreateArgs *a, NotifyCtx *nc)
{
    WlContext *ctx = bc->priv;
    WlToplevelNode *wtn = calloc(1, sizeof(*wtn));
    if (!wtn) return false;

    if (!wl_surface_init(&wtn->surface, ctx)) {
        free(wtn);
        return false;
    }

    wtn->nc = nc;

    if (!wl_toplevel_node_init(
            wtn, ctx, a->width, a->height, a->title, a->app_id)) {
        wl_surface_fini(&wtn->surface);
        free(wtn);
        return false;
    }

    n->priv = wtn;
    log_debug("wl_toplevel_create: node priv=%p size=%ux%u", (void *)wtn,
        wtn->width, wtn->height);
    return true;
}

static void
wl_node_destroy(Context *bc, Node *n)
{
    (void)bc;
    if (!n->priv) return;

    switch (n->kind) {
    case NODE_LAYER: {
        WlLayerNode *wln = n->priv;
        wl_layer_node_destroy(wln);
        wl_surface_fini(&wln->surface);
        free(wln);
        break;
    }
    case NODE_POPUP: {
        WlPopupNode *wpn = n->priv;
        wl_popup_node_destroy(wpn);
        wl_surface_fini(&wpn->surface);
        free(wpn);
        break;
    }
    case NODE_TOPLEVEL: {
        WlToplevelNode *wtn = n->priv;
        wl_toplevel_node_destroy(wtn);
        wl_surface_fini(&wtn->surface);
        free(wtn);
        break;
    }
    }
    n->priv = NULL;
}

static void *
wl_get_native_display(Context *bc)
{
    WlContext *ctx = bc->priv;
    return ctx ? ctx->display : NULL;
}

static void *
wl_get_native_window(Context *bc, Node *n)
{
    (void)bc;
    if (!n || !n->priv) return NULL;
    switch (n->kind) {
    case NODE_LAYER: {
        WlLayerNode *wln = n->priv;
        return wln->surface.surface;
    }
    case NODE_POPUP: {
        WlPopupNode *wpn = n->priv;
        return wpn->surface.surface;
    }
    case NODE_TOPLEVEL: {
        WlToplevelNode *wtn = n->priv;
        return wtn->surface.surface;
    }
    }
    return NULL;
}

static void
wl_get_dimensions(Context *bc, Node *n, uint32_t *w, uint32_t *h)
{
    (void)bc;
    if (w) *w = 0;
    if (h) *h = 0;
    if (!n || !n->priv) return;
    switch (n->kind) {
    case NODE_LAYER: {
        WlLayerNode *wln = n->priv;
        if (w) *w = wln->width;
        if (h) *h = wln->height;
        break;
    }
    case NODE_POPUP: {
        WlPopupNode *wpn = n->priv;
        if (w) *w = wpn->width;
        if (h) *h = wpn->height;
        break;
    }
    case NODE_TOPLEVEL: {
        WlToplevelNode *wtn = n->priv;
        if (w) *w = wtn->width;
        if (h) *h = wtn->height;
        break;
    }
    }
}

static int
wl_get_display_type(Context *bc)
{
    (void)bc;
    return 0; /* DISPLAY_WAYLAND */
}

static const Ops wl_ops = {
    .init = wl_backend_init,
    .destroy = wl_backend_destroy,
    .get_fd = wl_backend_get_fd,
    .prepare = wl_backend_prepare,
    .dispatch = wl_backend_dispatch,
    .cancel = wl_backend_cancel,
    .flush = wl_backend_flush,
    .layer_create = wl_layer_create,
    .popup_create = wl_popup_create,
    .toplevel_create = wl_toplevel_create,
    .node_destroy = wl_node_destroy,
    .ewmh_get_active_window = NULL,
    .ewmh_get_client_list = NULL,
    .get_native_display = wl_get_native_display,
    .get_native_window = wl_get_native_window,
    .get_dimensions = wl_get_dimensions,
    .get_display_type = wl_get_display_type,
};

Context *
wl_backend_create(WlContext *ctx)
{
    Context *bc = calloc(1, sizeof(*bc));
    if (!bc) return NULL;
    bc->ops = &wl_ops;
    bc->priv = ctx;
    return bc;
}

void
wl_backend_free(Context *bc)
{ free(bc); }