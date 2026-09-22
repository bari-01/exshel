#ifndef __BACKEND_WL_H_
#define __BACKEND_WL_H_

#include "../backend.h"
#include "context.h"

Context *wl_backend_create(WlContext *ctx);
void wl_backend_free(Context *bc);

#endif // __BACKEND_WL_H_
