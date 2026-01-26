#ifndef CSONG_MPD_CLIENT_H
#define CSONG_MPD_CLIENT_H

typedef struct mpd_track {
  char artist[256];
  char title[256];
  double elapsed;
  int is_playing;
  int is_paused;
  int is_stopped;
  int has_song;
} mpd_track;

int mpd_client_connect(const char *host, int port);
void mpd_client_disconnect(void);
int mpd_client_get_current(mpd_track *out);

#endif
