#include "app/ytmusic.h"

#ifdef _WIN32
ytmusic_status ytmusic_win_get_current(player_track *out, char *err,
                                       size_t err_cap);
#else
ytmusic_status ytmusic_mpris_get_current(player_track *out, char *err,
                                         size_t err_cap);
#endif

ytmusic_status ytmusic_get_current(player_track *out, char *err, size_t err_cap) {
#ifdef _WIN32
  return ytmusic_win_get_current(out, err, err_cap);
#else
  return ytmusic_mpris_get_current(out, err, err_cap);
#endif
}
