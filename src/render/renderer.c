#include "app/renderer.h"
#include <stdio.h>

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

int renderer_init(void) {
  return 0;
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
    style_color(120, 120, 120);
    printf("%s\n", status);
    style_reset();
  } else {
    printf("\n");
  }
  fflush(stdout);
}

void renderer_draw(const char *artist, const char *title, const lyrics_doc *doc,
                   int current_index, double elapsed, const char *status,
                   const char *icon, int pulse) {
  int max_lines = 5;
  int context = 2;
  size_t i;
  size_t start;
  size_t end;

  renderer_clear();
  if (artist && title) {
    if (icon && icon[0] != '\0') {
      printf("%s ", icon);
    }
    printf("%s - %s (", artist, title);
    print_time(elapsed);
    printf(")\n\n");
  }

  if (status && status[0] != '\0') {
    style_color(120, 120, 120);
    printf("%s\n\n", status);
    style_reset();
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

  if (!doc->has_timestamps || current_index < 0) {
    start = 0;
    end = doc->count > (size_t)max_lines ? (size_t)max_lines - 1 : doc->count - 1;
    for (i = start; i <= end; i++) {
      style_color(140, 140, 140);
      printf("%s\n", doc->lines[i].text ? doc->lines[i].text : "");
      style_reset();
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
      style_color(255, 255, 255);
      style_bold(pulse);
      printf("> %s\n", text);
      style_reset();
    } else {
      style_color(140, 140, 140);
      printf("  %s\n", text);
      style_reset();
    }
  }
  fflush(stdout);
}

void renderer_shutdown(void) {
}
