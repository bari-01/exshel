#include "plugin.h"
#include "utils.h"

#include <dlfcn.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// dispatch events (from plugin)
static void
dispatch_event(LoadedPlugin *lp, SurfaceEvent *ev)
{
    PluginHandler *ph = lp->handle.handler;
    Node *n = node_lookup(ph, ev->handle);
    if (!n || !n->callbacks.on_event) return;
    n->callbacks.on_event(ev->handle, ev, n->callbacks.user);
}

static void *
plugin_thread_main(void *arg)
{
    LoadedPlugin *lp = arg;
    PluginHandler *ph = lp->handle.handler;

    log_debug("plugin thread started: %s", lp->plugin->name);

    if (lp->plugin->init(
            &ph->api, &lp->handle, lp->has_config ? &lp->config : NULL) == 0) {
        log_debug("plugin initialized: %s", lp->plugin->name);

        while (!lp->shutdown) {
            pthread_mutex_lock(&lp->event_lock);
            while (!lp->event_head && !lp->shutdown)
                pthread_cond_wait(&lp->event_cond, &lp->event_lock);
            SurfaceEvent *ev = lp->event_head;
            if (ev) {
                lp->event_head = ev->next;
                if (!lp->event_head) lp->event_tail = NULL;
            }
            pthread_mutex_unlock(&lp->event_lock);

            if (ev) {
                dispatch_event(lp, ev);
                free(ev);
            }
        }

        log_debug("plugin thread shutting down: %s", lp->plugin->name);
        if (lp->plugin->destroy) lp->plugin->destroy(&ph->api, &lp->handle);
    } else {
        log_debug("plugin init failed: %s", lp->plugin->name);
    }

    pthread_mutex_lock(&ph->exit_lock);
    ph->exited_stack[ph->exited_count++] = lp;
    pthread_mutex_unlock(&ph->exit_lock);
    plugin_handler_wake(ph);

    return NULL;
}

bool
plugin_handler_load(
    PluginHandler *ph, const char *path, const ShellConfigDoc *doc)
{
    void *dl = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!dl) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return false;
    }

    ShellPlugin *sp = dlsym(dl, "shell_plugin");
    if (!sp) {
        fprintf(stderr, "dlsym: %s\n", dlerror());
        dlclose(dl);
        return false;
    }

    if (sp->abi_version != SHELL_ABI_VERSION) {
        fprintf(stderr,
            "plugin ABI mismatch for '%s': expected %u, got %u — refusing to "
            "load\n",
            path, SHELL_ABI_VERSION, sp->abi_version);
        dlclose(dl);
        return false;
    }

    uint32_t idx = ph->plugin_count;
    LoadedPlugin *new_arr =
        realloc(ph->plugins, (idx + 1) * sizeof(LoadedPlugin));
    if (!new_arr) {
        dlclose(dl);
        return false;
    }
    ph->plugins = new_arr;

    LoadedPlugin **new_stack =
        realloc(ph->exited_stack, (idx + 1) * sizeof(LoadedPlugin *));
    if (!new_stack) {
        dlclose(dl);
        return false;
    }
    ph->exited_stack = new_stack;

    ph->plugin_count = idx + 1;

    LoadedPlugin *lp = &ph->plugins[idx];
    memset(lp, 0, sizeof(*lp));
    lp->plugin = sp;
    lp->dl_handle = dl;
    lp->handle.handler = ph;
    lp->handle.plugin_index = idx;
    lp->handle.plugin_data = NULL;

    if (doc && shell_config_get_plugin(doc, sp->name, &lp->config)) {
        lp->has_config = true;
        log_debug("loaded config for plugin: %s", sp->name);
    }

    pthread_mutex_init(&lp->event_lock, NULL);
    pthread_cond_init(&lp->event_cond, NULL);

    log_debug("loaded plugin: %s (%s)", sp->name, path);

    if (pthread_create(&lp->thread, NULL, plugin_thread_main, lp) != 0) {
        fprintf(stderr, "pthread_create failed for plugin %s\n", sp->name);
        pthread_mutex_destroy(&lp->event_lock);
        pthread_cond_destroy(&lp->event_cond);
        dlclose(dl);
        ph->plugin_count--;
        return false;
    }

    return true;
}

void
plugin_handler_unload_all(PluginHandler *ph)
{
    if (ph->plugin_count == 0) return;

    for (uint32_t i = 0; i < ph->plugin_count; i++) {
        LoadedPlugin *lp = &ph->plugins[i];
        pthread_mutex_lock(&lp->event_lock);
        lp->shutdown = true;
        pthread_cond_signal(&lp->event_cond);
        pthread_mutex_unlock(&lp->event_lock);
    }

    // block on the requests channel, so a thread exiting and a thread waiting
    // both make progress here
    struct pollfd pfd = {.fd = ph->wake_fds[0], .events = POLLIN};
    pthread_mutex_lock(&ph->exit_lock);
    uint32_t done = ph->exited_count;
    pthread_mutex_unlock(&ph->exit_lock);

    while (done < ph->plugin_count) {
        poll(&pfd, 1, -1);
        plugin_handler_drain_wake(ph);
        plugin_handler_process_requests(ph);

        pthread_mutex_lock(&ph->exit_lock);
        done = ph->exited_count;
        pthread_mutex_unlock(&ph->exit_lock);
    }

    for (uint32_t i = 0; i < ph->plugin_count; i++) {
        pthread_join(ph->plugins[i].thread, NULL);
        node_destroy_all_for(ph, i);

        // drain remaining events
        SurfaceEvent *ev = ph->plugins[i].event_head;
        while (ev) {
            SurfaceEvent *next = ev->next;
            free(ev);
            ev = next;
        }

        pthread_mutex_destroy(&ph->plugins[i].event_lock);
        pthread_cond_destroy(&ph->plugins[i].event_cond);
        dlclose(ph->plugins[i].dl_handle);
        log_debug("unloaded plugin: %s", ph->plugins[i].plugin->name);
    }

    free(ph->plugins);
    ph->plugins = NULL;

    free(ph->exited_stack);
    ph->exited_stack = NULL;
    ph->exited_count = 0;

    ph->plugin_count = 0;
}
