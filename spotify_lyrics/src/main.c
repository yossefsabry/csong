#include "cache.h"
#include "lyrics.h"
#include "normalize.h"
#include "platform.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
static void sleep_seconds(int seconds) {
  Sleep((DWORD)(seconds * 1000));
}
#else
#include <unistd.h>
static void sleep_seconds(int seconds) {
  sleep((unsigned int)seconds);
}
#endif

typedef struct {
  int watch;
  int interval;
  int use_cache;
  int debug;
  const char *cache_dir;
} Options;

static void print_usage(const char *name) {
  fprintf(stdout,
          "Usage: %s [--watch] [--interval N] [--cache-dir PATH] [--no-cache] [--debug]\n",
          name);
}

static int parse_int(const char *s, int *out) {
  char *end = NULL;
  long val = strtol(s, &end, 10);
  if (!s || *s == '\0' || (end && *end != '\0')) {
    return 0;
  }
  if (val <= 0 || val > 3600) {
    return 0;
  }
  *out = (int)val;
  return 1;
}

static char *dup_string(const char *s) {
  size_t len = strlen(s);
  char *out = (char *)malloc(len + 1);
  if (!out) {
    return NULL;
  }
  memcpy(out, s, len + 1);
  return out;
}

static void format_time_ms(int64_t ms, char *out, size_t cap) {
  if (ms < 0) {
    snprintf(out, cap, "--:--");
    return;
  }
  int64_t total_seconds = ms / 1000;
  int64_t seconds = total_seconds % 60;
  int64_t minutes = (total_seconds / 60) % 60;
  int64_t hours = total_seconds / 3600;
  if (hours > 0) {
    snprintf(out, cap, "%lld:%02lld:%02lld", (long long)hours, (long long)minutes,
             (long long)seconds);
  } else {
    snprintf(out, cap, "%02lld:%02lld", (long long)minutes, (long long)seconds);
  }
}

static void print_status_line(const PlaybackInfo *info) {
  if (!info) {
    return;
  }
  char pos[32];
  char dur[32];
  format_time_ms(info->position_ms, pos, sizeof(pos));
  format_time_ms(info->duration_ms, dur, sizeof(dur));

  const char *label = "Stopped";
  if (info->status == SPOTIFY_PLAYBACK_PLAYING) {
    label = "Playing";
  } else if (info->status == SPOTIFY_PLAYBACK_PAUSED) {
    label = "Paused";
  }

  if (info->status == SPOTIFY_PLAYBACK_PAUSED) {
    if (info->duration_ms > 0) {
      printf("%s at %s / %s\n", label, pos, dur);
    } else {
      printf("%s at %s\n", label, pos);
    }
  } else if (info->status == SPOTIFY_PLAYBACK_PLAYING) {
    if (info->duration_ms > 0) {
      printf("%s %s / %s\n", label, pos, dur);
    } else {
      printf("%s %s\n", label, pos);
    }
  } else {
    printf("%s\n", label);
  }
}

static void print_track(const Track *track, const PlaybackInfo *info, const char *lyrics) {
  printf("%s - %s\n", track->artist, track->title);
  print_status_line(info);
  if (lyrics) {
    printf("\n%s\n", lyrics);
  }
}

int main(int argc, char **argv) {
  Options opts = {0};
  opts.interval = 2;
  opts.use_cache = 1;

  for (int i = 1; i < argc; ++i) {
    const char *arg = argv[i];
    if (strcmp(arg, "--watch") == 0) {
      opts.watch = 1;
    } else if (strcmp(arg, "--interval") == 0) {
      if (i + 1 >= argc || !parse_int(argv[i + 1], &opts.interval)) {
        fprintf(stderr, "Invalid interval\n");
        print_usage(argv[0]);
        return 1;
      }
      ++i;
    } else if (strcmp(arg, "--cache-dir") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Missing cache directory\n");
        print_usage(argv[0]);
        return 1;
      }
      opts.cache_dir = argv[++i];
    } else if (strcmp(arg, "--no-cache") == 0) {
      opts.use_cache = 0;
    } else if (strcmp(arg, "--debug") == 0) {
      opts.debug = 1;
    } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
      print_usage(argv[0]);
      return 0;
    } else {
      fprintf(stderr, "Unknown option: %s\n", arg);
      print_usage(argv[0]);
      return 1;
    }
  }

  if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
    fprintf(stderr, "Failed to initialize curl\n");
    return 1;
  }

  char *cache_dir = NULL;
  if (opts.use_cache) {
    if (opts.cache_dir) {
      cache_dir = dup_string(opts.cache_dir);
    } else {
      cache_dir = cache_default_dir();
    }
  }

  char *last_key = NULL;
  SpotifyStatus last_status = SPOTIFY_ERROR;
  SpotifyPlaybackStatus last_playback_status = SPOTIFY_PLAYBACK_STOPPED;
  int64_t last_position_ms = -1;
  char last_err[256] = {0};

  while (1) {
    Track track;
    char err[256] = {0};
    PlaybackInfo playback;
    playback.status = SPOTIFY_PLAYBACK_STOPPED;
    playback.position_ms = -1;
    playback.duration_ms = -1;
    SpotifyStatus st = spotify_get_current_track(&track, &playback, err, sizeof(err));
    if (st == SPOTIFY_OK) {
      char *norm_artist = normalize_artist(track.artist);
      char *norm_title = normalize_title(track.title);
      if (!norm_artist || !norm_title) {
        fprintf(stderr, "Failed to normalize track\n");
        free(norm_artist);
        free(norm_title);
        norm_artist = NULL;
        norm_title = NULL;
      } else {
        char *key = cache_build_key(norm_artist, norm_title);
        int track_changed = 1;
        if (key && last_key && strcmp(key, last_key) == 0) {
          track_changed = 0;
        }
        int print_status = 0;
        if (track_changed || !opts.watch) {
          print_status = 1;
        } else {
          if (playback.status != last_playback_status) {
            print_status = 1;
          } else if (playback.position_ms != last_position_ms) {
            int64_t diff = playback.position_ms - last_position_ms;
            if (diff < 0) {
              diff = -diff;
            }
            if (diff >= 1000) {
              print_status = 1;
            }
          }
        }

        if (track_changed) {
          if (opts.debug) {
            fprintf(stderr, "Artist: %s\nTitle: %s\n", track.artist, track.title);
            fprintf(stderr, "Normalized artist: %s\nNormalized title: %s\n", norm_artist,
                    norm_title);
            fprintf(stderr, "Playback status: %s\n",
                    playback.status == SPOTIFY_PLAYBACK_PLAYING
                        ? "Playing"
                        : (playback.status == SPOTIFY_PLAYBACK_PAUSED ? "Paused" : "Stopped"));
            fprintf(stderr, "Position: %lld ms\nDuration: %lld ms\n",
                    (long long)playback.position_ms, (long long)playback.duration_ms);
          }
          char *lyrics = NULL;
          if (opts.use_cache && cache_dir && key) {
            lyrics = cache_load(cache_dir, key, NULL, 0);
          }
          if (!lyrics) {
            lyrics = lyrics_fetch(norm_artist, norm_title, err, sizeof(err));
            if (!lyrics) {
              lyrics = lyrics_fetch(track.artist, track.title, err, sizeof(err));
            }
            if (lyrics && opts.use_cache && cache_dir && key) {
              cache_store(cache_dir, key, lyrics, NULL, 0);
            }
          }
          if (lyrics) {
            print_track(&track, &playback, lyrics);
            free(lyrics);
          } else {
            print_track(&track, &playback, NULL);
            fprintf(stderr, "Lyrics error: %s\n", err[0] ? err : "unknown error");
          }
          free(last_key);
          last_key = key ? dup_string(key) : NULL;
        } else if (opts.watch && print_status) {
          print_status_line(&playback);
        }
        free(key);
      }
      spotify_free_track(&track);
      free(norm_artist);
      free(norm_title);
    } else {
      if (!opts.watch || st != last_status || strcmp(err, last_err) != 0) {
        if (st == SPOTIFY_NO_SESSION) {
          fprintf(stderr, "No active Spotify session\n");
        } else if (st == SPOTIFY_NO_TRACK) {
          fprintf(stderr, "Spotify is not playing\n");
        } else {
          fprintf(stderr, "Spotify error: %s\n", err[0] ? err : "unknown error");
        }
      }
      free(last_key);
      last_key = NULL;
    }

    last_status = st;
    if (st == SPOTIFY_OK) {
      last_playback_status = playback.status;
      last_position_ms = playback.position_ms;
    } else {
      last_playback_status = SPOTIFY_PLAYBACK_STOPPED;
      last_position_ms = -1;
    }
    snprintf(last_err, sizeof(last_err), "%s", err);

    if (!opts.watch) {
      break;
    }
    sleep_seconds(opts.interval);
  }

  free(last_key);
  free(cache_dir);
  curl_global_cleanup();
  return 0;
}
