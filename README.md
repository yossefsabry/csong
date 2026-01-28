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
  - `[render].bidi` (`fribidi`, `terminal`)
  - `[render].rtl_mode` (`auto`, `on`, `off`)
  - `[render].rtl_align` (`left`, `right`)
  - `[render].rtl_shape` (`auto`, `on`, `off`)

If Arabic words look reversed, set `[render].bidi = "fribidi"` (default) so the
app locks visual order and avoids double BiDi from terminals.

## Project layout
- include/app/: public headers
- src/: implementation
- config/: sample config
- assets/: fonts
- tests/: test scaffolding
- scripts/: helper scripts
