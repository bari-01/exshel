#ifndef __PLUGIN_H_
#define __PLUGIN_H_

#include "surface.h"
#include "surface/layer.h"
#include "surface/xdg_popup.h"
#include "surface/xdg_toplevel.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct Shell Shell;

typedef struct {
    uint32_t version;
    uint32_t size;
    const char *name;

    WaylandContext *wayland;

    WaylandSurface *(*surface_create)(Shell *shell);
    void (*surface_destroy)(WaylandSurface *surface);

    LayerSurface *(*layer_create)(Shell *shell, WaylandSurface *surface,
            enum zwlr_layer_shell_v1_layer layer, const char *namespace,
            uint32_t anchor, uint32_t width, uint32_t height,
            int32_t exclusive_zone);
    void (*layer_destroy)(LayerSurface *ls);

    PopupSurface *(*popup_create)(Shell *shell, WaylandSurface *surface,
            PopupParent *parent,
            int32_t anchor_x, int32_t anchor_y,
            int32_t anchor_width, int32_t anchor_height,
            uint32_t width, uint32_t height,
            enum xdg_positioner_anchor anchor,
            enum xdg_positioner_gravity gravity);
    void (*popup_destroy)(PopupSurface *ps);

    WindowSurface *(*window_create)(Shell *shell, WaylandSurface *surface,
            uint32_t width, uint32_t height,
            const char *title, const char *app_id);
    void (*window_destroy)(WindowSurface *ws);

    void (*log)(const char *fmt, ...);
    void (*quit)(Shell *shell);
} ShellAPI;

struct Shell {
    ShellAPI *api;
    WaylandContext ctx;
    void *plugin_data;
    bool running;
};

typedef struct {
    int (*init)(Shell *shell);
    void (*destroy)(Shell *shell);
} ShellPlugin;

bool shell_setup(Shell *shell);
void shell_teardown(Shell *shell);

#endif // __PLUGIN_H_
