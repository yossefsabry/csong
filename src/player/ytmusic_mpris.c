#include "app/ytmusic.h"
#include "mpris_common.h"

#include <stdio.h>

#ifndef _WIN32

static const char *ytmusic_bus_names[] = {
    "org.mpris.MediaPlayer2.youtube-music",
    "org.mpris.MediaPlayer2.youtube_music",
    "org.mpris.MediaPlayer2.YoutubeMusic",
    "org.mpris.MediaPlayer2.ytmdesktop",
    "org.mpris.MediaPlayer2.ytmdesktopapp",
    NULL,
};

ytmusic_status ytmusic_mpris_get_current(player_track *out, char *err,
                                         size_t err_cap) {
  int saw_session = 0;
  int saw_error = 0;
  char last_err[256] = {0};

  for (size_t i = 0; ytmusic_bus_names[i]; i++) {
    char err_buf[256] = {0};
    mpris_status st = mpris_get_current(ytmusic_bus_names[i], out, err_buf,
                                        sizeof(err_buf));
    if (st == MPRIS_OK) {
      out->source = PLAYER_SOURCE_YOUTUBE;
      return YTMUSIC_OK;
    }
    if (st == MPRIS_NO_SESSION) {
      continue;
    }
    if (st == MPRIS_NO_TRACK) {
      saw_session = 1;
      continue;
    }
    if (st == MPRIS_ERROR) {
      saw_error = 1;
      if (err_buf[0] != '\0') {
        snprintf(last_err, sizeof(last_err), "%s", err_buf);
      }
    }
  }

  if (saw_session) {
    return YTMUSIC_NO_TRACK;
  }
  if (saw_error) {
    if (err && err_cap > 0 && last_err[0] != '\0') {
      snprintf(err, err_cap, "%s", last_err);
    }
    return YTMUSIC_ERROR;
  }
  return YTMUSIC_NO_SESSION;
}

#endif
