#ifndef CSONG_CONFIG_H
#define CSONG_CONFIG_H

typedef struct app_config {
  int reserved;
} app_config;

void config_default(app_config *out);
int config_load(const char *path, app_config *out);

#endif
