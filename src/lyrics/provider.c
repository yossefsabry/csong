#include "app/lyrics.h"
#include "app/log.h"
#include <curl/curl.h>
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
                               char **out_text, int *out_timed) {
  char url[1024];
  char *json;
  char *synced;
  char *plain;

  if (!out_text || !out_timed) {
    return -1;
  }
  *out_text = NULL;
  *out_timed = 0;

  if (artist && artist[0] != '\0' && strcasecmp(artist, "Unknown Artist") != 0) {
    if (build_lrclib_get_url(artist, title, url, sizeof(url)) == 0 &&
        http_get(url, &json) == 0) {
      synced = extract_json_string(json, "\"syncedLyrics\"");
      if (synced && synced[0] != '\0') {
        free(json);
        *out_text = synced;
        *out_timed = 1;
        return 0;
      }
      free(synced);

      plain = extract_json_string(json, "\"plainLyrics\"");
      free(json);
      if (plain && plain[0] != '\0') {
        *out_text = plain;
        *out_timed = 0;
        return 0;
      }
      free(plain);
    }
  }

  if (build_lrclib_search_url(artist, title, url, sizeof(url)) != 0) {
    return -1;
  }

  if (http_get(url, &json) != 0) {
    return -1;
  }

  synced = extract_json_string(json, "\"syncedLyrics\"");
  if (synced && synced[0] != '\0') {
    free(json);
    *out_text = synced;
    *out_timed = 1;
    return 0;
  }
  free(synced);

  plain = extract_json_string(json, "\"plainLyrics\"");
  free(json);
  if (plain && plain[0] != '\0') {
    *out_text = plain;
    *out_timed = 0;
    return 0;
  }
  free(plain);
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

int lyrics_fetch(const char *artist, const char *title, char **out_text,
                 int *out_timed) {
  if (!artist || !title || !out_text || !out_timed) {
    return -1;
  }

  if (lyrics_fetch_lrclib(artist, title, out_text, out_timed) == 0) {
    return 0;
  }

  if (lyrics_fetch_ovh(artist, title, out_text) == 0) {
    *out_timed = 0;
    return 0;
  }

  return -1;
}
