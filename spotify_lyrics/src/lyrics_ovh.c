#include "lyrics_ovh.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json_utils.h"

typedef struct {
  char *data;
  size_t len;
  size_t cap;
} Buffer;

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
  size_t new_cap = *cap ? *cap * 2 : 256;
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

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
  size_t total = size * nmemb;
  Buffer *buf = (Buffer *)userdata;
  if (!ensure_capacity(&buf->data, &buf->cap, buf->len + total + 1)) {
    return 0;
  }
  memcpy(buf->data + buf->len, ptr, total);
  buf->len += total;
  buf->data[buf->len] = '\0';
  return total;
}

static int is_unreserved(unsigned char c) {
  if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
    return 1;
  }
  return c == '-' || c == '_' || c == '.' || c == '~';
}

static char *url_encode(const char *s) {
  size_t len = strlen(s);
  char *out = (char *)malloc(len * 3 + 1);
  if (!out) {
    return NULL;
  }
  char *p = out;
  for (size_t i = 0; i < len; ++i) {
    unsigned char c = (unsigned char)s[i];
    if (is_unreserved(c)) {
      *p++ = (char)c;
    } else {
      snprintf(p, 4, "%%%02X", c);
      p += 3;
    }
  }
  *p = '\0';
  return out;
}

char *lyrics_ovh_fetch(const char *artist, const char *title, char *err, size_t err_cap) {
  if (!artist || !title) {
    set_err(err, err_cap, "Missing artist or title");
    return NULL;
  }
  char *artist_enc = url_encode(artist);
  char *title_enc = url_encode(title);
  if (!artist_enc || !title_enc) {
    free(artist_enc);
    free(title_enc);
    set_err(err, err_cap, "Out of memory");
    return NULL;
  }
  size_t url_len = strlen(artist_enc) + strlen(title_enc) + 32;
  char *url = (char *)malloc(url_len);
  if (!url) {
    free(artist_enc);
    free(title_enc);
    set_err(err, err_cap, "Out of memory");
    return NULL;
  }
  snprintf(url, url_len, "https://api.lyrics.ovh/v1/%s/%s", artist_enc, title_enc);
  free(artist_enc);
  free(title_enc);

  CURL *curl = curl_easy_init();
  if (!curl) {
    free(url);
    set_err(err, err_cap, "Failed to init curl");
    return NULL;
  }

  Buffer buf = {0};
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "spotify_lyrics/0.1");
  CURLcode res = curl_easy_perform(curl);
  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_easy_cleanup(curl);
  free(url);

  if (res != CURLE_OK) {
    free(buf.data);
    set_err(err, err_cap, "Network request failed");
    return NULL;
  }
  if (!buf.data) {
    set_err(err, err_cap, "Empty response from lyrics service");
    return NULL;
  }
  if (status != 200) {
    char *msg = json_get_string(buf.data, "error", err, err_cap);
    if (!msg) {
      set_err(err, err_cap, "Lyrics not found");
      free(buf.data);
      return NULL;
    }
    set_err(err, err_cap, msg);
    free(msg);
    free(buf.data);
    return NULL;
  }
  char *lyrics = json_get_string(buf.data, "lyrics", err, err_cap);
  free(buf.data);
  if (!lyrics) {
    set_err(err, err_cap, "Lyrics not found");
    return NULL;
  }
  return lyrics;
}
