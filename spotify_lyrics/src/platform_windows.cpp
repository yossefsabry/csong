#include "platform.h"

#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/base.h>

using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession;
using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager;
using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus;

static void set_err(char *err, size_t err_cap, const char *msg) {
  if (!err || err_cap == 0) {
    return;
  }
  snprintf(err, err_cap, "%s", msg);
}

static char *dup_string(const std::string &s) {
  char *out = (char *)malloc(s.size() + 1);
  if (!out) {
    return NULL;
  }
  memcpy(out, s.c_str(), s.size() + 1);
  return out;
}

static std::string to_lower_ascii(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    if (c >= 'A' && c <= 'Z') {
      return static_cast<unsigned char>(c - 'A' + 'a');
    }
    return c;
  });
  return s;
}

static bool contains_spotify(const std::string &value) {
  std::string lowered = to_lower_ascii(value);
  return lowered.find("spotify") != std::string::npos;
}

static bool is_spotify_session(const GlobalSystemMediaTransportControlsSession &session) {
  if (!session) {
    return false;
  }
  std::string aumid = winrt::to_string(session.SourceAppUserModelId());
  return contains_spotify(aumid);
}

static GlobalSystemMediaTransportControlsSession find_spotify_session(
    const GlobalSystemMediaTransportControlsSessionManager &manager) {
  GlobalSystemMediaTransportControlsSession current = manager.GetCurrentSession();
  if (is_spotify_session(current)) {
    return current;
  }
  auto sessions = manager.GetSessions();
  for (auto const &session : sessions) {
    if (is_spotify_session(session)) {
      return session;
    }
  }
  return {};
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

  try {
    static bool initialized = false;
    if (!initialized) {
      winrt::init_apartment(winrt::apartment_type::multi_threaded);
      initialized = true;
    }

    auto manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
    if (!manager) {
      set_err(err, err_cap, "Failed to access media session manager");
      return SPOTIFY_ERROR;
    }

    auto session = find_spotify_session(manager);
    if (!session) {
      return SPOTIFY_NO_SESSION;
    }

    auto playback_info = session.GetPlaybackInfo();
    SpotifyPlaybackStatus playback_status = SPOTIFY_PLAYBACK_STOPPED;
    auto status = playback_info.PlaybackStatus();
    if (status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing) {
      playback_status = SPOTIFY_PLAYBACK_PLAYING;
    } else if (status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused) {
      playback_status = SPOTIFY_PLAYBACK_PAUSED;
    }
    if (playback) {
      playback->status = playback_status;
    }
    if (playback_status == SPOTIFY_PLAYBACK_STOPPED) {
      return SPOTIFY_NO_TRACK;
    }

    auto timeline = session.GetTimelineProperties();
    int64_t position_ms = (int64_t)(timeline.Position().count() / 10000);
    int64_t duration_ms = (int64_t)(timeline.EndTime().count() / 10000);
    if (playback) {
      if (position_ms >= 0) {
        playback->position_ms = position_ms;
      }
      if (duration_ms > 0) {
        playback->duration_ms = duration_ms;
      }
    }

    auto props = session.TryGetMediaPropertiesAsync().get();
    std::string title = winrt::to_string(props.Title());
    std::string artist = winrt::to_string(props.Artist());
    if (title.empty() || artist.empty()) {
      set_err(err, err_cap, "Spotify metadata incomplete");
      return SPOTIFY_ERROR;
    }

    out->title = dup_string(title);
    out->artist = dup_string(artist);
    if (!out->title || !out->artist) {
      spotify_free_track(out);
      set_err(err, err_cap, "Out of memory");
      return SPOTIFY_ERROR;
    }
    return SPOTIFY_OK;
  } catch (const winrt::hresult_error &e) {
    std::string msg = winrt::to_string(e.message());
    set_err(err, err_cap, msg.c_str());
    return SPOTIFY_ERROR;
  } catch (...) {
    set_err(err, err_cap, "Windows media session failed");
    return SPOTIFY_ERROR;
  }
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
