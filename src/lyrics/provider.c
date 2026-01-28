#include "app/lyrics.h"
#include "app/log.h"
#include "app/unicode.h"
#include <curl/curl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#define JSMN_STATIC
#include "jsmn.h"

typedef struct http_buffer {
  char *data;
  size_t size;
} http_buffer;

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
  size_t total = size * nmemb;
  http_buffer *buf = (http_buffer *)userdata;
  char *next = (char *)realloc(buf->data, buf->size + total + 1);

  if (!next) {
    return 0;
  }

  buf->data = next;
  memcpy(buf->data + buf->size, ptr, total);
  buf->size += total;
  buf->data[buf->size] = '\0';
  return total;
}

static int hex_value(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  return -1;
}

static char *json_unescape_range(const char *input, size_t len) {
  size_t i;
  size_t out_len = 0;
  char *out;

  if (!input) {
    return NULL;
  }

  out = (char *)malloc(len + 1);
  if (!out) {
    return NULL;
  }

  for (i = 0; i < len; i++) {
    char c = input[i];
    if (c == '\\' && i + 1 < len) {
      char n = input[++i];
      switch (n) {
        case 'n':
          out[out_len++] = '\n';
          break;
        case 'r':
          out[out_len++] = '\r';
          break;
        case 't':
          out[out_len++] = '\t';
          break;
        case '\\':
          out[out_len++] = '\\';
          break;
        case '"':
          out[out_len++] = '"';
          break;
        case '/':
          out[out_len++] = '/';
          break;
        case 'u':
          if (i + 4 < len) {
            int h1 = hex_value(input[i + 1]);
            int h2 = hex_value(input[i + 2]);
            int h3 = hex_value(input[i + 3]);
            int h4 = hex_value(input[i + 4]);
            uint32_t codepoint;
            size_t wrote = 0;
            char buffer[4];
            if (h1 >= 0 && h2 >= 0 && h3 >= 0 && h4 >= 0) {
              codepoint = (uint32_t)((h1 << 12) | (h2 << 8) | (h3 << 4) | h4);
              i += 4;
              if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                if (i + 6 < len && input[i + 1] == '\\' && input[i + 2] == 'u') {
                  int l1 = hex_value(input[i + 3]);
                  int l2 = hex_value(input[i + 4]);
                  int l3 = hex_value(input[i + 5]);
                  int l4 = hex_value(input[i + 6]);
                  if (l1 >= 0 && l2 >= 0 && l3 >= 0 && l4 >= 0) {
                    uint32_t low = (uint32_t)((l1 << 12) | (l2 << 8) | (l3 << 4) | l4);
                    if (low >= 0xDC00 && low <= 0xDFFF) {
                      codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                      i += 6;
                    }
                  }
                }
              }
              if (unicode_encode_utf8(codepoint, buffer, &wrote) == 0) {
                size_t j;
                for (j = 0; j < wrote; j++) {
                  out[out_len++] = buffer[j];
                }
                break;
              }
            }
          }
          out[out_len++] = '?';
          break;
        default:
          out[out_len++] = n;
          break;
      }
    } else {
      out[out_len++] = c;
    }
  }

  out[out_len] = '\0';
  return out;
}

static int json_token_equal(const char *json, const jsmntok_t *tok,
                            const char *text) {
  size_t len;

  if (!json || !tok || !text || tok->type != JSMN_STRING) {
    return 0;
  }

  len = strlen(text);
  if ((size_t)(tok->end - tok->start) != len) {
    return 0;
  }

  return strncmp(json + tok->start, text, len) == 0;
}

static int json_token_is_null(const char *json, const jsmntok_t *tok) {
  size_t len;

  if (!json || !tok || tok->type != JSMN_PRIMITIVE) {
    return 0;
  }

  len = (size_t)(tok->end - tok->start);
  if (len != 4) {
    return 0;
  }

  return strncmp(json + tok->start, "null", 4) == 0;
}

static int json_token_next(const jsmntok_t *tokens, int index) {
  int i;
  int count;

  if (!tokens || index < 0) {
    return index + 1;
  }

  switch (tokens[index].type) {
    case JSMN_OBJECT:
      count = tokens[index].size;
      i = index + 1;
      for (; count > 0; count--) {
        i = json_token_next(tokens, i);
        i = json_token_next(tokens, i);
      }
      return i;
    case JSMN_ARRAY:
      count = tokens[index].size;
      i = index + 1;
      for (; count > 0; count--) {
        i = json_token_next(tokens, i);
      }
      return i;
    default:
      return index + 1;
  }
}

static int json_obj_get(const char *json, const jsmntok_t *tokens, int obj_index,
                        const char *key, int *out_index) {
  int i;
  int count;

  if (!json || !tokens || !key) {
    return 0;
  }
  if (tokens[obj_index].type != JSMN_OBJECT) {
    return 0;
  }

  i = obj_index + 1;
  count = tokens[obj_index].size;
  for (; count > 0; count--) {
    if (json_token_equal(json, &tokens[i], key)) {
      if (out_index) {
        *out_index = i + 1;
      }
      return 1;
    }
    i = json_token_next(tokens, i + 1);
  }
  return 0;
}

static int json_parse(const char *json, jsmntok_t **out_tokens,
                      int *out_count) {
  jsmn_parser parser;
  jsmntok_t *tokens;
  int count;

  if (!json || !out_tokens || !out_count) {
    return -1;
  }

  jsmn_init(&parser);
  count = jsmn_parse(&parser, json, strlen(json), NULL, 0);
  if (count <= 0) {
    return -1;
  }

  tokens = (jsmntok_t *)calloc((size_t)count, sizeof(*tokens));
  if (!tokens) {
    return -1;
  }

  jsmn_init(&parser);
  count = jsmn_parse(&parser, json, strlen(json), tokens, (unsigned int)count);
  if (count < 0) {
    free(tokens);
    return -1;
  }

  *out_tokens = tokens;
  *out_count = count;
  return 0;
}

static int json_number_from_token(const char *json, const jsmntok_t *tok,
                                  double *out_value) {
  char buf[64];
  int len;
  char *endptr;

  if (!json || !tok || !out_value || tok->type != JSMN_PRIMITIVE) {
    return 0;
  }

  len = tok->end - tok->start;
  if (len <= 0 || len >= (int)sizeof(buf)) {
    return 0;
  }

  memcpy(buf, json + tok->start, (size_t)len);
  buf[len] = '\0';

  *out_value = strtod(buf, &endptr);
  if (endptr == buf) {
    return 0;
  }
  return 1;
}

static char *json_string_from_token(const char *json, const jsmntok_t *tok) {
  if (!json || !tok || tok->type != JSMN_STRING) {
    return NULL;
  }
  return json_unescape_range(json + tok->start,
                             (size_t)(tok->end - tok->start));
}

static int http_get(const char *url, char **out_body) {
  CURL *curl;
  CURLcode res;
  long http_code = 0;
  http_buffer buf = {0};
  static int curl_ready;

  if (!url || !out_body) {
    return -1;
  }
  *out_body = NULL;

  if (!curl_ready) {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
      return -1;
    }
    curl_ready = 1;
  }

  curl = curl_easy_init();
  if (!curl) {
    return -1;
  }

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "csong/0.1.0");
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    log_error(curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    free(buf.data);
    return -1;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  curl_easy_cleanup(curl);

  if (http_code != 200) {
    free(buf.data);
    return -1;
  }

  *out_body = buf.data;
  return 0;
}

static int duration_close(double target, double actual) {
  if (target <= 0.0 || actual <= 0.0) {
    return 1;
  }
  return fabs(target - actual) <= 3.0;
}

static int parse_lrclib_search_best(const char *json, double target_duration,
                                    char **out_text, int *out_timed) {
  jsmntok_t *tokens = NULL;
  int count = 0;
  char *best_synced = NULL;
  char *best_plain = NULL;
  double best_synced_diff = 1e9;
  double best_plain_diff = 1e9;
  int i;

  if (!json || !out_text || !out_timed) {
    return -1;
  }

  if (json_parse(json, &tokens, &count) != 0) {
    return -1;
  }
  if (count < 1 || tokens[0].type != JSMN_ARRAY) {
    free(tokens);
    return -1;
  }

  i = 1;
  for (int item = 0; item < tokens[0].size; item++) {
    int obj_index = i;
    int value_index = -1;
    double diff = 1e6;
    double duration = 0.0;

    if (tokens[obj_index].type == JSMN_OBJECT) {
      if (json_obj_get(json, tokens, obj_index, "duration", &value_index)) {
        if (json_number_from_token(json, &tokens[value_index], &duration)) {
          if (target_duration > 0.0 && duration > 0.0) {
            diff = fabs(target_duration - duration);
          }
        }
      }

      if (json_obj_get(json, tokens, obj_index, "syncedLyrics", &value_index) &&
          !json_token_is_null(json, &tokens[value_index])) {
        char *synced = json_string_from_token(json, &tokens[value_index]);
        if (synced && synced[0] != '\0') {
          if (diff < best_synced_diff) {
            free(best_synced);
            best_synced = synced;
            best_synced_diff = diff;
          } else {
            free(synced);
          }
        } else {
          free(synced);
        }
      }

      if (json_obj_get(json, tokens, obj_index, "plainLyrics", &value_index) &&
          !json_token_is_null(json, &tokens[value_index])) {
        char *plain = json_string_from_token(json, &tokens[value_index]);
        if (plain && plain[0] != '\0') {
          if (diff < best_plain_diff) {
            free(best_plain);
            best_plain = plain;
            best_plain_diff = diff;
          } else {
            free(plain);
          }
        } else {
          free(plain);
        }
      }
    }

    i = json_token_next(tokens, i);
  }

  if (best_synced) {
    *out_text = best_synced;
    *out_timed = 1;
    free(best_plain);
    free(tokens);
    return 0;
  }
  if (best_plain) {
    *out_text = best_plain;
    *out_timed = 0;
    free(tokens);
    return 0;
  }

  free(tokens);
  return -1;
}

static int parse_lrclib_get_best(const char *json, double target_duration,
                                 char **out_text, int *out_timed) {
  jsmntok_t *tokens = NULL;
  int count = 0;
  int value_index = -1;
  double duration = 0.0;

  if (!json || !out_text || !out_timed) {
    return -1;
  }

  if (json_parse(json, &tokens, &count) != 0) {
    return -1;
  }
  if (count < 1 || tokens[0].type != JSMN_OBJECT) {
    free(tokens);
    return -1;
  }

  if (json_obj_get(json, tokens, 0, "duration", &value_index)) {
    if (json_number_from_token(json, &tokens[value_index], &duration)) {
      if (!duration_close(target_duration, duration)) {
        free(tokens);
        return -1;
      }
    }
  }

  if (json_obj_get(json, tokens, 0, "syncedLyrics", &value_index) &&
      !json_token_is_null(json, &tokens[value_index])) {
    char *synced = json_string_from_token(json, &tokens[value_index]);
    if (synced && synced[0] != '\0') {
      *out_text = synced;
      *out_timed = 1;
      free(tokens);
      return 0;
    }
    free(synced);
  }

  if (json_obj_get(json, tokens, 0, "plainLyrics", &value_index) &&
      !json_token_is_null(json, &tokens[value_index])) {
    char *plain = json_string_from_token(json, &tokens[value_index]);
    if (plain && plain[0] != '\0') {
      *out_text = plain;
      *out_timed = 0;
      free(tokens);
      return 0;
    }
    free(plain);
  }

  free(tokens);
  return -1;
}

static int build_lrclib_get_url(const char *artist, const char *title, char *out,
                                size_t out_size) {
  CURL *curl;
  char *artist_enc = NULL;
  char *title_enc = NULL;
  int result = -1;

  if (!artist || !title || !out || out_size == 0) {
    return -1;
  }

  curl = curl_easy_init();
  if (!curl) {
    return -1;
  }

  title_enc = curl_easy_escape(curl, title, 0);
  if (!title_enc) {
    curl_easy_cleanup(curl);
    return -1;
  }

  artist_enc = curl_easy_escape(curl, artist, 0);
  if (!artist_enc) {
    curl_free(title_enc);
    curl_easy_cleanup(curl);
    return -1;
  }

  snprintf(out, out_size,
           "https://lrclib.net/api/get?track_name=%s&artist_name=%s",
           title_enc, artist_enc);
  curl_free(artist_enc);

  curl_free(title_enc);
  curl_easy_cleanup(curl);
  result = 0;
  return result;
}

static int build_lrclib_search_url(const char *artist, const char *title,
                                   char *out, size_t out_size) {
  CURL *curl;
  char *artist_enc = NULL;
  char *title_enc = NULL;
  int result = -1;

  if (!title || !out || out_size == 0) {
    return -1;
  }

  curl = curl_easy_init();
  if (!curl) {
    return -1;
  }

  title_enc = curl_easy_escape(curl, title, 0);
  if (!title_enc) {
    curl_easy_cleanup(curl);
    return -1;
  }

  if (artist && artist[0] != '\0' && strcasecmp(artist, "Unknown Artist") != 0) {
    artist_enc = curl_easy_escape(curl, artist, 0);
  }

  if (artist_enc) {
    snprintf(out, out_size,
             "https://lrclib.net/api/search?track_name=%s&artist_name=%s",
             title_enc, artist_enc);
    curl_free(artist_enc);
  } else {
    snprintf(out, out_size, "https://lrclib.net/api/search?track_name=%s",
             title_enc);
  }

  curl_free(title_enc);
  curl_easy_cleanup(curl);
  result = 0;
  return result;
}

static int build_ovh_url(const char *artist, const char *title, char *out,
                         size_t out_size) {
  CURL *curl;
  char *artist_enc;
  char *title_enc;
  int result = -1;

  if (!artist || !title || !out || out_size == 0) {
    return -1;
  }

  curl = curl_easy_init();
  if (!curl) {
    return -1;
  }

  artist_enc = curl_easy_escape(curl, artist, 0);
  title_enc = curl_easy_escape(curl, title, 0);
  if (!artist_enc || !title_enc) {
    if (artist_enc) {
      curl_free(artist_enc);
    }
    if (title_enc) {
      curl_free(title_enc);
    }
    curl_easy_cleanup(curl);
    return -1;
  }

  snprintf(out, out_size, "https://api.lyrics.ovh/v1/%s/%s", artist_enc,
           title_enc);
  curl_free(artist_enc);
  curl_free(title_enc);
  curl_easy_cleanup(curl);
  result = 0;
  return result;
}

static int lyrics_fetch_lrclib(const char *artist, const char *title,
                               double duration, char **out_text,
                               int *out_timed) {
  char url[1024];
  char *json;

  if (!out_text || !out_timed) {
    return -1;
  }
  *out_text = NULL;
  *out_timed = 0;

  if (artist && artist[0] != '\0' && strcasecmp(artist, "Unknown Artist") != 0) {
    if (build_lrclib_get_url(artist, title, url, sizeof(url)) == 0 &&
        http_get(url, &json) == 0) {
      if (parse_lrclib_get_best(json, duration, out_text, out_timed) == 0) {
        free(json);
        return 0;
      }
      free(json);
    }
  }

  if (build_lrclib_search_url(artist, title, url, sizeof(url)) != 0) {
    return -1;
  }

  if (http_get(url, &json) != 0) {
    return -1;
  }

  if (parse_lrclib_search_best(json, duration, out_text, out_timed) == 0) {
    free(json);
    return 0;
  }

  free(json);
  return -1;
}

static int lyrics_fetch_ovh(const char *artist, const char *title,
                            char **out_text) {
  char url[1024];
  char *json;
  char *lyrics;
  jsmntok_t *tokens = NULL;
  int count = 0;
  int value_index = -1;

  if (!out_text) {
    return -1;
  }
  *out_text = NULL;

  if (build_ovh_url(artist, title, url, sizeof(url)) != 0) {
    return -1;
  }

  if (http_get(url, &json) != 0) {
    return -1;
  }

  lyrics = NULL;
  if (json_parse(json, &tokens, &count) == 0 && count > 0 &&
      tokens[0].type == JSMN_OBJECT &&
      json_obj_get(json, tokens, 0, "lyrics", &value_index) &&
      !json_token_is_null(json, &tokens[value_index])) {
    lyrics = json_string_from_token(json, &tokens[value_index]);
  }
  free(tokens);
  free(json);
  if (!lyrics || lyrics[0] == '\0') {
    free(lyrics);
    return -1;
  }

  *out_text = lyrics;
  return 0;
}

int lyrics_fetch(const char *artist, const char *title, double duration,
                 char **out_text, int *out_timed) {
  if (!artist || !title || !out_text || !out_timed) {
    return -1;
  }

  if (lyrics_fetch_lrclib(artist, title, duration, out_text, out_timed) == 0) {
    return 0;
  }

  if (lyrics_fetch_ovh(artist, title, out_text) == 0) {
    *out_timed = 0;
    return 0;
  }

  return -1;
}
