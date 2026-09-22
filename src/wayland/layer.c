#include "layer.h"
#include "../utils.h"
#include <stdio.h>

static void
layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *layer_surface,
    uint32_t serial, uint32_t width, uint32_t height)
{
    WlLayerNode *n = data;
    log_debug("layer_surface_configure: serial=%u, width=%u, height=%u", serial,
        width, height);

    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);

    if (n->configured && (n->width != width || n->height != height)) {
        log_debug("WlLayerNode resize pending: %ux%u -> %ux%u", n->width,
            n->height, width, height);
        n->resize_pending = true;
    }
    n->width = width;
    n->height = height;
    n->configured = true;

    if (n->nc && n->nc->post_configure)
        n->nc->post_configure(n->nc, width, height);
}

static void
layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *layer_surface)
{
    WlLayerNode *n = data;
    (void)layer_surface;
    log_debug("layer_surface_closed event received");
    n->closed = true;

    if (n->nc && n->nc->post_close) n->nc->post_close(n->nc);
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

bool
wl_layer_node_init(WlLayerNode *n, WlContext *ctx,
    enum zwlr_layer_shell_v1_layer layer, const char *namespace,
    uint32_t anchor, uint32_t width, uint32_t height, int32_t exclusive_zone)
{
    log_debug("Initializing WlLayerNode: namespace=%s, layer=%d, anchor=%u, "
              "width=%u, height=%u, exclusive_zone=%d",
        namespace, layer, anchor, width, height, exclusive_zone);

    n->ctx = ctx;
    check(!ctx->layer_shell, "compositor does not provide wlr-layer-shell\n");

    n->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        ctx->layer_shell, n->surface.surface, NULL, layer, namespace);
    check(!n->layer_surface, "failed to create layer surface\n");

    zwlr_layer_surface_v1_add_listener(
        n->layer_surface, &layer_surface_listener, n);
    zwlr_layer_surface_v1_set_size(
        n->layer_surface, n->width = width, n->height = height);
    zwlr_layer_surface_v1_set_anchor(n->layer_surface, anchor);
    zwlr_layer_surface_v1_set_exclusive_zone(n->layer_surface, exclusive_zone);

    log_debug("Committing surface and waiting for initial configure");
    wl_surface_commit(n->surface.surface);
    while (!n->configured)
        check(
            wl_display_dispatch(ctx->display) < 0, "Wayland dispatch failed\n");

    log_debug("WlLayerNode initialized: size=%ux%u", n->width, n->height);
    return true;
fail:
    log_debug("WlLayerNode init failed");
    return false;
}

void
wl_layer_node_destroy(WlLayerNode *n)
{
    log_debug("Destroying WlLayerNode");
    wl_destroy(n->layer_surface, zwlr_layer_surface_v1_destroy);
}
