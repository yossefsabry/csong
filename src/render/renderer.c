#include "app/renderer.h"
#include "app/text_layout.h"
#include <stdio.h>
#include <string.h>

static void style_reset(void) {
  printf("\033[0m");
}

static void style_bold(int on) {
  if (on) {
    printf("\033[1m");
  } else {
    printf("\033[22m");
  }
}

static void style_color(int r, int g, int b) {
  printf("\033[38;2;%d;%d;%dm", r, g, b);
}

static void print_time(double elapsed) {
  int minutes = (int)(elapsed / 60.0);
  int seconds = (int)elapsed % 60;
  printf("%02d:%02d", minutes, seconds);
}

static void render_wrapped(const char *text, const char *prefix,
                           const char *indent, int r, int g, int b, int bold,
                           int content_width) {
  text_layout_lines lines;
  size_t i;
  const char *body = text ? text : "";

  if (!prefix) {
    prefix = "";
  }
  if (!indent) {
    indent = "";
  }

  if (text_layout_wrap(body, content_width, &lines) != 0) {
    style_color(r, g, b);
    style_bold(bold);
    printf("%s%s\n", prefix, body);
    style_reset();
    return;
  }

  for (i = 0; i < lines.count; i++) {
    style_color(r, g, b);
    style_bold(bold);
    if (i == 0) {
      printf("%s%s\n", prefix, lines.lines[i]);
    } else {
      printf("%s%s\n", indent, lines.lines[i]);
    }
    style_reset();
  }

  text_layout_free(&lines);
}

int renderer_init(void) {
  return 0;
}

void renderer_clear(void) {
  printf("\033[2J\033[H");
}

void renderer_draw_status(const char *status, const char *icon) {
  renderer_clear();
  if (icon && icon[0] != '\0') {
    printf("%s ", icon);
  }
  if (status) {
    style_color(120, 120, 120);
    printf("%s\n", status);
    style_reset();
  } else {
    printf("\n");
  }
  fflush(stdout);
}

void renderer_draw(const char *artist, const char *title, const lyrics_doc *doc,
                   int current_index, double elapsed, const char *status,
                   const char *icon, int pulse, int prev_index,
                   int transition_step, int transition_total) {
  int max_lines = 5;
  int context = 2;
  int term_width = text_layout_terminal_width();
  int prefix_width = 2;
  int content_width = term_width - prefix_width;
  size_t i;
  size_t start;
  size_t end;
  int is_transition = 0;
  float t = 1.0f;

  renderer_clear();
  if (artist && title) {
    if (icon && icon[0] != '\0') {
      printf("%s ", icon);
    }
    printf("%s - %s (", artist, title);
    print_time(elapsed);
    printf(")\n\n");
  }

  if (status && status[0] != '\0') {
    style_color(120, 120, 120);
    printf("%s\n\n", status);
    style_reset();
  }

  if (!doc || doc->count == 0) {
    if (!status || status[0] == '\0') {
      style_color(120, 120, 120);
      printf("No lyrics found.\n");
      style_reset();
    }
    fflush(stdout);
    return;
  }

  if (transition_total > 1 && prev_index >= 0 && current_index >= 0 &&
      prev_index != current_index) {
    is_transition = 1;
    if (transition_step < 0) {
      transition_step = 0;
    }
    if (transition_step >= transition_total) {
      transition_step = transition_total - 1;
    }
    t = (float)transition_step / (float)(transition_total - 1);
  }

  if (!doc->has_timestamps || current_index < 0) {
    start = 0;
    end = doc->count > (size_t)max_lines ? (size_t)max_lines - 1 : doc->count - 1;
    for (i = start; i <= end; i++) {
      int width = term_width > 0 ? term_width : 80;
      render_wrapped(doc->lines[i].text, "", "", 140, 140, 140, 0, width);
    }
    fflush(stdout);
    return;
  }

  start = current_index > context ? (size_t)(current_index - context) : 0;
  end = start + (size_t)max_lines - 1;
  if (end >= doc->count) {
    end = doc->count - 1;
    if (end + 1 > (size_t)max_lines) {
      start = end - (size_t)max_lines + 1;
    } else {
      start = 0;
    }
  }

  for (i = start; i <= end; i++) {
    const char *text = doc->lines[i].text ? doc->lines[i].text : "";
    if ((int)i == current_index) {
      int curr_base = 210;
      int curr_max = 255;
      int curr_color = is_transition ? (int)(curr_base + (curr_max - curr_base) * t)
                                      : 255;
      int bold = is_transition ? (t > 0.6f) : pulse;
      int width = content_width > 0 ? content_width : (term_width > 0 ? term_width : 80);
      render_wrapped(text, "> ", "  ", curr_color, curr_color, curr_color, bold,
                     width);
    } else if (is_transition && (int)i == prev_index) {
      int prev_min = 140;
      int prev_max = 255;
      int prev_color = (int)(prev_max + (prev_min - prev_max) * t);
      int width = content_width > 0 ? content_width : (term_width > 0 ? term_width : 80);
      render_wrapped(text, "  ", "  ", prev_color, prev_color, prev_color, 0,
                     width);
    } else {
      int width = content_width > 0 ? content_width : (term_width > 0 ? term_width : 80);
      render_wrapped(text, "  ", "  ", 140, 140, 140, 0, width);
    }
  }
  fflush(stdout);
}

void renderer_shutdown(void) {
}
