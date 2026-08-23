#include "popup.h"
#include "../../utils.h"
#include <stdint.h>
#include <stdio.h>

static void
xdg_surface_configure(
        void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
    WlPopupNode *n = data;
    log_debug("xdg_surface_configure (popup): serial=%u", serial);
    xdg_surface_ack_configure(xdg_surface, serial);
    n->configured = true;
}

static const struct xdg_surface_listener xdg_surface_listener = {
        .configure = xdg_surface_configure,
};

static void
popup_configure(void *data, struct xdg_popup *xdg_popup, int32_t x, int32_t y,
        int32_t width, int32_t height)
{
    WlPopupNode *n = data;
    (void)xdg_popup;

    log_debug("popup_configure: x=%d, y=%d, width=%d, height=%d", x, y, width,
            height);

    if (n->configured &&
            (n->x != x || n->y != y || n->width != (uint32_t)width ||
                    n->height != (uint32_t)height)) {
        log_debug(
                "WlPopupNode geometry changed: (%d,%d %ux%u) -> (%d,%d %dx%d)",
                n->x, n->y, n->width, n->height, x, y, width, height);
        n->resize_pending = true;
    }
    n->x = x;
    n->y = y;
    n->width = (uint32_t)width;
    n->height = (uint32_t)height;

    if (n->nc && n->nc->post_popup_configure)
        n->nc->post_popup_configure(
                n->nc, x, y, (uint32_t)width, (uint32_t)height);
}

static void
popup_done(void *data, struct xdg_popup *xdg_popup)
{
    WlPopupNode *n = data;
    (void)xdg_popup;
    log_debug("popup_done event received");
    n->closed = true;

    if (n->nc && n->nc->post_popup_done) n->nc->post_popup_done(n->nc);
}

static const struct xdg_popup_listener popup_listener = {
        .configure = popup_configure,
        .popup_done = popup_done,
};

bool
wl_popup_node_init(WlPopupNode *n, WlContext *ctx, WlPopupParent *parent,
        int32_t anchor_x, int32_t anchor_y, int32_t anchor_width,
        int32_t anchor_height, uint32_t width, uint32_t height,
        enum xdg_positioner_anchor anchor, enum xdg_positioner_gravity gravity)
{
    struct xdg_positioner *positioner = NULL;

    log_debug("Initializing WlPopupNode: parent_type=%d, "
              "anchor_rect=(%d,%d,%d,%d), size=%ux%u, anchor=%u, gravity=%u",
            parent->type, anchor_x, anchor_y, anchor_width, anchor_height,
            width, height, anchor, gravity);

    n->ctx = ctx;
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

    n->xdg_surface =
            xdg_wm_base_get_xdg_surface(ctx->xdg_wm_base, n->surface.surface);
    check(!n->xdg_surface, "failed to create xdg_surface\n");
    xdg_surface_add_listener(n->xdg_surface, &xdg_surface_listener, n);

    switch (parent->type) {
    case WL_POPUP_PARENT_XDG:
        log_debug("Creating popup with xdg_surface parent");
        n->xdg_popup = xdg_surface_get_popup(
                n->xdg_surface, parent->xdg_surface, positioner);
        break;
    case WL_POPUP_PARENT_LAYER:
        log_debug("Creating popup with layer_surface parent");
        n->xdg_popup = xdg_surface_get_popup(n->xdg_surface, NULL, positioner);
        break;
    }
    check(!n->xdg_popup, "failed to create xdg_popup\n");
    xdg_popup_add_listener(n->xdg_popup, &popup_listener, n);

    if (parent->type == WL_POPUP_PARENT_LAYER)
        zwlr_layer_surface_v1_get_popup(parent->layer_surface, n->xdg_popup);

    xdg_positioner_destroy(positioner);
    positioner = NULL;

    n->width = width;
    n->height = height;

    log_debug("Committing popup surface and waiting for initial configure");
    wl_surface_commit(n->surface.surface);
    while (!n->configured)
        check(wl_display_dispatch(ctx->display) < 0,
                "Wayland dispatch failed\n");

    log_debug("WlPopupNode initialized: pos=(%d,%d) size=%ux%u", n->x, n->y,
            n->width, n->height);
    return true;
fail:
    log_debug("WlPopupNode init failed");
    if (positioner) xdg_positioner_destroy(positioner);
    wl_popup_node_destroy(n);
    return false;
}

void
wl_popup_node_destroy(WlPopupNode *n)
{
    log_debug("Destroying WlPopupNode");
    wl_destroy(n->xdg_popup, xdg_popup_destroy);
    wl_destroy(n->xdg_surface, xdg_surface_destroy);
}
