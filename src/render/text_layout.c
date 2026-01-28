#include "app/text_layout.h"
#include <ctype.h>
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

int text_layout_terminal_width(void) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
    return (int)ws.ws_col;
  }
  return 80;
}

int text_layout_wrap(const char *text, int max_width, text_layout_lines *out) {
  const char *p;
  char *line;
  size_t line_len = 0;
  size_t width;

  if (!out) {
    return -1;
  }
  out->lines = NULL;
  out->count = 0;

  if (max_width <= 0) {
    max_width = 80;
  }
  width = (size_t)max_width;

  if (!text || text[0] == '\0') {
    return push_line(out, "", 0);
  }

  line = (char *)malloc(width + 1);
  if (!line) {
    return -1;
  }

  p = text;
  while (*p) {
    const char *word_start;
    size_t word_len = 0;

    while (*p && isspace((unsigned char)*p)) {
      p++;
    }
    if (!*p) {
      break;
    }

    word_start = p;
    while (*p && !isspace((unsigned char)*p)) {
      word_len++;
      p++;
    }

    if (word_len > width) {
      size_t offset = 0;
      if (line_len > 0) {
        if (push_line(out, line, line_len) != 0) {
          free(line);
          text_layout_free(out);
          return -1;
        }
        line_len = 0;
      }
      while (offset < word_len) {
        size_t chunk = word_len - offset;
        if (chunk > width) {
          chunk = width;
        }
        if (push_line(out, word_start + offset, chunk) != 0) {
          free(line);
          text_layout_free(out);
          return -1;
        }
        offset += chunk;
      }
      continue;
    }

    if (line_len == 0) {
      memcpy(line, word_start, word_len);
      line_len = word_len;
    } else if (line_len + 1 + word_len <= width) {
      line[line_len++] = ' ';
      memcpy(line + line_len, word_start, word_len);
      line_len += word_len;
    } else {
      if (push_line(out, line, line_len) != 0) {
        free(line);
        text_layout_free(out);
        return -1;
      }
      memcpy(line, word_start, word_len);
      line_len = word_len;
    }
  }

  if (line_len > 0 || out->count == 0) {
    if (push_line(out, line, line_len) != 0) {
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
