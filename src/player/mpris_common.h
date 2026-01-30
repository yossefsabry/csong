#ifndef CSONG_MPRIS_COMMON_H
#define CSONG_MPRIS_COMMON_H

#include "app/player.h"
#include <stddef.h>

typedef enum {
  MPRIS_OK = 0,
  MPRIS_NO_SESSION = 1,
  MPRIS_NO_TRACK = 2,
  MPRIS_ERROR = 3
} mpris_status;

mpris_status mpris_get_current(const char *bus_name, player_track *out, char *err,
                               size_t err_cap);

#endif
