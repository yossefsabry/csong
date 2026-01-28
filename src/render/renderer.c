#include "app/renderer.h"
#include "app/text_layout.h"
#include "app/unicode.h"
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_rtl_mode = UNICODE_RTL_AUTO;
static int g_rtl_align = UNICODE_RTL_ALIGN_LEFT;
static int g_rtl_shape = UNICODE_RTL_SHAPE_AUTO;
static int g_bidi_mode = UNICODE_BIDI_FRIBIDI;

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
    char *visual = NULL;
    int is_rtl = 0;
    const char *line_text = lines.lines[i];
    int align_right = 0;
    int padding = 0;

    if (unicode_visual_order(line_text, g_rtl_mode, g_rtl_shape, g_bidi_mode,
                             &visual, &is_rtl) == 0 && visual) {
      line_text = visual;
    }

    if (g_rtl_align == UNICODE_RTL_ALIGN_RIGHT) {
      align_right = 1;
    }

    if (align_right && content_width > 0) {
      int line_width = unicode_display_width(line_text);
      if (line_width < content_width) {
        padding = content_width - line_width;
      }
    }

    style_color(r, g, b);
    style_bold(bold);
    if (i == 0) {
      if (g_bidi_mode == UNICODE_BIDI_TERMINAL) {
        char *wrapped = NULL;
        if (unicode_wrap_with_lrm(prefix, &wrapped) == 0 && wrapped) {
          printf("%s", wrapped);
          free(wrapped);
        } else {
          printf("%s", prefix);
        }
      } else {
        printf("%s", prefix);
      }
    } else {
      if (g_bidi_mode == UNICODE_BIDI_TERMINAL) {
        char *wrapped = NULL;
        if (unicode_wrap_with_lrm(indent, &wrapped) == 0 && wrapped) {
          printf("%s", wrapped);
          free(wrapped);
        } else {
          printf("%s", indent);
        }
      } else {
        printf("%s", indent);
      }
    }
    if (padding > 0) {
      printf("%*s", padding, "");
    }
    if (g_bidi_mode == UNICODE_BIDI_FRIBIDI) {
      char *wrapped = NULL;
      if (unicode_wrap_with_lro(line_text, &wrapped) == 0 && wrapped) {
        printf("%s\n", wrapped);
        free(wrapped);
      } else {
        printf("%s\n", line_text);
      }
    } else {
      printf("%s\n", line_text);
    }
    style_reset();

    free(visual);
  }

  text_layout_free(&lines);
}

int renderer_init(void) {
  setlocale(LC_CTYPE, "");
  return 0;
}

void renderer_set_rtl(int rtl_mode, int rtl_align, int rtl_shape, int bidi_mode) {
  g_rtl_mode = rtl_mode;
  g_rtl_align = rtl_align;
  g_rtl_shape = rtl_shape;
  g_bidi_mode = bidi_mode;
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
    char *visual = NULL;
    int is_rtl = 0;
    const char *text = status;
    if (unicode_visual_order(status, g_rtl_mode, g_rtl_shape, g_bidi_mode,
                             &visual, &is_rtl) == 0 && visual) {
      text = visual;
    }
    style_color(120, 120, 120);
    if (g_bidi_mode == UNICODE_BIDI_FRIBIDI) {
      char *wrapped = NULL;
      if (unicode_wrap_with_lro(text, &wrapped) == 0 && wrapped) {
        printf("%s\n", wrapped);
        free(wrapped);
      } else {
        printf("%s\n", text);
      }
    } else {
      printf("%s\n", text);
    }
    style_reset();
    free(visual);
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
    char header[768];
    char *visual = NULL;
    const char *header_text = NULL;
    if (icon && icon[0] != '\0') {
      printf("%s ", icon);
    }
    snprintf(header, sizeof(header), "%s - %s", artist, title);
    if (unicode_visual_order(header, g_rtl_mode, g_rtl_shape, g_bidi_mode,
                             &visual, NULL) == 0 && visual) {
      header_text = visual;
    } else {
      header_text = header;
    }
    if (g_bidi_mode == UNICODE_BIDI_FRIBIDI) {
      char *wrapped = NULL;
      if (unicode_wrap_with_lro(header_text, &wrapped) == 0 && wrapped) {
        printf("%s (", wrapped);
        free(wrapped);
      } else {
        printf("%s (", header_text);
      }
    } else {
      printf("%s (", header_text);
    }
    print_time(elapsed);
    printf(")\n\n");
    free(visual);
  }

  if (status && status[0] != '\0') {
    char *visual = NULL;
    const char *text = status;
    if (unicode_visual_order(status, g_rtl_mode, g_rtl_shape, g_bidi_mode,
                             &visual, NULL) == 0 && visual) {
      text = visual;
    }
    style_color(120, 120, 120);
    if (g_bidi_mode == UNICODE_BIDI_FRIBIDI) {
      char *wrapped = NULL;
      if (unicode_wrap_with_lro(text, &wrapped) == 0 && wrapped) {
        printf("%s\n\n", wrapped);
        free(wrapped);
      } else {
        printf("%s\n\n", text);
      }
    } else {
      printf("%s\n\n", text);
    }
    style_reset();
    free(visual);
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
