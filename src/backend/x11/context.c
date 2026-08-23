#include "context.h"
#include "../../utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

bool
x11_win_register(X11Context *ctx, xcb_window_t win, void *node)
{
    X11WinMap *m = &ctx->win_map;
    if (m->count == m->capacity) {
        uint32_t new_cap = m->capacity ? m->capacity * 2 : 16;
        xcb_window_t *new_ids = realloc(m->ids, new_cap * sizeof(*m->ids));
        void **new_nodes = realloc(m->nodes, new_cap * sizeof(*m->nodes));
        if (!new_ids || !new_nodes) {
            free(new_ids);
            free(new_nodes);
            return false;
        }
        m->ids = new_ids;
        m->nodes = new_nodes;
        m->capacity = new_cap;
    }
    m->ids[m->count] = win;
    m->nodes[m->count] = node;
    m->count++;
    return true;
}

void
x11_win_unregister(X11Context *ctx, xcb_window_t win)
{
    X11WinMap *m = &ctx->win_map;
    for (uint32_t i = 0; i < m->count; i++) {
        if (m->ids[i] == win) {
            m->ids[i] = m->ids[m->count - 1];
            m->nodes[i] = m->nodes[m->count - 1];
            m->count--;
            return;
        }
    }
}

void *
x11_win_lookup(X11Context *ctx, xcb_window_t win)
{
    X11WinMap *m = &ctx->win_map;
    for (uint32_t i = 0; i < m->count; i++)
        if (m->ids[i] == win) return m->nodes[i];
    return NULL;
}

bool
x11_context_init(X11Context *ctx)
{
    log_debug("Initializing X11Context");
    memset(ctx, 0, sizeof(*ctx));

    ctx->conn = xcb_connect(NULL, &ctx->screen_nbr);
    check(xcb_connection_has_error(ctx->conn),
            "failed to connect to X11 display\n");

    const xcb_setup_t *setup = xcb_get_setup(ctx->conn);
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < ctx->screen_nbr; i++)
        xcb_screen_next(&it);
    ctx->screen = it.data;
    check(!ctx->screen, "failed to get X11 screen\n");

    xcb_intern_atom_cookie_t *ewmh_cookies =
            xcb_ewmh_init_atoms(ctx->conn, &ctx->ewmh);
    check(!ewmh_cookies, "xcb_ewmh_init_atoms failed\n");
    xcb_ewmh_init_atoms_replies(&ctx->ewmh, ewmh_cookies, NULL);
    log_debug("X11: EWMH atoms initialised");

    xcb_intern_atom_cookie_t del_cookie =
            xcb_intern_atom(ctx->conn, 0, 16, "WM_DELETE_WINDOW");
    xcb_intern_atom_reply_t *del_reply =
            xcb_intern_atom_reply(ctx->conn, del_cookie, NULL);
    check(!del_reply, "failed to intern WM_DELETE_WINDOW\n");
    ctx->wm_delete_window = del_reply->atom;
    free(del_reply);

    ctx->check_win = xcb_generate_id(ctx->conn);
    xcb_create_window(ctx->conn, XCB_COPY_FROM_PARENT, ctx->check_win,
            ctx->screen->root, -1, -1, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_ONLY,
            ctx->screen->root_visual, 0, NULL);

    xcb_ewmh_set_supporting_wm_check(
            &ctx->ewmh, ctx->screen->root, ctx->check_win);
    xcb_ewmh_set_supporting_wm_check(
            &ctx->ewmh, ctx->check_win, ctx->check_win);

    xcb_ewmh_set_wm_name(&ctx->ewmh, ctx->check_win, 6, "exshel");

    xcb_atom_t supported[] = {
            ctx->ewmh._NET_SUPPORTED,
            ctx->ewmh._NET_SUPPORTING_WM_CHECK,
            ctx->ewmh._NET_WM_NAME,
            ctx->ewmh._NET_WM_DESKTOP,
            ctx->ewmh._NET_WM_STATE,
            ctx->ewmh._NET_WM_STATE_STICKY,
            ctx->ewmh._NET_WM_STATE_ABOVE,
            ctx->ewmh._NET_WM_WINDOW_TYPE,
            ctx->ewmh._NET_WM_WINDOW_TYPE_DOCK,
            ctx->ewmh._NET_WM_STRUT,
            ctx->ewmh._NET_WM_STRUT_PARTIAL,
            ctx->ewmh._NET_ACTIVE_WINDOW,
            ctx->ewmh._NET_CLIENT_LIST,
    };
    xcb_ewmh_set_supported(&ctx->ewmh, ctx->screen_nbr,
            sizeof(supported) / sizeof(supported[0]), supported);

    xcb_flush(ctx->conn);
    log_debug("X11Context initialised: screen=%d root=0x%x", ctx->screen_nbr,
            ctx->screen->root);
    return true;
fail:
    log_debug("X11Context init failed");
    x11_context_destroy(ctx);
    return false;
}

void
x11_context_destroy(X11Context *ctx)
{
    log_debug("Destroying X11Context");
    free(ctx->win_map.ids);
    free(ctx->win_map.nodes);

    if (ctx->check_win && ctx->conn)
        xcb_destroy_window(ctx->conn, ctx->check_win);

    if (ctx->ewmh.connection) xcb_ewmh_connection_wipe(&ctx->ewmh);

    if (ctx->conn) xcb_disconnect(ctx->conn);
}
