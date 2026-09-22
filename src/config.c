#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <yyjson.h>

struct ShellConfigDoc {
    yyjson_doc *doc;
};

// yyjson ≥0.9 has YYJSON_READ_ALLOW_JSON5 but I had an older version
#define JSON5_FLAGS \
    (YYJSON_READ_ALLOW_TRAILING_COMMAS | YYJSON_READ_ALLOW_COMMENTS | \
        YYJSON_READ_ALLOW_INF_AND_NAN | YYJSON_READ_ALLOW_UNQUOTED_KEY | \
        YYJSON_READ_ALLOW_SINGLE_QUOTED_STR)

ShellConfigDoc *
shell_config_load(const char *path)
{
    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_file(path, JSON5_FLAGS, NULL, &err);
    if (!doc) {
        fprintf(stderr, "config: failed to parse '%s': %s\n", path, err.msg);
        return NULL;
    }

    ShellConfigDoc *d = malloc(sizeof(*d));
    if (!d) {
        yyjson_doc_free(doc);
        return NULL;
    }
    d->doc = doc;
    return d;
}

bool
shell_config_get_plugin(
    const ShellConfigDoc *doc, const char *name, ShellConfig *out)
{
    if (!doc || !name || !out) return false;

    yyjson_val *root = yyjson_doc_get_root(doc->doc);
    yyjson_val *plugins = yyjson_obj_get(root, "plugins");
    if (!plugins || !yyjson_is_obj(plugins)) return false;

    yyjson_val *plugin_cfg = yyjson_obj_get(plugins, name);
    if (!plugin_cfg || !yyjson_is_obj(plugin_cfg)) return false;

    out->instance_name = name;
    out->root = plugin_cfg;
    return true;
}

void
shell_config_free(ShellConfigDoc *doc)
{
    if (!doc) return;
    yyjson_doc_free(doc->doc);
    free(doc);
}
