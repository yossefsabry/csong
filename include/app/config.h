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
