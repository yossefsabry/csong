#include "x11_backend.h"
#include "app/log.h"
#include "app/unicode.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xfixes.h>
#include <X11/Xft/Xft.h>
#include <fontconfig/fontconfig.h>
#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct x11_lines {
  char **lines;
  size_t count;
} x11_lines;

typedef struct x11_state {
  Display *dpy;
  int screen;
  Window root;
  Window win;
  Visual *visual;
  Colormap colormap;
  XftDraw *draw;
  XftFont *font;
  XftFont *title_font;
  XftColor color_main;
  XftColor color_title;
  XftColor color_dim;
  XftColor color_prev;
  XftColor color_bg;
  int width;
  int height;
  int padding_x;
  int padding_y;
  int line_height;
  int title_line_height;
  int content_width;
  int rtl_mode;
  int rtl_align;
  int rtl_shape;
  int bidi_mode;
  Pixmap pixmap;
  GC gc;
  ui_options options;
  double line_spacing;
  double title_scale;
  int xfixes_available;
  int ready;
  int colors_ready;
} x11_state;

static x11_state g_x11;

static int x11_ensure_buffer(x11_state *s);

static int lines_push(x11_lines *out, const char *start, size_t len) {
  char *line;
  char **next;

  if (!out) {
    return -1;
  }

  line = (char *)malloc(len + 1);
  if (!line) {
    return -1;
  }
  if (len > 0 && start) {
    memcpy(line, start, len);
  }
  line[len] = '\0';

  next = (char **)realloc(out->lines, sizeof(char *) * (out->count + 1));
  if (!next) {
    free(line);
    return -1;
  }
  out->lines = next;
  out->lines[out->count] = line;
  out->count++;
  return 0;
}

static void lines_free(x11_lines *lines) {
  size_t i;
  if (!lines) {
    return;
  }
  for (i = 0; i < lines->count; i++) {
    free(lines->lines[i]);
  }
  free(lines->lines);
  lines->lines = NULL;
  lines->count = 0;
}

static int x11_is_size_token(const char *token) {
  int has_digit = 0;
  const char *p = token;
  if (!token || token[0] == '\0') {
    return 0;
  }
  while (*p) {
    if (*p == '.') {
      p++;
      continue;
    }
    if (!isdigit((unsigned char)*p)) {
      return 0;
    }
    has_digit = 1;
    p++;
  }
  return has_digit;
}

static const char *x11_compose_title_font_name(const ui_options *options,
                                               char *buffer, size_t buffer_size) {
  const char *base;
  const char *weight;
  const char *style;
  const char *size_token = NULL;
  size_t base_len;
  size_t name_len;
  const char *last_space;
  size_t n = 0;

  if (!options) {
    return "Sans 12";
  }

  if (options->title_font[0] != '\0') {
    return options->title_font;
  }

  base = options->font[0] != '\0' ? options->font : "Sans 12";
  weight = options->title_weight;
  style = options->title_style;

  if ((weight[0] == '\0') && (style[0] == '\0')) {
    return base;
  }

  if (!buffer || buffer_size == 0) {
    return base;
  }

  base_len = strlen(base);
  name_len = base_len;
  last_space = strrchr(base, ' ');
  if (last_space && x11_is_size_token(last_space + 1)) {
    size_token = last_space + 1;
    name_len = (size_t)(last_space - base);
  }

  if (name_len > 0) {
    n = (size_t)snprintf(buffer, buffer_size, "%.*s", (int)name_len, base);
    if (n >= buffer_size) {
      return base;
    }
  }

  if (weight[0] != '\0') {
    n += (size_t)snprintf(buffer + n, buffer_size - n, "%s%s",
                          n > 0 ? " " : "", weight);
    if (n >= buffer_size) {
      return base;
    }
  }

  if (style[0] != '\0') {
    n += (size_t)snprintf(buffer + n, buffer_size - n, "%s%s",
                          n > 0 ? " " : "", style);
    if (n >= buffer_size) {
      return base;
    }
  }

  if (size_token) {
    n += (size_t)snprintf(buffer + n, buffer_size - n, " %s", size_token);
    if (n >= buffer_size) {
      return base;
    }
  }

  if (buffer[0] == '\0') {
    return base;
  }

  return buffer;
}

static int x11_calc_line_height(XftFont *font, double line_spacing) {
  int base;
  int height;
  if (!font) {
    return 16;
  }
  if (line_spacing <= 0.0) {
    line_spacing = 1.0;
  }
  base = font->ascent + font->descent + 2;
  height = (int)((double)base * line_spacing + 0.5);
  if (height < base) {
    height = base;
  }
  return height;
}

static XftFont *x11_open_scaled_font(Display *dpy, const char *font_name,
                                     double scale) {
  FcPattern *pattern;
  double size = 12.0;
  XftFont *font = NULL;

  if (!dpy || !font_name || font_name[0] == '\0' || scale <= 0.0) {
    return NULL;
  }

  pattern = FcNameParse((const FcChar8 *)font_name);
  if (!pattern) {
    return NULL;
  }
  FcConfigSubstitute(NULL, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);
  if (FcPatternGetDouble(pattern, FC_SIZE, 0, &size) != FcResultMatch ||
      size <= 0.0) {
    size = 12.0;
  }
  size *= scale;
  FcPatternDel(pattern, FC_SIZE);
  FcPatternAddDouble(pattern, FC_SIZE, size);
  font = XftFontOpenPattern(dpy, pattern);
  if (!font) {
    FcPatternDestroy(pattern);
  }
  return font;
}

static int x11_text_width_n(x11_state *s, XftFont *font, const char *text,
                            int len) {
  XGlyphInfo ext;
  if (!s || !font || !text || len <= 0) {
    return 0;
  }
  XftTextExtentsUtf8(s->dpy, font, (const FcChar8 *)text, len, &ext);
  return (int)ext.xOff;
}

static int x11_text_width(x11_state *s, XftFont *font, const char *text) {
  if (!text) {
    return 0;
  }
  return x11_text_width_n(s, font, text, (int)strlen(text));
}

static int x11_wrap_text(x11_state *s, XftFont *font, const char *text,
                         int max_width, x11_lines *out) {
  const char *p;
  int space_width;
  char *line = NULL;
  size_t line_len = 0;
  size_t line_cap = 0;
  int line_width = 0;

  if (!out || !s) {
    return -1;
  }
  out->lines = NULL;
  out->count = 0;

  if (!text || text[0] == '\0') {
    return lines_push(out, "", 0);
  }

  if (max_width <= 0) {
    max_width = 600;
  }

  space_width = x11_text_width(s, font, " ");
  p = text;
  while (*p) {
    const char *word_start;
    const char *word_end;
    size_t word_len;
    int word_width;

    while (*p && isspace((unsigned char)*p)) {
      p++;
    }
    if (!*p) {
      break;
    }

    word_start = p;
    while (*p && !isspace((unsigned char)*p)) {
      p++;
    }
    word_end = p;
    word_len = (size_t)(word_end - word_start);

    word_width = x11_text_width_n(s, font, word_start, (int)word_len);
    if (word_width == 0 && word_len > 0) {
      word_width = (int)word_len * space_width;
    }

    if (line_len == 0) {
      size_t needed = word_len + 1;
      if (needed > line_cap) {
        size_t next_cap = line_cap == 0 ? 64 : line_cap * 2;
        while (next_cap < needed) {
          next_cap *= 2;
        }
        line = (char *)realloc(line, next_cap);
        if (!line) {
          lines_free(out);
          return -1;
        }
        line_cap = next_cap;
      }
      memcpy(line, word_start, word_len);
      line[word_len] = '\0';
      line_len = word_len;
      line_width = word_width;
      if (word_width > max_width && word_len > 0) {
        if (lines_push(out, line, line_len) != 0) {
          free(line);
          lines_free(out);
          return -1;
        }
        line_len = 0;
        line_width = 0;
      }
      continue;
    }

    if (line_width + space_width + word_width <= max_width) {
      size_t needed = line_len + 1 + word_len + 1;
      if (needed > line_cap) {
        size_t next_cap = line_cap == 0 ? 64 : line_cap * 2;
        while (next_cap < needed) {
          next_cap *= 2;
        }
        line = (char *)realloc(line, next_cap);
        if (!line) {
          lines_free(out);
          return -1;
        }
        line_cap = next_cap;
      }
      line[line_len] = ' ';
      memcpy(line + line_len + 1, word_start, word_len);
      line_len += 1 + word_len;
      line[line_len] = '\0';
      line_width += space_width + word_width;
    } else {
      if (lines_push(out, line, line_len) != 0) {
        free(line);
        lines_free(out);
        return -1;
      }
      line_len = 0;
      line_width = 0;
      p = word_start;
    }
  }

  if (line_len > 0 || out->count == 0) {
    if (lines_push(out, line ? line : "", line_len) != 0) {
      free(line);
      lines_free(out);
      return -1;
    }
  }

  free(line);
  return 0;
}

static void x11_apply_opacity(x11_state *s, double opacity) {
  Atom opacity_atom;
  unsigned long value;
  if (!s || !s->dpy || !s->win) {
    return;
  }
  if (opacity < 0.0) {
    opacity = 0.0;
  }
  if (opacity > 1.0) {
    opacity = 1.0;
  }
  value = (unsigned long)(opacity * 0xFFFFFFFFu);
  opacity_atom = XInternAtom(s->dpy, "_NET_WM_WINDOW_OPACITY", False);
  XChangeProperty(s->dpy, s->win, opacity_atom, XA_CARDINAL, 32, PropModeReplace,
                  (unsigned char *)&value, 1);
}

static void x11_apply_window_type(x11_state *s) {
  Atom wm_type;
  Atom type_notification;
  if (!s || !s->dpy || !s->win) {
    return;
  }
  wm_type = XInternAtom(s->dpy, "_NET_WM_WINDOW_TYPE", False);
  type_notification = XInternAtom(s->dpy, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
  XChangeProperty(s->dpy, s->win, wm_type, XA_ATOM, 32, PropModeReplace,
                  (unsigned char *)&type_notification, 1);
}

static void x11_apply_window_state(x11_state *s) {
  Atom wm_state;
  Atom atoms[4];
  int count = 0;
  if (!s || !s->dpy || !s->win) {
    return;
  }
  wm_state = XInternAtom(s->dpy, "_NET_WM_STATE", False);
  atoms[count++] = XInternAtom(s->dpy, "_NET_WM_STATE_ABOVE", False);
  atoms[count++] = XInternAtom(s->dpy, "_NET_WM_STATE_STICKY", False);
  atoms[count++] = XInternAtom(s->dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
  atoms[count++] = XInternAtom(s->dpy, "_NET_WM_STATE_SKIP_PAGER", False);
  XChangeProperty(s->dpy, s->win, wm_state, XA_ATOM, 32, PropModeReplace,
                  (unsigned char *)atoms, count);
}

static void x11_apply_desktop_all(x11_state *s) {
  Atom desktop;
  unsigned long value = 0xFFFFFFFFu;
  if (!s || !s->dpy || !s->win) {
    return;
  }
  desktop = XInternAtom(s->dpy, "_NET_WM_DESKTOP", False);
  XChangeProperty(s->dpy, s->win, desktop, XA_CARDINAL, 32, PropModeReplace,
                  (unsigned char *)&value, 1);
}

static void x11_apply_click_through(x11_state *s, int enabled) {
  XserverRegion region;
  if (!s || !s->dpy || !s->win || !enabled) {
    return;
  }
  if (!s->xfixes_available) {
    log_error("x11: XFixes not available for click-through");
    return;
  }
  region = XFixesCreateRegion(s->dpy, NULL, 0);
  XFixesSetWindowShapeRegion(s->dpy, s->win, ShapeInput, 0, 0, region);
  XFixesDestroyRegion(s->dpy, region);
}

static void x11_position_window(x11_state *s) {
  int screen_w;
  int screen_h;
  int x = 0;
  int y = 0;
  int anchor_right = 0;
  int anchor_bottom = 0;

  if (!s || !s->dpy || !s->win) {
    return;
  }

  screen_w = DisplayWidth(s->dpy, s->screen);
  screen_h = DisplayHeight(s->dpy, s->screen);

  if (s->options.anchor[0] != '\0') {
    if (strstr(s->options.anchor, "right")) {
      anchor_right = 1;
    }
    if (strstr(s->options.anchor, "bottom")) {
      anchor_bottom = 1;
    }
  } else {
    anchor_right = 1;
    anchor_bottom = 1;
  }

  if (anchor_right) {
    x = screen_w - s->width - s->options.offset_x;
  } else {
    x = s->options.offset_x;
  }

  if (anchor_bottom) {
    y = screen_h - s->height - s->options.offset_y;
  } else {
    y = s->options.offset_y;
  }

  if (x < 0) {
    x = 0;
  }
  if (y < 0) {
    y = 0;
  }

  XMoveWindow(s->dpy, s->win, x, y);
}

static int x11_alloc_color(x11_state *s, int r, int g, int b, XftColor *out) {
  XRenderColor color;
  if (!s || !out) {
    return -1;
  }
  color.red = (unsigned short)(r * 257);
  color.green = (unsigned short)(g * 257);
  color.blue = (unsigned short)(b * 257);
  color.alpha = 0xFFFF;
  if (!XftColorAllocValue(s->dpy, s->visual, s->colormap, &color, out)) {
    return -1;
  }
  return 0;
}

static void x11_process_events(x11_state *s) {
  XEvent ev;
  if (!s || !s->dpy) {
    return;
  }
  while (XPending(s->dpy)) {
    XNextEvent(s->dpy, &ev);
    if (ev.type == ConfigureNotify) {
      XConfigureEvent *cfg = (XConfigureEvent *)&ev;
      if (cfg->width > 0 && cfg->height > 0) {
        if (cfg->width != s->width || cfg->height != s->height) {
          s->width = cfg->width;
          s->height = cfg->height;
          s->content_width = s->width - s->padding_x * 2;
          if (s->content_width < 1) {
            s->content_width = s->width > 0 ? s->width : 1;
          }
          x11_ensure_buffer(s);
        }
      }
    }
  }
}

static int x11_ensure_buffer(x11_state *s) {
  if (!s || !s->dpy || !s->win) {
    return -1;
  }
  if (s->pixmap) {
    XFreePixmap(s->dpy, s->pixmap);
    s->pixmap = 0;
  }
  if (s->draw) {
    XftDrawDestroy(s->draw);
    s->draw = NULL;
  }
  if (s->gc) {
    XFreeGC(s->dpy, s->gc);
    s->gc = NULL;
  }

  s->pixmap = XCreatePixmap(s->dpy, s->win, (unsigned int)s->width,
                            (unsigned int)s->height,
                            (unsigned int)DefaultDepth(s->dpy, s->screen));
  if (!s->pixmap) {
    return -1;
  }
  s->gc = XCreateGC(s->dpy, s->pixmap, 0, NULL);
  if (!s->gc) {
    return -1;
  }
  s->draw = XftDrawCreate(s->dpy, s->pixmap, s->visual, s->colormap);
  if (!s->draw) {
    return -1;
  }
  return 0;
}

static void x11_clear(x11_state *s) {
  if (!s || !s->dpy || !s->gc) {
    return;
  }
  XSetForeground(s->dpy, s->gc, s->color_bg.pixel);
  XFillRectangle(s->dpy, s->pixmap, s->gc, 0, 0,
                 (unsigned int)s->width, (unsigned int)s->height);
}

static void x11_draw_text(x11_state *s, XftFont *font, int x, int y,
                          const char *text, XftColor *color) {
  if (!s || !s->draw || !font || !text || text[0] == '\0') {
    return;
  }
  XftDrawStringUtf8(s->draw, color, font, x, y,
                    (const FcChar8 *)text, (int)strlen(text));
}

static void x11_draw_wrapped(x11_state *s, XftFont *font, int line_height,
                             const char *text, const char *prefix,
                             const char *indent, XftColor *color,
                             int content_width, int *io_y) {
  x11_lines lines;
  size_t i;
  int y;

  if (!s || !io_y) {
    return;
  }
  if (!font) {
    font = s->font;
  }
  if (line_height <= 0) {
    line_height = s->line_height;
  }
  if (!prefix) {
    prefix = "";
  }
  if (!indent) {
    indent = "";
  }

  if (x11_wrap_text(s, font, text ? text : "", content_width, &lines) != 0) {
    return;
  }

  y = *io_y;
  for (i = 0; i < lines.count; i++) {
    char *visual = NULL;
    int is_rtl = 0;
    char *prefix_wrapped = NULL;
    char *text_wrapped = NULL;
    const char *line_text = lines.lines[i] ? lines.lines[i] : "";
    const char *prefix_text = i == 0 ? prefix : indent;
    int baseline = y + (font ? font->ascent : 12);
    int prefix_width = 0;
    int text_width = 0;
    int text_x;
    int align_right = s->rtl_align == UNICODE_RTL_ALIGN_RIGHT;

    if (unicode_visual_order(line_text, s->rtl_mode, s->rtl_shape, s->bidi_mode,
                             &visual, &is_rtl) == 0 && visual) {
      line_text = visual;
    }

    if (is_rtl) {
      if (unicode_wrap_with_lrm(prefix_text, &prefix_wrapped) == 0 &&
          prefix_wrapped) {
        prefix_text = prefix_wrapped;
      }
      if (unicode_wrap_with_lro(line_text, &text_wrapped) == 0 && text_wrapped) {
        line_text = text_wrapped;
      }
    }

    prefix_width = x11_text_width(s, font, prefix_text);
    text_width = x11_text_width(s, font, line_text);
    text_x = s->padding_x + prefix_width;
    if (align_right && content_width > prefix_width) {
      int remaining = content_width - prefix_width - text_width;
      if (remaining > 0) {
        text_x = s->padding_x + prefix_width + remaining;
      }
    }

    x11_draw_text(s, font, s->padding_x, baseline, prefix_text, color);
    x11_draw_text(s, font, text_x, baseline, line_text, color);

    free(visual);
    free(prefix_wrapped);
    free(text_wrapped);

    y += line_height;
  }

  *io_y = y;
  lines_free(&lines);
}

static void x11_present(x11_state *s) {
  if (!s || !s->dpy || !s->win || !s->pixmap) {
    return;
  }
  XCopyArea(s->dpy, s->pixmap, s->win, s->gc, 0, 0,
            (unsigned int)s->width, (unsigned int)s->height, 0, 0);
  XFlush(s->dpy);
}

int x11_backend_init(const ui_options *options) {
  XSetWindowAttributes attrs;
  XClassHint class_hint;
  XWMHints wm_hints;
  int event_base = 0;
  int error_base = 0;
  int width;
  int height;

  memset(&g_x11, 0, sizeof(g_x11));
  if (options) {
    g_x11.options = *options;
  }
  g_x11.line_spacing = g_x11.options.line_spacing > 0.0
                             ? g_x11.options.line_spacing
                             : 1.0;
  g_x11.title_scale = g_x11.options.title_scale > 0.0
                          ? g_x11.options.title_scale
                          : 1.0;

  setlocale(LC_CTYPE, "");

  g_x11.dpy = XOpenDisplay(NULL);
  if (!g_x11.dpy) {
    log_error("x11: failed to open display");
    return -1;
  }

  g_x11.screen = DefaultScreen(g_x11.dpy);
  g_x11.root = RootWindow(g_x11.dpy, g_x11.screen);
  g_x11.visual = DefaultVisual(g_x11.dpy, g_x11.screen);
  g_x11.colormap = DefaultColormap(g_x11.dpy, g_x11.screen);

  width = g_x11.options.width > 0 ? g_x11.options.width : 600;
  height = g_x11.options.height > 0 ? g_x11.options.height : 240;
  g_x11.width = width;
  g_x11.height = height;
  g_x11.padding_x = g_x11.options.padding_x;
  g_x11.padding_y = g_x11.options.padding_y;
  if (g_x11.padding_x < 0) {
    g_x11.padding_x = 0;
  }
  if (g_x11.padding_y < 0) {
    g_x11.padding_y = 0;
  }
  g_x11.content_width = g_x11.width - g_x11.padding_x * 2;
  if (g_x11.content_width < 1) {
    g_x11.content_width = g_x11.width > 0 ? g_x11.width : 1;
  }

  attrs.background_pixel = BlackPixel(g_x11.dpy, g_x11.screen);
  attrs.border_pixel = 0;
  attrs.colormap = g_x11.colormap;

  g_x11.win = XCreateWindow(g_x11.dpy, g_x11.root, 0, 0,
                            (unsigned int)g_x11.width,
                            (unsigned int)g_x11.height, 0,
                            DefaultDepth(g_x11.dpy, g_x11.screen), InputOutput,
                            g_x11.visual, CWBackPixel | CWBorderPixel | CWColormap,
                            &attrs);
  if (!g_x11.win) {
    log_error("x11: failed to create window");
    XCloseDisplay(g_x11.dpy);
    return -1;
  }

  XSelectInput(g_x11.dpy, g_x11.win, ExposureMask | StructureNotifyMask);

  class_hint.res_name = "csong";
  class_hint.res_class = "csong";
  XSetClassHint(g_x11.dpy, g_x11.win, &class_hint);

  wm_hints.flags = InputHint;
  wm_hints.input = False;
  XSetWMHints(g_x11.dpy, g_x11.win, &wm_hints);
  XStoreName(g_x11.dpy, g_x11.win, "csong");

  x11_apply_window_type(&g_x11);
  x11_apply_window_state(&g_x11);
  x11_apply_desktop_all(&g_x11);
  x11_apply_opacity(&g_x11, g_x11.options.opacity);

  g_x11.xfixes_available = XFixesQueryExtension(g_x11.dpy, &event_base, &error_base);
  x11_apply_click_through(&g_x11, g_x11.options.click_through);
  x11_position_window(&g_x11);

  XMapRaised(g_x11.dpy, g_x11.win);

  g_x11.font = XftFontOpenName(g_x11.dpy, g_x11.screen,
                               g_x11.options.font[0] != '\0'
                                   ? g_x11.options.font
                                   : "Sans 12");
  if (!g_x11.font) {
    log_error("x11: failed to load font");
    x11_backend_shutdown();
    return -1;
  }

  g_x11.title_font = g_x11.font;
  {
    char title_buf[256];
    const char *title_name = x11_compose_title_font_name(
        &g_x11.options, title_buf, sizeof(title_buf));
    int need_title_font =
        (g_x11.title_scale > 0.0 && g_x11.title_scale != 1.0) ||
        g_x11.options.title_font[0] != '\0' ||
        g_x11.options.title_weight[0] != '\0' ||
        g_x11.options.title_style[0] != '\0';

    if (need_title_font) {
      if (g_x11.title_scale > 0.0 && g_x11.title_scale != 1.0) {
        g_x11.title_font =
            x11_open_scaled_font(g_x11.dpy, title_name, g_x11.title_scale);
      } else {
        g_x11.title_font =
            XftFontOpenName(g_x11.dpy, g_x11.screen, title_name);
      }
      if (!g_x11.title_font) {
        g_x11.title_font = g_x11.font;
      }
    }
  }

  g_x11.line_height = x11_calc_line_height(g_x11.font, g_x11.line_spacing);
  g_x11.title_line_height =
      x11_calc_line_height(g_x11.title_font, g_x11.line_spacing);
  if (x11_alloc_color(&g_x11, g_x11.options.fg_r, g_x11.options.fg_g,
                      g_x11.options.fg_b, &g_x11.color_main) != 0 ||
      x11_alloc_color(&g_x11, g_x11.options.title_r, g_x11.options.title_g,
                      g_x11.options.title_b, &g_x11.color_title) != 0 ||
      x11_alloc_color(&g_x11, g_x11.options.dim_r, g_x11.options.dim_g,
                      g_x11.options.dim_b, &g_x11.color_dim) != 0 ||
      x11_alloc_color(&g_x11, g_x11.options.prev_r, g_x11.options.prev_g,
                      g_x11.options.prev_b, &g_x11.color_prev) != 0 ||
      x11_alloc_color(&g_x11, g_x11.options.bg_r, g_x11.options.bg_g,
                      g_x11.options.bg_b, &g_x11.color_bg) != 0) {
    log_error("x11: failed to allocate colors");
    x11_backend_shutdown();
    return -1;
  }
  g_x11.colors_ready = 1;

  if (x11_ensure_buffer(&g_x11) != 0) {
    log_error("x11: failed to allocate buffer");
    x11_backend_shutdown();
    return -1;
  }

  g_x11.ready = 1;
  return 0;
}

void x11_backend_set_rtl(int rtl_mode, int rtl_align, int rtl_shape,
                         int bidi_mode) {
  g_x11.rtl_mode = rtl_mode;
  g_x11.rtl_align = rtl_align;
  g_x11.rtl_shape = rtl_shape;
  g_x11.bidi_mode = bidi_mode;
}

void x11_backend_draw_status(const char *status, const char *icon) {
  char line[768];
  int y;

  if (!g_x11.ready) {
    return;
  }

  x11_process_events(&g_x11);
  x11_clear(&g_x11);

  y = g_x11.padding_y;
  if (icon && icon[0] != '\0') {
    snprintf(line, sizeof(line), "%s %s", icon, status ? status : "");
  } else {
    snprintf(line, sizeof(line), "%s", status ? status : "");
  }
  x11_draw_wrapped(&g_x11, g_x11.font, g_x11.line_height, line, "", "",
                   &g_x11.color_dim, g_x11.content_width, &y);
  x11_present(&g_x11);
}

void x11_backend_draw(const char *artist, const char *title, const lyrics_doc *doc,
                      int current_index, double elapsed, const char *status,
                      const char *icon, int pulse, int prev_index,
                      int transition_step, int transition_total) {
  int max_lines = 5;
  int context = 2;
  size_t i;
  size_t start;
  size_t end;
  int y;
  int is_transition = 0;
  float t = 1.0f;
  (void)pulse;

  if (!g_x11.ready) {
    return;
  }

  x11_process_events(&g_x11);
  x11_clear(&g_x11);

  y = g_x11.padding_y;
  if (artist && title) {
    char header[768];
    char timebuf[32];
    int minutes = (int)(elapsed / 60.0);
    int seconds = (int)elapsed % 60;
    snprintf(timebuf, sizeof(timebuf), "(%02d:%02d)", minutes, seconds);
    if (icon && icon[0] != '\0') {
      snprintf(header, sizeof(header), "%s %s - %s %s", icon, artist, title,
               timebuf);
    } else {
      snprintf(header, sizeof(header), "%s - %s %s", artist, title, timebuf);
    }
    x11_draw_wrapped(&g_x11, g_x11.title_font, g_x11.title_line_height, header,
                     "", "", &g_x11.color_title, g_x11.content_width, &y);
    y += g_x11.title_line_height / 2;
  }

  if (status && status[0] != '\0') {
    x11_draw_wrapped(&g_x11, g_x11.font, g_x11.line_height, status, "", "",
                     &g_x11.color_dim, g_x11.content_width, &y);
    y += g_x11.line_height / 2;
  }

  if (!doc || doc->count == 0) {
    if (!status || status[0] == '\0') {
      x11_draw_wrapped(&g_x11, g_x11.font, g_x11.line_height, "No lyrics found.",
                       "", "", &g_x11.color_dim, g_x11.content_width, &y);
    }
    x11_present(&g_x11);
    return;
  }

  if (transition_total > 1 && prev_index >= 0 && current_index >= 0 &&
      prev_index != current_index) {
    is_transition = 1;
    if (transition_step < 0) {
      transition_step = 0;
    }
    if (transition_step >= transition_total) {
      transition_step = transition_total - 1;
    }
    t = (float)transition_step / (float)(transition_total - 1);
  }

  if (!doc->has_timestamps || current_index < 0) {
    start = 0;
    end = doc->count > (size_t)max_lines ? (size_t)max_lines - 1 : doc->count - 1;
    for (i = start; i <= end; i++) {
      x11_draw_wrapped(&g_x11, g_x11.font, g_x11.line_height,
                       doc->lines[i].text, "", "", &g_x11.color_dim,
                       g_x11.content_width, &y);
    }
    x11_present(&g_x11);
    return;
  }

  start = current_index > context ? (size_t)(current_index - context) : 0;
  end = start + (size_t)max_lines - 1;
  if (end >= doc->count) {
    end = doc->count - 1;
    if (end + 1 > (size_t)max_lines) {
      start = end - (size_t)max_lines + 1;
    } else {
      start = 0;
    }
  }

  for (i = start; i <= end; i++) {
    const char *text = doc->lines[i].text ? doc->lines[i].text : "";
    if ((int)i == current_index) {
      XftColor *color = &g_x11.color_main;
      if (is_transition && t < 0.6f) {
        color = &g_x11.color_prev;
      }
      x11_draw_wrapped(&g_x11, g_x11.font, g_x11.line_height, text, "> ", "  ",
                       color, g_x11.content_width, &y);
    } else if (is_transition && (int)i == prev_index) {
      x11_draw_wrapped(&g_x11, g_x11.font, g_x11.line_height, text, "  ", "  ",
                       &g_x11.color_prev, g_x11.content_width, &y);
    } else {
      x11_draw_wrapped(&g_x11, g_x11.font, g_x11.line_height, text, "  ", "  ",
                       &g_x11.color_dim, g_x11.content_width, &y);
    }
  }

  x11_present(&g_x11);
}

void x11_backend_shutdown(void) {
  if (g_x11.draw) {
    XftDrawDestroy(g_x11.draw);
    g_x11.draw = NULL;
  }
  if (g_x11.colors_ready && g_x11.dpy) {
    XftColorFree(g_x11.dpy, g_x11.visual, g_x11.colormap, &g_x11.color_main);
    XftColorFree(g_x11.dpy, g_x11.visual, g_x11.colormap, &g_x11.color_title);
    XftColorFree(g_x11.dpy, g_x11.visual, g_x11.colormap, &g_x11.color_dim);
    XftColorFree(g_x11.dpy, g_x11.visual, g_x11.colormap, &g_x11.color_prev);
    XftColorFree(g_x11.dpy, g_x11.visual, g_x11.colormap, &g_x11.color_bg);
    g_x11.colors_ready = 0;
  }
  if (g_x11.title_font && g_x11.title_font != g_x11.font) {
    XftFontClose(g_x11.dpy, g_x11.title_font);
    g_x11.title_font = NULL;
  }
  if (g_x11.font) {
    XftFontClose(g_x11.dpy, g_x11.font);
    g_x11.font = NULL;
  }
  if (g_x11.pixmap) {
    XFreePixmap(g_x11.dpy, g_x11.pixmap);
    g_x11.pixmap = 0;
  }
  if (g_x11.gc) {
    XFreeGC(g_x11.dpy, g_x11.gc);
    g_x11.gc = NULL;
  }
  if (g_x11.win) {
    XDestroyWindow(g_x11.dpy, g_x11.win);
    g_x11.win = 0;
  }
  if (g_x11.dpy) {
    XCloseDisplay(g_x11.dpy);
    g_x11.dpy = NULL;
  }
  g_x11.ready = 0;
}
