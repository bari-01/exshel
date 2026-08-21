#ifndef __PLUGIN_H_
#define __PLUGIN_H_

#include "plugin_api.h"
#include "surface.h"
#include "surface/layer.h"
#include "surface/xdg_popup.h"
#include "surface/xdg_toplevel.h"

#include <pthread.h>
#include <stdbool.h>

typedef enum {
    NODE_LAYER,
    NODE_POPUP,
    NODE_TOPLEVEL,
} NodeKind;

typedef struct {
    bool in_use;
    uint32_t generation;
    NodeKind kind;
    WaylandSurface *wl_surface;
    void *impl;     // LayerSurface*, PopupSurface*, etc.
    uint32_t owner; // index into plugins array 
    SurfaceHandle parent;
    SurfaceCallbacks callbacks;
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

// loaded plugins

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

    pthread_t thread;
    pthread_mutex_t event_lock;
    pthread_cond_t event_cond;
    SurfaceEvent *event_head;
    SurfaceEvent *event_tail;
    bool shutdown;
} LoadedPlugin;

struct PluginHandler {
    WaylandContext *ctx;

    Node *nodes;
    uint32_t node_count;
    uint32_t node_capacity;

    LoadedPlugin *plugins;
    uint32_t plugin_count;

    pthread_mutex_t request_lock;
    Request *request_head;
    Request *request_tail;

    int wake_fds[2];

    ShellAPI api;
    CoreAPI core;
    SurfaceAPI surfaces;

    bool running;
};

bool plugin_handler_init(PluginHandler *ph, WaylandContext *ctx);
void plugin_handler_teardown(PluginHandler *ph);

bool plugin_handler_load(PluginHandler *ph, const char *path);
void plugin_handler_unload_all(PluginHandler *ph);

void plugin_handler_process_requests(PluginHandler *ph);
void plugin_handler_wake(PluginHandler *ph);
void plugin_handler_drain_wake(PluginHandler *ph);

Node *node_lookup(PluginHandler *ph, SurfaceHandle h);

#endif // __PLUGIN_H_
