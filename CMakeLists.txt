cmake_minimum_required(VERSION 3.10)
project(player-client VERSION 1.0.2 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(LIBNOTIFY REQUIRED libnotify)
pkg_check_modules(JSONC REQUIRED json-c)
pkg_check_modules(LIBMHD REQUIRED libmicrohttpd)
pkg_check_modules(AO REQUIRED ao)
pkg_check_modules(MPG123 REQUIRED libmpg123)

# Include directories
include_directories(
    ${GTK3_INCLUDE_DIRS}
    ${LIBNOTIFY_INCLUDE_DIRS}
    ${JSONC_INCLUDE_DIRS}
    ${LIBMHD_INCLUDE_DIRS}
    ${AO_INCLUDE_DIRS}
    ${MPG123_INCLUDE_DIRS}
    src/
)

# Source files
set(SOURCES
    src/main.c
    src/http_server.c
    src/audio_player.c
    src/notification.c
)

# Create executable
add_executable(player-client ${SOURCES})

# Link libraries
target_link_libraries(player-client
    ${GTK3_LIBRARIES}
    ${LIBNOTIFY_LIBRARIES}
    ${JSONC_LIBRARIES}
    ${LIBMHD_LIBRARIES}
    ${AO_LIBRARIES}
    ${MPG123_LIBRARIES}
    pthread
    m
)

# Installation rules
install(TARGETS player-client DESTINATION /usr/local/bin)
install(DIRECTORY sounds/ DESTINATION /usr/share/player-client/sounds)
install(FILES debian/player-client.service DESTINATION /lib/systemd/system)
install(FILES debian/player-client.desktop DESTINATION /usr/share/applications)