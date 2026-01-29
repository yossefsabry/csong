#include "cache.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define PATH_SEP '\\'
#else
#include <sys/stat.h>
#include <sys/types.h>
#define PATH_SEP '/'
#endif

static void set_err(char *err, size_t err_cap, const char *msg) {
  if (!err || err_cap == 0) {
    return;
  }
  snprintf(err, err_cap, "%s", msg);
}

static int is_path_sep(char c) {
  return c == '/' || c == '\\';
}

static char *dup_string(const char *s) {
  size_t len = strlen(s);
  char *out = (char *)malloc(len + 1);
  if (!out) {
    return NULL;
  }
  memcpy(out, s, len + 1);
  return out;
}

static char *path_join(const char *a, const char *b) {
  size_t a_len = strlen(a);
  size_t b_len = strlen(b);
  int need_sep = a_len > 0 && !is_path_sep(a[a_len - 1]);
  size_t total = a_len + (need_sep ? 1 : 0) + b_len + 1;
  char *out = (char *)malloc(total);
  if (!out) {
    return NULL;
  }
  memcpy(out, a, a_len);
  size_t pos = a_len;
  if (need_sep) {
    out[pos++] = PATH_SEP;
  }
  memcpy(out + pos, b, b_len);
  out[pos + b_len] = '\0';
  return out;
}

static int make_dir(const char *path, char *err, size_t err_cap) {
#ifdef _WIN32
  if (_mkdir(path) == 0) {
    return 0;
  }
#else
  if (mkdir(path, 0700) == 0) {
    return 0;
  }
#endif
  if (errno == EEXIST) {
    return 0;
  }
  set_err(err, err_cap, "Failed to create cache directory");
  return -1;
}

static int ensure_dir(const char *path, char *err, size_t err_cap) {
  if (!path || !*path) {
    set_err(err, err_cap, "Cache directory is empty");
    return -1;
  }
  char *buf = dup_string(path);
  if (!buf) {
    set_err(err, err_cap, "Out of memory");
    return -1;
  }
  size_t len = strlen(buf);
  size_t start = 1;
#ifdef _WIN32
  if (len >= 2 && buf[1] == ':') {
    start = 3;
  }
#endif
  for (size_t i = start; i < len; ++i) {
    if (is_path_sep(buf[i])) {
      buf[i] = '\0';
      if (strlen(buf) > 0) {
        if (make_dir(buf, err, err_cap) != 0) {
          free(buf);
          return -1;
        }
      }
      buf[i] = PATH_SEP;
    }
  }
  if (make_dir(buf, err, err_cap) != 0) {
    free(buf);
    return -1;
  }
  free(buf);
  return 0;
}

char *cache_default_dir(void) {
#ifdef _WIN32
  const char *base = getenv("LOCALAPPDATA");
  if (!base) {
    base = getenv("USERPROFILE");
  }
  if (!base) {
    base = ".";
  }
  return path_join(base, "spotify_lyrics");
#else
  const char *base = getenv("XDG_CACHE_HOME");
  if (base) {
    return path_join(base, "spotify_lyrics");
  }
  const char *home = getenv("HOME");
  if (!home) {
    return dup_string(".");
  }
  char *cache_root = path_join(home, ".cache");
  if (!cache_root) {
    return NULL;
  }
  char *out = path_join(cache_root, "spotify_lyrics");
  free(cache_root);
  return out;
#endif
}

char *cache_build_key(const char *artist, const char *title) {
  if (!artist || !title) {
    return NULL;
  }
  size_t len = strlen(artist) + strlen(title) + 2;
  char *raw = (char *)malloc(len);
  if (!raw) {
    return NULL;
  }
  snprintf(raw, len, "%s-%s", artist, title);
  for (size_t i = 0; raw[i]; ++i) {
    unsigned char c = (unsigned char)raw[i];
    if (isalnum(c)) {
      raw[i] = (char)tolower(c);
    } else if (c == ' ') {
      raw[i] = '_';
    } else {
      raw[i] = '_';
    }
  }
  return raw;
}

static char *cache_path(const char *cache_dir, const char *key) {
  size_t key_len = strlen(key);
  char *file = (char *)malloc(key_len + 5);
  if (!file) {
    return NULL;
  }
  snprintf(file, key_len + 5, "%s.txt", key);
  char *out = path_join(cache_dir, file);
  free(file);
  return out;
}

char *cache_load(const char *cache_dir, const char *key, char *err, size_t err_cap) {
  if (!cache_dir || !key) {
    return NULL;
  }
  char *path = cache_path(cache_dir, key);
  if (!path) {
    set_err(err, err_cap, "Out of memory");
    return NULL;
  }
  FILE *fp = fopen(path, "rb");
  free(path);
  if (!fp) {
    return NULL;
  }
  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return NULL;
  }
  long size = ftell(fp);
  if (size <= 0) {
    fclose(fp);
    return NULL;
  }
  if (fseek(fp, 0, SEEK_SET) != 0) {
    fclose(fp);
    return NULL;
  }
  char *buf = (char *)malloc((size_t)size + 1);
  if (!buf) {
    fclose(fp);
    set_err(err, err_cap, "Out of memory");
    return NULL;
  }
  size_t read = fread(buf, 1, (size_t)size, fp);
  fclose(fp);
  if (read != (size_t)size) {
    free(buf);
    return NULL;
  }
  buf[size] = '\0';
  return buf;
}

int cache_store(const char *cache_dir, const char *key, const char *lyrics, char *err,
                size_t err_cap) {
  if (!cache_dir || !key || !lyrics) {
    return -1;
  }
  if (ensure_dir(cache_dir, err, err_cap) != 0) {
    return -1;
  }
  char *path = cache_path(cache_dir, key);
  if (!path) {
    set_err(err, err_cap, "Out of memory");
    return -1;
  }
  FILE *fp = fopen(path, "wb");
  free(path);
  if (!fp) {
    set_err(err, err_cap, "Failed to write cache file");
    return -1;
  }
  size_t len = strlen(lyrics);
  size_t written = fwrite(lyrics, 1, len, fp);
  fclose(fp);
  if (written != len) {
    set_err(err, err_cap, "Failed to write cache file");
    return -1;
  }
  return 0;
}
