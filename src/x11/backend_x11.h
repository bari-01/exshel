#ifndef __BACKEND_X11_H_
#define __BACKEND_X11_H_

#include "../backend.h"
#include "context.h"

Context *x11_backend_create(X11Context *ctx);
void x11_backend_free(Context *bc);

#endif // __BACKEND_X11_H_
