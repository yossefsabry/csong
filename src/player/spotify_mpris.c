#include "app/spotify.h"
#include "mpris_common.h"

#ifndef _WIN32

spotify_status spotify_mpris_get_current(player_track *out, char *err,
                                         size_t err_cap) {
  mpris_status st = mpris_get_current("org.mpris.MediaPlayer2.spotify", out, err,
                                      err_cap);
  if (st == MPRIS_OK) {
    out->source = PLAYER_SOURCE_SPOTIFY;
  }
  return (spotify_status)st;
}

#endif
