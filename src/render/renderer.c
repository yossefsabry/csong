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
static int g_fg_r = 255;
static int g_fg_g = 255;
static int g_fg_b = 255;
static int g_title_r = 255;
static int g_title_g = 255;
static int g_title_b = 255;
static int g_dim_r = 140;
static int g_dim_g = 140;
static int g_dim_b = 140;
static int g_prev_r = 210;
static int g_prev_g = 210;
static int g_prev_b = 210;
static int g_bg_r = 0;
static int g_bg_g = 0;
static int g_bg_b = 0;
static int g_pad_x = 0;
static int g_pad_y = 0;

static int clamp_color(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > 255) {
    return 255;
  }
  return value;
}

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

static void style_bg(int r, int g, int b) {
  printf("\033[48;2;%d;%d;%dm", r, g, b);
}

static void print_left_padding(void) {
  if (g_pad_x > 0) {
    printf("%*s", g_pad_x, "");
  }
}

static void render_top_padding(void) {
  int i;
  for (i = 0; i < g_pad_y; i++) {
    printf("\n");
  }
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
    style_bg(g_bg_r, g_bg_g, g_bg_b);
    style_bold(bold);
    print_left_padding();
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
    style_bg(g_bg_r, g_bg_g, g_bg_b);
    style_bold(bold);
    print_left_padding();
    if (i == 0) {
    if (is_rtl) {
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
    if (is_rtl) {
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
    if (is_rtl) {
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

void renderer_set_style(int fg_r, int fg_g, int fg_b, int dim_r, int dim_g,
                        int dim_b, int prev_r, int prev_g, int prev_b, int bg_r,
                        int bg_g, int bg_b, int title_r, int title_g,
                        int title_b, int padding_x, int padding_y) {
  g_fg_r = clamp_color(fg_r);
  g_fg_g = clamp_color(fg_g);
  g_fg_b = clamp_color(fg_b);
  g_dim_r = clamp_color(dim_r);
  g_dim_g = clamp_color(dim_g);
  g_dim_b = clamp_color(dim_b);
  g_prev_r = clamp_color(prev_r);
  g_prev_g = clamp_color(prev_g);
  g_prev_b = clamp_color(prev_b);
  g_bg_r = clamp_color(bg_r);
  g_bg_g = clamp_color(bg_g);
  g_bg_b = clamp_color(bg_b);
  g_title_r = clamp_color(title_r);
  g_title_g = clamp_color(title_g);
  g_title_b = clamp_color(title_b);
  g_pad_x = padding_x < 0 ? 0 : padding_x;
  g_pad_y = padding_y < 0 ? 0 : padding_y;
}

void renderer_clear(void) {
  printf("\033[2J\033[H");
}

void renderer_draw_status(const char *status, const char *icon) {
  int term_width = text_layout_terminal_width();
  int inner_width = term_width - g_pad_x * 2;
  char line[768];

  renderer_clear();
  render_top_padding();
  if (inner_width < 1) {
    inner_width = term_width > 0 ? term_width : 80;
  }

  if (icon && icon[0] != '\0' && status && status[0] != '\0') {
    snprintf(line, sizeof(line), "%s %s", icon, status);
  } else if (icon && icon[0] != '\0') {
    snprintf(line, sizeof(line), "%s", icon);
  } else if (status && status[0] != '\0') {
    snprintf(line, sizeof(line), "%s", status);
  } else {
    line[0] = '\0';
  }

  render_wrapped(line, "", "", g_dim_r, g_dim_g, g_dim_b, 0, inner_width);
  fflush(stdout);
}

void renderer_draw(const char *artist, const char *title, const lyrics_doc *doc,
                   int current_index, double elapsed, const char *status,
                   const char *icon, int pulse, int prev_index,
                   int transition_step, int transition_total) {
  int max_lines = 5;
  int context = 2;
  int term_width = text_layout_terminal_width();
  int inner_width = term_width - g_pad_x * 2;
  int prefix_width = 2;
  int content_width = 0;
  size_t i;
  size_t start;
  size_t end;
  int is_transition = 0;
  float t = 1.0f;

  renderer_clear();
  render_top_padding();
  if (inner_width < 1) {
    inner_width = term_width > 0 ? term_width : 80;
  }
  content_width = inner_width - prefix_width;
  if (content_width < 1) {
    content_width = inner_width;
  }
  if (artist && title) {
    char header[768];
    int minutes = (int)(elapsed / 60.0);
    int seconds = (int)elapsed % 60;
    if (icon && icon[0] != '\0') {
      snprintf(header, sizeof(header), "%s %s - %s (%02d:%02d)", icon, artist,
               title, minutes, seconds);
    } else {
      snprintf(header, sizeof(header), "%s - %s (%02d:%02d)", artist, title,
               minutes, seconds);
    }
    render_wrapped(header, "", "", g_title_r, g_title_g, g_title_b, 1,
                   inner_width);
    printf("\n");
  }

  if (status && status[0] != '\0') {
    render_wrapped(status, "", "", g_dim_r, g_dim_g, g_dim_b, 0, inner_width);
    printf("\n");
  }

  if (!doc || doc->count == 0) {
    if (!status || status[0] == '\0') {
      render_wrapped("No lyrics found.", "", "", g_dim_r, g_dim_g, g_dim_b, 0,
                     inner_width);
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
      render_wrapped(doc->lines[i].text, "", "", g_dim_r, g_dim_g, g_dim_b, 0,
                     inner_width);
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
      int curr_r = g_fg_r;
      int curr_g = g_fg_g;
      int curr_b = g_fg_b;
      int bold = is_transition ? (t > 0.6f) : pulse;
      if (is_transition) {
        curr_r = g_prev_r + (int)((g_fg_r - g_prev_r) * t);
        curr_g = g_prev_g + (int)((g_fg_g - g_prev_g) * t);
        curr_b = g_prev_b + (int)((g_fg_b - g_prev_b) * t);
      }
      render_wrapped(text, "> ", "  ", curr_r, curr_g, curr_b, bold,
                     content_width);
    } else if (is_transition && (int)i == prev_index) {
      int prev_r = g_prev_r + (int)((g_dim_r - g_prev_r) * t);
      int prev_g = g_prev_g + (int)((g_dim_g - g_prev_g) * t);
      int prev_b = g_prev_b + (int)((g_dim_b - g_prev_b) * t);
      render_wrapped(text, "  ", "  ", prev_r, prev_g, prev_b, 0, content_width);
    } else {
      render_wrapped(text, "  ", "  ", g_dim_r, g_dim_g, g_dim_b, 0,
                     content_width);
    }
  }
  fflush(stdout);
}

void renderer_shutdown(void) {
}
