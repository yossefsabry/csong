#ifndef CSONG_LYRICS_H
#define CSONG_LYRICS_H

#include <stddef.h>

typedef struct lyrics_line {
  double time;
  char *text;
  int has_time;
} lyrics_line;

typedef struct lyrics_doc {
  lyrics_line *lines;
  size_t count;
  int has_timestamps;
} lyrics_doc;

char *lyrics_cache_load(const char *artist, const char *title);
int lyrics_cache_store(const char *artist, const char *title, const char *text,
                       int timed);
void lyrics_cache_set_dir(const char *path);

int lyrics_fetch(const char *artist, const char *title, double duration,
                 char **out_text, int *out_timed);

lyrics_doc *lyrics_parse(const char *text);
void lyrics_free(lyrics_doc *doc);
int lyrics_find_current(const lyrics_doc *doc, double elapsed);

#endif
