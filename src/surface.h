#ifndef __SURFACE_H__
#define __SURFACE_H__

#include <stdint.h>
#include <stdbool.h>

#include <wayland-client.h>
#include "../generated/wlr-layer-shell-unstable-v1-client-protocol.h"

typedef struct {
    struct wl_display *display;
    struct wl_registry *registry;

    struct wl_compositor *compositor;
    struct zwlr_layer_shell_v1 *layer_shell;

    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer_surface;

    uint32_t width;
    uint32_t height;

    bool configured;
    bool closed;
    bool resize_pending;
} WaylandSurface;

bool surface_init(WaylandSurface *s);
void surface_destroy(WaylandSurface *s);

bool surface_roundtrip(WaylandSurface *s);

#endif // __SURFACE_H__
