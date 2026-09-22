#ifndef __CONFIG_H_
#define __CONFIG_H_

#include "plugin_api.h"
#include <stdbool.h>

typedef struct ShellConfigDoc ShellConfigDoc;

// Load and parse a JSON5 config file.
ShellConfigDoc *shell_config_load(const char *path);

// Populates out with the config subtree
bool shell_config_get_plugin(
    const ShellConfigDoc *doc, const char *name, ShellConfig *out);

void shell_config_free(ShellConfigDoc *doc);

#endif // __CONFIG_H_
