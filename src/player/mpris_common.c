#include "mpris_common.h"

#ifndef _WIN32

#include <dbus/dbus.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_err(char *err, size_t err_cap, const char *msg) {
  if (!err || err_cap == 0) {
    return;
  }
  snprintf(err, err_cap, "%s", msg ? msg : "unknown error");
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

static DBusMessage *get_property_reply(DBusConnection *conn, const char *bus_name,
                                       const char *prop, DBusError *dbus_err) {
  DBusMessage *msg = dbus_message_new_method_call(
      bus_name, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties",
      "Get");
  if (!msg) {
    return NULL;
  }
  const char *iface = "org.mpris.MediaPlayer2.Player";
  if (!dbus_message_append_args(msg, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING,
                                &prop, DBUS_TYPE_INVALID)) {
    dbus_message_unref(msg);
    return NULL;
  }
  DBusMessage *reply =
      dbus_connection_send_with_reply_and_block(conn, msg, 1000, dbus_err);
  dbus_message_unref(msg);
  return reply;
}

static mpris_status get_string_property(DBusConnection *conn, const char *bus_name,
                                        const char *prop, char **out, char *err,
                                        size_t err_cap) {
  DBusError dbus_err;
  dbus_error_init(&dbus_err);
  DBusMessage *reply = get_property_reply(conn, bus_name, prop, &dbus_err);
  if (!reply) {
    if (dbus_error_is_set(&dbus_err)) {
      set_err(err, err_cap, dbus_err.message);
      dbus_error_free(&dbus_err);
    } else {
      set_err(err, err_cap, "Failed to read MPRIS status");
    }
    return MPRIS_ERROR;
  }

  DBusMessageIter iter;
  if (!dbus_message_iter_init(reply, &iter) ||
      dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT) {
    dbus_message_unref(reply);
    set_err(err, err_cap, "Unexpected DBus response");
    return MPRIS_ERROR;
  }
  DBusMessageIter variant;
  dbus_message_iter_recurse(&iter, &variant);
  if (dbus_message_iter_get_arg_type(&variant) != DBUS_TYPE_STRING) {
    dbus_message_unref(reply);
    set_err(err, err_cap, "Unexpected DBus response");
    return MPRIS_ERROR;
  }
  const char *value = NULL;
  dbus_message_iter_get_basic(&variant, &value);
  if (value) {
    *out = dup_string(value);
  }
  dbus_message_unref(reply);
  return value ? MPRIS_OK : MPRIS_ERROR;
}

static mpris_status get_int64_property(DBusConnection *conn, const char *bus_name,
                                       const char *prop, int64_t *out, char *err,
                                       size_t err_cap) {
  DBusError dbus_err;
  dbus_error_init(&dbus_err);
  DBusMessage *reply = get_property_reply(conn, bus_name, prop, &dbus_err);
  if (!reply) {
    if (dbus_error_is_set(&dbus_err)) {
      set_err(err, err_cap, dbus_err.message);
      dbus_error_free(&dbus_err);
    } else {
      set_err(err, err_cap, "Failed to read MPRIS position");
    }
    return MPRIS_ERROR;
  }

  DBusMessageIter iter;
  if (!dbus_message_iter_init(reply, &iter) ||
      dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT) {
    dbus_message_unref(reply);
    set_err(err, err_cap, "Unexpected DBus response");
    return MPRIS_ERROR;
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
    return MPRIS_ERROR;
  }
  dbus_message_unref(reply);
  return MPRIS_OK;
}

static mpris_status get_metadata(DBusConnection *conn, const char *bus_name,
                                 char **artist, char **title, int64_t *duration_ms,
                                 char *err, size_t err_cap) {
  DBusError dbus_err;
  dbus_error_init(&dbus_err);
  DBusMessage *reply = get_property_reply(conn, bus_name, "Metadata", &dbus_err);
  if (!reply) {
    if (dbus_error_is_set(&dbus_err)) {
      set_err(err, err_cap, dbus_err.message);
      dbus_error_free(&dbus_err);
    } else {
      set_err(err, err_cap, "Failed to read MPRIS metadata");
    }
    return MPRIS_ERROR;
  }

  DBusMessageIter iter;
  if (!dbus_message_iter_init(reply, &iter) ||
      dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT) {
    dbus_message_unref(reply);
    set_err(err, err_cap, "Unexpected DBus response");
    return MPRIS_ERROR;
  }

  DBusMessageIter variant;
  dbus_message_iter_recurse(&iter, &variant);
  if (dbus_message_iter_get_arg_type(&variant) != DBUS_TYPE_ARRAY) {
    dbus_message_unref(reply);
    set_err(err, err_cap, "Unexpected DBus response");
    return MPRIS_ERROR;
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
    set_err(err, err_cap, "MPRIS metadata incomplete");
    return MPRIS_ERROR;
  }
  return MPRIS_OK;
}

mpris_status mpris_get_current(const char *bus_name, player_track *out, char *err,
                               size_t err_cap) {
  char *artist = NULL;
  char *title = NULL;
  char *status = NULL;
  int64_t position_us = -1;
  int64_t duration_ms = -1;

  if (!bus_name || !out) {
    set_err(err, err_cap, "Invalid output pointer");
    return MPRIS_ERROR;
  }
  player_track_reset(out);

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
    return MPRIS_ERROR;
  }

  dbus_bool_t has_owner = dbus_bus_name_has_owner(conn, bus_name, &dbus_err);
  if (dbus_error_is_set(&dbus_err)) {
    set_err(err, err_cap, dbus_err.message);
    dbus_error_free(&dbus_err);
    dbus_connection_unref(conn);
    return MPRIS_ERROR;
  }
  if (!has_owner) {
    dbus_connection_unref(conn);
    return MPRIS_NO_SESSION;
  }

  mpris_status st = get_string_property(conn, bus_name, "PlaybackStatus", &status,
                                        err, err_cap);
  if (st != MPRIS_OK) {
    dbus_connection_unref(conn);
    return st;
  }

  if (status) {
    if (strcmp(status, "Playing") == 0) {
      out->is_playing = 1;
      out->is_stopped = 0;
    } else if (strcmp(status, "Paused") == 0) {
      out->is_paused = 1;
      out->is_stopped = 0;
    } else {
      out->is_stopped = 1;
    }
  }
  free(status);

  if (out->is_stopped) {
    dbus_connection_unref(conn);
    return MPRIS_NO_TRACK;
  }

  if (get_int64_property(conn, bus_name, "Position", &position_us, NULL, 0) ==
      MPRIS_OK) {
    if (position_us >= 0) {
      out->elapsed = (double)position_us / 1000000.0;
    }
  }

  st = get_metadata(conn, bus_name, &artist, &title, &duration_ms, err, err_cap);
  if (st != MPRIS_OK) {
    free(artist);
    free(title);
    dbus_connection_unref(conn);
    return st;
  }

  snprintf(out->artist, sizeof(out->artist), "%s", artist ? artist : "");
  snprintf(out->title, sizeof(out->title), "%s", title ? title : "");
  out->duration = duration_ms > 0 ? (double)duration_ms / 1000.0 : 0.0;
  out->has_song = (out->artist[0] != '\0' && out->title[0] != '\0');
  if (!out->has_song) {
    set_err(err, err_cap, "MPRIS metadata incomplete");
    free(artist);
    free(title);
    dbus_connection_unref(conn);
    return MPRIS_ERROR;
  }

  free(artist);
  free(title);
  dbus_connection_unref(conn);
  return MPRIS_OK;
}

#else

mpris_status mpris_get_current(const char *bus_name, player_track *out, char *err,
                               size_t err_cap) {
  (void)bus_name;
  (void)out;
  (void)err;
  (void)err_cap;
  return MPRIS_NO_SESSION;
}

#endif
