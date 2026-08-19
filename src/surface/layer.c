#include "../utils.h"
#include "layer.h"
#include "stdio.h"

static void
layer_surface_configure(void *data,
        struct zwlr_layer_surface_v1 *layer_surface, uint32_t serial,
        uint32_t width, uint32_t height)
{
    WaylandSurface *s = data;

    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);

    if (s->width != width || s->height != height) {
        s->width = width;
        s->height = height;
        s->resize_pending = true;
    }
    s->configured = true;
}

static void
layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *layer_surface)
{
    WaylandSurface *s = data;
    (void)layer_surface;
    s->closed = true;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};


bool
layer_surface_init(WaylandSurface *s, enum zwlr_layer_shell_v1_layer layer,
        const char *namespace, uint32_t anchor, uint32_t width,
        uint32_t height, int32_t exclusive_zone)
{
    s->role = SURFACE_LAYER;
    check(!s->layer_shell, "compositor does not provide wlr-layer-shell\n");

    s->layer_surface = zwlr_layer_shell_v1_get_layer_surface(s->layer_shell,
            s->surface, NULL, layer, namespace);
    check(!s->layer_surface, "failed to create layer surface\n");

    zwlr_layer_surface_v1_add_listener(s->layer_surface,
            &layer_surface_listener, s);
    zwlr_layer_surface_v1_set_size(s->layer_surface, s->width = width,
            s->height = height);

    zwlr_layer_surface_v1_set_anchor(s->layer_surface, anchor);
    zwlr_layer_surface_v1_set_exclusive_zone( s->layer_surface,
            exclusive_zone);
    wl_surface_commit(s->surface);
    while (!s->configured) 
        check(wl_display_dispatch( s->display) < 0,
                "Wayland dispatch failed\n");

    return true;
fail:
    // probably not supposed to destroy, just give up
    return false;
}

