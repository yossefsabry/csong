#ifndef CSONG_X11_BACKEND_H
#define CSONG_X11_BACKEND_H

#include "app/ui.h"

int x11_backend_init(const ui_options *options);
void x11_backend_set_rtl(int rtl_mode, int rtl_align, int rtl_shape,
                         int bidi_mode);
void x11_backend_draw_status(const char *status, const char *icon);
void x11_backend_draw(const char *artist, const char *title, const lyrics_doc *doc,
                      int current_index, double elapsed, const char *status,
                      const char *icon, int pulse, int prev_index,
                      int transition_step, int transition_total);
void x11_backend_shutdown(void);

#endif
