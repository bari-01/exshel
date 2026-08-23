#include "dock.h"
#include "../../plugin_api.h"
#include "../../utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool
x11_dock_node_init(X11DockNode *dn, X11Context *ctx, int32_t x, int32_t y,
        uint32_t width, uint32_t height, uint32_t anchor,
        int32_t exclusive_zone, const char *name)
{
    log_debug("Initializing X11DockNode: pos=(%d,%d) size=%ux%u anchor=0x%x "
              "ez=%d",
            x, y, width, height, anchor, exclusive_zone);

    dn->x = x;
    dn->y = y;
    dn->width = width;
    dn->height = height;

    dn->window = xcb_generate_id(ctx->conn);

    uint32_t mask =
            XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    uint32_t vals[] = {
            ctx->screen->black_pixel,
            1,
            XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY,
    };

    xcb_create_window(ctx->conn, XCB_COPY_FROM_PARENT, dn->window,
            ctx->screen->root, (int16_t)x, (int16_t)y, (uint16_t)width,
            (uint16_t)height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
            ctx->screen->root_visual, mask, vals);

    xcb_ewmh_set_wm_window_type(
            &ctx->ewmh, dn->window, 1, &ctx->ewmh._NET_WM_WINDOW_TYPE_DOCK);

    xcb_atom_t states[] = {
            ctx->ewmh._NET_WM_STATE_STICKY,
            ctx->ewmh._NET_WM_STATE_ABOVE,
    };
    xcb_ewmh_set_wm_state(&ctx->ewmh, dn->window, 2, states);

    xcb_ewmh_set_wm_desktop(&ctx->ewmh, dn->window, 0xFFFFFFFFu);

    if (name)
        xcb_ewmh_set_wm_name(
                &ctx->ewmh, dn->window, (uint32_t)strlen(name), name);

    xcb_ewmh_wm_strut_partial_t strut;
    memset(&strut, 0, sizeof(strut));

    uint16_t sw = ctx->screen->width_in_pixels;
    uint16_t sh = ctx->screen->height_in_pixels;

    int32_t ez = exclusive_zone;
    if (ez != 0) {
        bool is_top = (anchor & SHELL_ANCHOR_TOP) != 0;
        bool is_bottom = (anchor & SHELL_ANCHOR_BOTTOM) != 0;
        bool is_left = (anchor & SHELL_ANCHOR_LEFT) != 0;
        bool is_right = (anchor & SHELL_ANCHOR_RIGHT) != 0;

        if (is_top && !is_bottom) {
            uint32_t ez_val = (ez == -1) ? height : (uint32_t)ez;
            strut.top = (uint32_t)(y + (int32_t)ez_val);
            strut.top_start_x = (uint32_t)(x < 0 ? 0 : x);
            strut.top_end_x = (uint32_t)(x + (int32_t)width - 1);
        } else if (is_bottom && !is_top) {
            uint32_t ez_val = (ez == -1) ? height : (uint32_t)ez;
            strut.bottom = (uint32_t)((int32_t)sh - y - (int32_t)height +
                                      (int32_t)ez_val);
            strut.bottom_start_x = (uint32_t)(x < 0 ? 0 : x);
            strut.bottom_end_x = (uint32_t)(x + (int32_t)width - 1);
        } else if (is_left && !is_right) {
            uint32_t ez_val = (ez == -1) ? width : (uint32_t)ez;
            strut.left = (uint32_t)(x + (int32_t)ez_val);
            strut.left_start_y = (uint32_t)(y < 0 ? 0 : y);
            strut.left_end_y = (uint32_t)(y + (int32_t)height - 1);
        } else if (is_right && !is_left) {
            uint32_t ez_val = (ez == -1) ? width : (uint32_t)ez;
            strut.right = (uint32_t)((int32_t)sw - x - (int32_t)width +
                                     (int32_t)ez_val);
            strut.right_start_y = (uint32_t)(y < 0 ? 0 : y);
            strut.right_end_y = (uint32_t)(y + (int32_t)height - 1);
        }
    }

    xcb_ewmh_set_wm_strut_partial(&ctx->ewmh, dn->window, strut);
    xcb_ewmh_set_wm_strut(&ctx->ewmh, dn->window, strut.left, strut.right,
            strut.top, strut.bottom);

    xcb_map_window(ctx->conn, dn->window);
    xcb_flush(ctx->conn);

    log_debug("X11DockNode created: win=0x%x", dn->window);
    return true;
}

void
x11_dock_node_destroy(X11DockNode *dn, X11Context *ctx)
{
    if (!dn->window) return;
    log_debug("Destroying X11DockNode: win=0x%x", dn->window);
    xcb_unmap_window(ctx->conn, dn->window);
    xcb_destroy_window(ctx->conn, dn->window);
    xcb_flush(ctx->conn);
    dn->window = 0;
}
