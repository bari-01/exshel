#ifndef __LAYER_H_
#define __LAYER_H_
#include "../surface.h"

static void layer_surface_configure(void *data,
        struct zwlr_layer_surface_v1 *layer_surface, uint32_t serial,
        uint32_t width, uint32_t height);

bool layer_surface_init(WaylandSurface *s, enum zwlr_layer_shell_v1_layer layer,
        const char *namespace, uint32_t anchor, uint32_t width,
        uint32_t height, int32_t exclusive_zone);

#endif // __LAYER_H_
