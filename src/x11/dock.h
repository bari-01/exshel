#ifndef __X11_DOCK_H_
#define __X11_DOCK_H_

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
} X11DockNode;

bool x11_dock_node_init(X11DockNode *dn, X11Context *ctx, int32_t x, int32_t y,
    uint32_t width, uint32_t height, uint32_t anchor, int32_t exclusive_zone,
    const char *name);

void x11_dock_node_destroy(X11DockNode *dn, X11Context *ctx);

#endif // __X11_DOCK_H_
