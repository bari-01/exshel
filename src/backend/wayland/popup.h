#ifndef __WL_POPUP_H_
#define __WL_POPUP_H_

#include "../../backend/backend.h"
#include "context.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WL_POPUP_PARENT_XDG,
    WL_POPUP_PARENT_LAYER,
} WlPopupParentType;

typedef struct {
    WlPopupParentType type;
    union {
        struct xdg_surface *xdg_surface;
        struct zwlr_layer_surface_v1 *layer_surface;
    };
} WlPopupParent;

typedef struct {
    WlSurface surface;
    WlContext *ctx;
    struct xdg_surface *xdg_surface;
    struct xdg_popup *xdg_popup;
    uint32_t width;
    uint32_t height;
    int32_t x;
    int32_t y;
    bool configured;
    bool closed;
    bool resize_pending;

    NotifyCtx *nc;
} WlPopupNode;

bool wl_popup_node_init(WlPopupNode *n, WlContext *ctx, WlPopupParent *parent,
        int32_t anchor_x, int32_t anchor_y, int32_t anchor_width,
        int32_t anchor_height, uint32_t width, uint32_t height,
        enum xdg_positioner_anchor anchor, enum xdg_positioner_gravity gravity);

void wl_popup_node_destroy(WlPopupNode *n);

#endif // __WL_POPUP_H_
