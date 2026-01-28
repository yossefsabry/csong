#include "app/time.h"
#include <time.h>

long time_now_ms(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }
  return (long)(ts.tv_sec * 1000L + ts.tv_nsec / 1000000L);
}
