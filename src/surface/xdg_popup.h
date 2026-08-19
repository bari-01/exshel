#ifndef __XDG_POPUP_H_
#define __XDG_POPUP_H_
#include "../surface.h"

static void
popup_surface_configure(void *data, struct xdg_surface *xdg_surface,
        uint32_t serial, uint32_t width, uint32_t height);

bool popup_surface_init(WaylandSurface *popup, WaylandSurface *parent,
    int32_t anchor_x, int32_t anchor_y, int32_t anchor_width,
    int32_t anchor_height, uint32_t width, uint32_t height,
    enum xdg_positioner_anchor anchor, enum xdg_positioner_gravity gravity);

#endif // __XDG_POPUP_H_
