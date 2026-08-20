#include "../utils.h"
#include "layer.h"
#include <stdio.h>

static void
layer_surface_configure(void *data,
        struct zwlr_layer_surface_v1 *layer_surface, uint32_t serial,
        uint32_t width, uint32_t height)
{
    LayerSurface *ls = data;

    log_debug("layer_surface_configure: serial=%u, width=%u, height=%u",
            serial, width, height);

    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);

    if (ls->configured &&
            (ls->width != width || ls->height != height)) {
        log_debug("LayerSurface resize pending: %ux%u -> %ux%u",
                ls->width, ls->height, width, height);
        ls->resize_pending = true;
    }
    ls->width = width;
    ls->height = height;
    ls->configured = true;
}

static void
layer_surface_closed(void *data,
        struct zwlr_layer_surface_v1 *layer_surface)
{
    LayerSurface *ls = data;
    (void)layer_surface;
    log_debug("layer_surface_closed event received");
    ls->closed = true;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

bool
layer_surface_init(LayerSurface *ls, WaylandContext *ctx,
        WaylandSurface *s, enum zwlr_layer_shell_v1_layer layer,
        const char *namespace, uint32_t anchor, uint32_t width,
        uint32_t height, int32_t exclusive_zone)
{
    log_debug("Initializing LayerSurface: namespace=%s, layer=%d, anchor=%u, "
            "width=%u, height=%u, exclusive_zone=%d",
            namespace, layer, anchor, width, height, exclusive_zone);

    ls->ctx = ctx;
    ls->surface = s;
    check(!ctx->layer_shell, "compositor does not provide wlr-layer-shell\n");

    ls->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
            ctx->layer_shell, s->surface, NULL, layer, namespace);
    check(!ls->layer_surface, "failed to create layer surface\n");

    zwlr_layer_surface_v1_add_listener(ls->layer_surface,
            &layer_surface_listener, ls);
    zwlr_layer_surface_v1_set_size(ls->layer_surface,
            ls->width = width, ls->height = height);
    zwlr_layer_surface_v1_set_anchor(ls->layer_surface, anchor);
    zwlr_layer_surface_v1_set_exclusive_zone(ls->layer_surface,
            exclusive_zone);

    log_debug("Committing surface and waiting for initial configure");
    wl_surface_commit(s->surface);
    while (!ls->configured)
        check(wl_display_dispatch(ctx->display) < 0,
                "Wayland dispatch failed\n");

    log_debug("LayerSurface initialized: size=%ux%u",
            ls->width, ls->height);
    return true;
fail:
    log_debug("LayerSurface init failed");
    return false;
}

void
layer_surface_destroy(LayerSurface *ls)
{
    log_debug("Destroying LayerSurface");
    wl_destroy(ls->layer_surface, zwlr_layer_surface_v1_destroy);
}
