#include "../src/plugin_api.h"
#include <stdlib.h>

typedef struct {
    const ShellAPI *api;
    PluginHandle *self;
    SurfaceHandle bar;
} BarState;

static void
bar_event(SurfaceHandle handle, const SurfaceEvent *event, void *user)
{
    BarState *state = user;
    (void)handle;

    switch (event->kind) {
    case SURFACE_EVENT_CONFIGURE:
        state->api->core->log("bar configure: %ux%u", event->configure.width,
                event->configure.height);
        break;
    case SURFACE_EVENT_CLOSE:
        state->api->core->log("bar closed");
        state->api->core->quit(state->self);
        break;
    default:
        break;
    }
}

static int
bar_init(const ShellAPI *api, PluginHandle *self, const ShellConfig *config)
{
    (void)config;

    BarState *state = calloc(1, sizeof(*state));
    if (!state) return -1;
    state->api = api;
    state->self = self;
    api->core->set_data(self, state);

    state->bar = api->surfaces->layer_create(self,
            (LayerCreateArgs){
                    .layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP,
                    .namespace = "exshel",
                    .anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                              ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                              ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
                    .width = 0,
                    .height = 30,
                    .exclusive_zone = 30,
            },
            (SurfaceCallbacks){
                    .on_event = bar_event,
                    .user = state,
            });
    if (!surface_handle_valid(state->bar)) return -1;

    api->core->log("bar created: handle=%u/%u", state->bar.index,
            state->bar.generation);
    return 0;
}

static void
bar_destroy(const ShellAPI *api, PluginHandle *self)
{
    BarState *state = api->core->get_data(self);
    if (!state) return;

    /* surfaces already destroyed by host shutdown ordering */
    free(state);
}

ShellPlugin shell_plugin = {
        .name = "bar",
        .version = "0.1",
        .init = bar_init,
        .destroy = bar_destroy,
};
