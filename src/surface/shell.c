#include "shell.h"
#include "../utils.h"
#include "layer.h"

bool
shell_init(WaylandContext *ctx, WaylandSurface *ws, LayerSurface *ls,
        const char *namespace, uint32_t height)
{
    log_debug("Initializing shell: namespace=%s, height=%u", namespace, height);

    if (!context_init(ctx)) {
        log_debug("shell_init: context_init failed");
        return false;
    }
    if (!surface_init(ws, ctx)) {
        log_debug("shell_init: surface_init failed");
        return false;
    }
    if (!layer_surface_init(ls, ctx, ws, ZWLR_LAYER_SHELL_V1_LAYER_TOP,
                namespace,
                ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                        ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
                0, height, height)) {
        log_debug("shell_init: layer_surface_init failed");
        return false;
    }

    log_debug("shell_init: shell initialized");
    return true;
}

void
shell_destroy(WaylandContext *ctx, WaylandSurface *ws, LayerSurface *ls)
{
    log_debug("Destroying shell");
    layer_surface_destroy(ls);
    surface_destroy(ws);
    context_destroy(ctx);
}
