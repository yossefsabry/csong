#include "app/config.h"

void config_default(app_config *out) {
  if (out) {
    out->reserved = 0;
  }
}

int config_load(const char *path, app_config *out) {
  (void)path;
  (void)out;
  return 0;
}
