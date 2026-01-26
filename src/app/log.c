#include "app/log.h"
#include <stdio.h>

void log_info(const char *msg) {
  if (msg) {
    fprintf(stderr, "info: %s\n", msg);
  }
}

void log_error(const char *msg) {
  if (msg) {
    fprintf(stderr, "error: %s\n", msg);
  }
}
