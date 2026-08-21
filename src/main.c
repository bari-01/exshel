#include "plugin.h"
#include "utils.h"
#include <poll.h>
#include <stdio.h>
#include <string.h>

int
main(int argc, char *argv[])
{
    WaylandContext ctx;
    PluginHandler ph;

    log_debug("Starting host");

    if (!context_init(&ctx)) {
        fprintf(stderr, "failed to init wayland context\n");
        return 1;
    }

    if (!plugin_handler_init(&ph, &ctx)) {
        fprintf(stderr, "failed to init plugin handler\n");
        context_destroy(&ctx);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (!plugin_handler_load(&ph, argv[i]))
            fprintf(stderr, "failed to load: %s\n", argv[i]);
    }
    if (argc <= 1) {
        if (!plugin_handler_load(&ph, "./build/bar.so"))
            fprintf(stderr, "failed to load default bar.so\n");
    }

    log_debug("Entering main loop");

    struct pollfd fds[2] = {
            {.fd = wl_display_get_fd(ctx.display), .events = POLLIN},
            {.fd = ph.wake_fds[0], .events = POLLIN},
    };

    while (ph.running) {
        wl_display_flush(ctx.display);

        if (poll(fds, 2, -1) < 0) {
            log_debug("poll error");
            break;
        }

        if (fds[1].revents & POLLIN) {
            plugin_handler_drain_wake(&ph);
            plugin_handler_process_requests(&ph);
        }

        if (fds[0].revents & POLLIN) {
            if (wl_display_dispatch(ctx.display) < 0) {
                log_debug("Wayland dispatch error");
                break;
            }
        }
    }

    log_debug("Exiting main loop");
    plugin_handler_teardown(&ph);
    context_destroy(&ctx);
    return 0;
}
