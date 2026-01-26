#ifndef CSONG_RENDERER_H
#define CSONG_RENDERER_H

#include "lyrics.h"

int renderer_init(void);
void renderer_clear(void);
void renderer_draw_status(const char *status, const char *icon);
void renderer_draw(const char *artist, const char *title,
                   const lyrics_doc *doc, int current_index, double elapsed,
                   const char *status, const char *icon);
void renderer_shutdown(void);

#endif
