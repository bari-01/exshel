#include "toplevel.h"
#include "../utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool
x11_toplevel_node_init(X11ToplevelNode *tn, X11Context *ctx, uint32_t width,
    uint32_t height, const char *title, const char *app_id)
{
    log_debug("Initializing X11ToplevelNode: %ux%u title=%s app_id=%s", width,
        height, title ? title : "(none)", app_id ? app_id : "(none)");

    tn->x = (int32_t)((ctx->screen->width_in_pixels - width) / 2);
    tn->y = (int32_t)((ctx->screen->height_in_pixels - height) / 2);
    tn->width = width;
    tn->height = height;

    tn->window = xcb_generate_id(ctx->conn);

    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t vals[] = {
        ctx->screen->white_pixel,
        XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY,
    };

    xcb_create_window(ctx->conn, XCB_COPY_FROM_PARENT, tn->window,
        ctx->screen->root, (int16_t)tn->x, (int16_t)tn->y, (uint16_t)width,
        (uint16_t)height, 2, XCB_WINDOW_CLASS_INPUT_OUTPUT,
        ctx->screen->root_visual, mask, vals);

    const char *effective_title = title ? title : (app_id ? app_id : "exshel");
    xcb_ewmh_set_wm_name(&ctx->ewmh, tn->window,
        (uint32_t)strlen(effective_title), effective_title);

    xcb_ewmh_set_wm_desktop(&ctx->ewmh, tn->window, 0);

    xcb_atom_t wm_protocols = 0;
    {
        xcb_intern_atom_cookie_t c =
            xcb_intern_atom(ctx->conn, 1, 12, "WM_PROTOCOLS");
        xcb_intern_atom_reply_t *r = xcb_intern_atom_reply(ctx->conn, c, NULL);
        if (r) {
            wm_protocols = r->atom;
            free(r);
        }
    }
    if (wm_protocols && ctx->wm_delete_window)
        xcb_change_property(ctx->conn, XCB_PROP_MODE_REPLACE, tn->window,
            wm_protocols, XCB_ATOM_ATOM, 32, 1, &ctx->wm_delete_window);

    xcb_map_window(ctx->conn, tn->window);
    xcb_flush(ctx->conn);

    log_debug("X11ToplevelNode created: win=0x%x pos=(%d,%d) size=%ux%u",
        tn->window, tn->x, tn->y, tn->width, tn->height);
    return true;
}

void
x11_toplevel_node_destroy(X11ToplevelNode *tn, X11Context *ctx)
{
    if (!tn->window) return;
    log_debug("Destroying X11ToplevelNode: win=0x%x", tn->window);
    xcb_unmap_window(ctx->conn, tn->window);
    xcb_destroy_window(ctx->conn, tn->window);
    xcb_flush(ctx->conn);
    tn->window = 0;
}
