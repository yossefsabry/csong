#include "app/lyrics.h"
#include "app/log.h"
#include <curl/curl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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

static char *json_unescape(const char *input) {
  size_t len;
  size_t i;
  size_t out_len = 0;
  char *out;

  if (!input) {
    return NULL;
  }

  len = strlen(input);
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
            out[out_len++] = '?';
            i += 4;
          }
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

static char *extract_json_string(const char *json, const char *key) {
  const char *pos;
  const char *start;
  const char *end;
  char *raw;
  char *out;

  if (!json || !key) {
    return NULL;
  }

  pos = strstr(json, key);
  if (!pos) {
    return NULL;
  }

  pos = strchr(pos, ':');
  if (!pos) {
    return NULL;
  }

  pos++;
  while (*pos == ' ' || *pos == '\n' || *pos == '\r' || *pos == '\t') {
    pos++;
  }
  if (strncmp(pos, "null", 4) == 0) {
    return NULL;
  }
  if (*pos != '"') {
    return NULL;
  }
  start = pos + 1;
  end = start;
  while (*end) {
    if (*end == '"' && end[-1] != '\\') {
      break;
    }
    end++;
  }
  if (*end != '"') {
    return NULL;
  }

  raw = (char *)malloc((size_t)(end - start) + 1);
  if (!raw) {
    return NULL;
  }
  memcpy(raw, start, (size_t)(end - start));
  raw[end - start] = '\0';

  out = json_unescape(raw);
  free(raw);
  return out;
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

static const char *range_strstr(const char *start, const char *end,
                                const char *needle) {
  size_t nlen;
  const char *p;

  if (!start || !end || !needle) {
    return NULL;
  }

  nlen = strlen(needle);
  if (nlen == 0) {
    return start;
  }

  for (p = start; p + nlen <= end; p++) {
    if (*p == needle[0] && memcmp(p, needle, nlen) == 0) {
      return p;
    }
  }

  return NULL;
}

static char *extract_json_string_range(const char *start, const char *end,
                                       const char *key) {
  const char *pos;
  const char *value;
  const char *scan;
  char *raw;
  char *out;

  if (!start || !end || !key) {
    return NULL;
  }

  pos = range_strstr(start, end, key);
  if (!pos) {
    return NULL;
  }

  pos = memchr(pos, ':', (size_t)(end - pos));
  if (!pos) {
    return NULL;
  }
  pos++;
  while (pos < end && (*pos == ' ' || *pos == '\n' || *pos == '\r' ||
                       *pos == '\t')) {
    pos++;
  }
  if (pos + 4 <= end && strncmp(pos, "null", 4) == 0) {
    return NULL;
  }
  if (pos >= end || *pos != '"') {
    return NULL;
  }
  value = pos + 1;
  scan = value;
  while (scan < end) {
    if (*scan == '"' && scan[-1] != '\\') {
      break;
    }
    scan++;
  }
  if (scan >= end || *scan != '"') {
    return NULL;
  }

  raw = (char *)malloc((size_t)(scan - value) + 1);
  if (!raw) {
    return NULL;
  }
  memcpy(raw, value, (size_t)(scan - value));
  raw[scan - value] = '\0';

  out = json_unescape(raw);
  free(raw);
  return out;
}

static int extract_json_number_range(const char *start, const char *end,
                                     const char *key, double *out_value) {
  const char *pos;
  char *endptr;

  if (!start || !end || !key || !out_value) {
    return 0;
  }

  pos = range_strstr(start, end, key);
  if (!pos) {
    return 0;
  }

  pos = memchr(pos, ':', (size_t)(end - pos));
  if (!pos) {
    return 0;
  }
  pos++;
  while (pos < end && (*pos == ' ' || *pos == '\n' || *pos == '\r' ||
                       *pos == '\t')) {
    pos++;
  }
  if (pos >= end) {
    return 0;
  }

  *out_value = strtod(pos, &endptr);
  if (endptr == pos) {
    return 0;
  }
  return 1;
}

static int duration_close(double target, double actual) {
  if (target <= 0.0 || actual <= 0.0) {
    return 1;
  }
  return fabs(target - actual) <= 3.0;
}

static int parse_lrclib_object(const char *start, const char *end,
                               double target_duration, char **out_synced,
                               char **out_plain, double *out_diff) {
  double duration = 0.0;
  char *synced = NULL;
  char *plain = NULL;
  double diff = 1e9;

  if (extract_json_number_range(start, end, "\"duration\"", &duration)) {
    if (target_duration > 0.0 && duration > 0.0) {
      diff = fabs(target_duration - duration);
    } else {
      diff = 1e6;
    }
  }

  synced = extract_json_string_range(start, end, "\"syncedLyrics\"");
  plain = extract_json_string_range(start, end, "\"plainLyrics\"");

  if (out_synced) {
    *out_synced = synced;
  } else {
    free(synced);
  }
  if (out_plain) {
    *out_plain = plain;
  } else {
    free(plain);
  }
  if (out_diff) {
    *out_diff = diff;
  }
  return 1;
}

static int parse_lrclib_search_best(const char *json, double target_duration,
                                    char **out_text, int *out_timed) {
  const char *p = json;
  char *best_synced = NULL;
  char *best_plain = NULL;
  double best_synced_diff = 1e9;
  double best_plain_diff = 1e9;

  if (!json || !out_text || !out_timed) {
    return -1;
  }

  while (*p) {
    if (*p == '{') {
      int depth = 1;
      int in_string = 0;
      int escape = 0;
      const char *start = p;
      const char *end = NULL;
      p++;
      while (*p && depth > 0) {
        char c = *p;
        if (in_string) {
          if (escape) {
            escape = 0;
          } else if (c == '\\') {
            escape = 1;
          } else if (c == '"') {
            in_string = 0;
          }
        } else {
          if (c == '"') {
            in_string = 1;
          } else if (c == '{') {
            depth++;
          } else if (c == '}') {
            depth--;
            if (depth == 0) {
              end = p + 1;
              break;
            }
          }
        }
        p++;
      }

      if (end) {
        char *synced = NULL;
        char *plain = NULL;
        double diff = 1e9;
        parse_lrclib_object(start, end, target_duration, &synced, &plain, &diff);
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
    } else {
      p++;
    }
  }

  if (best_synced) {
    *out_text = best_synced;
    *out_timed = 1;
    free(best_plain);
    return 0;
  }
  if (best_plain) {
    *out_text = best_plain;
    *out_timed = 0;
    return 0;
  }

  return -1;
}

static int parse_lrclib_get_best(const char *json, double target_duration,
                                 char **out_text, int *out_timed) {
  double duration = 0.0;
  char *synced;
  char *plain;
  const char *end;

  if (!json || !out_text || !out_timed) {
    return -1;
  }

  end = json + strlen(json);
  if (extract_json_number_range(json, end, "\"duration\"", &duration)) {
    if (!duration_close(target_duration, duration)) {
      return -1;
    }
  }

  synced = extract_json_string(json, "\"syncedLyrics\"");
  if (synced && synced[0] != '\0') {
    *out_text = synced;
    *out_timed = 1;
    return 0;
  }
  free(synced);

  plain = extract_json_string(json, "\"plainLyrics\"");
  if (plain && plain[0] != '\0') {
    *out_text = plain;
    *out_timed = 0;
    return 0;
  }
  free(plain);
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

  lyrics = extract_json_string(json, "\"lyrics\"");
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
