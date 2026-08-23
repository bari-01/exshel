#include "toplevel.h"
#include "../../utils.h"
#include <stdint.h>
#include <stdio.h>
#include <wayland-client.h>

static void
xdg_surface_configure(
        void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
    WlToplevelNode *n = data;
    log_debug("xdg_surface_configure (toplevel): serial=%u", serial);
    xdg_surface_ack_configure(xdg_surface, serial);
    n->configured = true;
}

static const struct xdg_surface_listener xdg_surface_listener = {
        .configure = xdg_surface_configure,
};

static void
toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width,
        int32_t height, struct wl_array *states)
{
    WlToplevelNode *n = data;
    (void)xdg_toplevel;
    (void)states;

    log_debug("toplevel_configure: width=%d, height=%d", width, height);

    if (n->configured && width > 0 && height > 0 &&
            ((uint32_t)width != n->width || (uint32_t)height != n->height)) {
        log_debug("WlToplevelNode resize pending: %ux%u -> %dx%d", n->width,
                n->height, width, height);
        n->resize_pending = true;
    }
    if (width > 0) n->width = (uint32_t)width;
    if (height > 0) n->height = (uint32_t)height;

    if (n->nc && n->nc->post_toplevel_configure)
        n->nc->post_toplevel_configure(n->nc, n->width, n->height);
}

static void
toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
    WlToplevelNode *n = data;
    (void)xdg_toplevel;
    log_debug("toplevel_close event received");
    n->closed = true;

    if (n->nc && n->nc->post_toplevel_close) n->nc->post_toplevel_close(n->nc);
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
        .configure = toplevel_configure,
        .close = toplevel_close,
};

bool
wl_toplevel_node_init(WlToplevelNode *n, WlContext *ctx, uint32_t width,
        uint32_t height, const char *title, const char *app_id)
{
    log_debug("Initializing WlToplevelNode: %ux%u title=%s app_id=%s", width,
            height, title ? title : "(none)", app_id ? app_id : "(none)");

    n->ctx = ctx;
    n->width = width;
    n->height = height;
    check(!ctx->xdg_wm_base, "compositor does not provide xdg_wm_base\n");

    n->xdg_surface =
            xdg_wm_base_get_xdg_surface(ctx->xdg_wm_base, n->surface.surface);
    check(!n->xdg_surface, "failed to create xdg_surface\n");
    xdg_surface_add_listener(n->xdg_surface, &xdg_surface_listener, n);

    n->xdg_toplevel = xdg_surface_get_toplevel(n->xdg_surface);
    check(!n->xdg_toplevel, "failed to create xdg_toplevel\n");
    xdg_toplevel_add_listener(n->xdg_toplevel, &xdg_toplevel_listener, n);

    if (title) xdg_toplevel_set_title(n->xdg_toplevel, title);
    if (app_id) xdg_toplevel_set_app_id(n->xdg_toplevel, app_id);

    log_debug("Committing window surface and waiting for initial configure");
    wl_surface_commit(n->surface.surface);
    while (!n->configured)
        check(wl_display_dispatch(ctx->display) < 0,
                "Wayland dispatch failed\n");

    log_debug("WlToplevelNode initialized: size=%ux%u", n->width, n->height);
    return true;
fail:
    log_debug("WlToplevelNode init failed");
    wl_toplevel_node_destroy(n);
    return false;
}

void
wl_toplevel_node_destroy(WlToplevelNode *n)
{
    log_debug("Destroying WlToplevelNode");
    wl_destroy(n->xdg_toplevel, xdg_toplevel_destroy);
    wl_destroy(n->xdg_surface, xdg_surface_destroy);
}
