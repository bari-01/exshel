#ifndef __BACKEND_WL_H_
#define __BACKEND_WL_H_

#include "../../backend/backend.h"
#include "context.h"

BackendContext *wl_backend_create(WlContext *ctx);
void wl_backend_free(BackendContext *bc);

#endif // __BACKEND_WL_H_
