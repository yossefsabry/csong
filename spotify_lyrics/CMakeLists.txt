cmake_minimum_required(VERSION 3.16)
project(spotify_lyrics LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(spotify_lyrics
  src/main.c
  src/lyrics.c
  src/lyrics_ovh.c
  src/lyrics_lrclib.c
  src/json_utils.c
  src/cache.c
  src/normalize.c
)

target_include_directories(spotify_lyrics PRIVATE src)

find_package(CURL REQUIRED)
target_link_libraries(spotify_lyrics PRIVATE CURL::libcurl)

if(WIN32)
  target_sources(spotify_lyrics PRIVATE src/platform_windows.cpp)
  target_compile_definitions(spotify_lyrics PRIVATE SPOTIFY_WINDOWS)
  target_link_libraries(spotify_lyrics PRIVATE windowsapp)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(DBUS REQUIRED dbus-1)
  target_sources(spotify_lyrics PRIVATE src/platform_linux.c)
  target_include_directories(spotify_lyrics PRIVATE ${DBUS_INCLUDE_DIRS})
  target_link_libraries(spotify_lyrics PRIVATE ${DBUS_LIBRARIES})
endif()
