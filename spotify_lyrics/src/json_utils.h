#ifndef SPOTIFY_JSON_UTILS_H
#define SPOTIFY_JSON_UTILS_H

#include <stddef.h>

char *json_get_string(const char *json, const char *key, char *err, size_t err_cap);

#endif
