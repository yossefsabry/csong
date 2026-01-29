#include "platform.h"

#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static void set_err(char *err, size_t err_cap, const char *msg) {
  if (!err || err_cap == 0) {
    return;
  }
  snprintf(err, err_cap, "%s", msg);
}

static char *dup_string(const char *s) {
  size_t len = strlen(s);
  char *out = (char *)malloc(len + 1);
  if (!out) {
    return NULL;
  }
  memcpy(out, s, len + 1);
  return out;
}

static DBusMessage *get_property_reply(DBusConnection *conn, const char *prop,
                                       DBusError *dbus_err) {
  DBusMessage *msg = dbus_message_new_method_call(
      "org.mpris.MediaPlayer2.spotify", "/org/mpris/MediaPlayer2",
      "org.freedesktop.DBus.Properties", "Get");
  if (!msg) {
    return NULL;
  }
  const char *iface = "org.mpris.MediaPlayer2.Player";
  if (!dbus_message_append_args(msg, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop,
                                DBUS_TYPE_INVALID)) {
    dbus_message_unref(msg);
    return NULL;
  }
  DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, 1000, dbus_err);
  dbus_message_unref(msg);
  return reply;
}

static SpotifyStatus get_string_property(DBusConnection *conn, const char *prop, char **out,
                                         char *err, size_t err_cap) {
  DBusError dbus_err;
  dbus_error_init(&dbus_err);
  DBusMessage *reply = get_property_reply(conn, prop, &dbus_err);
  if (!reply) {
    if (dbus_error_is_set(&dbus_err)) {
      set_err(err, err_cap, dbus_err.message);
      dbus_error_free(&dbus_err);
    } else {
      set_err(err, err_cap, "Failed to read Spotify status");
    }
    return SPOTIFY_ERROR;
  }

  DBusMessageIter iter;
  if (!dbus_message_iter_init(reply, &iter) ||
      dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT) {
    dbus_message_unref(reply);
    set_err(err, err_cap, "Unexpected DBus response");
    return SPOTIFY_ERROR;
  }
  DBusMessageIter variant;
  dbus_message_iter_recurse(&iter, &variant);
  if (dbus_message_iter_get_arg_type(&variant) != DBUS_TYPE_STRING) {
    dbus_message_unref(reply);
    set_err(err, err_cap, "Unexpected DBus response");
    return SPOTIFY_ERROR;
  }
  const char *value = NULL;
  dbus_message_iter_get_basic(&variant, &value);
  if (value) {
    *out = dup_string(value);
  }
  dbus_message_unref(reply);
  return value ? SPOTIFY_OK : SPOTIFY_ERROR;
}

static SpotifyStatus get_int64_property(DBusConnection *conn, const char *prop, int64_t *out,
                                        char *err, size_t err_cap) {
  DBusError dbus_err;
  dbus_error_init(&dbus_err);
  DBusMessage *reply = get_property_reply(conn, prop, &dbus_err);
  if (!reply) {
    if (dbus_error_is_set(&dbus_err)) {
      set_err(err, err_cap, dbus_err.message);
      dbus_error_free(&dbus_err);
    } else {
      set_err(err, err_cap, "Failed to read Spotify position");
    }
    return SPOTIFY_ERROR;
  }

  DBusMessageIter iter;
  if (!dbus_message_iter_init(reply, &iter) ||
      dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT) {
    dbus_message_unref(reply);
    set_err(err, err_cap, "Unexpected DBus response");
    return SPOTIFY_ERROR;
  }
  DBusMessageIter variant;
  dbus_message_iter_recurse(&iter, &variant);
  int type = dbus_message_iter_get_arg_type(&variant);
  if (type == DBUS_TYPE_INT64) {
    dbus_int64_t value = 0;
    dbus_message_iter_get_basic(&variant, &value);
    if (out) {
      *out = (int64_t)value;
    }
  } else if (type == DBUS_TYPE_UINT64) {
    dbus_uint64_t value = 0;
    dbus_message_iter_get_basic(&variant, &value);
    if (out) {
      *out = (int64_t)value;
    }
  } else {
    dbus_message_unref(reply);
    set_err(err, err_cap, "Unexpected DBus response");
    return SPOTIFY_ERROR;
  }
  dbus_message_unref(reply);
  return SPOTIFY_OK;
}

static SpotifyStatus get_metadata(DBusConnection *conn, char **artist, char **title,
                                  int64_t *duration_ms, char *err, size_t err_cap) {
  DBusError dbus_err;
  dbus_error_init(&dbus_err);
  DBusMessage *reply = get_property_reply(conn, "Metadata", &dbus_err);
  if (!reply) {
    if (dbus_error_is_set(&dbus_err)) {
      set_err(err, err_cap, dbus_err.message);
      dbus_error_free(&dbus_err);
    } else {
      set_err(err, err_cap, "Failed to read Spotify metadata");
    }
    return SPOTIFY_ERROR;
  }

  DBusMessageIter iter;
  if (!dbus_message_iter_init(reply, &iter) ||
      dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT) {
    dbus_message_unref(reply);
    set_err(err, err_cap, "Unexpected DBus response");
    return SPOTIFY_ERROR;
  }

  DBusMessageIter variant;
  dbus_message_iter_recurse(&iter, &variant);
  if (dbus_message_iter_get_arg_type(&variant) != DBUS_TYPE_ARRAY) {
    dbus_message_unref(reply);
    set_err(err, err_cap, "Unexpected DBus response");
    return SPOTIFY_ERROR;
  }

  DBusMessageIter array;
  dbus_message_iter_recurse(&variant, &array);
  while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_DICT_ENTRY) {
    DBusMessageIter dict;
    dbus_message_iter_recurse(&array, &dict);
    if (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_STRING) {
      dbus_message_iter_next(&array);
      continue;
    }
    const char *key = NULL;
    dbus_message_iter_get_basic(&dict, &key);
    if (!dbus_message_iter_next(&dict)) {
      dbus_message_iter_next(&array);
      continue;
    }
    if (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_VARIANT) {
      DBusMessageIter val;
      dbus_message_iter_recurse(&dict, &val);
      if (key && strcmp(key, "xesam:title") == 0 &&
          dbus_message_iter_get_arg_type(&val) == DBUS_TYPE_STRING) {
        const char *t = NULL;
        dbus_message_iter_get_basic(&val, &t);
        if (t && !*title) {
          *title = dup_string(t);
        }
      } else if (key && strcmp(key, "xesam:artist") == 0 &&
                 dbus_message_iter_get_arg_type(&val) == DBUS_TYPE_ARRAY) {
        DBusMessageIter artist_arr;
        dbus_message_iter_recurse(&val, &artist_arr);
        if (dbus_message_iter_get_arg_type(&artist_arr) == DBUS_TYPE_STRING) {
          const char *a = NULL;
          dbus_message_iter_get_basic(&artist_arr, &a);
          if (a && !*artist) {
            *artist = dup_string(a);
          }
        }
      } else if (key && strcmp(key, "mpris:length") == 0) {
        int type = dbus_message_iter_get_arg_type(&val);
        if (type == DBUS_TYPE_INT64) {
          dbus_int64_t len = 0;
          dbus_message_iter_get_basic(&val, &len);
          if (duration_ms && len > 0) {
            *duration_ms = (int64_t)(len / 1000);
          }
        } else if (type == DBUS_TYPE_UINT64) {
          dbus_uint64_t len = 0;
          dbus_message_iter_get_basic(&val, &len);
          if (duration_ms && len > 0) {
            *duration_ms = (int64_t)(len / 1000);
          }
        }
      }
    }
    dbus_message_iter_next(&array);
  }

  dbus_message_unref(reply);
  if (!*artist || !*title) {
    set_err(err, err_cap, "Spotify metadata incomplete");
    return SPOTIFY_ERROR;
  }
  return SPOTIFY_OK;
}

SpotifyStatus spotify_get_current_track(Track *out, PlaybackInfo *playback, char *err,
                                        size_t err_cap) {
  if (!out) {
    set_err(err, err_cap, "Invalid output pointer");
    return SPOTIFY_ERROR;
  }
  out->artist = NULL;
  out->title = NULL;
  if (playback) {
    playback->status = SPOTIFY_PLAYBACK_STOPPED;
    playback->position_ms = -1;
    playback->duration_ms = -1;
  }

  DBusError dbus_err;
  dbus_error_init(&dbus_err);
  DBusConnection *conn = dbus_bus_get(DBUS_BUS_SESSION, &dbus_err);
  if (!conn) {
    if (dbus_error_is_set(&dbus_err)) {
      set_err(err, err_cap, dbus_err.message);
      dbus_error_free(&dbus_err);
    } else {
      set_err(err, err_cap, "Failed to connect to session bus");
    }
    return SPOTIFY_ERROR;
  }

  dbus_bool_t has_owner =
      dbus_bus_name_has_owner(conn, "org.mpris.MediaPlayer2.spotify", &dbus_err);
  if (dbus_error_is_set(&dbus_err)) {
    set_err(err, err_cap, dbus_err.message);
    dbus_error_free(&dbus_err);
    dbus_connection_unref(conn);
    return SPOTIFY_ERROR;
  }
  if (!has_owner) {
    dbus_connection_unref(conn);
    return SPOTIFY_NO_SESSION;
  }

  char *status = NULL;
  SpotifyStatus st = get_string_property(conn, "PlaybackStatus", &status, err, err_cap);
  if (st != SPOTIFY_OK) {
    dbus_connection_unref(conn);
    return st;
  }
  SpotifyPlaybackStatus playback_status = SPOTIFY_PLAYBACK_STOPPED;
  if (status) {
    if (strcmp(status, "Playing") == 0) {
      playback_status = SPOTIFY_PLAYBACK_PLAYING;
    } else if (strcmp(status, "Paused") == 0) {
      playback_status = SPOTIFY_PLAYBACK_PAUSED;
    }
  }
  if (playback) {
    playback->status = playback_status;
  }
  free(status);

  if (playback_status == SPOTIFY_PLAYBACK_STOPPED) {
    dbus_connection_unref(conn);
    return SPOTIFY_NO_TRACK;
  }

  int64_t position_us = -1;
  if (get_int64_property(conn, "Position", &position_us, NULL, 0) == SPOTIFY_OK &&
      playback) {
    if (position_us >= 0) {
      playback->position_ms = position_us / 1000;
    }
  }

  int64_t duration_ms = -1;
  st = get_metadata(conn, &out->artist, &out->title, &duration_ms, err, err_cap);
  if (playback && duration_ms > 0) {
    playback->duration_ms = duration_ms;
  }
  dbus_connection_unref(conn);
  if (st != SPOTIFY_OK) {
    spotify_free_track(out);
    return st;
  }
  return SPOTIFY_OK;
}

void spotify_free_track(Track *track) {
  if (!track) {
    return;
  }
  free(track->artist);
  free(track->title);
  track->artist = NULL;
  track->title = NULL;
}
