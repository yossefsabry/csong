# spotify_lyrics

CLI tool that prints lyrics for the current Spotify track, including play/pause status and
position.

Status:
- Linux: uses MPRIS over DBus to read Spotify metadata.
- Windows: uses Global System Media Transport Controls (GSMTC).

Lyrics sources: Lyrics.ovh, LRCLIB (fallback).

## Build

Linux dependencies:
- libdbus-1-dev
- libcurl4-openssl-dev
- cmake
- build-essential

Windows dependencies:
- Visual Studio 2022 with C++/WinRT and Windows 10+ SDK
- libcurl (vcpkg or system install)

Build steps:
1. mkdir build
2. cd build
3. cmake ..
4. cmake --build .

Run:
./spotify_lyrics

## Usage

./spotify_lyrics [--watch] [--interval N] [--cache-dir PATH] [--no-cache] [--debug]

Examples:
- ./spotify_lyrics
- ./spotify_lyrics --watch --interval 2
- ./spotify_lyrics --cache-dir /tmp/spotify_lyrics_cache

Cache locations (default):
- Linux: $XDG_CACHE_HOME/spotify_lyrics or ~/.cache/spotify_lyrics
- Windows: %LOCALAPPDATA%\spotify_lyrics

## Notes

- The tool prints lyrics when Spotify is playing or paused.
- Status line shows play/pause and current position.
- Use --debug to print raw and normalized track metadata.
