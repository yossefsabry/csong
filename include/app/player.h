#ifndef CSONG_PLAYER_H
#define CSONG_PLAYER_H

typedef enum {
  PLAYER_SOURCE_NONE = 0,
  PLAYER_SOURCE_MPD = 1,
  PLAYER_SOURCE_SPOTIFY = 2,
  PLAYER_SOURCE_YOUTUBE = 3
} player_source;

typedef struct player_track {
  char artist[256];
  char title[256];
  double elapsed;
  double duration;
  int is_playing;
  int is_paused;
  int is_stopped;
  int has_song;
  player_source source;
} player_track;

void player_track_reset(player_track *out);

#endif
