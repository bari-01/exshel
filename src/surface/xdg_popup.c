#include "xdg_popup.h"
#include "../utils.h"
#include <stdint.h>
#include <stdio.h>

static void
xdg_surface_configure(
        void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
    PopupSurface *ps = data;
    log_debug("xdg_surface_configure (popup): serial=%u", serial);
    xdg_surface_ack_configure(xdg_surface, serial);
    ps->configured = true;
}

static const struct xdg_surface_listener xdg_surface_listener = {
        .configure = xdg_surface_configure,
};

static void
popup_configure(void *data, struct xdg_popup *xdg_popup, int32_t x, int32_t y,
        int32_t width, int32_t height)
{
    PopupSurface *ps = data;
    (void)xdg_popup;

    log_debug("popup_configure: x=%d, y=%d, width=%d, height=%d", x, y, width,
            height);

    if (ps->configured &&
            (ps->x != x || ps->y != y || ps->width != (uint32_t)width ||
                    ps->height != (uint32_t)height)) {
        log_debug(
                "PopupSurface geometry changed: (%d,%d %ux%u) -> (%d,%d %dx%d)",
                ps->x, ps->y, ps->width, ps->height, x, y, width, height);
        ps->resize_pending = true;
    }
    ps->x = x;
    ps->y = y;
    ps->width = (uint32_t)width;
    ps->height = (uint32_t)height;

    if (ps->on_configure)
        ps->on_configure(
                ps->notify_data, x, y, (uint32_t)width, (uint32_t)height);
}

static void
popup_done(void *data, struct xdg_popup *xdg_popup)
{
    PopupSurface *ps = data;
    (void)xdg_popup;
    log_debug("popup_done event received");
    ps->closed = true;

    if (ps->on_closed) ps->on_closed(ps->notify_data);
}

static const struct xdg_popup_listener popup_listener = {
        .configure = popup_configure,
        .popup_done = popup_done,
};

bool
popup_surface_init(PopupSurface *ps, WaylandContext *ctx,
        WaylandSurface *surface, PopupParent *parent, int32_t anchor_x,
        int32_t anchor_y, int32_t anchor_width, int32_t anchor_height,
        uint32_t width, uint32_t height, enum xdg_positioner_anchor anchor,
        enum xdg_positioner_gravity gravity)
{
    struct xdg_positioner *positioner = NULL;

    log_debug("Initializing PopupSurface: parent_type=%d, "
              "anchor_rect=(%d,%d,%d,%d), size=%ux%u, anchor=%u, gravity=%u",
            parent->type, anchor_x, anchor_y, anchor_width, anchor_height,
            width, height, anchor, gravity);

    ps->ctx = ctx;
    ps->surface = surface;
    check(!ctx->xdg_wm_base, "compositor does not provide xdg_wm_base\n");

    positioner = xdg_wm_base_create_positioner(ctx->xdg_wm_base);
    check(!positioner, "failed to create xdg_positioner\n");
    xdg_positioner_set_size(positioner, (int32_t)width, (int32_t)height);
    xdg_positioner_set_anchor_rect(
            positioner, anchor_x, anchor_y, anchor_width, anchor_height);
    xdg_positioner_set_anchor(positioner, anchor);
    xdg_positioner_set_gravity(positioner, gravity);
    xdg_positioner_set_constraint_adjustment(
            positioner, XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X |
                                XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y |
                                XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X |
                                XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y);

    ps->xdg_surface =
            xdg_wm_base_get_xdg_surface(ctx->xdg_wm_base, surface->surface);
    check(!ps->xdg_surface, "failed to create xdg_surface\n");
    xdg_surface_add_listener(ps->xdg_surface, &xdg_surface_listener, ps);

    switch (parent->type) {
    case POPUP_PARENT_XDG:
        log_debug("Creating popup with xdg_surface parent");
        ps->xdg_popup = xdg_surface_get_popup(
                ps->xdg_surface, parent->xdg_surface, positioner);
        break;
    case POPUP_PARENT_LAYER:
        log_debug("Creating popup with layer_surface parent");
        ps->xdg_popup =
                xdg_surface_get_popup(ps->xdg_surface, NULL, positioner);
        break;
    }
    check(!ps->xdg_popup, "failed to create xdg_popup\n");
    xdg_popup_add_listener(ps->xdg_popup, &popup_listener, ps);

    if (parent->type == POPUP_PARENT_LAYER)
        zwlr_layer_surface_v1_get_popup(parent->layer_surface, ps->xdg_popup);

    xdg_positioner_destroy(positioner);
    positioner = NULL;

    ps->width = width;
    ps->height = height;

    log_debug("Committing popup surface and waiting for initial configure");
    wl_surface_commit(surface->surface);
    while (!ps->configured)
        check(wl_display_dispatch(ctx->display) < 0,
                "Wayland dispatch failed\n");

    log_debug("PopupSurface initialized: pos=(%d,%d) size=%ux%u", ps->x, ps->y,
            ps->width, ps->height);
    return true;
fail:
    log_debug("PopupSurface init failed");
    if (positioner) xdg_positioner_destroy(positioner);
    popup_surface_destroy(ps);
    return false;
}

void
popup_surface_destroy(PopupSurface *ps)
{
    log_debug("Destroying PopupSurface");
    wl_destroy(ps->xdg_popup, xdg_popup_destroy);
    wl_destroy(ps->xdg_surface, xdg_surface_destroy);
}
