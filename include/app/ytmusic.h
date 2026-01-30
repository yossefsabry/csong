#ifndef CSONG_YTMUSIC_H
#define CSONG_YTMUSIC_H

#include "app/player.h"
#include <stddef.h>

typedef enum {
  YTMUSIC_OK = 0,
  YTMUSIC_NO_SESSION = 1,
  YTMUSIC_NO_TRACK = 2,
  YTMUSIC_ERROR = 3
} ytmusic_status;

ytmusic_status ytmusic_get_current(player_track *out, char *err, size_t err_cap);

#endif
