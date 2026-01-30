#ifndef CSONG_SPOTIFY_H
#define CSONG_SPOTIFY_H

#include "app/player.h"
#include <stddef.h>

typedef enum {
  SPOTIFY_OK = 0,
  SPOTIFY_NO_SESSION = 1,
  SPOTIFY_NO_TRACK = 2,
  SPOTIFY_ERROR = 3
} spotify_status;

spotify_status spotify_get_current(player_track *out, char *err, size_t err_cap);

#endif
