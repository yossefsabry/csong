#include "app/config.h"
#include "app/log.h"
#include "toml.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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

int config_resolve_path(const char *path, char *out, size_t out_size) {
  const char *home;

  if (!path || !out || out_size == 0) {
    return -1;
  }

  if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
    home = getenv("HOME");
    if (!home || home[0] == '\0') {
      return -1;
    }
    snprintf(out, out_size, "%s%s", home, path + 1);
    trim_spaces(out);
    return 0;
  }

  snprintf(out, out_size, "%s", path);
  trim_spaces(out);
  return 0;
}

int config_default_path(char *out, size_t out_size) {
  const char *xdg;
  const char *home;

  if (!out || out_size == 0) {
    return -1;
  }

  xdg = getenv("XDG_CONFIG_HOME");
  if (xdg && xdg[0] != '\0') {
    snprintf(out, out_size, "%s/csong/config.toml", xdg);
    trim_spaces(out);
    return 0;
  }

  home = getenv("HOME");
  if (!home || home[0] == '\0') {
    return -1;
  }

  snprintf(out, out_size, "%s/.config/csong/config.toml", home);
  trim_spaces(out);
  return 0;
}

void config_default(app_config *out) {
  if (!out) {
    return;
  }
  snprintf(out->mpd_host, sizeof(out->mpd_host), "%s", "127.0.0.1");
  out->mpd_port = 6600;
  out->interval = 1;
  out->show_plain = 0;
  out->cache_dir[0] = '\0';
  out->rtl_mode = UNICODE_RTL_AUTO;
  out->rtl_align = UNICODE_RTL_ALIGN_LEFT;
  out->rtl_shape = UNICODE_RTL_SHAPE_AUTO;
  out->bidi_mode = UNICODE_BIDI_FRIBIDI;
}

static void apply_toml_string(char *dest, size_t dest_size, toml_datum_t value) {
  if (!dest || dest_size == 0 || !value.ok || !value.u.s) {
    return;
  }
  snprintf(dest, dest_size, "%s", value.u.s);
  trim_spaces(dest);
  free(value.u.s);
}

static int parse_rtl_mode(const char *value, int fallback) {
  if (!value) {
    return fallback;
  }
  if (strcasecmp(value, "auto") == 0) {
    return UNICODE_RTL_AUTO;
  }
  if (strcasecmp(value, "on") == 0 || strcasecmp(value, "rtl") == 0 ||
      strcasecmp(value, "true") == 0) {
    return UNICODE_RTL_ON;
  }
  if (strcasecmp(value, "off") == 0 || strcasecmp(value, "ltr") == 0 ||
      strcasecmp(value, "false") == 0) {
    return UNICODE_RTL_OFF;
  }
  return fallback;
}

static int parse_rtl_align(const char *value, int fallback) {
  if (!value) {
    return fallback;
  }
  if (strcasecmp(value, "auto") == 0) {
    return UNICODE_RTL_ALIGN_AUTO;
  }
  if (strcasecmp(value, "left") == 0) {
    return UNICODE_RTL_ALIGN_LEFT;
  }
  if (strcasecmp(value, "right") == 0) {
    return UNICODE_RTL_ALIGN_RIGHT;
  }
  return fallback;
}

static int parse_rtl_shape(const char *value, int fallback) {
  if (!value) {
    return fallback;
  }
  if (strcasecmp(value, "auto") == 0) {
    return UNICODE_RTL_SHAPE_AUTO;
  }
  if (strcasecmp(value, "on") == 0 || strcasecmp(value, "true") == 0) {
    return UNICODE_RTL_SHAPE_ON;
  }
  if (strcasecmp(value, "off") == 0 || strcasecmp(value, "false") == 0) {
    return UNICODE_RTL_SHAPE_OFF;
  }
  return fallback;
}

static int parse_bidi_mode(const char *value, int fallback) {
  if (!value) {
    return fallback;
  }
  if (strcasecmp(value, "fribidi") == 0) {
    return UNICODE_BIDI_FRIBIDI;
  }
  if (strcasecmp(value, "terminal") == 0) {
    return UNICODE_BIDI_TERMINAL;
  }
  return fallback;
}

int config_load(const char *path, app_config *out) {
  FILE *file;
  toml_table_t *root;
  toml_table_t *table;
  toml_datum_t value;
  char resolved[512];
  char errbuf[200];

  if (!path || !out) {
    return -1;
  }

  if (config_resolve_path(path, resolved, sizeof(resolved)) != 0) {
    snprintf(resolved, sizeof(resolved), "%s", path);
  }

  file = fopen(resolved, "r");
  if (!file) {
    return 1;
  }

  root = toml_parse_file(file, errbuf, sizeof(errbuf));
  fclose(file);
  if (!root) {
    log_error(errbuf[0] != '\0' ? errbuf : "config: parse failed");
    return -1;
  }

  value = toml_int_in(root, "interval");
  if (value.ok && value.u.i > 0) {
    out->interval = (int)value.u.i;
  }

  value = toml_bool_in(root, "show_plain");
  if (value.ok) {
    out->show_plain = value.u.b != 0;
  }

  table = toml_table_in(root, "mpd");
  if (table) {
    value = toml_string_in(table, "host");
    apply_toml_string(out->mpd_host, sizeof(out->mpd_host), value);

    value = toml_int_in(table, "port");
    if (value.ok && value.u.i > 0) {
      out->mpd_port = (int)value.u.i;
    }
  }

  table = toml_table_in(root, "lyrics");
  if (table) {
    value = toml_string_in(table, "cache_dir");
    if (value.ok && value.u.s) {
      if (config_resolve_path(value.u.s, out->cache_dir,
                              sizeof(out->cache_dir)) != 0) {
        snprintf(out->cache_dir, sizeof(out->cache_dir), "%s", value.u.s);
        trim_spaces(out->cache_dir);
      }
      free(value.u.s);
    }
  }

  table = toml_table_in(root, "render");
  if (table) {
    value = toml_string_in(table, "rtl_mode");
    if (value.ok && value.u.s) {
      out->rtl_mode = parse_rtl_mode(value.u.s, out->rtl_mode);
      free(value.u.s);
    }

    value = toml_string_in(table, "rtl_align");
    if (value.ok && value.u.s) {
      out->rtl_align = parse_rtl_align(value.u.s, out->rtl_align);
      free(value.u.s);
    }

    value = toml_string_in(table, "rtl_shape");
    if (value.ok && value.u.s) {
      out->rtl_shape = parse_rtl_shape(value.u.s, out->rtl_shape);
      free(value.u.s);
    }

    value = toml_string_in(table, "bidi");
    if (value.ok && value.u.s) {
      out->bidi_mode = parse_bidi_mode(value.u.s, out->bidi_mode);
      free(value.u.s);
    }
  }

  toml_free(root);
  return 0;
}
