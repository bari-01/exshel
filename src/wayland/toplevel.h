#ifndef __WL_TOPLEVEL_H_
#define __WL_TOPLEVEL_H_

#include "../backend.h"
#include "context.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    WlSurface surface;
    WlContext *ctx;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    uint32_t width;
    uint32_t height;
    bool configured;
    bool closed;
    bool resize_pending;

    NotifyCtx *nc;
} WlToplevelNode;

bool wl_toplevel_node_init(WlToplevelNode *n, WlContext *ctx, uint32_t width,
    uint32_t height, const char *title, const char *app_id);

void wl_toplevel_node_destroy(WlToplevelNode *n);

#endif // __WL_TOPLEVEL_H_
