#ifndef SPOTIFY_CACHE_H
#define SPOTIFY_CACHE_H

#include <stddef.h>

char *cache_default_dir(void);
char *cache_build_key(const char *artist, const char *title);
char *cache_load(const char *cache_dir, const char *key, char *err, size_t err_cap);
int cache_store(const char *cache_dir, const char *key, const char *lyrics, char *err, size_t err_cap);

#endif
