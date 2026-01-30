#include "app/spotify.h"

#ifdef _WIN32
spotify_status spotify_win_get_current(player_track *out, char *err,
                                       size_t err_cap);
#else
spotify_status spotify_mpris_get_current(player_track *out, char *err,
                                         size_t err_cap);
#endif

spotify_status spotify_get_current(player_track *out, char *err, size_t err_cap) {
#ifdef _WIN32
  return spotify_win_get_current(out, err, err_cap);
#else
  return spotify_mpris_get_current(out, err, err_cap);
#endif
}
