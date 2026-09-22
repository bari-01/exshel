#ifndef __PLUGIN_H_
#define __PLUGIN_H_

#include "config.h"
#include "backend.h"
#include "plugin_api.h"
#include "renderer/vulkan/vulkan_runtime.h"
#include "widget/widget.h"

#include <pthread.h>
#include <stdbool.h>

typedef enum {
    NODE_LAYER,
    NODE_POPUP,
    NODE_TOPLEVEL,
} NodeKind;

typedef struct Node {
    bool in_use;
    uint32_t generation;
    NodeKind kind;
    void *priv;
    NotifyCtx *nc;
    uint32_t owner;
    SurfaceHandle parent;
    SurfaceCallbacks callbacks;
    Widget *root_widget;
    bool render_requested;

    VulkanContext vk;
    bool vk_initialized;
    RenderList render_list;
} Node;

typedef enum {
    REQ_LAYER_CREATE,
    REQ_POPUP_CREATE,
    REQ_TOPLEVEL_CREATE,
    REQ_DESTROY,
} ReqKind;

typedef struct Request {
    ReqKind kind;
    uint32_t owner;

    union {
        LayerCreateArgs layer;
        PopupCreateArgs popup;
        ToplevelCreateArgs toplevel;
        SurfaceHandle destroy_handle;
    } args;
    SurfaceCallbacks callbacks;

    pthread_mutex_t *done_lock;
    pthread_cond_t *done_cond;
    bool *done;
    SurfaceHandle *out;

    struct Request *next;
} Request;

typedef struct PluginHandler PluginHandler;

struct PluginHandle {
    PluginHandler *handler;
    uint32_t plugin_index;
    void *plugin_data;
};

typedef struct {
    ShellPlugin *plugin;
    void *dl_handle;
    PluginHandle handle;

    ShellConfig config; // subtree slice (root may be NULL)
    bool has_config;

    pthread_t thread;
    pthread_mutex_t event_lock;
    pthread_cond_t event_cond;
    SurfaceEvent *event_head;
    SurfaceEvent *event_tail;
    bool shutdown;
} LoadedPlugin;

struct PluginHandler {
    Context *backend;

    Node *nodes;
    uint32_t node_count;
    uint32_t node_capacity;

    LoadedPlugin *plugins;
    uint32_t plugin_count;

    pthread_mutex_t request_lock;
    Request *request_head;
    Request *request_tail;

    int wake_fds[2];
    bool running;

    pthread_mutex_t exit_lock;
    LoadedPlugin **exited_stack;
    uint32_t exited_count;

    ShellAPI api;
    CoreAPI core;
    SurfaceAPI surfaces;
    EWMHAPI ewmh_api; // optional
};

bool plugin_handler_init(PluginHandler *ph, Context *backend);
void plugin_handler_teardown(PluginHandler *ph);

void plugin_handler_process_requests(PluginHandler *ph);
void plugin_handler_wake(PluginHandler *ph);
void plugin_handler_drain_wake(PluginHandler *ph);

// post surface events (thread safe)
void plugin_handler_post_event(
    PluginHandler *ph, uint32_t owner, SurfaceEvent *ev);

Node *node_lookup(PluginHandler *ph, SurfaceHandle h);

void node_destroy_all_for(PluginHandler *ph, uint32_t owner);

// doc may be NULL
bool plugin_handler_load(
    PluginHandler *ph, const char *path, const ShellConfigDoc *doc);
void plugin_handler_unload_all(PluginHandler *ph);

void plugin_handler_render_all(PluginHandler *ph);

#endif // __PLUGIN_H_
