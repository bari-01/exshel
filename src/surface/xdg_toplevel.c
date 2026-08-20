#include "../utils.h"
#include "xdg_toplevel.h"
#include <stdio.h>
#include <stdint.h>
#include <wayland-client.h>

static void
xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
        uint32_t serial)
{
    WindowSurface *ws = data;
    log_debug("xdg_surface_configure (toplevel): serial=%u", serial);
    xdg_surface_ack_configure(xdg_surface, serial);
    ws->configured = true;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void
toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
        int32_t width, int32_t height, struct wl_array *states)
{
    WindowSurface *ws = data;
    (void)xdg_toplevel;
    (void)states;

    log_debug("toplevel_configure: width=%d, height=%d", width, height);

    if (ws->configured &&
            width > 0 && height > 0 &&
            ((uint32_t)width != ws->width ||
             (uint32_t)height != ws->height)) {
        log_debug("WindowSurface resize pending: %ux%u -> %dx%d",
                ws->width, ws->height, width, height);
        ws->resize_pending = true;
    }
    if (width > 0)
        ws->width = (uint32_t)width;
    if (height > 0)
        ws->height = (uint32_t)height;
}

static void
toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
    WindowSurface *ws = data;
    (void)xdg_toplevel;
    log_debug("toplevel_close event received");
    ws->closed = true;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = toplevel_configure,
    .close = toplevel_close,
};

bool
window_surface_init(WindowSurface *ws, WaylandContext *ctx,
        WaylandSurface *surface, uint32_t width, uint32_t height,
        const char *title, const char *app_id)
{
    log_debug("Initializing WindowSurface: %ux%u title=%s app_id=%s",
            width, height, title ? title : "(none)", app_id ? app_id : "(none)");

    ws->ctx = ctx;
    ws->surface = surface;
    ws->width = width;
    ws->height = height;
    check(!ctx->xdg_wm_base, "compositor does not provide xdg_wm_base\n");

    ws->xdg_surface = xdg_wm_base_get_xdg_surface(ctx->xdg_wm_base,
            surface->surface);
    check(!ws->xdg_surface, "failed to create xdg_surface\n");
    xdg_surface_add_listener(ws->xdg_surface, &xdg_surface_listener, ws);

    ws->xdg_toplevel = xdg_surface_get_toplevel(ws->xdg_surface);
    check(!ws->xdg_toplevel, "failed to create xdg_toplevel\n");
    xdg_toplevel_add_listener(ws->xdg_toplevel, &xdg_toplevel_listener, ws);

    if (title)
        xdg_toplevel_set_title(ws->xdg_toplevel, title);
    if (app_id)
        xdg_toplevel_set_app_id(ws->xdg_toplevel, app_id);

    log_debug("Committing window surface and waiting for initial configure");
    wl_surface_commit(surface->surface);
    while (!ws->configured)
        check(wl_display_dispatch(ctx->display) < 0,
                "Wayland dispatch failed\n");

    log_debug("WindowSurface initialized: size=%ux%u",
            ws->width, ws->height);
    return true;
fail:
    log_debug("WindowSurface init failed");
    window_surface_destroy(ws);
    return false;
}

void
window_surface_destroy(WindowSurface *ws)
{
    log_debug("Destroying WindowSurface");
    wl_destroy(ws->xdg_toplevel, xdg_toplevel_destroy);
    wl_destroy(ws->xdg_surface, xdg_surface_destroy);
}
