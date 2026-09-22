#ifndef __CONFIG_QUERY_H_
#define __CONFIG_QUERY_H_

// helpers for plugins to read their config subtree.

#include "plugin_api.h"

#include <stdint.h>
#include <stdlib.h>
#include <yyjson.h>

static inline const char *
cfg_str(const ShellConfig *c, const char *key, const char *def)
{
    if (!c || !c->root) return def;
    yyjson_val *v = yyjson_obj_get((yyjson_val *)c->root, key);
    if (!v || !yyjson_is_str(v)) return def;
    return yyjson_get_str(v);
}

static inline float
cfg_float(const ShellConfig *c, const char *key, float def)
{
    if (!c || !c->root) return def;
    yyjson_val *v = yyjson_obj_get((yyjson_val *)c->root, key);
    if (!v) return def;
    if (yyjson_is_real(v)) return (float)yyjson_get_real(v);
    if (yyjson_is_int(v)) return (float)yyjson_get_int(v);
    return def;
}

static inline uint32_t
cfg_color(const ShellConfig *c, const char *key, uint32_t def)
{
    if (!c || !c->root) return def;
    yyjson_val *v = yyjson_obj_get((yyjson_val *)c->root, key);
    if (!v) return def;
    if (yyjson_is_str(v)) {
        const char *s = yyjson_get_str(v);
        return s ? (uint32_t)strtoul(s, NULL, 0) : def;
    }
    if (yyjson_is_int(v)) return (uint32_t)yyjson_get_int(v);
    return def;
}

static inline int
cfg_int(const ShellConfig *c, const char *key, int def)
{
    if (!c || !c->root) return def;
    yyjson_val *v = yyjson_obj_get((yyjson_val *)c->root, key);
    if (!v) return def;
    if (yyjson_is_int(v)) return yyjson_get_int(v);
    if (yyjson_is_real(v)) return (int)yyjson_get_real(v);
    return def;
}

#endif // __CONFIG_QUERY_H_
