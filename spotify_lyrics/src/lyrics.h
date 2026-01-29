#ifndef SPOTIFY_LYRICS_H
#define SPOTIFY_LYRICS_H

#include <stddef.h>

char *lyrics_fetch(const char *artist, const char *title, char *err, size_t err_cap);

#endif
