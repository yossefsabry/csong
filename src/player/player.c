#include "app/player.h"
#include <string.h>

void player_track_reset(player_track *out) {
  if (!out) {
    return;
  }
  memset(out, 0, sizeof(*out));
  out->is_stopped = 1;
  out->source = PLAYER_SOURCE_NONE;
}
