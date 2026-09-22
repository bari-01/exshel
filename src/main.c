#include "config.h"
#include "plugin.h"
#include "utils.h"

#ifdef USE_WAYLAND
#include "wayland/backend_wl.h"
#include "wayland/context.h"
#endif

#ifdef USE_X11
#include "x11/backend_x11.h"
#include "x11/context.h"
#endif

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [--backend=wayland|x11] [--config=<path.json5>] "
        "<plugin.so> ...\n",
        prog);
}

int
main(int argc, char *argv[])
{
    log_debug("Starting host");

    const char *backend_override = NULL;
    const char *config_path = NULL;

    const char *plugin_paths[64];
    int plugin_count = 0;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--backend=", 10) == 0) {
            backend_override = argv[i] + 10;
        } else if (strncmp(argv[i], "--config=", 9) == 0) {
            config_path = argv[i] + 9;
        } else if (strncmp(argv[i], "--", 2) == 0) {
            fprintf(stderr, "unknown flag: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        } else {
            if (plugin_count < 64) plugin_paths[plugin_count++] = argv[i];
        }
    }

    if (plugin_count == 0) {
        fprintf(stderr, "no plugins specified\n");
        usage(argv[0]);
        return 1;
    }

    Context *backend = NULL;

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

    ShellConfigDoc *cfg_doc = NULL;
    if (config_path) {
        cfg_doc = shell_config_load(config_path);
        if (!cfg_doc) {
            fprintf(stderr, "Failed to load config: %s\n", config_path);
            goto cleanup_backend;
        }
        log_debug("Loaded config from: %s", config_path);
    }

    PluginHandler ph;
    if (!plugin_handler_init(&ph, backend)) {
        fprintf(stderr, "Failed to init plugin handler\n");
        goto cleanup_config;
    }

    for (int i = 0; i < plugin_count; i++) {
        if (!plugin_handler_load(&ph, plugin_paths[i], cfg_doc))
            fprintf(stderr, "failed to load: %s\n", plugin_paths[i]);
    }

    log_debug("Entering main loop");

    struct pollfd fds[2] = {
        {.fd = backend->ops->get_fd(backend), .events = POLLIN},
        {.fd = ph.wake_fds[0], .events = POLLIN},
    };

    while (ph.running) {
        if (backend->ops->prepare && backend->ops->prepare(backend) < 0) {
            log_debug("Backend prepare error");
            break;
        }

        backend->ops->flush(backend);

        if (poll(fds, 2, -1) < 0) {
            if (errno == EINTR) continue;
            log_debug("poll error");
            break;
        }

        if (fds[0].revents & POLLIN) {
            if (backend->ops->dispatch(backend) < 0) {
                log_debug("Backend dispatch error");
                break;
            }
        } else {
            if (backend->ops->cancel) { backend->ops->cancel(backend); }
        }

        if (fds[1].revents & POLLIN) {
            plugin_handler_drain_wake(&ph);
            plugin_handler_process_requests(&ph);
        }

        plugin_handler_render_all(&ph);
    }

    log_debug("Exiting main loop");
    plugin_handler_teardown(&ph);

cleanup_config:
    shell_config_free(cfg_doc);

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
