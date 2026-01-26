#include "app/lyrics.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int has_timestamp(const char *text) {
  const char *p = text;
  while (p && *p) {
    if (*p == '[' && isdigit((unsigned char)p[1]) && isdigit((unsigned char)p[2]) &&
        p[3] == ':') {
      return 1;
    }
    p++;
  }
  return 0;
}

static int parse_time_tag(const char *tag, double *out_time) {
  int minutes = 0;
  int seconds = 0;
  int hundredths = 0;
  int read;

  if (!tag || tag[0] != '[') {
    return 0;
  }

  read = sscanf(tag, "[%d:%d.%d]", &minutes, &seconds, &hundredths);
  if (read < 2) {
    read = sscanf(tag, "[%d:%d]", &minutes, &seconds);
    if (read < 2) {
      return 0;
    }
    hundredths = 0;
  }

  if (minutes < 0 || seconds < 0) {
    return 0;
  }

  if (out_time) {
    *out_time = (double)minutes * 60.0 + (double)seconds +
                (double)hundredths / 100.0;
  }
  return 1;
}

static char *strip_cr(char *line) {
  size_t len = strlen(line);
  if (len > 0 && line[len - 1] == '\r') {
    line[len - 1] = '\0';
  }
  return line;
}

static int compare_lines(const void *a, const void *b) {
  const lyrics_line *la = (const lyrics_line *)a;
  const lyrics_line *lb = (const lyrics_line *)b;
  if (la->time < lb->time) {
    return -1;
  }
  if (la->time > lb->time) {
    return 1;
  }
  return 0;
}

lyrics_doc *lyrics_parse(const char *text) {
  lyrics_doc *doc;
  char *copy;
  char *line;
  char *saveptr;
  int timed;

  if (!text) {
    return NULL;
  }

  doc = (lyrics_doc *)calloc(1, sizeof(*doc));
  if (!doc) {
    return NULL;
  }

  timed = has_timestamp(text);
  doc->has_timestamps = timed;

  copy = strdup(text);
  if (!copy) {
    free(doc);
    return NULL;
  }

  line = strtok_r(copy, "\n", &saveptr);
  while (line) {
    double times[8];
    int time_count = 0;
    char *p = line;
    char *text_start;
    strip_cr(p);

    while (*p == '[' && time_count < 8) {
      double t;
      if (parse_time_tag(p, &t)) {
        times[time_count++] = t;
        p = strchr(p, ']');
        if (!p) {
          break;
        }
        p++;
      } else {
        break;
      }
    }

    text_start = p;
    if (timed) {
      int i;
      if (time_count > 0) {
        for (i = 0; i < time_count; i++) {
          lyrics_line *next = (lyrics_line *)realloc(
              doc->lines, sizeof(*doc->lines) * (doc->count + 1));
          if (!next) {
            break;
          }
          doc->lines = next;
          doc->lines[doc->count].time = times[i];
          doc->lines[doc->count].text = strdup(text_start);
          doc->lines[doc->count].has_time = 1;
          doc->count++;
        }
      }
    } else {
      lyrics_line *next = (lyrics_line *)realloc(
          doc->lines, sizeof(*doc->lines) * (doc->count + 1));
      if (!next) {
        break;
      }
      doc->lines = next;
      doc->lines[doc->count].time = 0.0;
      doc->lines[doc->count].text = strdup(text_start);
      doc->lines[doc->count].has_time = 0;
      doc->count++;
    }

    line = strtok_r(NULL, "\n", &saveptr);
  }

  free(copy);

  if (doc->has_timestamps && doc->count > 1) {
    qsort(doc->lines, doc->count, sizeof(*doc->lines), compare_lines);
  }

  return doc;
}

void lyrics_free(lyrics_doc *doc) {
  size_t i;
  if (!doc) {
    return;
  }
  for (i = 0; i < doc->count; i++) {
    free(doc->lines[i].text);
  }
  free(doc->lines);
  free(doc);
}

int lyrics_find_current(const lyrics_doc *doc, double elapsed) {
  size_t i;
  int current = -1;

  if (!doc || !doc->has_timestamps || doc->count == 0) {
    return -1;
  }

  for (i = 0; i < doc->count; i++) {
    if (doc->lines[i].has_time && doc->lines[i].time <= elapsed) {
      current = (int)i;
    } else if (doc->lines[i].has_time && doc->lines[i].time > elapsed) {
      break;
    }
  }

  return current;
}
