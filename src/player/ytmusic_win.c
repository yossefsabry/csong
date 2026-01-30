#ifdef _WIN32

#include "app/ytmusic.h"

ytmusic_status ytmusic_win_get_current(player_track *out, char *err,
                                       size_t err_cap) {
  (void)out;
  (void)err;
  (void)err_cap;
  return YTMUSIC_NO_SESSION;
}

#endif
