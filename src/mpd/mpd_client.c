#include "app/mpd_client.h"
#include "app/log.h"
#include <ctype.h>
#include <mpd/client.h>
#include <mpd/idle.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct mpd_connection *mpd_conn;

static void copy_tag(char *dest, size_t dest_size, const char *value) {
  if (!dest || dest_size == 0) {
    return;
  }
  if (value && value[0] != '\0') {
    snprintf(dest, dest_size, "%s", value);
  } else {
    dest[0] = '\0';
  }
}

static void trim_spaces(char *text) {
  char *end;
  if (!text || text[0] == '\0') {
    return;
  }
  while (isspace((unsigned char)*text)) {
    memmove(text, text + 1, strlen(text));
  }
  end = text + strlen(text);
  while (end > text && isspace((unsigned char)end[-1])) {
    end[-1] = '\0';
    end--;
  }
}

static void basename_from_uri(const char *uri, char *out, size_t out_size) {
  const char *slash;
  char *dot;

  if (!uri || !out || out_size == 0) {
    return;
  }

  slash = strrchr(uri, '/');
  if (slash) {
    snprintf(out, out_size, "%s", slash + 1);
  } else {
    snprintf(out, out_size, "%s", uri);
  }

  dot = strrchr(out, '.');
  if (dot && dot != out) {
    *dot = '\0';
  }
  trim_spaces(out);
}

static int split_artist_title(const char *input, char *artist,
                              size_t artist_size, char *title,
                              size_t title_size) {
  const char *sep;
  if (!input || !artist || !title || artist_size == 0 || title_size == 0) {
    return 0;
  }

  sep = strstr(input, " - ");
  if (!sep) {
    return 0;
  }

  snprintf(artist, artist_size, "%.*s", (int)(sep - input), input);
  snprintf(title, title_size, "%s", sep + 3);
  trim_spaces(artist);
  trim_spaces(title);
  return (artist[0] != '\0' && title[0] != '\0');
}

int mpd_client_connect(const char *host, int port) {
  if (mpd_conn) {
    return 0;
  }

  mpd_conn = mpd_connection_new(host, port, 30000);
  if (!mpd_conn) {
    log_error("mpd: failed to create connection");
    return -1;
  }

  if (mpd_connection_get_error(mpd_conn) != MPD_ERROR_SUCCESS) {
    log_error(mpd_connection_get_error_message(mpd_conn));
    mpd_connection_free(mpd_conn);
    mpd_conn = NULL;
    return -1;
  }

  return 0;
}

void mpd_client_disconnect(void) {
  if (mpd_conn) {
    mpd_connection_free(mpd_conn);
    mpd_conn = NULL;
  }
}

int mpd_client_get_current(mpd_track *out) {
  struct mpd_status *status;
  struct mpd_song *song;
  enum mpd_state state;
  const char *tag;
  const char *uri;
  char base[256] = {0};
  char artist_guess[256] = {0};
  char title_guess[256] = {0};
  int artist_from_tag = 0;
  int title_from_tag = 0;

  if (!mpd_conn || !out) {
    return -1;
  }

  memset(out, 0, sizeof(*out));

  status = mpd_run_status(mpd_conn);
  if (!status) {
    return -1;
  }

  state = mpd_status_get_state(status);
  out->is_playing = (state == MPD_STATE_PLAY);
  out->is_paused = (state == MPD_STATE_PAUSE);
  out->is_stopped = (state == MPD_STATE_STOP);
  out->elapsed = (double)mpd_status_get_elapsed_time(status);
  out->duration = 0.0;

  song = mpd_run_current_song(mpd_conn);
  if (!song) {
    out->has_song = 0;
    mpd_status_free(status);
    return 0;
  }

  tag = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);
  if (tag && tag[0] != '\0') {
    copy_tag(out->artist, sizeof(out->artist), tag);
    artist_from_tag = 1;
  } else {
    tag = mpd_song_get_tag(song, MPD_TAG_ALBUM_ARTIST, 0);
    if (tag && tag[0] != '\0') {
      copy_tag(out->artist, sizeof(out->artist), tag);
      artist_from_tag = 1;
    }
  }

  tag = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
  if (tag && tag[0] != '\0') {
    copy_tag(out->title, sizeof(out->title), tag);
    title_from_tag = 1;
  } else {
    tag = mpd_song_get_tag(song, MPD_TAG_NAME, 0);
    if (tag && tag[0] != '\0') {
      copy_tag(out->title, sizeof(out->title), tag);
      title_from_tag = 1;
    }
  }

  uri = mpd_song_get_uri(song);
  if (uri && uri[0] != '\0') {
    basename_from_uri(uri, base, sizeof(base));
  }

  if (base[0] != '\0') {
    if (split_artist_title(base, artist_guess, sizeof(artist_guess),
                           title_guess, sizeof(title_guess))) {
      if (!artist_from_tag) {
        copy_tag(out->artist, sizeof(out->artist), artist_guess);
      }
      if (!title_from_tag) {
        copy_tag(out->title, sizeof(out->title), title_guess);
      }
    } else if (!title_from_tag && out->title[0] == '\0') {
      copy_tag(out->title, sizeof(out->title), base);
    }
  }

  if (out->artist[0] == '\0') {
    copy_tag(out->artist, sizeof(out->artist), "Unknown Artist");
  }
  if (out->title[0] == '\0') {
    copy_tag(out->title, sizeof(out->title), "Unknown Title");
  }

  out->has_song = 1;
  if (mpd_song_get_duration(song) > 0) {
    out->duration = (double)mpd_song_get_duration(song);
  }
  mpd_song_free(song);
  mpd_status_free(status);
  return 0;
}

int mpd_client_get_fd(void) {
  if (!mpd_conn) {
    return -1;
  }
  return mpd_connection_get_fd(mpd_conn);
}

int mpd_client_idle_begin(unsigned int mask) {
  if (!mpd_conn) {
    return -1;
  }
  if (mask != 0) {
    return mpd_send_idle_mask(mpd_conn, (enum mpd_idle)mask) ? 0 : -1;
  }
  return mpd_send_idle(mpd_conn) ? 0 : -1;
}

int mpd_client_idle_end(unsigned int *events) {
  enum mpd_idle idle;
  if (!mpd_conn) {
    return -1;
  }
  idle = mpd_recv_idle(mpd_conn, false);
  if (events) {
    *events = (unsigned int)idle;
  }
  if (mpd_connection_get_error(mpd_conn) != MPD_ERROR_SUCCESS) {
    return -1;
  }
  return 0;
}

int mpd_client_noidle(unsigned int *events) {
  enum mpd_idle idle;
  if (!mpd_conn) {
    return -1;
  }
  if (!mpd_send_noidle(mpd_conn)) {
    return -1;
  }
  idle = mpd_recv_idle(mpd_conn, false);
  if (events) {
    *events = (unsigned int)idle;
  }
  if (mpd_connection_get_error(mpd_conn) != MPD_ERROR_SUCCESS) {
    return -1;
  }
  return 0;
}
