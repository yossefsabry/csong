#ifndef CSONG_CONFIG_H
#define CSONG_CONFIG_H

#include <stddef.h>

typedef struct app_config {
  char mpd_host[128];
  int mpd_port;
  int interval;
  int show_plain;
  char cache_dir[512];
} app_config;

void config_default(app_config *out);
int config_load(const char *path, app_config *out);
int config_default_path(char *out, size_t out_size);
int config_resolve_path(const char *path, char *out, size_t out_size);

#endif
