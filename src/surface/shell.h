#ifndef __SHELL_H_
#define __SHELL_H_

#include "../surface.h"
#include "layer.h"

bool shell_init(WaylandContext *ctx, WaylandSurface *ws, LayerSurface *ls,
        const char *namespace, uint32_t height);
void shell_destroy(WaylandContext *ctx, WaylandSurface *ws, LayerSurface *ls);

#endif // __SHELL_H_
