# csong

Terminal lyrics viewer for MPD/ncmpcpp (X11 overlay planned).

## Goals
- Follow MPD track changes
- Fetch and cache lyrics
- Render lyrics on all X11 workspaces

## Dependencies
- libmpdclient
- libcurl

## Build (GCC + Make)
```sh
make
```

## Run
```sh
./csong
```

Options:
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

## Project layout
- include/app/: public headers
- src/: implementation
- config/: sample config
- assets/: fonts
- tests/: test scaffolding
- scripts/: helper scripts
