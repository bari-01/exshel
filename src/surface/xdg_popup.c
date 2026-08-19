#include "../utils.h"
#include "xdg_popup.h"
#include "stdio.h"
#include <stdarg.h>
#include <stdint.h>

static void
popup_surface_configure(void *data, struct xdg_surface *popup_surface,
        uint32_t serial, uint32_t width, uint32_t height)
{
    WaylandSurface *s = data;

    xdg_surface_ack_configure(popup_surface, serial);

    if (s->width != width || s->height != height || s->x != x || s->y != y) {
        s->width = width;
        s->height = height;
        s->x;
        s->y;
        s->resize_pending = true;
    }
    s->configured = true;
}

static void
popup_surface_closed(void *data, struct zwlr_layer_surface_v1 *layer_surface)
{
    WaylandSurface *s = data;
    (void)layer_surface;
    s->closed = true;
}

static const struct xdg_surface_listener popup_surface_listener = {
    .configure = popup_surface_configure,
    .closed = popup_surface_closed,
};


bool popup_surface_init(WaylandSurface *popup, WaylandSurface *parent,
    int32_t anchor_x, int32_t anchor_y, int32_t anchor_width,
    int32_t anchor_height, uint32_t width, uint32_t height,
    enum xdg_positioner_anchor anchor, enum xdg_positioner_gravity gravity)
{
    popup->role = SURFACE_POPUP;
    check(!popup->xdg_wm_base, "compositor does not provide xdg_wm_base\n");

    popup->popup_surface = xdg_wm_base_get_xdg_surface(popup->xdg_wm_base,
            popup->surface);
    check(!popup->popup_surface, "failed to create xdg surface\n");

    xdg_surface_add_listener(popup->popup_surface, &popup_surface_listener,
            popup);
    xdg_surface_set_window_geometry(popup->popup_surface, popup->x = x,
            popup->y = y,
            popup->width = width, popup->height = height);

    wl_surface_commit(popup->surface);
    while (!popup->configured) 
        check(wl_display_dispatch( popup->display) < 0,
                "Wayland dispatch failed\n");

    return true;
fail:
    // probably not supposed to destroy, just give up
    return false;
}

