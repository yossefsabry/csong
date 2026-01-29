#include "app/ui.h"
#include "app/log.h"
#include "app/renderer.h"
#include "x11_backend.h"
#include <strings.h>

typedef enum {
  UI_BACKEND_TERMINAL = 0,
  UI_BACKEND_X11 = 1
} ui_backend_kind;

static ui_backend_kind g_backend = UI_BACKEND_TERMINAL;

static void ui_apply_terminal_style(const ui_options *options) {
  if (!options) {
    return;
  }
  renderer_set_style(options->fg_r, options->fg_g, options->fg_b, options->dim_r,
                     options->dim_g, options->dim_b, options->prev_r,
                     options->prev_g, options->prev_b, options->bg_r,
                     options->bg_g, options->bg_b, options->title_r,
                     options->title_g, options->title_b, options->padding_x,
                     options->padding_y);
}

static int ui_select_backend(const char *name) {
  if (!name || name[0] == '\0') {
    g_backend = UI_BACKEND_TERMINAL;
    return 0;
  }
  if (strcasecmp(name, "terminal") == 0) {
    g_backend = UI_BACKEND_TERMINAL;
    return 0;
  }
  if (strcasecmp(name, "x11") == 0) {
    g_backend = UI_BACKEND_X11;
    return 0;
  }
  return -1;
}

int ui_init(const ui_options *options) {
  if (options) {
    if (ui_select_backend(options->backend) != 0) {
      log_error("ui: unknown backend, falling back to terminal");
      g_backend = UI_BACKEND_TERMINAL;
    }
  }

  switch (g_backend) {
    case UI_BACKEND_X11:
      if (x11_backend_init(options) == 0) {
        return 0;
      }
      log_error("ui: x11 init failed, falling back to terminal");
      g_backend = UI_BACKEND_TERMINAL;
      ui_apply_terminal_style(options);
      return renderer_init();
    case UI_BACKEND_TERMINAL:
    default:
      ui_apply_terminal_style(options);
      return renderer_init();
  }
}

void ui_set_rtl(int rtl_mode, int rtl_align, int rtl_shape, int bidi_mode) {
  switch (g_backend) {
    case UI_BACKEND_X11:
      x11_backend_set_rtl(rtl_mode, rtl_align, rtl_shape, bidi_mode);
      return;
    case UI_BACKEND_TERMINAL:
    default:
      renderer_set_rtl(rtl_mode, rtl_align, rtl_shape, bidi_mode);
      return;
  }
}

void ui_draw_status(const char *status, const char *icon) {
  switch (g_backend) {
    case UI_BACKEND_X11:
      x11_backend_draw_status(status, icon);
      return;
    case UI_BACKEND_TERMINAL:
    default:
      renderer_draw_status(status, icon);
      return;
  }
}

void ui_draw(const char *artist, const char *title, const lyrics_doc *doc,
             int current_index, double elapsed, const char *status,
             const char *icon, int pulse, int prev_index, int transition_step,
             int transition_total) {
  switch (g_backend) {
    case UI_BACKEND_X11:
      x11_backend_draw(artist, title, doc, current_index, elapsed, status, icon,
                       pulse, prev_index, transition_step, transition_total);
      return;
    case UI_BACKEND_TERMINAL:
    default:
      renderer_draw(artist, title, doc, current_index, elapsed, status, icon,
                    pulse, prev_index, transition_step, transition_total);
      return;
  }
}

void ui_shutdown(void) {
  switch (g_backend) {
    case UI_BACKEND_X11:
      x11_backend_shutdown();
      return;
    case UI_BACKEND_TERMINAL:
    default:
      renderer_shutdown();
      return;
  }
}
