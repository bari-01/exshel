#include "../src/plugin_api.h"
#include "../src/config_query.h"
#include "../src/widget/widget.h"
#include "../src/font/font.h"
#include "../src/font/glyph_atlas.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    const char *font_path;
    float font_size;
    uint32_t text_color;
    uint32_t bg_color;
    float spacing;
    float padding_x;
    float padding_y;
    int height;
    const char *time_format;
} BarConfig;

static const BarConfig default_bar_config = {
    .font_path = "/usr/share/fonts/liberation-fonts/LiberationSans-Regular.ttf",
    .font_size = 20.0f,
    .text_color = 0xFFFFFFFF,
    .bg_color = 0x282A36FF,
    .spacing = 12.0f,
    .padding_x = 16.0f,
    .padding_y = 6.0f,
    .height = 36,
    .time_format = "%H:%M:%S",
};

typedef struct {
    const ShellAPI *api;
    PluginHandle *self;
    SurfaceHandle bar;

    BarConfig config;
    Font font;
    GlyphAtlas atlas;

    HBox root_box;
    RectangleWidget badge_pill;
    TextWidget clock_text;

    char time_buf[64];

    pthread_t worker_thread;
    bool worker_running;
} Bar2State;

static void
bar2_event(SurfaceHandle handle, const SurfaceEvent *event, void *user)
{
    Bar2State *state = user;
    (void)handle;

    switch (event->kind) {
    case SURFACE_EVENT_CONFIGURE:
        state->api->core->log("bar2 configure: %ux%u", event->configure.width,
            event->configure.height);
        state->root_box.base.width = (float)event->configure.width;
        state->root_box.base.height = (float)event->configure.height;
        widget_layout(&state->root_box.base);
        state->api->surfaces->request_render(state->self, state->bar);
        break;
    case SURFACE_EVENT_CLOSE:
        state->api->core->log("bar2 closed");
        state->api->core->quit(state->self);
        break;
    default:
        break;
    }
}

static void *
bar2_worker_thread(void *arg)
{
    Bar2State *state = arg;

    state->api->core->log("bar2 clock worker thread started");

    while (state->worker_running) {
        usleep(500000);
        if (!state->worker_running) break;

        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        if (tm_info) {
            strftime(state->time_buf, sizeof(state->time_buf),
                state->config.time_format, tm_info);
        } else {
            snprintf(state->time_buf, sizeof(state->time_buf), "00:00:00");
        }
        state->clock_text.text = state->time_buf;

        widget_layout(&state->root_box.base);

        state->api->surfaces->request_render(state->self, state->bar);

        // state->api->core->log(
        //     "bar2 tick: %s | widget size: %.1fx%.1f | pos: (%.1f, %.1f)",
        //     state->time_buf, state->clock_text.base.width,
        //     state->clock_text.base.height, state->clock_text.base.x,
        //     state->clock_text.base.y);
    }

    state->api->core->log("bar2 clock worker thread exiting");
    return NULL;
}

static int
bar2_init(const ShellAPI *api, PluginHandle *self, const ShellConfig *config)
{
    Bar2State *state = calloc(1, sizeof(*state));
    if (!state) return -1;
    state->api = api;
    state->self = self;
    api->core->set_data(self, state);

    BarConfig cfg = default_bar_config;
    cfg.font_path = cfg_str(config, "font", cfg.font_path);
    cfg.font_size = cfg_float(config, "font_size", cfg.font_size);
    cfg.text_color = cfg_color(config, "text_color", cfg.text_color);
    cfg.bg_color = cfg_color(config, "bg_color", cfg.bg_color);
    cfg.spacing = cfg_float(config, "spacing", cfg.spacing);
    cfg.padding_x = cfg_float(config, "padding_x", cfg.padding_x);
    cfg.padding_y = cfg_float(config, "padding_y", cfg.padding_y);
    cfg.height = cfg_int(config, "height", cfg.height);
    cfg.time_format = cfg_str(config, "time_format", cfg.time_format);
    state->config = cfg;

    api->core->log("bar2 initializing: font='%s', size=%.1f, format='%s'",
        state->config.font_path, state->config.font_size,
        state->config.time_format);

    if (!font_init(&state->font, state->config.font_path)) {
        api->core->log(
            "bar2 warning: failed to open font %s", state->config.font_path);
    }
    glyph_atlas_init(&state->atlas, 512, 512);

    state->bar = api->surfaces->layer_create(self,
        (LayerCreateArgs){
            .layer = SHELL_LAYER_TOP,
            .namespace = "exshel-bar2",
            .anchor = SHELL_ANCHOR_TOP | SHELL_ANCHOR_LEFT | SHELL_ANCHOR_RIGHT,
            .x = 0,
            .y = 0,
            .width = 0,
            .height = (uint32_t)state->config.height,
            .exclusive_zone = state->config.height,
        },
        (SurfaceCallbacks){
            .on_event = bar2_event,
            .user = state,
        });

    if (!surface_handle_valid(state->bar)) {
        api->core->log("bar2 error: failed to create layer surface");
        glyph_atlas_destroy(&state->atlas);
        font_destroy(&state->font);
        free(state);
        return -1;
    }

    hbox_init(&state->root_box, state->config.spacing);
    state->root_box.base.x = state->config.padding_x;
    state->root_box.base.y = state->config.padding_y;

    // rectangle_widget_init(&state->badge_pill, 0.0f, 0.0f, 10.0f, 20.0f,
    //     0x50FA7BFF, 4.0f); // don't have proper overlapping/center widget

    snprintf(state->time_buf, sizeof(state->time_buf), "00:00:00");
    text_widget_init(&state->clock_text, state->time_buf, &state->font,
        &state->atlas, state->config.font_size, state->config.text_color);

    // widget_add_child(&state->root_box.base, &state->badge_pill.base);
    widget_add_child(&state->root_box.base, &state->clock_text.base);

    api->surfaces->set_root_widget(self, state->bar, &state->root_box.base);

    state->root_box.base.width = 0.0f;
    state->root_box.base.height = (float)state->config.height;
    widget_layout(&state->root_box.base);

    api->core->log("bar2 initialized successfully: surface=%u/%u",
        state->bar.index, state->bar.generation);

    state->worker_running = true;
    if (pthread_create(
            &state->worker_thread, NULL, bar2_worker_thread, state) != 0) {
        api->core->log("bar2 error: failed to spawn worker thread");
        state->worker_running = false;
    }

    return 0;
}

static void
bar2_destroy(const ShellAPI *api, PluginHandle *self)
{
    Bar2State *state = api->core->get_data(self);
    if (!state) return;

    if (state->worker_running) {
        state->worker_running = false;
        pthread_join(state->worker_thread, NULL);
    }

    glyph_atlas_destroy(&state->atlas);
    font_destroy(&state->font);

    api->core->log("bar2 plugin destroyed");
    free(state);
}

ShellPlugin shell_plugin = {
    .abi_version = SHELL_ABI_VERSION,
    .name = "bar2",
    .version = "0.2",
    .init = bar2_init,
    .destroy = bar2_destroy,
};
