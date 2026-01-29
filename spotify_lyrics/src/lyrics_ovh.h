#ifndef SPOTIFY_LYRICS_OVH_H
#define SPOTIFY_LYRICS_OVH_H

#include <stddef.h>

char *lyrics_ovh_fetch(const char *artist, const char *title, char *err, size_t err_cap);

#endif
