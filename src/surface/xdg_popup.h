#ifndef __XDG_POPUP_H_
#define __XDG_POPUP_H_

#include <stdbool.h>
#include <stdint.h>
#include "../surface.h"

typedef enum {
    POPUP_PARENT_XDG,
    POPUP_PARENT_LAYER,
} PopupParentType;

typedef struct {
    PopupParentType type;
    union {
        struct xdg_surface           *xdg_surface;
        struct zwlr_layer_surface_v1 *layer_surface;
    };
} PopupParent;

typedef struct PopupSurface {
    WaylandSurface     *surface;
    WaylandContext     *ctx;
    struct xdg_surface *xdg_surface;
    struct xdg_popup   *xdg_popup;
    uint32_t width;
    uint32_t height;
    int32_t  x;
    int32_t  y;
    bool configured;
    bool closed;
    bool resize_pending;
} PopupSurface;

bool popup_surface_init(PopupSurface *ps, WaylandContext *ctx,
        WaylandSurface *surface, PopupParent *parent,
        int32_t anchor_x, int32_t anchor_y,
        int32_t anchor_width, int32_t anchor_height,
        uint32_t width, uint32_t height,
        enum xdg_positioner_anchor anchor,
        enum xdg_positioner_gravity gravity);
void popup_surface_destroy(PopupSurface *ps);

#endif // __XDG_POPUP_H_
