#ifndef __BACKEND_H_
#define __BACKEND_H_

#include "../plugin_api.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct Node Node;
typedef struct PluginHandler PluginHandler;
typedef struct BackendContext BackendContext;

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
    bool (*init)(BackendContext *ctx);
    void (*destroy)(BackendContext *ctx);

    int (*get_fd)(BackendContext *ctx);

    int (*dispatch)(BackendContext *ctx);

    void (*flush)(BackendContext *ctx);

    bool (*layer_create)(BackendContext *ctx, Node *n, const LayerCreateArgs *a,
            NotifyCtx *nc);

    bool (*popup_create)(BackendContext *ctx, Node *n, const PopupCreateArgs *a,
            Node *parent, NotifyCtx *nc);

    bool (*toplevel_create)(BackendContext *ctx, Node *n,
            const ToplevelCreateArgs *a, NotifyCtx *nc);

    void (*node_destroy)(BackendContext *ctx, Node *n);

    bool (*ewmh_get_active_window)(BackendContext *ctx, uint32_t *out_wid);
    bool (*ewmh_get_client_list)(
            BackendContext *ctx, uint32_t **out_wids, uint32_t *out_count);
} BackendOps;

struct BackendContext {
    const BackendOps *ops;
    void *priv;
};

#endif // __BACKEND_H_
