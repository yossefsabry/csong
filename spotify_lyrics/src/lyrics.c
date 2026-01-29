#include "lyrics.h"

#include <stdio.h>
#include <string.h>

#include "lyrics_lrclib.h"
#include "lyrics_ovh.h"

static void set_err(char *err, size_t err_cap, const char *msg) {
  if (!err || err_cap == 0) {
    return;
  }
  snprintf(err, err_cap, "%s", msg);
}

char *lyrics_fetch(const char *artist, const char *title, char *err, size_t err_cap) {
  char err_ovh[256] = {0};
  char err_lrclib[256] = {0};

  char *lyrics = lyrics_ovh_fetch(artist, title, err_ovh, sizeof(err_ovh));
  if (lyrics) {
    return lyrics;
  }

  lyrics = lyrics_lrclib_fetch(artist, title, err_lrclib, sizeof(err_lrclib));
  if (lyrics) {
    return lyrics;
  }

  if (err_lrclib[0]) {
    set_err(err, err_cap, err_lrclib);
  } else if (err_ovh[0]) {
    set_err(err, err_cap, err_ovh);
  } else {
    set_err(err, err_cap, "Lyrics not found");
  }
  return NULL;
}
