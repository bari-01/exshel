#ifndef __SURFACE_H_
#define __SURFACE_H_

#include <stdbool.h>

#include <wayland-client.h>
#include "../generated/wlr-layer-shell-unstable-v1-client-protocol.h"
#include "../generated/xdg-shell-client-protocol.h"

typedef struct WaylandContext {
    struct wl_display          *display;
    struct wl_registry         *registry;
    struct wl_compositor       *compositor;
    struct zwlr_layer_shell_v1 *layer_shell;
    struct xdg_wm_base         *xdg_wm_base;
} WaylandContext;

typedef struct WaylandSurface {
    struct wl_surface *surface;
} WaylandSurface;

bool context_init(WaylandContext *ctx);
void context_destroy(WaylandContext *ctx);
bool context_roundtrip(WaylandContext *ctx);

bool surface_init(WaylandSurface *s, WaylandContext *ctx);
void surface_destroy(WaylandSurface *s);

#endif // __SURFACE_H_
