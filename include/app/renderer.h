#ifndef CSONG_RENDERER_H
#define CSONG_RENDERER_H

#include "lyrics.h"

int renderer_init(void);
void renderer_clear(void);
void renderer_draw_status(const char *status, const char *icon);
void renderer_set_rtl(int rtl_mode, int rtl_align, int rtl_shape, int bidi_mode);
void renderer_set_style(int fg_r, int fg_g, int fg_b, int dim_r, int dim_g,
                        int dim_b, int prev_r, int prev_g, int prev_b, int bg_r,
                        int bg_g, int bg_b, int title_r, int title_g,
                        int title_b, int padding_x, int padding_y);
void renderer_draw(const char *artist, const char *title,
                   const lyrics_doc *doc, int current_index, double elapsed,
                   const char *status, const char *icon, int pulse,
                   int prev_index, int transition_step, int transition_total);
void renderer_shutdown(void);

#endif
