#include "app/lyrics.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static char g_cache_dir[512];

static int ensure_dir(const char *path) {
  if (mkdir(path, 0700) == 0) {
    return 0;
  }
  if (errno == EEXIST) {
    return 0;
  }
  return -1;
}

static int ensure_dir_recursive(const char *path) {
  char temp[512];
  size_t len;
  char *p;

  if (!path || path[0] == '\0') {
    return -1;
  }

  snprintf(temp, sizeof(temp), "%s", path);
  len = strlen(temp);
  if (len == 0) {
    return -1;
  }
  if (temp[len - 1] == '/') {
    temp[len - 1] = '\0';
  }

  for (p = temp + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      if (ensure_dir(temp) != 0) {
        return -1;
      }
      *p = '/';
    }
  }

  return ensure_dir(temp);
}

static void trim_spaces(char *text) {
  char *end;
  if (!text || text[0] == '\0') {
    return;
  }
  while (isspace((unsigned char)*text)) {
    memmove(text, text + 1, strlen(text));
  }
  end = text + strlen(text);
  while (end > text && isspace((unsigned char)end[-1])) {
    end[-1] = '\0';
    end--;
  }
}

static void sanitize_component(const char *in, char *out, size_t out_size) {
  size_t i;
  size_t len = 0;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';

  if (!in) {
    return;
  }

  for (i = 0; in[i] != '\0' && len + 1 < out_size; i++) {
    unsigned char c = (unsigned char)in[i];
    if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
        c == '"' || c == '<' || c == '>' || c == '|') {
      out[len++] = '_';
    } else {
      out[len++] = (char)c;
    }
  }
  out[len] = '\0';
  trim_spaces(out);
}

static int build_path_artist_title(const char *artist, const char *title,
                                   const char *ext, char *out,
                                   size_t out_size) {
  const char *base = NULL;
  char safe_artist[256];
  char safe_title[256];
  const char *artist_text = artist ? artist : "Unknown Artist";
  const char *title_text = title ? title : "Unknown Title";

  if (!out || out_size == 0) {
    return -1;
  }

  if (g_cache_dir[0] != '\0') {
    base = g_cache_dir;
  } else {
    base = getenv("HOME");
  }

  if (!base || base[0] == '\0') {
    return -1;
  }

  sanitize_component(artist_text, safe_artist, sizeof(safe_artist));
  sanitize_component(title_text, safe_title, sizeof(safe_title));

  if (safe_artist[0] == '\0') {
    snprintf(safe_artist, sizeof(safe_artist), "%s", "Unknown Artist");
  }
  if (safe_title[0] == '\0') {
    snprintf(safe_title, sizeof(safe_title), "%s", "Unknown Title");
  }

  if (!ext || ext[0] == '\0') {
    ext = ".txt";
  }

  if (g_cache_dir[0] != '\0') {
    snprintf(out, out_size, "%s/%s - %s%s", base, safe_artist, safe_title, ext);
  } else {
    snprintf(out, out_size, "%s/lyrics/%s - %s%s", base, safe_artist, safe_title,
             ext);
  }
  return 0;
}

static int build_path_title_only(const char *title, const char *ext, char *out,
                                 size_t out_size) {
  const char *base = NULL;
  char safe_title[256];
  const char *title_text = title ? title : "Unknown Title";

  if (!out || out_size == 0) {
    return -1;
  }

  if (g_cache_dir[0] != '\0') {
    base = g_cache_dir;
  } else {
    base = getenv("HOME");
  }

  if (!base || base[0] == '\0') {
    return -1;
  }

  sanitize_component(title_text, safe_title, sizeof(safe_title));
  if (safe_title[0] == '\0') {
    snprintf(safe_title, sizeof(safe_title), "%s", "Unknown Title");
  }

  if (!ext || ext[0] == '\0') {
    ext = ".txt";
  }

  if (g_cache_dir[0] != '\0') {
    snprintf(out, out_size, "%s/%s%s", base, safe_title, ext);
  } else {
    snprintf(out, out_size, "%s/lyrics/%s%s", base, safe_title, ext);
  }
  return 0;
}

static int ensure_cache_dirs(void) {
  char path[512];

  if (g_cache_dir[0] != '\0') {
    snprintf(path, sizeof(path), "%s", g_cache_dir);
  } else {
    const char *home = getenv("HOME");
    if (!home || home[0] == '\0') {
      return -1;
    }
    snprintf(path, sizeof(path), "%s/lyrics", home);
  }

  return ensure_dir_recursive(path);
}

static int is_unknown_artist(const char *artist) {
  if (!artist || artist[0] == '\0') {
    return 1;
  }
  if (strcasecmp(artist, "Unknown Artist") == 0) {
    return 1;
  }
  return 0;
}

static char *read_file(const char *path) {
  FILE *file;
  long size;
  char *buffer;

  if (!path) {
    return NULL;
  }

  file = fopen(path, "rb");
  if (!file) {
    return NULL;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return NULL;
  }
  size = ftell(file);
  if (size < 0) {
    fclose(file);
    return NULL;
  }
  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return NULL;
  }

  buffer = (char *)malloc((size_t)size + 1);
  if (!buffer) {
    fclose(file);
    return NULL;
  }

  if (fread(buffer, 1, (size_t)size, file) != (size_t)size) {
    fclose(file);
    free(buffer);
    return NULL;
  }
  buffer[size] = '\0';
  fclose(file);
  return buffer;
}

char *lyrics_cache_load(const char *artist, const char *title) {
  char path[512];
  char *buffer;

  if (!is_unknown_artist(artist)) {
    if (build_path_artist_title(artist, title, ".lrc", path, sizeof(path)) ==
        0) {
      buffer = read_file(path);
      if (buffer) {
        return buffer;
      }
    }
    if (build_path_artist_title(artist, title, ".txt", path, sizeof(path)) ==
        0) {
      buffer = read_file(path);
      if (buffer) {
        return buffer;
      }
    }
  }

  if (build_path_title_only(title, ".lrc", path, sizeof(path)) == 0) {
    buffer = read_file(path);
    if (buffer) {
      return buffer;
    }
  }

  if (build_path_title_only(title, ".txt", path, sizeof(path)) == 0) {
    return read_file(path);
  }

  return NULL;
}

int lyrics_cache_store(const char *artist, const char *title, const char *text,
                       int timed) {
  FILE *file;
  char path[512];
  const char *ext = timed ? ".lrc" : ".txt";

  if (!text || ensure_cache_dirs() != 0) {
    return -1;
  }

  if (is_unknown_artist(artist)) {
    if (build_path_title_only(title, ext, path, sizeof(path)) != 0) {
      return -1;
    }
  } else {
    if (build_path_artist_title(artist, title, ext, path, sizeof(path)) != 0) {
      return -1;
    }
  }

  file = fopen(path, "wb");
  if (!file) {
    return -1;
  }

  if (fwrite(text, 1, strlen(text), file) != strlen(text)) {
    fclose(file);
    return -1;
  }

  fclose(file);
  return 0;
}

void lyrics_cache_set_dir(const char *path) {
  if (!path || path[0] == '\0') {
    g_cache_dir[0] = '\0';
    return;
  }
  snprintf(g_cache_dir, sizeof(g_cache_dir), "%s", path);
  trim_spaces(g_cache_dir);
}
