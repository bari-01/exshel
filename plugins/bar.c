#include "../src/plugin.h"
#include <stdlib.h>

typedef struct {
    WaylandSurface *ws;
    LayerSurface   *ls;
} BarState;

static int
bar_init(Shell *shell)
{
    BarState *state = calloc(1, sizeof(*state));
    if (!state) return -1;
    shell->plugin_data = state;

    state->ws = shell->api->surface_create(shell);
    if (!state->ws) return -1;

    state->ls = shell->api->layer_create(shell, state->ws,
            ZWLR_LAYER_SHELL_V1_LAYER_TOP, "vshell",
            ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
            ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
            ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
            0, 30, 30);
    if (!state->ls) return -1;

    shell->api->log("bar configured: %ux%u",
            state->ls->width, state->ls->height);
    return 0;
}

static void
bar_destroy(Shell *shell)
{
    BarState *state = shell->plugin_data;
    if (!state) return;

    shell->api->layer_destroy(state->ls);
    shell->api->surface_destroy(state->ws);
    free(state);
    shell->plugin_data = NULL;
}

ShellPlugin shell_plugin = {
    .init = bar_init,
    .destroy = bar_destroy,
};
