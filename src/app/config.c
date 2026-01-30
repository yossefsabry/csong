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
  out->lyrics_lead_seconds = 1.0;
  snprintf(out->ui_backend, sizeof(out->ui_backend), "%s", "terminal");
  snprintf(out->ui_font, sizeof(out->ui_font), "%s", "Sans 12");
  out->ui_title_font[0] = '\0';
  out->ui_title_weight[0] = '\0';
  out->ui_title_style[0] = '\0';
  out->ui_opacity = 0.85;
  out->ui_width = 0;
  out->ui_height = 0;
  out->ui_offset_x = 24;
  out->ui_offset_y = 24;
  out->ui_padding_x = 16;
  out->ui_padding_y = 16;
  out->ui_click_through = 1;
  out->ui_fg_r = 255;
  out->ui_fg_g = 255;
  out->ui_fg_b = 255;
  out->ui_dim_r = 150;
  out->ui_dim_g = 150;
  out->ui_dim_b = 150;
  out->ui_prev_r = 210;
  out->ui_prev_g = 210;
  out->ui_prev_b = 210;
  out->ui_bg_r = 0;
  out->ui_bg_g = 0;
  out->ui_bg_b = 0;
  out->ui_title_r = out->ui_fg_r;
  out->ui_title_g = out->ui_fg_g;
  out->ui_title_b = out->ui_fg_b;
  out->ui_line_spacing = 1.0;
  out->ui_title_scale = 1.0;
  snprintf(out->ui_anchor, sizeof(out->ui_anchor), "%s", "bottom-right");
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

static int parse_hex_color(const char *value, int *r, int *g, int *b) {
  int h1, h2, h3, h4, h5, h6;
  const char *p;
  if (!value || !r || !g || !b) {
    return -1;
  }
  p = value;
  if (p[0] == '#') {
    p++;
  }
  if (strlen(p) != 6) {
    return -1;
  }
  h1 = hex_value(p[0]);
  h2 = hex_value(p[1]);
  h3 = hex_value(p[2]);
  h4 = hex_value(p[3]);
  h5 = hex_value(p[4]);
  h6 = hex_value(p[5]);
  if (h1 < 0 || h2 < 0 || h3 < 0 || h4 < 0 || h5 < 0 || h6 < 0) {
    return -1;
  }
  *r = (h1 << 4) | h2;
  *g = (h3 << 4) | h4;
  *b = (h5 << 4) | h6;
  return 0;
}

static void apply_toml_color(int *r, int *g, int *b, toml_datum_t value) {
  if (!r || !g || !b || !value.ok || !value.u.s) {
    return;
  }
  trim_spaces(value.u.s);
  if (parse_hex_color(value.u.s, r, g, b) != 0) {
    free(value.u.s);
    return;
  }
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

  value = toml_string_in(root, "font");
  apply_toml_string(out->ui_font, sizeof(out->ui_font), value);

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

    value = toml_double_in(table, "lead_seconds");
    if (value.ok && value.u.d >= 0.0) {
      out->lyrics_lead_seconds = value.u.d;
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

    value = toml_string_in(table, "backend");
    apply_toml_string(out->ui_backend, sizeof(out->ui_backend), value);

    value = toml_string_in(table, "font");
    apply_toml_string(out->ui_font, sizeof(out->ui_font), value);

    value = toml_string_in(table, "title_font");
    apply_toml_string(out->ui_title_font, sizeof(out->ui_title_font), value);

    value = toml_string_in(table, "title_weight");
    apply_toml_string(out->ui_title_weight, sizeof(out->ui_title_weight), value);

    value = toml_string_in(table, "title_style");
    apply_toml_string(out->ui_title_style, sizeof(out->ui_title_style), value);

    value = toml_double_in(table, "opacity");
    if (value.ok && value.u.d >= 0.0 && value.u.d <= 1.0) {
      out->ui_opacity = value.u.d;
    }

    value = toml_string_in(table, "anchor");
    apply_toml_string(out->ui_anchor, sizeof(out->ui_anchor), value);

    value = toml_int_in(table, "offset_x");
    if (value.ok) {
      out->ui_offset_x = (int)value.u.i;
    }

    value = toml_int_in(table, "offset_y");
    if (value.ok) {
      out->ui_offset_y = (int)value.u.i;
    }

    value = toml_int_in(table, "width");
    if (value.ok) {
      out->ui_width = (int)value.u.i;
    }

    value = toml_int_in(table, "height");
    if (value.ok) {
      out->ui_height = (int)value.u.i;
    }

    value = toml_bool_in(table, "click_through");
    if (value.ok) {
      out->ui_click_through = value.u.b != 0;
    }

    value = toml_double_in(table, "line_spacing");
    if (value.ok && value.u.d > 0.0) {
      out->ui_line_spacing = value.u.d;
    }

    value = toml_double_in(table, "title_scale");
    if (value.ok && value.u.d > 0.0) {
      out->ui_title_scale = value.u.d;
    }

    value = toml_string_in(table, "color");
    apply_toml_color(&out->ui_fg_r, &out->ui_fg_g, &out->ui_fg_b, value);

    value = toml_string_in(table, "color");
    if (value.ok && value.u.s) {
      apply_toml_color(&out->ui_fg_r, &out->ui_fg_g, &out->ui_fg_b, value);
      out->ui_title_r = out->ui_fg_r;
      out->ui_title_g = out->ui_fg_g;
      out->ui_title_b = out->ui_fg_b;
    }

    value = toml_string_in(table, "fg_color");
    if (value.ok && value.u.s) {
      apply_toml_color(&out->ui_fg_r, &out->ui_fg_g, &out->ui_fg_b, value);
      out->ui_title_r = out->ui_fg_r;
      out->ui_title_g = out->ui_fg_g;
      out->ui_title_b = out->ui_fg_b;
    }

    value = toml_string_in(table, "dim_color");
    apply_toml_color(&out->ui_dim_r, &out->ui_dim_g, &out->ui_dim_b, value);

    value = toml_string_in(table, "prev_color");
    apply_toml_color(&out->ui_prev_r, &out->ui_prev_g, &out->ui_prev_b, value);

    value = toml_string_in(table, "bg_color");
    apply_toml_color(&out->ui_bg_r, &out->ui_bg_g, &out->ui_bg_b, value);

    value = toml_string_in(table, "title_color");
    apply_toml_color(&out->ui_title_r, &out->ui_title_g, &out->ui_title_b, value);

    value = toml_int_in(table, "padding_x");
    if (value.ok) {
      out->ui_padding_x = (int)value.u.i;
    }

    value = toml_int_in(table, "padding_y");
    if (value.ok) {
      out->ui_padding_y = (int)value.u.i;
    }
  }

  table = toml_table_in(root, "ui");
  if (table) {
    value = toml_string_in(table, "backend");
    apply_toml_string(out->ui_backend, sizeof(out->ui_backend), value);

    value = toml_string_in(table, "font");
    apply_toml_string(out->ui_font, sizeof(out->ui_font), value);

    value = toml_string_in(table, "title_font");
    apply_toml_string(out->ui_title_font, sizeof(out->ui_title_font), value);

    value = toml_string_in(table, "title_weight");
    apply_toml_string(out->ui_title_weight, sizeof(out->ui_title_weight), value);

    value = toml_string_in(table, "title_style");
    apply_toml_string(out->ui_title_style, sizeof(out->ui_title_style), value);

    value = toml_double_in(table, "opacity");
    if (value.ok && value.u.d >= 0.0 && value.u.d <= 1.0) {
      out->ui_opacity = value.u.d;
    }

    value = toml_string_in(table, "anchor");
    apply_toml_string(out->ui_anchor, sizeof(out->ui_anchor), value);

    value = toml_int_in(table, "offset_x");
    if (value.ok) {
      out->ui_offset_x = (int)value.u.i;
    }

    value = toml_int_in(table, "offset_y");
    if (value.ok) {
      out->ui_offset_y = (int)value.u.i;
    }

    value = toml_int_in(table, "width");
    if (value.ok) {
      out->ui_width = (int)value.u.i;
    }

    value = toml_int_in(table, "height");
    if (value.ok) {
      out->ui_height = (int)value.u.i;
    }

    value = toml_bool_in(table, "click_through");
    if (value.ok) {
      out->ui_click_through = value.u.b != 0;
    }

    value = toml_double_in(table, "line_spacing");
    if (value.ok && value.u.d > 0.0) {
      out->ui_line_spacing = value.u.d;
    }

    value = toml_double_in(table, "title_scale");
    if (value.ok && value.u.d > 0.0) {
      out->ui_title_scale = value.u.d;
    }

    value = toml_string_in(table, "fg_color");
    if (value.ok && value.u.s) {
      apply_toml_color(&out->ui_fg_r, &out->ui_fg_g, &out->ui_fg_b, value);
      out->ui_title_r = out->ui_fg_r;
      out->ui_title_g = out->ui_fg_g;
      out->ui_title_b = out->ui_fg_b;
    }

    value = toml_string_in(table, "dim_color");
    apply_toml_color(&out->ui_dim_r, &out->ui_dim_g, &out->ui_dim_b, value);

    value = toml_string_in(table, "prev_color");
    apply_toml_color(&out->ui_prev_r, &out->ui_prev_g, &out->ui_prev_b, value);

    value = toml_string_in(table, "bg_color");
    apply_toml_color(&out->ui_bg_r, &out->ui_bg_g, &out->ui_bg_b, value);

    value = toml_string_in(table, "title_color");
    apply_toml_color(&out->ui_title_r, &out->ui_title_g, &out->ui_title_b, value);

    value = toml_int_in(table, "padding_x");
    if (value.ok) {
      out->ui_padding_x = (int)value.u.i;
    }

    value = toml_int_in(table, "padding_y");
    if (value.ok) {
      out->ui_padding_y = (int)value.u.i;
    }
  }

  toml_free(root);
  return 0;
}
