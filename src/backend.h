#ifndef __BACKEND_H_
#define __BACKEND_H_

#include "plugin_api.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct Node Node;
typedef struct PluginHandler PluginHandler;
typedef struct Context Context;

typedef struct NotifyCtx {
    void *ph;
    SurfaceHandle handle;
    uint32_t owner;

    void (*post_configure)(struct NotifyCtx *nc, uint32_t w, uint32_t h);
    void (*post_close)(struct NotifyCtx *nc);

    void (*post_popup_configure)(
        struct NotifyCtx *nc, int32_t x, int32_t y, uint32_t w, uint32_t h);
    void (*post_popup_done)(struct NotifyCtx *nc);

    void (*post_toplevel_configure)(
        struct NotifyCtx *nc, uint32_t w, uint32_t h);
    void (*post_toplevel_close)(struct NotifyCtx *nc);
} NotifyCtx;

typedef struct {
    bool (*init)(Context *ctx);
    void (*destroy)(Context *ctx);

    int (*get_fd)(Context *ctx);

    int (*prepare)(Context *ctx);
    int (*dispatch)(Context *ctx);
    void (*cancel)(Context *ctx);

    void (*flush)(Context *ctx);

    bool (*layer_create)(
        Context *ctx, Node *n, const LayerCreateArgs *a, NotifyCtx *nc);

    bool (*popup_create)(Context *ctx, Node *n, const PopupCreateArgs *a,
        Node *parent, NotifyCtx *nc);

    bool (*toplevel_create)(
        Context *ctx, Node *n, const ToplevelCreateArgs *a, NotifyCtx *nc);

    void (*node_destroy)(Context *ctx, Node *n);

    bool (*ewmh_get_active_window)(Context *ctx, uint32_t *out_wid);
    bool (*ewmh_get_client_list)(
        Context *ctx, uint32_t **out_wids, uint32_t *out_count);

    void *(*get_native_display)(Context *ctx);
    void *(*get_native_window)(Context *ctx, Node *n);
    void (*get_dimensions)(Context *ctx, Node *n, uint32_t *w, uint32_t *h);
    int (*get_display_type)(Context *ctx);
} Ops;

struct Context {
    const Ops *ops;
    void *priv;
};

#endif // __BACKEND_H_
