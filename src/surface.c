#include "utils.h"
#include "surface.h"

#include <stdio.h>
#include <string.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

static void
xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    (void)data;
    log_debug("Received xdg_wm_base ping: serial=%u", serial);
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name,
        const char *interface, uint32_t version)
{
    WaylandContext *ctx = data;
    (void)version;

    log_debug("Registry global event: name=%u, interface=%s, version=%u",
            name, interface, version);

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        ctx->compositor = wl_registry_bind(registry, name,
                &wl_compositor_interface, 4);
        log_debug("Bound wl_compositor interface");
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        ctx->layer_shell = wl_registry_bind(registry, name,
                &zwlr_layer_shell_v1_interface, 1);
        log_debug("Bound zwlr_layer_shell_v1 interface");
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        ctx->xdg_wm_base = wl_registry_bind(registry, name,
                &xdg_wm_base_interface, 1);
        log_debug("Bound xdg_wm_base interface");
    }
}

static void
registry_global_remove(void *data, struct wl_registry *registry,
        uint32_t name)
{
    (void)data;
    (void)registry;
    (void)name;
    log_debug("Registry global remove event: name=%u", name);
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

bool
context_init(WaylandContext *ctx)
{
    log_debug("Initializing WaylandContext");
    memset(ctx, 0, sizeof(*ctx));
    ctx->display = wl_display_connect(NULL);
    check(!ctx->display, "failed to connect to Wayland\n");

    ctx->registry = wl_display_get_registry(ctx->display);
    wl_registry_add_listener(ctx->registry, &registry_listener, ctx);

    log_debug("Dispatching roundtrip");
    check(wl_display_roundtrip(ctx->display) < 0, "Wayland roundtrip failed\n");
    check(!ctx->compositor, "compositor does not provide wl_compositor\n");

    if (ctx->xdg_wm_base) {
        log_debug("Adding listener for xdg_wm_base");
        xdg_wm_base_add_listener(ctx->xdg_wm_base,
                &xdg_wm_base_listener, NULL);
    }

    log_debug("WaylandContext initialized");
    return true;
fail:
    log_debug("WaylandContext init failed");
    context_destroy(ctx);
    return false;
}

bool
context_roundtrip(WaylandContext *ctx)
{
    log_debug("Performing context roundtrip");
    return wl_display_roundtrip(ctx->display) >= 0;
}

void
context_destroy(WaylandContext *ctx)
{
    log_debug("Destroying WaylandContext");
    wl_destroy(ctx->xdg_wm_base, xdg_wm_base_destroy);
    wl_destroy(ctx->layer_shell, zwlr_layer_shell_v1_destroy);
    wl_destroy(ctx->compositor, wl_compositor_destroy);
    wl_destroy(ctx->registry, wl_registry_destroy);
    wl_destroy(ctx->display, wl_display_disconnect);
}

bool
surface_init(WaylandSurface *s, WaylandContext *ctx)
{
    log_debug("Initializing WaylandSurface");
    memset(s, 0, sizeof(*s));
    s->surface = wl_compositor_create_surface(ctx->compositor);
    check(!s->surface, "failed to create wl_surface\n");
    log_debug("WaylandSurface created: surface=%p", (void *)s->surface);
    return true;
fail:
    log_debug("WaylandSurface init failed");
    surface_destroy(s);
    return false;
}

void
surface_destroy(WaylandSurface *s)
{
    log_debug("Destroying WaylandSurface: surface=%p", (void *)s->surface);
    wl_destroy(s->surface, wl_surface_destroy);
}
