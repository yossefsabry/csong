#ifndef SPOTIFY_PLATFORM_H
#define SPOTIFY_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  char *title;
  char *artist;
} Track;

typedef enum {
  SPOTIFY_PLAYBACK_PLAYING = 0,
  SPOTIFY_PLAYBACK_PAUSED = 1,
  SPOTIFY_PLAYBACK_STOPPED = 2
} SpotifyPlaybackStatus;

typedef struct {
  SpotifyPlaybackStatus status;
  int64_t position_ms;
  int64_t duration_ms;
} PlaybackInfo;

typedef enum {
  SPOTIFY_OK = 0,
  SPOTIFY_NO_SESSION = 1,
  SPOTIFY_NO_TRACK = 2,
  SPOTIFY_ERROR = 3
} SpotifyStatus;

SpotifyStatus spotify_get_current_track(Track *out, PlaybackInfo *playback, char *err,
                                        size_t err_cap);
void spotify_free_track(Track *track);

#ifdef __cplusplus
}
#endif

#endif
