#include "app/text_layout.h"
#include "app/unicode.h"
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int push_line(text_layout_lines *out, const char *start, size_t len) {
  char *line;
  char **next;

  if (!out) {
    return -1;
  }

  line = (char *)malloc(len + 1);
  if (!line) {
    return -1;
  }
  if (len > 0 && start) {
    memcpy(line, start, len);
  }
  line[len] = '\0';

  next = (char **)realloc(out->lines, sizeof(char *) * (out->count + 1));
  if (!next) {
    free(line);
    return -1;
  }
  out->lines = next;
  out->lines[out->count] = line;
  out->count++;
  return 0;
}

static int is_unicode_space(uint32_t cp) {
  if (cp <= 0x7F) {
    return isspace((unsigned char)cp) != 0;
  }
  switch (cp) {
    case 0x00A0:
    case 0x1680:
    case 0x2000:
    case 0x2001:
    case 0x2002:
    case 0x2003:
    case 0x2004:
    case 0x2005:
    case 0x2006:
    case 0x2007:
    case 0x2008:
    case 0x2009:
    case 0x200A:
    case 0x2028:
    case 0x2029:
    case 0x202F:
    case 0x205F:
    case 0x3000:
      return 1;
    default:
      return 0;
  }
}

static const char *next_codepoint(const char *p, const char *end,
                                  uint32_t *out_cp, size_t *out_len) {
  size_t remaining;
  if (!p || !out_cp || !out_len) {
    return p;
  }
  if (p >= end) {
    *out_cp = 0;
    *out_len = 0;
    return p;
  }
  remaining = (size_t)(end - p);
  unicode_decode_utf8(p, remaining, out_cp, out_len);
  if (*out_len == 0) {
    *out_cp = (unsigned char)*p;
    *out_len = 1;
  }
  return p + *out_len;
}

static int line_append(char **line, size_t *line_len, size_t *line_cap,
                       const char *src, size_t src_len) {
  size_t needed;
  char *next;

  if (!line || !line_len || !line_cap || !src) {
    return -1;
  }

  needed = *line_len + src_len + 1;
  if (needed > *line_cap) {
    size_t next_cap = *line_cap == 0 ? 64 : *line_cap * 2;
    while (next_cap < needed) {
      next_cap *= 2;
    }
    next = (char *)realloc(*line, next_cap);
    if (!next) {
      return -1;
    }
    *line = next;
    *line_cap = next_cap;
  }

  memcpy(*line + *line_len, src, src_len);
  *line_len += src_len;
  (*line)[*line_len] = '\0';
  return 0;
}

int text_layout_terminal_width(void) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
    return (int)ws.ws_col;
  }
  return 80;
}

int text_layout_wrap(const char *text, int max_width, text_layout_lines *out) {
  const char *p;
  const char *end;
  char *line = NULL;
  size_t line_len = 0;
  size_t line_cap = 0;
  int line_width = 0;
  int width;

  if (!out) {
    return -1;
  }
  out->lines = NULL;
  out->count = 0;

  if (max_width <= 0) {
    max_width = 80;
  }
  width = max_width;

  if (!text || text[0] == '\0') {
    return push_line(out, "", 0);
  }

  p = text;
  end = text + strlen(text);
  while (p < end) {
    const char *word_start;
    const char *word_end;
    size_t word_bytes = 0;
    int word_width = 0;
    uint32_t cp = 0;
    size_t cp_len = 0;

    while (p < end) {
      const char *next = next_codepoint(p, end, &cp, &cp_len);
      if (cp_len == 0 || !is_unicode_space(cp)) {
        break;
      }
      p = next;
    }
    if (p >= end) {
      break;
    }

    word_start = p;
    while (p < end) {
      const char *next = next_codepoint(p, end, &cp, &cp_len);
      if (cp_len == 0 || is_unicode_space(cp)) {
        break;
      }
      word_bytes += cp_len;
      word_width += unicode_char_width(cp);
      p = next;
    }
    word_end = word_start + word_bytes;

    if (word_width > width) {
      const char *segment = word_start;
      if (line_len > 0) {
        if (push_line(out, line, line_len) != 0) {
          free(line);
          text_layout_free(out);
          return -1;
        }
        line_len = 0;
        line_width = 0;
      }
      while (segment < word_end) {
        const char *cursor = segment;
        size_t segment_bytes = 0;
        int segment_width = 0;
        while (cursor < word_end) {
          const char *next = next_codepoint(cursor, word_end, &cp, &cp_len);
          int cp_width = unicode_char_width(cp);
          if (segment_width > 0 && segment_width + cp_width > width) {
            break;
          }
          segment_width += cp_width;
          segment_bytes += cp_len;
          cursor = next;
        }
        if (segment_bytes == 0) {
          segment_bytes = 1;
          segment_width = 1;
          cursor = segment + 1;
        }
        if (push_line(out, segment, segment_bytes) != 0) {
          free(line);
          text_layout_free(out);
          return -1;
        }
        segment = cursor;
      }
      continue;
    }

    if (line_len == 0) {
      if (line_append(&line, &line_len, &line_cap, word_start, word_bytes) != 0) {
        free(line);
        text_layout_free(out);
        return -1;
      }
      line_width = word_width;
    } else if (line_width + 1 + word_width <= width) {
      if (line_append(&line, &line_len, &line_cap, " ", 1) != 0 ||
          line_append(&line, &line_len, &line_cap, word_start, word_bytes) != 0) {
        free(line);
        text_layout_free(out);
        return -1;
      }
      line_width += 1 + word_width;
    } else {
      if (push_line(out, line, line_len) != 0) {
        free(line);
        text_layout_free(out);
        return -1;
      }
      line_len = 0;
      line_width = 0;
      if (line_append(&line, &line_len, &line_cap, word_start, word_bytes) != 0) {
        free(line);
        text_layout_free(out);
        return -1;
      }
      line_width = word_width;
    }
  }

  if (line_len > 0 || out->count == 0) {
    if (push_line(out, line ? line : "", line_len) != 0) {
      free(line);
      text_layout_free(out);
      return -1;
    }
  }

  free(line);
  return 0;
}

void text_layout_free(text_layout_lines *lines) {
  size_t i;

  if (!lines) {
    return;
  }
  for (i = 0; i < lines->count; i++) {
    free(lines->lines[i]);
  }
  free(lines->lines);
  lines->lines = NULL;
  lines->count = 0;
}
