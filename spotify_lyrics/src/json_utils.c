#include "json_utils.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_err(char *err, size_t err_cap, const char *msg) {
  if (!err || err_cap == 0) {
    return;
  }
  snprintf(err, err_cap, "%s", msg);
}

static int ensure_capacity(char **buf, size_t *cap, size_t need) {
  if (need <= *cap) {
    return 1;
  }
  size_t new_cap = *cap ? *cap * 2 : 128;
  while (new_cap < need) {
    new_cap *= 2;
  }
  char *tmp = (char *)realloc(*buf, new_cap);
  if (!tmp) {
    return 0;
  }
  *buf = tmp;
  *cap = new_cap;
  return 1;
}

static int hex_val(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

static int append_utf8(char **out, size_t *cap, size_t *len, unsigned int code) {
  unsigned char tmp[4];
  size_t add = 0;
  if (code <= 0x7F) {
    tmp[0] = (unsigned char)code;
    add = 1;
  } else if (code <= 0x7FF) {
    tmp[0] = (unsigned char)(0xC0 | ((code >> 6) & 0x1F));
    tmp[1] = (unsigned char)(0x80 | (code & 0x3F));
    add = 2;
  } else {
    tmp[0] = (unsigned char)(0xE0 | ((code >> 12) & 0x0F));
    tmp[1] = (unsigned char)(0x80 | ((code >> 6) & 0x3F));
    tmp[2] = (unsigned char)(0x80 | (code & 0x3F));
    add = 3;
  }
  if (!ensure_capacity(out, cap, *len + add + 1)) {
    return 0;
  }
  for (size_t i = 0; i < add; ++i) {
    (*out)[(*len)++] = (char)tmp[i];
  }
  (*out)[*len] = '\0';
  return 1;
}

static char *parse_json_string(const char *start, char *err, size_t err_cap) {
  if (*start != '"') {
    set_err(err, err_cap, "Invalid JSON string");
    return NULL;
  }
  char *out = NULL;
  size_t cap = 0;
  size_t len = 0;
  for (const char *p = start + 1; *p; ++p) {
    char c = *p;
    if (c == '"') {
      if (!ensure_capacity(&out, &cap, len + 1)) {
        free(out);
        set_err(err, err_cap, "Out of memory");
        return NULL;
      }
      out[len] = '\0';
      return out;
    }
    if (c == '\\') {
      ++p;
      if (!*p) {
        break;
      }
      switch (*p) {
        case '"':
        case '\\':
        case '/':
          c = *p;
          break;
        case 'b':
          c = '\b';
          break;
        case 'f':
          c = '\f';
          break;
        case 'n':
          c = '\n';
          break;
        case 'r':
          c = '\r';
          break;
        case 't':
          c = '\t';
          break;
        case 'u': {
          if (!p[1] || !p[2] || !p[3] || !p[4]) {
            c = '?';
            break;
          }
          int h1 = hex_val(*(p + 1));
          int h2 = hex_val(*(p + 2));
          int h3 = hex_val(*(p + 3));
          int h4 = hex_val(*(p + 4));
          if (h1 < 0 || h2 < 0 || h3 < 0 || h4 < 0) {
            c = '?';
            p += 4;
            break;
          }
          unsigned int code = (unsigned int)((h1 << 12) | (h2 << 8) | (h3 << 4) | h4);
          if (!append_utf8(&out, &cap, &len, code)) {
            free(out);
            set_err(err, err_cap, "Out of memory");
            return NULL;
          }
          p += 4;
          continue;
        }
        default:
          c = *p;
          break;
      }
    }
    if (!ensure_capacity(&out, &cap, len + 2)) {
      free(out);
      set_err(err, err_cap, "Out of memory");
      return NULL;
    }
    out[len++] = c;
  }
  free(out);
  set_err(err, err_cap, "Unterminated JSON string");
  return NULL;
}

char *json_get_string(const char *json, const char *key, char *err, size_t err_cap) {
  char pattern[64];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char *pos = strstr(json, pattern);
  if (!pos) {
    return NULL;
  }
  const char *colon = strchr(pos + strlen(pattern), ':');
  if (!colon) {
    return NULL;
  }
  const char *p = colon + 1;
  while (*p && isspace((unsigned char)*p)) {
    ++p;
  }
  if (*p != '"') {
    return NULL;
  }
  return parse_json_string(p, err, err_cap);
}
