#include "app/app.h"
#include "app/config.h"
#include "app/log.h"
#include "app/lyrics.h"
#include "app/mpd_client.h"
#include "app/normalize.h"
#include "app/player.h"
#include "app/spotify.h"
#include "app/ytmusic.h"
#include "app/ui.h"
#include "app/time.h"
#include <ctype.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct app_args {
  char host[128];
  int port;
  int once;
  int interval;
  int show_plain;
  int has_config;
  char config_path[512];
} app_args;

static void print_usage(const char *name) {
  printf("Usage: %s [--config PATH] [--mpd-host HOST] [--mpd-port PORT] "
         "[--once] [--interval N] [--show-plain]\n",
         name);
}

static void args_default(app_args *out) {
  if (!out) {
    return;
  }
  snprintf(out->host, sizeof(out->host), "%s", "127.0.0.1");
  out->port = 6600;
  out->once = 0;
  out->interval = 1;
  out->show_plain = 0;
  out->has_config = 0;
  out->config_path[0] = '\0';
}

static int args_parse_config_path(app_args *out, int argc, char **argv) {
  int i = 1;
  if (!out) {
    return -1;
  }
  while (i < argc) {
    if (strcmp(argv[i], "--config") == 0) {
      if (i + 1 >= argc) {
        print_usage(argv[0]);
        return -1;
      }
      snprintf(out->config_path, sizeof(out->config_path), "%s", argv[i + 1]);
      out->has_config = 1;
      i += 2;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      print_usage(argv[0]);
      return 1;
    } else {
      i++;
    }
  }
  return 0;
}

static int args_parse(app_args *out, int argc, char **argv) {
  int i = 1;
  if (!out) {
    return -1;
  }
  while (i < argc) {
    if (strcmp(argv[i], "--mpd-host") == 0 && i + 1 < argc) {
      snprintf(out->host, sizeof(out->host), "%s", argv[i + 1]);
      i += 2;
    } else if (strcmp(argv[i], "--mpd-port") == 0 && i + 1 < argc) {
      out->port = atoi(argv[i + 1]);
      i += 2;
    } else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
      snprintf(out->config_path, sizeof(out->config_path), "%s", argv[i + 1]);
      out->has_config = 1;
      i += 2;
    } else if (strcmp(argv[i], "--once") == 0) {
      out->once = 1;
      i++;
    } else if (strcmp(argv[i], "--interval") == 0 && i + 1 < argc) {
      out->interval = atoi(argv[i + 1]);
      if (out->interval <= 0) {
        out->interval = 1;
      }
      i += 2;
    } else if (strcmp(argv[i], "--show-plain") == 0) {
      out->show_plain = 1;
      i++;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      print_usage(argv[0]);
      return 1;
    } else {
      print_usage(argv[0]);
      return -1;
    }
  }
  return 0;
}

static int line_has_text(const char *text) {
  const unsigned char *p = (const unsigned char *)text;
  if (!p) {
    return 0;
  }
  while (*p) {
    if (!isspace(*p)) {
      return 1;
    }
    p++;
  }
  return 0;
}

static int is_unknown_artist_name(const char *artist) {
  if (!artist || artist[0] == '\0') {
    return 1;
  }
  return strcmp(artist, "Unknown Artist") == 0;
}

static void trim_whitespace(char *text) {
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

static void sleep_ms(int ms) {
  struct timespec ts;

  if (ms <= 0) {
    return;
  }
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (long)(ms % 1000) * 1000000L;
  nanosleep(&ts, NULL);
}

static void append_text(char *out, size_t out_size, const char *text) {
  size_t len;
  size_t avail;
  size_t copy_len;

  if (!out || out_size == 0 || !text) {
    return;
  }

  len = strlen(out);
  if (len >= out_size - 1) {
    return;
  }
  avail = out_size - 1 - len;
  copy_len = strnlen(text, avail);
  memcpy(out + len, text, copy_len);
  out[len + copy_len] = '\0';
}

static int player_is_playing(const player_track *track) {
  if (!track) {
    return 0;
  }
  return track->has_song && track->is_playing && !track->is_paused &&
         !track->is_stopped;
}

static int player_is_paused(const player_track *track) {
  if (!track) {
    return 0;
  }
  return track->has_song && track->is_paused && !track->is_stopped;
}

static int player_track_matches(const player_track *a, const player_track *b) {
  if (!a || !b) {
    return 0;
  }
  return a->source == b->source && strcmp(a->artist, b->artist) == 0 &&
         strcmp(a->title, b->title) == 0;
}

static void player_track_update_last(player_track *target,
                                     const player_track *src) {
  if (!target || !src) {
    return;
  }
  if (src->elapsed > 0.0 || target->elapsed <= 0.0) {
    *target = *src;
    return;
  }
  target->is_playing = src->is_playing;
  target->is_paused = src->is_paused;
  target->is_stopped = src->is_stopped;
  target->has_song = src->has_song;
}

typedef struct play_probe {
  double last_elapsed;
  long last_ms;
  long grace_until_ms;
  int has_elapsed;
  int has_track;
  char artist[256];
  char title[256];
} play_probe;

static void play_probe_reset(play_probe *probe) {
  if (!probe) {
    return;
  }
  memset(probe, 0, sizeof(*probe));
}

static int play_probe_effective(play_probe *probe, const player_track *track,
                                long now_ms) {
  int moved = 0;

  if (!probe || !track) {
    return 0;
  }
  if (!track->has_song || track->is_stopped) {
    return 0;
  }

  if (!probe->has_track || strcmp(probe->artist, track->artist) != 0 ||
      strcmp(probe->title, track->title) != 0) {
    probe->has_track = 1;
    snprintf(probe->artist, sizeof(probe->artist), "%s", track->artist);
    snprintf(probe->title, sizeof(probe->title), "%s", track->title);
    probe->last_elapsed = track->elapsed;
    probe->has_elapsed = track->elapsed > 0.0;
    probe->grace_until_ms = 0;
    probe->last_ms = now_ms;
  }

  if (track->elapsed > 0.0) {
    if (probe->has_elapsed && track->elapsed > probe->last_elapsed + 0.25) {
      moved = 1;
    }
    probe->last_elapsed = track->elapsed;
    probe->has_elapsed = 1;
    probe->last_ms = now_ms;
  }

  if (track->is_playing || moved) {
    probe->grace_until_ms = now_ms + 1500;
    return 1;
  }

  if (now_ms < probe->grace_until_ms) {
    return 1;
  }

  return 0;
}

static void player_track_from_mpd(player_track *out, const mpd_track *mpd) {
  if (!out || !mpd) {
    return;
  }
  snprintf(out->artist, sizeof(out->artist), "%s", mpd->artist);
  snprintf(out->title, sizeof(out->title), "%s", mpd->title);
  out->elapsed = mpd->elapsed;
  out->duration = mpd->duration;
  out->is_playing = mpd->is_playing;
  out->is_paused = mpd->is_paused;
  out->is_stopped = mpd->is_stopped;
  out->has_song = mpd->has_song;
  out->source = PLAYER_SOURCE_MPD;
}

static double load_track_offset(const char *artist, const char *title) {
  const char *home = getenv("HOME");
  FILE *file;
  char path[512];
  char line[512];
  char match[512];

  if (!home || !title) {
    return 0.0;
  }

  match[0] = '\0';
  if (!is_unknown_artist_name(artist)) {
    append_text(match, sizeof(match), artist ? artist : "");
    append_text(match, sizeof(match), " - ");
    append_text(match, sizeof(match), title);
  } else {
    append_text(match, sizeof(match), title);
  }

  snprintf(path, sizeof(path), "%s/lyrics/.offsets", home);
  file = fopen(path, "r");
  if (!file) {
    return 0.0;
  }

  while (fgets(line, sizeof(line), file)) {
    char *eq;
    char *key;
    char *val;
    char *endptr;
    double seconds;

    if ((unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB &&
        (unsigned char)line[2] == 0xBF) {
      memmove(line, line + 3, strlen(line + 3) + 1);
    }

    trim_whitespace(line);
    if (line[0] == '\0' || line[0] == '#') {
      continue;
    }

    eq = strchr(line, '=');
    if (!eq) {
      continue;
    }
    *eq = '\0';
    key = line;
    val = eq + 1;
    trim_whitespace(key);
    trim_whitespace(val);

    if (strcmp(key, match) != 0) {
      continue;
    }

    seconds = strtod(val, &endptr);
    if (endptr == val) {
      continue;
    }
    fclose(file);
    return seconds;
  }

  fclose(file);
  return 0.0;
}

static int is_music_only_section(const lyrics_doc *doc, double elapsed) {
  const double lead_in = 2.0;
  const double gap_threshold = 10.0;
  const double gap_start_buffer = 1.0;
  const double gap_end_buffer = 1.0;
  const double outro_tail = 3.0;
  double first_time = -1.0;
  double prev_time = -1.0;
  double last_time = -1.0;
  size_t i;

  if (!doc || !doc->has_timestamps || doc->count == 0) {
    return 0;
  }

  for (i = 0; i < doc->count; i++) {
    if (!doc->lines[i].has_time) {
      continue;
    }
    if (!line_has_text(doc->lines[i].text)) {
      continue;
    }
    first_time = doc->lines[i].time;
    break;
  }

  if (first_time < 0.0) {
    return 0;
  }

  if (elapsed < first_time - lead_in) {
    return 1;
  }

  prev_time = -1.0;
  for (i = 0; i < doc->count; i++) {
    if (!doc->lines[i].has_time) {
      continue;
    }
    if (!line_has_text(doc->lines[i].text)) {
      continue;
    }
    if (prev_time < 0.0) {
      prev_time = doc->lines[i].time;
      last_time = prev_time;
      continue;
    }
    last_time = doc->lines[i].time;
    if (last_time - prev_time >= gap_threshold) {
      double gap_start = prev_time + gap_start_buffer;
      double gap_end = last_time - lead_in - gap_end_buffer;
      if (gap_end < gap_start) {
        gap_end = gap_start;
      }
      if (elapsed >= gap_start && elapsed < gap_end) {
        return 1;
      }
    }
    prev_time = last_time;
  }

  if (last_time >= 0.0 && elapsed > last_time + outro_tail) {
    return 1;
  }

  return 0;
}

static void free_lyrics(char **text, lyrics_doc **doc) {
  if (doc && *doc) {
    lyrics_free(*doc);
    *doc = NULL;
  }
  if (text && *text) {
    free(*text);
    *text = NULL;
  }
}

int app_run(int argc, char **argv) {
  app_args args;
  app_config config;
  ui_options ui = {0};
  mpd_track mpd_state;
  player_track track;
  player_track last_active;
  player_source last_source = PLAYER_SOURCE_NONE;
  int mpd_ready = 1;
  long mpd_retry_at = 0;
  long mpd_poll_ms = 0;
  char spotify_err[256] = {0};
  char ytmusic_err[256] = {0};
  int last_active_valid = 0;
  double ext_elapsed = 0.0;
  long ext_update_ms = 0;
  int ext_valid = 0;
  player_source ext_source = PLAYER_SOURCE_NONE;
  char ext_artist[256] = {0};
  char ext_title[256] = {0};
  play_probe mpd_probe;
  play_probe spotify_probe;
  play_probe ytmusic_probe;
  char last_artist[256] = {0};
  char last_title[256] = {0};
  int have_track = 0;
  int rendered_for_track = 0;
  int last_paused = -1;
  char *lyrics_text = NULL;
  lyrics_doc *doc = NULL;
  int fetched_timed = 0;
  char status[128] = {0};
  int has_lyrics = 0;
  int anim_frame = 0;
  int last_current_index = -1;
  int pulse_frames = 0;
  const int transition_total = 7;
  const int transition_delay_us = 100000;
  double lyric_lead_seconds = 1.0;
  double offset_seconds = 0.0;
  int showing_last_active = 0;
  int parse_result;
  int config_result = 1;
  char config_path[512] = {0};
  long last_tick_ms = 0;
  int refresh_mpd = 1;
  int idle_active = 0;
  int mpd_fd = -1;
  int tick_ms = 0;

  args_default(&args);
  parse_result = args_parse_config_path(&args, argc, argv);
  if (parse_result != 0) {
    return parse_result > 0 ? 0 : 1;
  }

  config_default(&config);
  if (args.has_config) {
    if (config_resolve_path(args.config_path, config_path,
                            sizeof(config_path)) != 0) {
      snprintf(config_path, sizeof(config_path), "%s", args.config_path);
    }
    config_result = config_load(config_path, &config);
    if (config_result == 1) {
      log_error("config: file not found");
      return 1;
    }
    if (config_result < 0) {
      return 1;
    }
  } else if (config_default_path(config_path, sizeof(config_path)) == 0) {
    config_result = config_load(config_path, &config);
    if (config_result < 0) {
      return 1;
    }
  }

  if (config.mpd_host[0] != '\0') {
    snprintf(args.host, sizeof(args.host), "%s", config.mpd_host);
  }
  if (config.mpd_port > 0) {
    args.port = config.mpd_port;
  }
  if (config.interval > 0) {
    args.interval = config.interval;
  }
  args.show_plain = config.show_plain;
  if (config.cache_dir[0] != '\0') {
    lyrics_cache_set_dir(config.cache_dir);
  }

  lyric_lead_seconds = config.lyrics_lead_seconds;

  parse_result = args_parse(&args, argc, argv);
  if (parse_result != 0) {
    return parse_result > 0 ? 0 : 1;
  }

  memset(&mpd_state, 0, sizeof(mpd_state));
  mpd_state.is_stopped = 1;
  player_track_reset(&track);
  player_track_reset(&last_active);
  play_probe_reset(&mpd_probe);
  play_probe_reset(&spotify_probe);
  play_probe_reset(&ytmusic_probe);

  snprintf(ui.backend, sizeof(ui.backend), "%s", config.ui_backend);
  snprintf(ui.font, sizeof(ui.font), "%s", config.ui_font);
  snprintf(ui.title_font, sizeof(ui.title_font), "%s", config.ui_title_font);
  snprintf(ui.title_weight, sizeof(ui.title_weight), "%s", config.ui_title_weight);
  snprintf(ui.title_style, sizeof(ui.title_style), "%s", config.ui_title_style);
  ui.opacity = config.ui_opacity;
  ui.width = config.ui_width;
  ui.height = config.ui_height;
  ui.offset_x = config.ui_offset_x;
  ui.offset_y = config.ui_offset_y;
  ui.padding_x = config.ui_padding_x;
  ui.padding_y = config.ui_padding_y;
  ui.click_through = config.ui_click_through;
  ui.fg_r = config.ui_fg_r;
  ui.fg_g = config.ui_fg_g;
  ui.fg_b = config.ui_fg_b;
  ui.dim_r = config.ui_dim_r;
  ui.dim_g = config.ui_dim_g;
  ui.dim_b = config.ui_dim_b;
  ui.prev_r = config.ui_prev_r;
  ui.prev_g = config.ui_prev_g;
  ui.prev_b = config.ui_prev_b;
  ui.bg_r = config.ui_bg_r;
  ui.bg_g = config.ui_bg_g;
  ui.bg_b = config.ui_bg_b;
  ui.title_r = config.ui_title_r;
  ui.title_g = config.ui_title_g;
  ui.title_b = config.ui_title_b;
  ui.line_spacing = config.ui_line_spacing;
  ui.title_scale = config.ui_title_scale;
  snprintf(ui.anchor, sizeof(ui.anchor), "%s", config.ui_anchor);

  ui_init(&ui);
  ui_set_rtl(config.rtl_mode, config.rtl_align, config.rtl_shape,
             config.bidi_mode);
  if (args.host[0] != '\0') {
    if (mpd_client_connect(args.host, args.port) != 0) {
      log_error("mpd: connection failed");
      mpd_ready = 0;
      mpd_retry_at = time_now_ms() + 5000;
      mpd_fd = -1;
    }
  } else {
    mpd_ready = 0;
    mpd_fd = -1;
  }
  tick_ms = args.interval > 0 ? args.interval * 1000 : 1000;
  if (tick_ms < 50) {
    tick_ms = 50;
  }
  last_tick_ms = time_now_ms();
  if (mpd_ready) {
    mpd_fd = mpd_client_get_fd();
  }
  idle_active = 0;

  for (;;) {
    long now = time_now_ms();
    long delta_ms = now - last_tick_ms;
    if (delta_ms < 0) {
      delta_ms = 0;
    }
    last_tick_ms = now;
    showing_last_active = 0;

    if (!mpd_ready && args.host[0] != '\0' && now >= mpd_retry_at) {
      if (mpd_client_connect(args.host, args.port) == 0) {
        mpd_ready = 1;
        mpd_fd = mpd_client_get_fd();
        refresh_mpd = 1;
      } else {
        mpd_retry_at = now + 5000;
      }
    }

    if (mpd_ready && !refresh_mpd && now - mpd_poll_ms >= 1000) {
      if (idle_active) {
        mpd_client_noidle(NULL);
        idle_active = 0;
      }
      if (mpd_client_get_current(&mpd_state) != 0) {
        mpd_ready = 0;
        mpd_retry_at = now + 5000;
        mpd_fd = -1;
      } else {
        mpd_poll_ms = now;
      }
    }

    if (track.source == PLAYER_SOURCE_MPD && !refresh_mpd && track.is_playing &&
        !track.is_paused && !track.is_stopped) {
      track.elapsed += (double)delta_ms / 1000.0;
      if (track.duration > 0.0 && track.elapsed > track.duration) {
        track.elapsed = track.duration;
      }
    }

    if (refresh_mpd && mpd_ready) {
      if (idle_active) {
        mpd_client_noidle(NULL);
        idle_active = 0;
      }
      if (mpd_client_get_current(&mpd_state) != 0) {
        mpd_ready = 0;
        mpd_retry_at = now + 5000;
        mpd_fd = -1;
        idle_active = 0;
      }
      refresh_mpd = 0;
      mpd_poll_ms = now;
    }

    {
      int have_playing = 0;
      int have_paused = 0;
      player_source current_source = PLAYER_SOURCE_NONE;
      player_track tmp;
      player_track paused_candidate;
      player_track playing_candidate;

      player_track_reset(&paused_candidate);
      player_track_reset(&playing_candidate);

      if (mpd_ready && mpd_state.has_song && !mpd_state.is_stopped) {
        player_track_from_mpd(&tmp, &mpd_state);
        if (play_probe_effective(&mpd_probe, &tmp, now)) {
          tmp.is_playing = 1;
          tmp.is_paused = 0;
          tmp.is_stopped = 0;
        }
        if (player_is_playing(&tmp)) {
          playing_candidate = tmp;
          current_source = PLAYER_SOURCE_MPD;
          have_playing = 1;
        } else if (player_is_paused(&tmp)) {
          if (!have_paused) {
            paused_candidate = tmp;
            have_paused = 1;
          }
          if (last_active_valid && player_track_matches(&last_active, &tmp)) {
            player_track_update_last(&last_active, &tmp);
          }
        }
      }

      if (!have_playing) {
        player_track_reset(&tmp);
        if (spotify_get_current(&tmp, spotify_err, sizeof(spotify_err)) ==
            SPOTIFY_OK) {
          if (play_probe_effective(&spotify_probe, &tmp, now)) {
            tmp.is_playing = 1;
            tmp.is_paused = 0;
            tmp.is_stopped = 0;
          }
          if (player_is_playing(&tmp)) {
            playing_candidate = tmp;
            current_source = PLAYER_SOURCE_SPOTIFY;
            have_playing = 1;
          } else if (player_is_paused(&tmp)) {
            if (!have_paused) {
              paused_candidate = tmp;
              have_paused = 1;
            }
            if (last_active_valid && player_track_matches(&last_active, &tmp)) {
              player_track_update_last(&last_active, &tmp);
            }
          }
        }
      }

      if (!have_playing) {
        player_track_reset(&tmp);
        if (ytmusic_get_current(&tmp, ytmusic_err, sizeof(ytmusic_err)) ==
            YTMUSIC_OK) {
          if (play_probe_effective(&ytmusic_probe, &tmp, now)) {
            tmp.is_playing = 1;
            tmp.is_paused = 0;
            tmp.is_stopped = 0;
          }
          if (player_is_playing(&tmp)) {
            playing_candidate = tmp;
            current_source = PLAYER_SOURCE_YOUTUBE;
            have_playing = 1;
          } else if (player_is_paused(&tmp)) {
            if (!have_paused) {
              paused_candidate = tmp;
              have_paused = 1;
            }
            if (last_active_valid && player_track_matches(&last_active, &tmp)) {
              player_track_update_last(&last_active, &tmp);
            }
          }
        }
      }

      if (have_playing) {
        track = playing_candidate;
        track.source = current_source;
        last_active = track;
        last_active_valid = 1;
        showing_last_active = 0;
      } else if (last_active_valid) {
        track = last_active;
        track.is_playing = 0;
        track.is_paused = 1;
        track.is_stopped = 0;
        track.has_song = 1;
        current_source = track.source;
        showing_last_active = 1;
      } else if (have_paused) {
        track = paused_candidate;
        track.is_playing = 0;
        track.is_paused = 1;
        track.is_stopped = 0;
        track.has_song = 1;
        current_source = track.source;
        last_active = track;
        last_active_valid = 1;
        showing_last_active = 0;
      } else {
        ui_draw_status("No active player", "■");
        have_track = 0;
        last_source = PLAYER_SOURCE_NONE;
        player_track_reset(&track);
        if (args.once) {
          break;
        }
        goto wait_loop;
      }

      if (track.source != current_source) {
        track.source = current_source;
      }
    }

    if (track.source != PLAYER_SOURCE_MPD && track.has_song) {
      int same_track = ext_valid && ext_source == track.source &&
                       strcmp(ext_artist, track.artist) == 0 &&
                       strcmp(ext_title, track.title) == 0;
      double raw_elapsed = track.elapsed;

      if (!same_track) {
        ext_elapsed = raw_elapsed;
        ext_update_ms = now;
        ext_valid = 1;
        ext_source = track.source;
        snprintf(ext_artist, sizeof(ext_artist), "%s", track.artist);
        snprintf(ext_title, sizeof(ext_title), "%s", track.title);
      }

      if (track.is_playing) {
        double next_elapsed = raw_elapsed;
        if (next_elapsed <= 0.0 ||
            (ext_valid && next_elapsed < ext_elapsed - 0.25)) {
          double delta = (double)(now - ext_update_ms) / 1000.0;
          if (delta < 0.0) {
            delta = 0.0;
          }
          next_elapsed = ext_elapsed + delta;
        }
        track.elapsed = next_elapsed;
        ext_elapsed = next_elapsed;
        ext_update_ms = now;
      } else if (track.is_paused) {
        if (raw_elapsed > 0.0) {
          ext_elapsed = raw_elapsed;
          ext_update_ms = now;
        }
        track.elapsed = ext_elapsed;
      }

      if (track.duration > 0.0 && track.elapsed > track.duration) {
        track.elapsed = track.duration;
        ext_elapsed = track.duration;
      }
    }

    if (track.is_playing && track.has_song) {
      if (last_active_valid && player_track_matches(&last_active, &track)) {
        player_track_update_last(&last_active, &track);
      } else {
        last_active = track;
        last_active_valid = 1;
      }
    }

    status[0] = '\0';

    if (!have_track || track.source != last_source ||
        strcmp(track.artist, last_artist) != 0 ||
        strcmp(track.title, last_title) != 0) {
      free_lyrics(&lyrics_text, &doc);
      rendered_for_track = 0;
      last_current_index = -1;
      pulse_frames = 0;
      anim_frame = 0;
      offset_seconds = load_track_offset(track.artist, track.title);
      snprintf(status, sizeof(status), "%s", "Loading lyrics...");
      ui_draw(track.artist, track.title, NULL, -1, track.elapsed, status,
              track.is_paused ? "⏸" : "♪", 0, -1, 0, 0);
      status[0] = '\0';
      lyrics_text = lyrics_cache_load(track.artist, track.title);
      if (!lyrics_text) {
        char *norm_artist = NULL;
        char *norm_title = NULL;
        int fetched = 0;

        if (track.source != PLAYER_SOURCE_MPD) {
          norm_artist = normalize_artist(track.artist);
          norm_title = normalize_title(track.title);
        }

        if (lyrics_fetch(track.artist, track.title, track.duration, &lyrics_text,
                         &fetched_timed) == 0) {
          fetched = 1;
        } else {
          if (!fetched && norm_title && norm_title[0] != '\0') {
            const char *use_artist = norm_artist && norm_artist[0] != '\0'
                                         ? norm_artist
                                         : track.artist;
            if ((norm_artist && strcmp(use_artist, track.artist) != 0) ||
                (norm_title && strcmp(norm_title, track.title) != 0)) {
              if (lyrics_fetch(use_artist, norm_title, track.duration,
                               &lyrics_text, &fetched_timed) == 0) {
                fetched = 1;
              }
            }
          }
          if (!fetched && norm_artist && norm_artist[0] != '\0' &&
              strcmp(norm_artist, track.artist) != 0) {
            if (lyrics_fetch(norm_artist, track.title, track.duration,
                             &lyrics_text, &fetched_timed) == 0) {
              fetched = 1;
            }
          }
          if (!fetched && norm_title && norm_title[0] != '\0' &&
              strcmp(norm_title, track.title) != 0) {
            if (lyrics_fetch(track.artist, norm_title, track.duration,
                             &lyrics_text, &fetched_timed) == 0) {
              fetched = 1;
            }
          }
        }

        if (!fetched &&
            (track.source == PLAYER_SOURCE_YOUTUBE || track.artist[0] == '\0')) {
          const char *title_only = norm_title && norm_title[0] != '\0'
                                       ? norm_title
                                       : track.title;
          if (title_only && title_only[0] != '\0') {
            if (lyrics_fetch("", title_only, track.duration, &lyrics_text,
                             &fetched_timed) == 0) {
              fetched = 1;
            }
          }
        }

        free(norm_artist);
        free(norm_title);

        if (fetched) {
          doc = lyrics_parse(lyrics_text);
          if (doc) {
            fetched_timed = doc->has_timestamps;
          }
          lyrics_cache_store(track.artist, track.title, lyrics_text,
                             fetched_timed);
          if (fetched_timed) {
            snprintf(status, sizeof(status), "%s", "Loaded synced lyrics");
          } else {
            snprintf(status, sizeof(status), "%s", "Loaded lyrics");
          }
        }
      } else {
        doc = lyrics_parse(lyrics_text);
        snprintf(status, sizeof(status), "%s", "Loaded from cache");
      }
      snprintf(last_artist, sizeof(last_artist), "%s", track.artist);
      snprintf(last_title, sizeof(last_title), "%s", track.title);
      have_track = 1;
      last_source = track.source;
    }

    has_lyrics = (doc && doc->count > 0);
    if (!has_lyrics) {
      snprintf(status, sizeof(status), "%s", "No lyrics found");
    } else if (!doc->has_timestamps && !args.show_plain) {
      snprintf(status, sizeof(status), "%s", "No synced lyrics");
    }

    if (track.is_paused) {
      const char *paused_label = showing_last_active ? "Paused (last active)"
                                                      : "Paused";
      if (status[0] != '\0') {
        char paused[128];
        snprintf(paused, sizeof(paused), "%s - %s", paused_label, status);
        snprintf(status, sizeof(status), "%s", paused);
      } else {
        snprintf(status, sizeof(status), "%s", paused_label);
      }
    }

    double position = track.elapsed + offset_seconds;
    double lyric_position;
    if (position < 0.0) {
      position = 0.0;
    }
    if (lyric_lead_seconds < 0.0) {
      lyric_lead_seconds = 0.0;
    } else if (lyric_lead_seconds > 5.0) {
      lyric_lead_seconds = 5.0;
    }
    lyric_position = position + lyric_lead_seconds;
    if (lyric_position < 0.0) {
      lyric_position = 0.0;
    }

    if (args.once) {
      int current_index = -1;
      int pulse = 0;
      const char *icon = track.is_paused ? "⏸" : "♪";
       int music_only = track.is_playing && !track.is_paused &&
                        is_music_only_section(doc, lyric_position);
       if (doc && doc->has_timestamps) {
        current_index = lyrics_find_current(doc, lyric_position);
        if (current_index >= 0 && current_index != last_current_index) {
          pulse_frames = 2;
          last_current_index = current_index;
        }
      }
      pulse = pulse_frames > 0;
      if (music_only) {
        static const char *frames[] = {"♪    ", " ♪   ", "  ♪  ", "   ♪ ", "    ♪"};
        snprintf(status, sizeof(status), "%s", frames[anim_frame % 5]);
        ui_draw(track.artist, track.title, NULL, -1, track.elapsed, status,
                icon, 0, -1, 0, 0);
      } else if (doc && doc->has_timestamps) {
        ui_draw(track.artist, track.title, doc,
                current_index, track.elapsed,
                status, icon, pulse, -1, 0, 0);
      } else if (doc && doc->count > 0 && args.show_plain) {
        ui_draw(track.artist, track.title, doc, -1, track.elapsed, status,
                icon, 0, -1, 0, 0);
      } else {
        ui_draw(track.artist, track.title, NULL, -1, track.elapsed, status,
                icon, 0, -1, 0, 0);
      }
      break;
    }

    if (track.is_playing && !track.is_paused &&
        is_music_only_section(doc, lyric_position)) {
      static const char *frames[] = {"♪    ", " ♪   ", "  ♪  ", "   ♪ ", "    ♪"};
      snprintf(status, sizeof(status), "%s", frames[anim_frame % 5]);
      ui_draw(track.artist, track.title, NULL, -1, track.elapsed, status,
              "♪", 0, -1, 0, 0);
    } else if (doc && doc->has_timestamps) {
      int current_index = lyrics_find_current(doc, lyric_position);
      int pulse = 0;
      int prev_index = last_current_index;
      int do_transition = 0;
      if (current_index >= 0 && current_index != last_current_index) {
        pulse_frames = 2;
        if (!track.is_paused && prev_index >= 0) {
          do_transition = 1;
        }
        last_current_index = current_index;
      }
      pulse = pulse_frames > 0;
      if (do_transition) {
        int step;
        for (step = 0; step < transition_total; step++) {
          struct timespec ts;
          ui_draw(track.artist, track.title, doc, current_index,
                  track.elapsed, status,
                  track.is_paused ? "⏸" : "♪", 1, prev_index, step,
                  transition_total);
          ts.tv_sec = 0;
          ts.tv_nsec = (long)transition_delay_us * 1000L;
          nanosleep(&ts, NULL);
        }
      } else {
        ui_draw(track.artist, track.title, doc, current_index,
                track.elapsed, status,
                track.is_paused ? "⏸" : "♪", pulse, -1, 0, 0);
      }
    } else if (!rendered_for_track || last_paused != track.is_paused) {
      if (doc && doc->count > 0 && args.show_plain) {
        ui_draw(track.artist, track.title, doc, -1, track.elapsed, status,
                track.is_paused ? "⏸" : "♪", 0, -1, 0, 0);
      } else {
        ui_draw(track.artist, track.title, NULL, -1, track.elapsed, status,
                track.is_paused ? "⏸" : "♪", 0, -1, 0, 0);
      }
      rendered_for_track = 1;
    }

    if (!track.is_paused) {
      anim_frame++;
      if (pulse_frames > 0) {
        pulse_frames--;
      }
    }

    last_paused = track.is_paused;
wait_loop:
    if (args.once) {
      break;
    }
    if (mpd_fd >= 0) {
      if (!idle_active) {
        if (mpd_client_idle_begin(0) == 0) {
          idle_active = 1;
        }
      }
      if (idle_active) {
        struct pollfd pfd;
        int poll_result;
        pfd.fd = mpd_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        poll_result = poll(&pfd, 1, tick_ms);
        if (poll_result > 0 && (pfd.revents & POLLIN)) {
          if (mpd_client_idle_end(NULL) != 0) {
            idle_active = 0;
          } else {
            idle_active = 0;
          }
          refresh_mpd = 1;
        } else if (poll_result < 0) {
          idle_active = 0;
          sleep_ms(tick_ms);
        }
      } else {
        sleep_ms(tick_ms);
      }
    } else {
      sleep((unsigned int)args.interval);
    }
  }

  if (idle_active) {
    mpd_client_noidle(NULL);
  }
  free_lyrics(&lyrics_text, &doc);
  ui_shutdown();
  mpd_client_disconnect();
  return 0;
}
