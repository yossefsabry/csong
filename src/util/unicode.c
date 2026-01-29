#include "app/unicode.h"
#include <fribidi/fribidi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

int unicode_decode_utf8(const char *text, size_t max_len, uint32_t *out_codepoint,
                        size_t *out_len) {
  const unsigned char *s = (const unsigned char *)text;
  uint32_t cp;

  if (!text || !out_codepoint || !out_len) {
    return -1;
  }

  if (max_len == 0) {
    *out_codepoint = 0;
    *out_len = 0;
    return -1;
  }

  if (s[0] < 0x80) {
    *out_codepoint = s[0];
    *out_len = 1;
    return 0;
  }

  if (s[0] >= 0xC2 && s[0] <= 0xDF && max_len >= 2) {
    if ((s[1] & 0xC0) != 0x80) {
      *out_codepoint = 0xFFFD;
      *out_len = 1;
      return -1;
    }
    cp = ((uint32_t)(s[0] & 0x1F) << 6) | (uint32_t)(s[1] & 0x3F);
    *out_codepoint = cp;
    *out_len = 2;
    return 0;
  }

  if (s[0] >= 0xE0 && s[0] <= 0xEF && max_len >= 3) {
    if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) {
      *out_codepoint = 0xFFFD;
      *out_len = 1;
      return -1;
    }
    if (s[0] == 0xE0 && s[1] < 0xA0) {
      *out_codepoint = 0xFFFD;
      *out_len = 1;
      return -1;
    }
    if (s[0] == 0xED && s[1] >= 0xA0) {
      *out_codepoint = 0xFFFD;
      *out_len = 1;
      return -1;
    }
    cp = ((uint32_t)(s[0] & 0x0F) << 12) |
         ((uint32_t)(s[1] & 0x3F) << 6) |
         (uint32_t)(s[2] & 0x3F);
    *out_codepoint = cp;
    *out_len = 3;
    return 0;
  }

  if (s[0] >= 0xF0 && s[0] <= 0xF4 && max_len >= 4) {
    if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 ||
        (s[3] & 0xC0) != 0x80) {
      *out_codepoint = 0xFFFD;
      *out_len = 1;
      return -1;
    }
    if (s[0] == 0xF0 && s[1] < 0x90) {
      *out_codepoint = 0xFFFD;
      *out_len = 1;
      return -1;
    }
    if (s[0] == 0xF4 && s[1] > 0x8F) {
      *out_codepoint = 0xFFFD;
      *out_len = 1;
      return -1;
    }
    cp = ((uint32_t)(s[0] & 0x07) << 18) |
         ((uint32_t)(s[1] & 0x3F) << 12) |
         ((uint32_t)(s[2] & 0x3F) << 6) |
         (uint32_t)(s[3] & 0x3F);
    *out_codepoint = cp;
    *out_len = 4;
    return 0;
  }

  *out_codepoint = 0xFFFD;
  *out_len = 1;
  return -1;
}

int unicode_encode_utf8(uint32_t codepoint, char out[4], size_t *out_len) {
  if (!out || !out_len) {
    return -1;
  }

  if (codepoint <= 0x7F) {
    out[0] = (char)codepoint;
    *out_len = 1;
    return 0;
  }
  if (codepoint <= 0x7FF) {
    out[0] = (char)(0xC0 | (codepoint >> 6));
    out[1] = (char)(0x80 | (codepoint & 0x3F));
    *out_len = 2;
    return 0;
  }
  if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
    codepoint = 0xFFFD;
  }
  if (codepoint <= 0xFFFF) {
    out[0] = (char)(0xE0 | (codepoint >> 12));
    out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
    out[2] = (char)(0x80 | (codepoint & 0x3F));
    *out_len = 3;
    return 0;
  }
  if (codepoint <= 0x10FFFF) {
    out[0] = (char)(0xF0 | (codepoint >> 18));
    out[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
    out[3] = (char)(0x80 | (codepoint & 0x3F));
    *out_len = 4;
    return 0;
  }

  out[0] = (char)0xEF;
  out[1] = (char)0xBF;
  out[2] = (char)0xBD;
  *out_len = 3;
  return -1;
}

int unicode_char_width(uint32_t codepoint) {
  int width;
  if (codepoint == 0) {
    return 0;
  }
  width = wcwidth((wchar_t)codepoint);
  if (width < 0) {
    return 0;
  }
  return width;
}

int unicode_display_width(const char *text) {
  const char *p = text;
  const char *end;
  int width = 0;
  size_t len = 0;
  uint32_t cp = 0;

  if (!text) {
    return 0;
  }

  end = text + strlen(text);
  while (p < end) {
    unicode_decode_utf8(p, (size_t)(end - p), &cp, &len);
    if (len == 0) {
      break;
    }
    width += unicode_char_width(cp);
    p += len;
  }

  return width;
}

static int unicode_to_codepoints(const char *text, FriBidiChar **out_chars,
                                 FriBidiStrIndex *out_len) {
  size_t capacity;
  size_t count = 0;
  FriBidiChar *chars;
  const char *p = text;
  const char *end;
  size_t len;
  uint32_t cp;

  if (!text || !out_chars || !out_len) {
    return -1;
  }

  capacity = strlen(text) + 1;
  chars = (FriBidiChar *)malloc(sizeof(*chars) * capacity);
  if (!chars) {
    return -1;
  }

  end = text + strlen(text);
  while (p < end) {
    unicode_decode_utf8(p, (size_t)(end - p), &cp, &len);
    if (len == 0) {
      break;
    }
    if (count >= capacity) {
      size_t next_cap = capacity + 32;
      FriBidiChar *next = (FriBidiChar *)realloc(chars, sizeof(*chars) * next_cap);
      if (!next) {
        free(chars);
        return -1;
      }
      chars = next;
      capacity = next_cap;
    }
    chars[count++] = (FriBidiChar)cp;
    p += len;
  }

  *out_chars = chars;
  *out_len = (FriBidiStrIndex)count;
  return 0;
}

static char *unicode_from_codepoints(const FriBidiChar *chars,
                                     FriBidiStrIndex len) {
  size_t out_cap = (size_t)len * 4 + 1;
  size_t out_len = 0;
  char *out;
  FriBidiStrIndex i;

  out = (char *)malloc(out_cap);
  if (!out) {
    return NULL;
  }

  for (i = 0; i < len; i++) {
    char buffer[4];
    size_t wrote = 0;
    if (unicode_encode_utf8((uint32_t)chars[i], buffer, &wrote) != 0) {
      continue;
    }
    if (out_len + wrote >= out_cap) {
      size_t next_cap = out_cap + (size_t)len * 2 + 8;
      char *next = (char *)realloc(out, next_cap);
      if (!next) {
        free(out);
        return NULL;
      }
      out = next;
      out_cap = next_cap;
    }
    memcpy(out + out_len, buffer, wrote);
    out_len += wrote;
  }

  out[out_len] = '\0';
  return out;
}

int unicode_visual_order(const char *text, int rtl_mode, int shape_mode,
                         int bidi_mode, char **out_visual, int *out_is_rtl) {
  FriBidiChar *logical = NULL;
  FriBidiChar *visual = NULL;
  FriBidiCharType *bidi_types = NULL;
  FriBidiBracketType *brackets = NULL;
  FriBidiLevel *levels = NULL;
  FriBidiArabicProp *arabic = NULL;
  FriBidiStrIndex len = 0;
  FriBidiParType base_dir = FRIBIDI_PAR_LTR;
  FriBidiParType detected_dir = FRIBIDI_PAR_LTR;
  int apply_shape = 0;
  int has_strong = 0;
  int has_arabic = 0;
  char *result = NULL;
  int i;

  if (!out_visual) {
    return -1;
  }
  (void)bidi_mode;
  *out_visual = NULL;
  if (out_is_rtl) {
    *out_is_rtl = 0;
  }

  if (!text || text[0] == '\0') {
    result = (char *)malloc(1);
    if (!result) {
      return -1;
    }
    result[0] = '\0';
    *out_visual = result;
    return 0;
  }

  if (rtl_mode == UNICODE_RTL_OFF) {
    result = (char *)malloc(strlen(text) + 1);
    if (!result) {
      return -1;
    }
    snprintf(result, strlen(text) + 1, "%s", text);
    *out_visual = result;
    if (out_is_rtl) {
      *out_is_rtl = 0;
    }
    return 0;
  }



  if (unicode_to_codepoints(text, &logical, &len) != 0 || len == 0) {
    free(logical);
    return -1;
  }

  if (rtl_mode == UNICODE_RTL_ON) {
    base_dir = FRIBIDI_PAR_RTL;
  } else if (rtl_mode == UNICODE_RTL_OFF) {
    base_dir = FRIBIDI_PAR_LTR;
  } else {
    base_dir = FRIBIDI_PAR_ON;
  }

  bidi_types = (FriBidiCharType *)malloc(sizeof(*bidi_types) * len);
  brackets = (FriBidiBracketType *)malloc(sizeof(*brackets) * len);
  levels = (FriBidiLevel *)malloc(sizeof(*levels) * len);
  if (!bidi_types || !brackets || !levels) {
    free(logical);
    free(bidi_types);
    free(brackets);
    free(levels);
    return -1;
  }

  fribidi_get_bidi_types(logical, len, bidi_types);
  fribidi_get_bracket_types(logical, len, bidi_types, brackets);

  for (i = 0; i < len; i++) {
    if (!has_strong && FRIBIDI_IS_STRONG(bidi_types[i])) {
      detected_dir = FRIBIDI_IS_RTL(bidi_types[i]) ? FRIBIDI_PAR_RTL
                                                   : FRIBIDI_PAR_LTR;
      has_strong = 1;
    }
    if (!has_arabic && FRIBIDI_IS_ARABIC(bidi_types[i])) {
      has_arabic = 1;
    }
  }

  if (rtl_mode == UNICODE_RTL_AUTO) {
    base_dir = has_strong ? detected_dir : FRIBIDI_PAR_LTR;
  }

  if (fribidi_get_par_embedding_levels_ex(bidi_types, brackets, len, &base_dir,
                                          levels) == 0) {
    free(logical);
    free(bidi_types);
    free(brackets);
    free(levels);
    return -1;
  }

  if (out_is_rtl) {
    *out_is_rtl = (base_dir == FRIBIDI_PAR_RTL);
  }

  if (shape_mode == UNICODE_RTL_SHAPE_ON ||
      shape_mode == UNICODE_RTL_SHAPE_AUTO) {
    apply_shape = has_arabic;
  }

  if (apply_shape) {
    arabic = (FriBidiArabicProp *)malloc(sizeof(*arabic) * len);
    if (arabic) {
      fribidi_get_joining_types(logical, len, (FriBidiJoiningType *)arabic);
      fribidi_join_arabic(bidi_types, len, levels, arabic);
      fribidi_shape((FriBidiFlags)(FRIBIDI_FLAGS_DEFAULT | FRIBIDI_FLAGS_ARABIC),
                    levels, len, arabic, logical);
    }
  }

  visual = (FriBidiChar *)malloc(sizeof(*visual) * len);
  if (!visual) {
    free(logical);
    free(bidi_types);
    free(brackets);
    free(levels);
    free(arabic);
    return -1;
  }
  memcpy(visual, logical, sizeof(*visual) * len);
  {
    FriBidiLevel reorder = fribidi_reorder_line(
        FRIBIDI_FLAGS_DEFAULT, bidi_types, len, 0, base_dir, levels, visual,
        NULL);
    (void)reorder;
  }

  result = unicode_from_codepoints(visual, len);
  if (!result) {
    free(logical);
    free(visual);
    free(bidi_types);
    free(brackets);
    free(levels);
    free(arabic);
    return -1;
  }

  free(logical);
  free(visual);
  free(bidi_types);
  free(brackets);
  free(levels);
  free(arabic);

  *out_visual = result;
  return 0;
}

int unicode_wrap_with_lro(const char *text, char **out_wrapped) {
  size_t len;
  char *result;

  if (!out_wrapped) {
    return -1;
  }
  *out_wrapped = NULL;

  if (!text) {
    return 0;
  }

  len = strlen(text);
  result = (char *)malloc(len + 7);
  if (!result) {
    return -1;
  }

  result[0] = 0xE2;
  result[1] = 0x80;
  result[2] = 0xAD;
  memcpy(result + 3, text, len);
  result[3 + len] = 0xE2;
  result[4 + len] = 0x80;
  result[5 + len] = 0xAC;
  result[6 + len] = '\0';

  *out_wrapped = result;
  return 0;
}

int unicode_wrap_with_lrm(const char *text, char **out_wrapped) {
  size_t len;
  char *result;

  if (!out_wrapped) {
    return -1;
  }
  *out_wrapped = NULL;

  if (!text) {
    return 0;
  }

  len = strlen(text);
  result = (char *)malloc(len + 7);
  if (!result) {
    return -1;
  }

  result[0] = 0xE2;
  result[1] = 0x80;
  result[2] = 0x8E;
  memcpy(result + 3, text, len);
  result[3 + len] = 0xE2;
  result[4 + len] = 0x80;
  result[5 + len] = 0x8E;
  result[6 + len] = '\0';

  *out_wrapped = result;
  return 0;
}
