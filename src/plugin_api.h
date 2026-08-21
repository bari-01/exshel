#ifndef __PLUGIN_API_H_
#define __PLUGIN_API_H_

#include "../generated/wlr-layer-shell-unstable-v1-client-protocol.h"
#include "../generated/xdg-shell-client-protocol.h"
#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>

typedef struct PluginHandle PluginHandle;
typedef struct ShellConfig ShellConfig;

typedef struct {
    uint32_t index;
    uint32_t generation;
} SurfaceHandle;

#define SURFACE_HANDLE_INVALID ((SurfaceHandle){UINT32_MAX, 0})

static inline bool
surface_handle_valid(SurfaceHandle h)
{ return h.index != UINT32_MAX; }

static inline bool
surface_handle_eq(SurfaceHandle a, SurfaceHandle b)
{ return a.index == b.index && a.generation == b.generation; }

typedef struct {
    enum zwlr_layer_shell_v1_layer layer;
    const char *namespace;
    uint32_t anchor;
    uint32_t width;
    uint32_t height;
    int32_t exclusive_zone;
} LayerCreateArgs;

typedef struct {
    SurfaceHandle parent;
    int32_t anchor_x;
    int32_t anchor_y;
    int32_t anchor_width;
    int32_t anchor_height;
    uint32_t width;
    uint32_t height;
    enum xdg_positioner_anchor anchor;
    enum xdg_positioner_gravity gravity;
} PopupCreateArgs;

typedef struct {
    uint32_t width;
    uint32_t height;
    const char *title;
    const char *app_id;
} ToplevelCreateArgs;

typedef enum {
    SURFACE_EVENT_CONFIGURE,
    SURFACE_EVENT_CLOSE,
    SURFACE_EVENT_POPUP_CONFIGURE,
    SURFACE_EVENT_POPUP_DONE,
    SURFACE_EVENT_TOPLEVEL_CONFIGURE,
    SURFACE_EVENT_TOPLEVEL_CLOSE,
} SurfaceEventKind;

typedef struct SurfaceEvent {
    SurfaceEventKind kind;
    SurfaceHandle handle;

    union {
        struct {
            uint32_t width;
            uint32_t height;
        } configure;

        struct {
            int32_t x;
            int32_t y;
            uint32_t width;
            uint32_t height;
        } popup_configure;
    };

    struct SurfaceEvent *next;
} SurfaceEvent;

typedef void (*SurfaceEventFn)(
        SurfaceHandle handle, const SurfaceEvent *event, void *user);

typedef struct {
    SurfaceEventFn on_event;
    void *user;
} SurfaceCallbacks;

typedef struct {
    uint32_t version;
    uint32_t size;
    void (*log)(const char *fmt, ...);
    void (*quit)(PluginHandle *self);
    void (*set_data)(PluginHandle *self, void *data);
    void *(*get_data)(PluginHandle *self);
} CoreAPI;

typedef struct {
    uint32_t version;
    uint32_t size;
    SurfaceHandle (*layer_create)(PluginHandle *self, LayerCreateArgs args,
            SurfaceCallbacks callbacks);
    SurfaceHandle (*popup_create)(PluginHandle *self, PopupCreateArgs args,
            SurfaceCallbacks callbacks);
    SurfaceHandle (*toplevel_create)(PluginHandle *self,
            ToplevelCreateArgs args, SurfaceCallbacks callbacks);
    void (*destroy)(PluginHandle *self, SurfaceHandle handle);
} SurfaceAPI;

typedef struct {
    uint32_t abi_version;
    const CoreAPI *core;
    const SurfaceAPI *surfaces;
} ShellAPI;

typedef struct {
    const char *name;
    const char *version;
    int (*init)(
            const ShellAPI *api, PluginHandle *self, const ShellConfig *config);
    void (*destroy)(const ShellAPI *api, PluginHandle *self);
} ShellPlugin;

#endif // __PLUGIN_API_H_
