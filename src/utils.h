#ifndef __UTILS_H__
#define __UTILS_H_

#define check(expr, errmsg)                                                    \
    do {                                                                       \
        if (expr) {                                                            \
            printf(errmsg);                                                    \
            fprintf(stderr, errmsg);                                           \
            goto fail;                                                         \
        }                                                                      \
    } while (0)

#define wl_destroy(expr, call)                                                 \
    do {                                                                       \
        if (expr) {                                                            \
            call(expr);                                                        \
            expr = NULL;                                                       \
        }                                                                      \
    } while (0)

#ifdef DEBUG
#include <stdio.h>
#define log_debug(...)                                                         \
    do {                                                                       \
        fprintf(stderr, "[DEBUG %s:%d %s] ", __FILE__, __LINE__, __func__);    \
        fprintf(stderr, __VA_ARGS__);                                          \
        fprintf(stderr, "\n");                                                 \
    } while (0)
#else
#define log_debug(...)                                                         \
    do {                                                                       \
    } while (0)
#endif

#endif // __UTILS_H_
