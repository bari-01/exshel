#include "../src/plugin_api.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    const ShellAPI *api;
    PluginHandle *self;
    SurfaceHandle bar;

    pthread_t worker_thread;
    bool worker_running;
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

static void *
bar_worker_thread(void *arg)
{
    BarState *state = arg;
    int count = 0;

    state->api->core->log("background worker thread started");

    while (state->worker_running) {
        sleep(2);
        if (!state->worker_running) break;

        count++;
        state->api->core->log("worker tick #%d from thread 0x%lx", count,
                (unsigned long)pthread_self());

        if (count == 2) {
            state->api->core->log("worker thread creating test popup...");
            SurfaceHandle popup = state->api->surfaces->popup_create(
                    state->self,
                    (PopupCreateArgs){
                            .parent = state->bar,
                            .anchor_x = 10,
                            .anchor_y = 30,
                            .anchor_width = 100,
                            .anchor_height = 10,
                            .width = 200,
                            .height = 100,
                            .anchor = XDG_POSITIONER_ANCHOR_BOTTOM_LEFT,
                            .gravity = XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT,
                    },
                    (SurfaceCallbacks){0});

            if (surface_handle_valid(popup)) {
                state->api->core->log(
                        "worker thread successfully created popup handle=%u/%u",
                        popup.index, popup.generation);
                sleep(2);
                state->api->surfaces->destroy(state->self, popup);
                state->api->core->log("worker thread destroyed popup");
            }
        }
    }

    state->api->core->log("background worker thread exiting");
    return NULL;
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
    if (!surface_handle_valid(state->bar)) {
        free(state);
        return -1;
    }

    api->core->log("bar created: handle=%u/%u", state->bar.index,
            state->bar.generation);

    state->worker_running = true;
    if (pthread_create(&state->worker_thread, NULL, bar_worker_thread, state) !=
            0) {
        api->core->log("failed to spawn worker thread");
        state->worker_running = false;
    }

    return 0;
}

static void
bar_destroy(const ShellAPI *api, PluginHandle *self)
{
    BarState *state = api->core->get_data(self);
    if (!state) return;

    if (state->worker_running) {
        state->worker_running = false;
        pthread_join(state->worker_thread, NULL);
    }

    /* surfaces are automatically cleaned up by host shutdown ordering */
    free(state);
}

ShellPlugin shell_plugin = {
        .name = "bar",
        .version = "0.1",
        .init = bar_init,
        .destroy = bar_destroy,
};
