#ifndef CSONG_CONFIG_H
#define CSONG_CONFIG_H

#include "app/unicode.h"
#include <stddef.h>

typedef struct app_config {
  char mpd_host[128];
  int mpd_port;
  int interval;
  int show_plain;
  char cache_dir[512];
  char ui_backend[32];
  char ui_font[128];
  char ui_title_font[128];
  char ui_title_weight[32];
  char ui_title_style[32];
  double ui_opacity;
  int ui_width;
  int ui_height;
  int ui_offset_x;
  int ui_offset_y;
  int ui_padding_x;
  int ui_padding_y;
  int ui_click_through;
  int ui_fg_r;
  int ui_fg_g;
  int ui_fg_b;
  int ui_dim_r;
  int ui_dim_g;
  int ui_dim_b;
  int ui_prev_r;
  int ui_prev_g;
  int ui_prev_b;
  int ui_bg_r;
  int ui_bg_g;
  int ui_bg_b;
  int ui_title_r;
  int ui_title_g;
  int ui_title_b;
  double ui_line_spacing;
  double ui_title_scale;
  char ui_anchor[32];
  int rtl_mode;
  int rtl_align;
  int rtl_shape;
  int bidi_mode;
} app_config;

void config_default(app_config *out);
int config_load(const char *path, app_config *out);
int config_default_path(char *out, size_t out_size);
int config_resolve_path(const char *path, char *out, size_t out_size);

#endif
