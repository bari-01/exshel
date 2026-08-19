#ifndef __SURFACE_H_
#define __SURFACE_H_

#include <stdint.h>
#include <stdbool.h>

#include <sys/types.h>
#include <wayland-client.h>
#include "../generated/wlr-layer-shell-unstable-v1-client-protocol.h"
#include "../generated/xdg-shell-client-protocol.h"

enum surface_role {
    SURFACE,
    SURFACE_LAYER,
    SURFACE_POPUP,
};

typedef struct WaylandSurface {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_surface *surface;

    struct zwlr_layer_shell_v1 *layer_shell;
    struct zwlr_layer_surface_v1 *layer_surface;

    struct xdg_wm_base *xdg_wm_base;
    struct xdg_surface *popup_surface;
    struct xdg_popup *popup;
    uint32_t x;
    uint32_t y;

    enum surface_role role;

    uint32_t width;
    uint32_t height;

    bool configured;
    bool closed;
    bool resize_pending;
} WaylandSurface;

bool surface_init(WaylandSurface *s);
void surface_destroy(WaylandSurface *s);

bool surface_roundtrip(WaylandSurface *s);

#endif // __SURFACE_H_
