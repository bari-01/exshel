#include "utils.h"
#include "surface.h"

#include <stdint.h>
#include <stdio.h>
//#include <stdlib.h>
#include <string.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name,
        const char *interface, uint32_t version)
{
    WaylandSurface *s = data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        s->compositor = wl_registry_bind(registry, name,
                &wl_compositor_interface, 4);

    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        s->layer_shell = wl_registry_bind(registry, name,
                &zwlr_layer_shell_v1_interface, 1);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        s->xdg_wm_base = wl_registry_bind(registry, name,
                &xdg_wm_base_interface, 1);
    }
}

static void
registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

bool
surface_init(WaylandSurface *s)
{
    memset(s, 0, sizeof(*s));
    s->display = wl_display_connect(NULL);
    check(!s->display, "failed to connect to Wayland\n");

    s->registry = wl_display_get_registry(s->display);
    wl_registry_add_listener(s->registry, &registry_listener, s);

    check(wl_display_roundtrip(s->display) < 0, "Wayland roundtrip failed\n");
    check(!s->compositor, "compositor does not provide wl_compositor\n");

    s->surface = wl_compositor_create_surface(s->compositor);
    check(!s->surface, "failed to create wl_surface\n");

    return true;
fail:
    surface_destroy(s);
    return false;
}

bool
surface_roundtrip(WaylandSurface *s)
{
    return wl_display_roundtrip(s->display) >= 0;
}

void
surface_destroy(WaylandSurface *s)
{
    destroy(s->layer_surface, zwlr_layer_surface_v1_destroy);
    destroy(s->surface, wl_surface_destroy);
    destroy(s->layer_shell, zwlr_layer_shell_v1_destroy);
    destroy(s->compositor, wl_compositor_destroy);
    destroy(s->registry, wl_registry_destroy);
    destroy(s->display, wl_display_disconnect);
}
