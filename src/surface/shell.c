#include "shell.h"
#include "layer.h"

bool shell_init(WaylandSurface *s, const char *namespace, uint32_t height)
{
    if (!surface_init(s))
        return false;

    return layer_surface_init(s, ZWLR_LAYER_SHELL_V1_LAYER_TOP, namespace,
            ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
            ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
            ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT, 0, height, height);
}
