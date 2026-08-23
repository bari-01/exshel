#include "plugin.h"
#include "utils.h"

#ifdef USE_WAYLAND
#include "backend/wayland/backend_wl.h"
#include "backend/wayland/context.h"
#endif

#ifdef USE_X11
#include "backend/x11/backend_x11.h"
#include "backend/x11/context.h"
#endif

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [--backend=wayland|x11] [plugin.so ...]\n",
            prog);
}

int
main(int argc, char *argv[])
{
    log_debug("Starting host");

    const char *backend_override = NULL;
    int plugin_start = 1;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--backend=", 10) == 0) {
            backend_override = argv[i] + 10;
            plugin_start = i + 1;
        }
    }

    BackendContext *backend = NULL;

#ifdef USE_WAYLAND
    static WlContext wl_ctx;
#endif
#ifdef USE_X11
    static X11Context x11_ctx;
#endif

    const char *chosen = backend_override;
    if (!chosen) {
#ifdef USE_WAYLAND
        if (getenv("WAYLAND_DISPLAY")) chosen = "wayland";
#endif
#ifdef USE_X11
        if (!chosen && getenv("DISPLAY")) chosen = "x11";
#endif
    }

    if (!chosen) {
        fprintf(stderr, "No display environment found. "
                        "Set WAYLAND_DISPLAY or DISPLAY, "
                        "or pass --backend=wayland|x11.\n");
        return 1;
    }

#ifdef USE_WAYLAND
    if (strcmp(chosen, "wayland") == 0) {
        if (!wl_context_init(&wl_ctx)) {
            fprintf(stderr, "Failed to init Wayland context\n");
            return 1;
        }
        backend = wl_backend_create(&wl_ctx);
        if (!backend) {
            wl_context_destroy(&wl_ctx);
            return 1;
        }
        log_debug("Using Wayland backend");
    }
#endif

#ifdef USE_X11
    if (!backend && strcmp(chosen, "x11") == 0) {
        if (!x11_context_init(&x11_ctx)) {
            fprintf(stderr, "Failed to init X11 context\n");
            return 1;
        }
        backend = x11_backend_create(&x11_ctx);
        if (!backend) {
            x11_context_destroy(&x11_ctx);
            return 1;
        }
        log_debug("Using X11 backend");
    }
#endif

    if (!backend) {
        fprintf(stderr, "Backend '%s' not compiled in.\n", chosen);
        usage(argv[0]);
        return 1;
    }

    PluginHandler ph;
    if (!plugin_handler_init(&ph, backend)) {
        fprintf(stderr, "Failed to init plugin handler\n");
        goto cleanup_backend;
    }

    bool any_loaded = false;
    for (int i = plugin_start; i < argc; i++) {
        if (strncmp(argv[i], "--", 2) == 0) continue;
        if (!plugin_handler_load(&ph, argv[i]))
            fprintf(stderr, "failed to load: %s\n", argv[i]);
        else
            any_loaded = true;
    }
    if (!any_loaded) {
        if (!plugin_handler_load(&ph, "./build/bar.so"))
            fprintf(stderr, "failed to load default bar.so\n");
    }

    log_debug("Entering main loop");

    struct pollfd fds[2] = {
            {.fd = backend->ops->get_fd(backend), .events = POLLIN},
            {.fd = ph.wake_fds[0], .events = POLLIN},
    };

    while (ph.running) {
        backend->ops->flush(backend);

        if (poll(fds, 2, -1) < 0) {
            log_debug("poll error");
            break;
        }

        if (fds[1].revents & POLLIN) {
            plugin_handler_drain_wake(&ph);
            plugin_handler_process_requests(&ph);
        }

        if (fds[0].revents & POLLIN) {
            if (backend->ops->dispatch(backend) < 0) {
                log_debug("Backend dispatch error");
                break;
            }
        }
    }

    log_debug("Exiting main loop");
    plugin_handler_teardown(&ph);

cleanup_backend:
    backend->ops->destroy(backend);

#ifdef USE_WAYLAND
    if (strcmp(chosen, "wayland") == 0) {
        wl_backend_free(backend);
        wl_context_destroy(&wl_ctx);
    }
#endif
#ifdef USE_X11
    if (strcmp(chosen, "x11") == 0) {
        x11_backend_free(backend);
        x11_context_destroy(&x11_ctx);
    }
#endif

    return 0;
}
