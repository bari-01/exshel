#ifndef __X11_TOPLEVEL_H_
#define __X11_TOPLEVEL_H_

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
} X11ToplevelNode;

bool x11_toplevel_node_init(X11ToplevelNode *tn, X11Context *ctx,
    uint32_t width, uint32_t height, const char *title, const char *app_id);

void x11_toplevel_node_destroy(X11ToplevelNode *tn, X11Context *ctx);

#endif
