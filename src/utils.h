#ifndef __UTILS_H__
#define __UTILS_H_

#define check(expr, errmsg) \
    do { \
        if (expr) { \
            printf(errmsg); \
            fprintf(stderr, errmsg); \
            goto fail; \
        } \
    } while(0)

#define destroy(expr, call) \
    do { \
        if (expr) { \
            call(expr); \
            expr = NULL; \
        } \
    } while(0)

#endif // __UTILS_H_
