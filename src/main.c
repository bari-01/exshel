#include <dlfcn.h>
#include <stdio.h>
#include "utils.h"
#include "plugin.h"

int
main(int argc, char *argv[])
{
    Shell shell;
    const char *plugin_path = argc > 1 ? argv[1] : "./build/bar.so";
    void *handle;
    ShellPlugin *plugin;

    log_debug("Starting host");

    if (!shell_setup(&shell)) {
        fprintf(stderr, "failed to setup shell\n");
        return 1;
    }

    handle = dlopen(plugin_path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        shell_teardown(&shell);
        return 1;
    }
    log_debug("Loaded plugin: %s", plugin_path);

    plugin = dlsym(handle, "shell_plugin");
    if (!plugin) {
        fprintf(stderr, "dlsym: %s\n", dlerror());
        dlclose(handle);
        shell_teardown(&shell);
        return 1;
    }

    if (plugin->init(&shell) != 0) {
        fprintf(stderr, "plugin init failed\n");
        dlclose(handle);
        shell_teardown(&shell);
        return 1;
    }
    log_debug("Plugin initialized");

    log_debug("Entering main loop");
    while (shell.running) {
        if (wl_display_dispatch(shell.ctx.display) < 0) {
            log_debug("Wayland dispatch error");
            break;
        }
    }

    log_debug("Exiting main loop");
    plugin->destroy(&shell);
    dlclose(handle);
    shell_teardown(&shell);
    return 0;
}
