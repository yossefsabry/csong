#include "app/app.h"
#include "app/log.h"
#include "app/lyrics.h"
#include "app/mpd_client.h"
#include "app/renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct app_args {
  char host[128];
  int port;
  int once;
  int interval;
} app_args;

static void print_usage(const char *name) {
  printf("Usage: %s [--mpd-host HOST] [--mpd-port PORT] [--once] [--interval N]\n",
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
}

static int args_parse(app_args *out, int argc, char **argv) {
  int i = 1;
  args_default(out);
  while (i < argc) {
    if (strcmp(argv[i], "--mpd-host") == 0 && i + 1 < argc) {
      snprintf(out->host, sizeof(out->host), "%s", argv[i + 1]);
      i += 2;
    } else if (strcmp(argv[i], "--mpd-port") == 0 && i + 1 < argc) {
      out->port = atoi(argv[i + 1]);
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
  mpd_track track;
  char last_artist[256] = {0};
  char last_title[256] = {0};
  int have_track = 0;
  int rendered_for_track = 0;
  char *lyrics_text = NULL;
  lyrics_doc *doc = NULL;
  int fetched_timed = 0;
  int parse_result;

  parse_result = args_parse(&args, argc, argv);
  if (parse_result != 0) {
    return parse_result > 0 ? 0 : 1;
  }

  if (mpd_client_connect(args.host, args.port) != 0) {
    log_error("mpd: connection failed");
    return 1;
  }

  renderer_init();

  for (;;) {
    if (mpd_client_get_current(&track) != 0) {
      renderer_draw_status("MPD unavailable");
      if (args.once) {
        break;
      }
      sleep((unsigned int)args.interval);
      continue;
    }

    if (!track.is_playing || !track.has_song) {
      renderer_draw_status("MPD not playing");
      if (args.once) {
        break;
      }
      sleep((unsigned int)args.interval);
      continue;
    }

    if (!have_track || strcmp(track.artist, last_artist) != 0 ||
        strcmp(track.title, last_title) != 0) {
      free_lyrics(&lyrics_text, &doc);
      lyrics_text = lyrics_cache_load(track.artist, track.title);
      if (!lyrics_text) {
        if (lyrics_fetch(track.artist, track.title, &lyrics_text,
                         &fetched_timed) == 0) {
          doc = lyrics_parse(lyrics_text);
          if (doc) {
            fetched_timed = doc->has_timestamps;
          }
          lyrics_cache_store(track.artist, track.title, lyrics_text,
                             fetched_timed);
        }
      } else {
        doc = lyrics_parse(lyrics_text);
      }
      snprintf(last_artist, sizeof(last_artist), "%s", track.artist);
      snprintf(last_title, sizeof(last_title), "%s", track.title);
      have_track = 1;
      rendered_for_track = 0;
    }

    if (args.once) {
      renderer_draw(track.artist, track.title, doc,
                    lyrics_find_current(doc, track.elapsed), track.elapsed);
      break;
    }

    if (doc && doc->has_timestamps) {
      renderer_draw(track.artist, track.title, doc,
                    lyrics_find_current(doc, track.elapsed), track.elapsed);
    } else if (!rendered_for_track) {
      renderer_draw(track.artist, track.title, doc, -1, track.elapsed);
      rendered_for_track = 1;
    }

    sleep((unsigned int)args.interval);
  }

  free_lyrics(&lyrics_text, &doc);
  renderer_shutdown();
  mpd_client_disconnect();
  return 0;
}
