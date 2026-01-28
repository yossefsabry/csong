#ifndef CSONG_TEXT_LAYOUT_H
#define CSONG_TEXT_LAYOUT_H

#include <stddef.h>

typedef struct text_layout_lines {
  char **lines;
  size_t count;
} text_layout_lines;

int text_layout_terminal_width(void);
int text_layout_wrap(const char *text, int max_width, text_layout_lines *out);
void text_layout_free(text_layout_lines *lines);

#endif
