#include "utils.h"
#include "plugin.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static WaylandSurface *
api_surface_create(Shell *shell)
{
    WaylandSurface *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    if (!surface_init(s, &shell->ctx)) {
        free(s);
        return NULL;
    }
    log_debug("api: created WaylandSurface %p", (void *)s);
    return s;
}

static void
api_surface_destroy(WaylandSurface *s)
{
    if (!s) return;
    log_debug("api: destroying WaylandSurface %p", (void *)s);
    surface_destroy(s);
    free(s);
}

static LayerSurface *
api_layer_create(Shell *shell, WaylandSurface *surface,
        enum zwlr_layer_shell_v1_layer layer, const char *namespace,
        uint32_t anchor, uint32_t width, uint32_t height,
        int32_t exclusive_zone)
{
    LayerSurface *ls = calloc(1, sizeof(*ls));
    if (!ls) return NULL;
    if (!layer_surface_init(ls, &shell->ctx, surface, layer, namespace,
            anchor, width, height, exclusive_zone)) {
        free(ls);
        return NULL;
    }
    log_debug("api: created LayerSurface %p (%ux%u)", (void *)ls,
            ls->width, ls->height);
    return ls;
}

static void
api_layer_destroy(LayerSurface *ls)
{
    if (!ls) return;
    log_debug("api: destroying LayerSurface %p", (void *)ls);
    layer_surface_destroy(ls);
    free(ls);
}

static PopupSurface *
api_popup_create(Shell *shell, WaylandSurface *surface,
        PopupParent *parent,
        int32_t anchor_x, int32_t anchor_y,
        int32_t anchor_width, int32_t anchor_height,
        uint32_t width, uint32_t height,
        enum xdg_positioner_anchor anchor,
        enum xdg_positioner_gravity gravity)
{
    PopupSurface *ps = calloc(1, sizeof(*ps));
    if (!ps) return NULL;
    if (!popup_surface_init(ps, &shell->ctx, surface, parent,
            anchor_x, anchor_y, anchor_width, anchor_height,
            width, height, anchor, gravity)) {
        free(ps);
        return NULL;
    }
    log_debug("api: created PopupSurface %p (%ux%u at %d,%d)", (void *)ps,
            ps->width, ps->height, ps->x, ps->y);
    return ps;
}

static void
api_popup_destroy(PopupSurface *ps)
{
    if (!ps) return;
    log_debug("api: destroying PopupSurface %p", (void *)ps);
    popup_surface_destroy(ps);
    free(ps);
}

static WindowSurface *
api_window_create(Shell *shell, WaylandSurface *surface,
        uint32_t width, uint32_t height,
        const char *title, const char *app_id)
{
    WindowSurface *ws = calloc(1, sizeof(*ws));
    if (!ws) return NULL;
    if (!window_surface_init(ws, &shell->ctx, surface,
            width, height, title, app_id)) {
        free(ws);
        return NULL;
    }
    log_debug("api: created WindowSurface %p (%ux%u)", (void *)ws,
            ws->width, ws->height);
    return ws;
}

static void
api_window_destroy(WindowSurface *ws)
{
    if (!ws) return;
    log_debug("api: destroying WindowSurface %p", (void *)ws);
    window_surface_destroy(ws);
    free(ws);
}

static void
api_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[plugin] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

static void
api_quit(Shell *shell)
{
    log_debug("api: quit requested");
    shell->running = false;
}

static ShellAPI shell_api = {
    .version = 1,
    .size = sizeof(ShellAPI),
    .name = "vshell",
    .surface_create = api_surface_create,
    .surface_destroy = api_surface_destroy,
    .layer_create = api_layer_create,
    .layer_destroy = api_layer_destroy,
    .popup_create = api_popup_create,
    .popup_destroy = api_popup_destroy,
    .window_create = api_window_create,
    .window_destroy = api_window_destroy,
    .log = api_log,
    .quit = api_quit,
};

bool
shell_setup(Shell *shell)
{
    log_debug("Setting up shell");
    memset(shell, 0, sizeof(*shell));
    shell->api = &shell_api;
    shell->running = true;

    if (!context_init(&shell->ctx))
        return false;

    shell_api.wayland = &shell->ctx;
    log_debug("Shell setup complete");
    return true;
}

void
shell_teardown(Shell *shell)
{
    log_debug("Tearing down shell");
    context_destroy(&shell->ctx);
}
