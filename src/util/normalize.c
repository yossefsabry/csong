#include "app/normalize.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int is_space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static char to_lower_ascii(char c) {
  if (c >= 'A' && c <= 'Z') {
    return (char)(c - 'A' + 'a');
  }
  return c;
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

static const char *strcasestr_ascii(const char *haystack, const char *needle) {
  if (!*needle) {
    return haystack;
  }
  for (const char *h = haystack; *h; ++h) {
    const char *h_it = h;
    const char *n_it = needle;
    while (*h_it && *n_it && to_lower_ascii(*h_it) == to_lower_ascii(*n_it)) {
      ++h_it;
      ++n_it;
    }
    if (!*n_it) {
      return h;
    }
  }
  return NULL;
}

static int contains_keyword_ci(const char *text) {
  static const char *keywords[] = {
      "feat",        "ft.",       "featuring", "with",
      "remaster",    "remastered", "live",      "mix",
      "version",     "edit",      "demo",      "acoustic",
      "instrumental", "mono",     "stereo",    "bonus",
      "explicit",    "from",      "soundtrack", "motion picture",
      "official",    "official video", "official audio", "music video",
      "video",       "lyric",     "lyrics",    "lyric video",
      "audio",       "visualizer", "visualiser", "topic",
      "provided to youtube", "youtube", "mv", "performance",
      "full album",  "cover", "prod.", "ost",
  };
  for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); ++i) {
    if (strcasestr_ascii(text, keywords[i])) {
      return 1;
    }
  }
  return 0;
}

static void trim_in_place(char *s) {
  size_t len = strlen(s);
  size_t start = 0;
  while (start < len && is_space(s[start])) {
    ++start;
  }
  size_t end = len;
  while (end > start && is_space(s[end - 1])) {
    --end;
  }
  if (start > 0 || end < len) {
    memmove(s, s + start, end - start);
    s[end - start] = '\0';
  }
}

static void collapse_spaces(char *s) {
  size_t out = 0;
  int in_space = 0;
  for (size_t i = 0; s[i]; ++i) {
    if (is_space(s[i])) {
      if (!in_space) {
        s[out++] = ' ';
        in_space = 1;
      }
    } else {
      s[out++] = s[i];
      in_space = 0;
    }
  }
  s[out] = '\0';
  trim_in_place(s);
}

static char *strip_brackets_if_keyword(const char *s) {
  size_t len = strlen(s);
  char *out = (char *)malloc(len + 1);
  if (!out) {
    return NULL;
  }
  size_t o = 0;
  for (size_t i = 0; i < len; ++i) {
    char c = s[i];
    if (c == '(' || c == '[' || c == '{') {
      char close = (c == '(') ? ')' : (c == '[' ? ']' : '}');
      size_t j = i + 1;
      while (j < len && s[j] != close) {
        ++j;
      }
      if (j < len) {
        size_t seg_len = j - (i + 1);
        char *seg = (char *)malloc(seg_len + 1);
        if (!seg) {
          free(out);
          return NULL;
        }
        memcpy(seg, s + i + 1, seg_len);
        seg[seg_len] = '\0';
        int drop = contains_keyword_ci(seg);
        free(seg);
        if (drop) {
          i = j;
          continue;
        }
      }
    }
    out[o++] = c;
  }
  out[o] = '\0';
  return out;
}

static void truncate_after_keyword(char *s) {
  const char *keywords[] = {"feat", "ft.", "featuring"};
  char *cut = NULL;
  for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); ++i) {
    const char *pos = strcasestr_ascii(s, keywords[i]);
    if (pos && (!cut || pos < cut)) {
      cut = (char *)pos;
    }
  }
  if (cut) {
    *cut = '\0';
  }
}

static void truncate_dash_descriptor(char *s) {
  char *dash = NULL;
  for (char *p = s; (p = strstr(p, " - ")) != NULL; p += 3) {
    dash = p;
  }
  if (!dash) {
    return;
  }
  if (contains_keyword_ci(dash + 3)) {
    *dash = '\0';
  }
}

static void truncate_artist_delimiters(char *s) {
  const char *delims[] = {" & ", ",", " and "};
  char *cut = NULL;
  for (size_t i = 0; i < sizeof(delims) / sizeof(delims[0]); ++i) {
    char *pos = strstr(s, delims[i]);
    if (pos && (!cut || pos < cut)) {
      cut = pos;
    }
  }
  if (cut) {
    *cut = '\0';
  }
}

char *normalize_title(const char *title) {
  if (!title) {
    return NULL;
  }
  char *out = strip_brackets_if_keyword(title);
  if (!out) {
    return NULL;
  }
  truncate_dash_descriptor(out);
  truncate_after_keyword(out);
  collapse_spaces(out);
  return out;
}

char *normalize_artist(const char *artist) {
  if (!artist) {
    return NULL;
  }
  char *out = dup_string(artist);
  if (!out) {
    return NULL;
  }
  truncate_after_keyword(out);
  truncate_artist_delimiters(out);
  truncate_dash_descriptor(out);
  collapse_spaces(out);
  return out;
}
