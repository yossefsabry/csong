CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -O2 -Iinclude -Ivendor/toml -Ivendor/jsmn -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
LDFLAGS ?= -lmpdclient -lcurl -lfribidi -lm -lXft -lfontconfig -lfreetype -lXrender -lX11 -lXfixes -lXext -ldbus-1

CFLAGS += $(shell pkg-config --cflags xft 2>/dev/null)
CFLAGS += $(shell pkg-config --cflags dbus-1 2>/dev/null)

BIN := csong

SRC := \
  vendor/toml/toml.c \
  src/main.c \
  src/app/app.c \
  src/app/config.c \
  src/app/log.c \
  src/app/state.c \
  src/mpd/mpd_client.c \
  src/mpd/event_loop.c \
  src/lyrics/provider.c \
  src/lyrics/cache.c \
  src/lyrics/format.c \
  src/render/renderer.c \
  src/render/text_layout.c \
  src/render/font.c \
  src/player/player.c \
  src/player/spotify.c \
  src/player/mpris_common.c \
  src/player/spotify_mpris.c \
  src/player/ytmusic.c \
  src/player/ytmusic_mpris.c \
  src/player/ytmusic_win.c \
  src/ui/ui.c \
  src/ui/x11_backend.c \
  src/x11/window.c \
  src/x11/workspace.c \
  src/x11/events.c \
  src/x11/compositor.c \
  src/util/fs.c \
  src/util/string.c \
  src/util/time.c \
  src/util/normalize.c \
  src/util/unicode.c

OBJ := $(SRC:%.c=out/%.o)

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

out/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf out $(BIN)

.PHONY: all clean
