# csong

Terminal lyrics viewer for MPD/ncmpcpp (X11 overlay planned).

## Goals
- Follow MPD track changes
- Fetch and cache lyrics
- Render lyrics on all X11 workspaces

## Dependencies
- libmpdclient
- libcurl
- libfribidi
- libX11 (X11 backend)
- libXft (X11 backend)
- libXfixes (X11 backend)
- libXrender (X11 backend)
- fontconfig + freetype (X11 backend)

## Build (GCC + Make)
```sh
make
```

## Run
```sh
./csong
```

Options:
- `--config PATH` (default: `~/.config/csong/config.toml` or `$XDG_CONFIG_HOME/csong/config.toml`)
- `--mpd-host HOST` (default: 127.0.0.1)
- `--mpd-port PORT` (default: 6600)
- `--once` (print once and exit)
- `--interval N` (seconds between updates, default: 1)
- `--show-plain` (display untimed lyrics)

## Notes
- Stores and reads lyrics in `~/lyrics/`
- Prefers `Artist - Title.lrc`, then `Artist - Title.txt`
- If artist is missing, tries `Title.lrc` then `Title.txt`
- Fetches synced lyrics from lrclib when available; falls back to lyrics.ovh
- Shows an animated music icon during intros and instrumental gaps (based on LRC)
- Supports LRC `[offset:+/-ms]` tags
- Optional per-track offsets in `~/lyrics/.offsets`:
  ```
  Dua Lipa - Houdini = -4.0
  Houdini = -4.0
  ```

## Config
- Default path: `~/.config/csong/config.toml` (or `$XDG_CONFIG_HOME/csong/config.toml`)
- Supported keys:
  - `interval` (seconds between updates)
  - `show_plain` (boolean)
  - `[mpd].host`, `[mpd].port`
  - `[lyrics].cache_dir` (overrides default `~/lyrics` cache)
  - `[ui].backend` (`terminal`, `x11`)
  - `[ui].font` (font name/size for GUI backends)
  - `[ui].title_font` (X11 only)
  - `[ui].title_weight`, `[ui].title_style` (X11 only, used when `title_font` is empty)
  - `[ui].opacity` (0.0-1.0)
  - `[ui].anchor` (`top-right`, `bottom-right`, etc.)
  - `[ui].offset_x`, `[ui].offset_y` (pixels)
  - `[ui].padding_x`, `[ui].padding_y` (pixels)
  - `[ui].fg_color`, `[ui].title_color`, `[ui].dim_color`, `[ui].prev_color`, `[ui].bg_color` (hex)
  - `[ui].line_spacing` (float, X11 only)
  - `[ui].title_scale` (float, X11 only)
  - `[ui].width`, `[ui].height` (pixels, 0 = auto)
  - `[ui].click_through` (boolean)
  - `[render].bidi` (`fribidi`, `terminal`)
  - `[render].rtl_mode` (`auto`, `on`, `off`)
  - `[render].rtl_align` (`left`, `right`)
  - `[render].rtl_shape` (`auto`, `on`, `off`)

To use the X11 overlay backend, set `[ui].backend = "x11"`. The `[ui]` color and
padding options also apply to the terminal renderer.

If Arabic words look reversed, set `[render].bidi = "fribidi"` (default) so the
app locks visual order and avoids double BiDi from terminals.

## Project layout
- include/app/: public headers
- src/: implementation
- config/: sample config
- assets/: fonts
- tests/: test scaffolding
- scripts/: helper scripts
