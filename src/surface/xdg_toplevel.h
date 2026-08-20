#ifndef __XDG_TOPLEVEL_H_
#define __XDG_TOPLEVEL_H_

#include <stdbool.h>
#include <stdint.h>
#include "../surface.h"

typedef struct WindowSurface {
    WaylandSurface      *surface;
    WaylandContext      *ctx;
    struct xdg_surface  *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    uint32_t width;
    uint32_t height;
    bool configured;
    bool closed;
    bool resize_pending;
} WindowSurface;

bool window_surface_init(WindowSurface *ws, WaylandContext *ctx,
        WaylandSurface *surface, uint32_t width, uint32_t height,
        const char *title, const char *app_id);
void window_surface_destroy(WindowSurface *ws);

#endif // __XDG_TOPLEVEL_H_
