#ifndef __X11_POPUP_H_
#define __X11_POPUP_H_

#include "../backend.h"
#include "context.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    xcb_window_t window;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    NotifyCtx *nc;
} X11PopupNode;

void x11_popup_compute_position(int32_t parent_x, int32_t parent_y,
    int32_t anchor_x, int32_t anchor_y, int32_t anchor_w, int32_t anchor_h,
    uint32_t popup_w, uint32_t popup_h, PopupAnchor anchor,
    PopupGravity gravity, int32_t *out_x, int32_t *out_y);

bool x11_popup_node_init(X11PopupNode *pn, X11Context *ctx, int32_t x,
    int32_t y, uint32_t width, uint32_t height);

void x11_popup_node_destroy(X11PopupNode *pn, X11Context *ctx);

#endif
