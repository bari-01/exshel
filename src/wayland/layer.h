#ifndef __WL_LAYER_H_
#define __WL_LAYER_H_

#include "../backend.h"
#include "context.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    WlSurface surface;
    WlContext *ctx;
    struct zwlr_layer_surface_v1 *layer_surface;
    uint32_t width;
    uint32_t height;
    bool configured;
    bool closed;
    bool resize_pending;

    NotifyCtx *nc;
} WlLayerNode;

bool wl_layer_node_init(WlLayerNode *n, WlContext *ctx,
    enum zwlr_layer_shell_v1_layer layer, const char *namespace,
    uint32_t anchor, uint32_t width, uint32_t height, int32_t exclusive_zone);

void wl_layer_node_destroy(WlLayerNode *n);

#endif // __WL_LAYER_H_
