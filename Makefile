CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -O2 -Iinclude -D_POSIX_C_SOURCE=200809L
LDFLAGS ?= -lmpdclient -lcurl

BIN := csong

SRC := \
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
  src/x11/window.c \
  src/x11/workspace.c \
  src/x11/events.c \
  src/x11/compositor.c \
  src/util/fs.c \
  src/util/string.c \
  src/util/time.c

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
