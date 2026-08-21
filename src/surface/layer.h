#ifndef __LAYER_H_
#define __LAYER_H_

#include "../surface.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct LayerSurface {
    WaylandSurface *surface;
    WaylandContext *ctx;
    struct zwlr_layer_surface_v1 *layer_surface;
    uint32_t width;
    uint32_t height;
    bool configured;
    bool closed;
    bool resize_pending;

    void (*on_configure)(void *data, uint32_t width, uint32_t height);
    void (*on_closed)(void *data);
    void *notify_data;
} LayerSurface;

bool layer_surface_init(LayerSurface *ls, WaylandContext *ctx,
        WaylandSurface *s, enum zwlr_layer_shell_v1_layer layer,
        const char *namespace, uint32_t anchor, uint32_t width, uint32_t height,
        int32_t exclusive_zone);
void layer_surface_destroy(LayerSurface *ls);

#endif // __LAYER_H_
