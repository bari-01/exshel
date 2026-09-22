#ifndef __PLUGIN_API_H_
#define __PLUGIN_API_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct PluginHandle PluginHandle;
typedef struct ShellConfig ShellConfig;
typedef struct Widget Widget;

#define SHELL_ABI_VERSION 2

// Opaque per-plugin config subtree
struct ShellConfig {
    const char *instance_name;
    const void *root;
};

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

// Backend independent enums
typedef enum {
    SHELL_LAYER_BACKGROUND = 0,
    SHELL_LAYER_BOTTOM = 1,
    SHELL_LAYER_TOP = 2,
    SHELL_LAYER_OVERLAY = 3,
} ShellLayer;

// Anchor flags
typedef enum {
    SHELL_ANCHOR_NONE = 0,
    SHELL_ANCHOR_TOP = 1 << 0,
    SHELL_ANCHOR_BOTTOM = 1 << 1,
    SHELL_ANCHOR_LEFT = 1 << 2,
    SHELL_ANCHOR_RIGHT = 1 << 3,
} ShellAnchor;

typedef enum {
    POPUP_ANCHOR_NONE = 0,
    POPUP_ANCHOR_TOP = 1,
    POPUP_ANCHOR_BOTTOM = 2,
    POPUP_ANCHOR_LEFT = 3,
    POPUP_ANCHOR_RIGHT = 4,
    POPUP_ANCHOR_TOP_LEFT = 5,
    POPUP_ANCHOR_BOTTOM_LEFT = 6,
    POPUP_ANCHOR_TOP_RIGHT = 7,
    POPUP_ANCHOR_BOTTOM_RIGHT = 8,
} PopupAnchor;

typedef enum {
    POPUP_GRAVITY_NONE = 0,
    POPUP_GRAVITY_TOP = 1,
    POPUP_GRAVITY_BOTTOM = 2,
    POPUP_GRAVITY_LEFT = 3,
    POPUP_GRAVITY_RIGHT = 4,
    POPUP_GRAVITY_TOP_LEFT = 5,
    POPUP_GRAVITY_BOTTOM_LEFT = 6,
    POPUP_GRAVITY_TOP_RIGHT = 7,
    POPUP_GRAVITY_BOTTOM_RIGHT = 8,
} PopupGravity;

typedef struct {
    ShellLayer layer;
    const char *namespace;
    uint32_t anchor; // Bitfield
    // Explicit window position used by the X11 backend
    int32_t x;
    int32_t y;
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
    PopupAnchor anchor;
    PopupGravity gravity;
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

// API vtables
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
    SurfaceHandle (*layer_create)(
        PluginHandle *self, LayerCreateArgs args, SurfaceCallbacks callbacks);
    SurfaceHandle (*popup_create)(
        PluginHandle *self, PopupCreateArgs args, SurfaceCallbacks callbacks);
    SurfaceHandle (*toplevel_create)(PluginHandle *self,
        ToplevelCreateArgs args, SurfaceCallbacks callbacks);
    void (*destroy)(PluginHandle *self, SurfaceHandle handle);

    // Widget hierarchy
    void (*set_root_widget)(
        PluginHandle *self, SurfaceHandle handle, Widget *root);
    Widget *(*get_root_widget)(PluginHandle *self, SurfaceHandle handle);
    void (*request_render)(PluginHandle *self, SurfaceHandle handle);
} SurfaceAPI;

// EWMH queries. TODO: wayland queries
typedef struct {
    uint32_t version;
    uint32_t size;
    bool (*get_active_window)(PluginHandle *self, uint32_t *out_wid);
    bool (*get_client_list)(
        PluginHandle *self, uint32_t **out_wids, uint32_t *out_count);
} EWMHAPI;

typedef struct {
    uint32_t abi_version;
    const CoreAPI *core;
    const SurfaceAPI *surfaces;
    const EWMHAPI *ewmh;
} ShellAPI;

typedef struct {
    uint32_t abi_version;
    const char *name;
    const char *version;
    int (*init)(
        const ShellAPI *api, PluginHandle *self, const ShellConfig *config);
    void (*destroy)(const ShellAPI *api, PluginHandle *self);
} ShellPlugin;

#endif // __PLUGIN_API_H_
