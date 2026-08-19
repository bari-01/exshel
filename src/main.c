#include <stdio.h>
#include "surface/shell.h"

int
main(void)
{
    WaylandSurface surface;

    if (!shell_init(&surface, "vshell", 30))
        return 1;

    printf(
        "bar configured: %ux%u\n",
        surface.width,
        surface.height
    );

    while (!surface.closed) {
        if (wl_display_dispatch(surface.display) < 0)
            break;
    }

    surface_destroy(&surface);
    return 0;
}
