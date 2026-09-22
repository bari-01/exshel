#include "popup.h"
#include "../utils.h"

#include <stdio.h>
#include <stdlib.h>

void
x11_popup_compute_position(int32_t parent_x, int32_t parent_y, int32_t anchor_x,
    int32_t anchor_y, int32_t anchor_w, int32_t anchor_h, uint32_t popup_w,
    uint32_t popup_h, PopupAnchor anchor, PopupGravity gravity, int32_t *out_x,
    int32_t *out_y)
{
    int32_t ax = parent_x + anchor_x;
    int32_t ay = parent_y + anchor_y;

    switch (anchor) {
    case POPUP_ANCHOR_NONE:
        ax += anchor_w / 2;
        ay += anchor_h / 2;
        break;
    case POPUP_ANCHOR_TOP:
        ax += anchor_w / 2;
        break;
    case POPUP_ANCHOR_BOTTOM:
        ax += anchor_w / 2;
        ay += anchor_h;
        break;
    case POPUP_ANCHOR_LEFT:
        ay += anchor_h / 2;
        break;
    case POPUP_ANCHOR_RIGHT:
        ax += anchor_w;
        ay += anchor_h / 2;
        break;
    case POPUP_ANCHOR_TOP_LEFT:
        break;
    case POPUP_ANCHOR_BOTTOM_LEFT:
        ay += anchor_h;
        break;
    case POPUP_ANCHOR_TOP_RIGHT:
        ax += anchor_w;
        break;
    case POPUP_ANCHOR_BOTTOM_RIGHT:
        ax += anchor_w;
        ay += anchor_h;
        break;
    }

    int32_t pw = (int32_t)popup_w;
    int32_t ph = (int32_t)popup_h;
    int32_t rx, ry;

    switch (gravity) {
    case POPUP_GRAVITY_NONE:
        rx = ax - pw / 2;
        ry = ay - ph / 2;
        break;
    case POPUP_GRAVITY_TOP:
        rx = ax - pw / 2;
        ry = ay - ph;
        break;
    case POPUP_GRAVITY_BOTTOM:
        rx = ax - pw / 2;
        ry = ay;
        break;
    case POPUP_GRAVITY_LEFT:
        rx = ax - pw;
        ry = ay - ph / 2;
        break;
    case POPUP_GRAVITY_RIGHT:
        rx = ax;
        ry = ay - ph / 2;
        break;
    case POPUP_GRAVITY_TOP_LEFT:
        rx = ax - pw;
        ry = ay - ph;
        break;
    case POPUP_GRAVITY_BOTTOM_LEFT:
        rx = ax - pw;
        ry = ay;
        break;
    case POPUP_GRAVITY_TOP_RIGHT:
        rx = ax;
        ry = ay - ph;
        break;
    case POPUP_GRAVITY_BOTTOM_RIGHT:
        rx = ax;
        ry = ay;
        break;
    default:
        rx = ax;
        ry = ay;
        break;
    }

    *out_x = rx;
    *out_y = ry;
}

bool
x11_popup_node_init(X11PopupNode *pn, X11Context *ctx, int32_t x, int32_t y,
    uint32_t width, uint32_t height)
{
    log_debug("Initializing X11PopupNode: pos=(%d,%d) size=%ux%u", x, y, width,
        height);

    pn->x = x;
    pn->y = y;
    pn->width = width;
    pn->height = height;

    pn->window = xcb_generate_id(ctx->conn);

    uint32_t mask =
        XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    uint32_t vals[] = {
        ctx->screen->white_pixel,
        1,
        XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
            XCB_EVENT_MASK_FOCUS_CHANGE,
    };

    xcb_create_window(ctx->conn, XCB_COPY_FROM_PARENT, pn->window,
        ctx->screen->root, (int16_t)x, (int16_t)y, (uint16_t)width,
        (uint16_t)height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
        ctx->screen->root_visual, mask, vals);

    xcb_map_window(ctx->conn, pn->window);
    xcb_flush(ctx->conn);

    log_debug("X11PopupNode created: win=0x%x", pn->window);
    return true;
}

void
x11_popup_node_destroy(X11PopupNode *pn, X11Context *ctx)
{
    if (!pn->window) return;
    log_debug("Destroying X11PopupNode: win=0x%x", pn->window);
    xcb_unmap_window(ctx->conn, pn->window);
    xcb_destroy_window(ctx->conn, pn->window);
    xcb_flush(ctx->conn);
    pn->window = 0;
}
