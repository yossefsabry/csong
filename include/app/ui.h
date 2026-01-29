#ifndef CSONG_UI_H
#define CSONG_UI_H

#include "lyrics.h"

typedef struct ui_options {
  char backend[32];
  char font[128];
  char title_font[128];
  char title_weight[32];
  char title_style[32];
  double opacity;
  int width;
  int height;
  int offset_x;
  int offset_y;
  int padding_x;
  int padding_y;
  int click_through;
  int fg_r;
  int fg_g;
  int fg_b;
  int dim_r;
  int dim_g;
  int dim_b;
  int prev_r;
  int prev_g;
  int prev_b;
  int bg_r;
  int bg_g;
  int bg_b;
  int title_r;
  int title_g;
  int title_b;
  double line_spacing;
  double title_scale;
  char anchor[32];
} ui_options;

int ui_init(const ui_options *options);
void ui_set_rtl(int rtl_mode, int rtl_align, int rtl_shape, int bidi_mode);
void ui_draw_status(const char *status, const char *icon);
void ui_draw(const char *artist, const char *title, const lyrics_doc *doc,
             int current_index, double elapsed, const char *status,
             const char *icon, int pulse, int prev_index, int transition_step,
             int transition_total);
void ui_shutdown(void);

#endif
