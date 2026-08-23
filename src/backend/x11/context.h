#ifndef __X11_CONTEXT_H_
#define __X11_CONTEXT_H_

#include <stdbool.h>
#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

typedef struct {
    xcb_window_t *ids;
    void **nodes;
    uint32_t count;
    uint32_t capacity;
} X11WinMap;

typedef struct {
    xcb_connection_t *conn;
    int screen_nbr;
    xcb_screen_t *screen;
    xcb_ewmh_connection_t ewmh;
    xcb_window_t check_win;
    xcb_atom_t wm_delete_window;
    X11WinMap win_map;
} X11Context;

bool x11_context_init(X11Context *ctx);
void x11_context_destroy(X11Context *ctx);

bool x11_win_register(X11Context *ctx, xcb_window_t win, void *node);
void x11_win_unregister(X11Context *ctx, xcb_window_t win);
void *x11_win_lookup(X11Context *ctx, xcb_window_t win);

#endif // __X11_CONTEXT_H_
