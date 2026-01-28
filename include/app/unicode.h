#ifndef CSONG_UNICODE_H
#define CSONG_UNICODE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
  UNICODE_RTL_AUTO = 0,
  UNICODE_RTL_ON = 1,
  UNICODE_RTL_OFF = 2
} unicode_rtl_mode;

typedef enum {
  UNICODE_RTL_ALIGN_AUTO = 0,
  UNICODE_RTL_ALIGN_LEFT = 1,
  UNICODE_RTL_ALIGN_RIGHT = 2
} unicode_rtl_align;

typedef enum {
  UNICODE_RTL_SHAPE_AUTO = 0,
  UNICODE_RTL_SHAPE_ON = 1,
  UNICODE_RTL_SHAPE_OFF = 2
} unicode_rtl_shape;

typedef enum {
  UNICODE_BIDI_FRIBIDI = 0,
  UNICODE_BIDI_TERMINAL = 1
} unicode_bidi_mode;

int unicode_decode_utf8(const char *text, size_t max_len, uint32_t *out_codepoint,
                        size_t *out_len);
int unicode_encode_utf8(uint32_t codepoint, char out[4], size_t *out_len);
int unicode_char_width(uint32_t codepoint);
int unicode_display_width(const char *text);
int unicode_visual_order(const char *text, int rtl_mode, int shape_mode,
                         int bidi_mode, char **out_visual, int *out_is_rtl);
int unicode_wrap_with_lro(const char *text, char **out_wrapped);
int unicode_wrap_with_lrm(const char *text, char **out_wrapped);

#endif
