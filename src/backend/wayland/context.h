#ifndef __WL_CONTEXT_H_
#define __WL_CONTEXT_H_

#include "../../../generated/wlr-layer-shell-unstable-v1-client-protocol.h"
#include "../../../generated/xdg-shell-client-protocol.h"
#include <stdbool.h>
#include <wayland-client.h>

typedef struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct zwlr_layer_shell_v1 *layer_shell;
    struct xdg_wm_base *xdg_wm_base;
} WlContext;

typedef struct {
    struct wl_surface *surface;
} WlSurface;

bool wl_context_init(WlContext *ctx);
void wl_context_destroy(WlContext *ctx);
bool wl_context_roundtrip(WlContext *ctx);

bool wl_surface_init(WlSurface *s, WlContext *ctx);
void wl_surface_fini(WlSurface *s);

#endif // __WL_CONTEXT_H_
