#include "surface.h"

bool shell_init(WaylandSurface *s, const char *namespace, uint32_t height)
{

 return surface_init(s, ZWLR_LAYER_SHELL_V1_LAYER_TOP, namespace,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT, 0, height, height);
}
