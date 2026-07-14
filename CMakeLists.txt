cmake_minimum_required(VERSION 3.10)
project(player-client VERSION 1.0.2 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set MIPS64 specific flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mabi=64 -static")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")

# Source files
set(SOURCES
    src/main.c
    src/http_server.c
    src/audio_player.c
    src/notification.c
)

# Include directories
include_directories(src/)

# Try to find packages, but don't fail if not found
find_package(PkgConfig QUIET)

# Optionally find GTK3
pkg_check_modules(GTK3 QUIET gtk+-3.0)
if(GTK3_FOUND)
    message(STATUS "Found GTK3")
    add_definitions(-DHAVE_GTK3)
    include_directories(${GTK3_INCLUDE_DIRS})
endif()

# Optionally find other libraries
pkg_check_modules(LIBNOTIFY QUIET libnotify)
if(LIBNOTIFY_FOUND)
    message(STATUS "Found libnotify")
    add_definitions(-DHAVE_LIBNOTIFY)
    include_directories(${LIBNOTIFY_INCLUDE_DIRS})
endif()

pkg_check_modules(JSONC QUIET json-c)
if(JSONC_FOUND)
    message(STATUS "Found json-c")
    include_directories(${JSONC_INCLUDE_DIRS})
endif()

pkg_check_modules(LIBMHD QUIET libmicrohttpd)
if(LIBMHD_FOUND)
    message(STATUS "Found libmicrohttpd")
    include_directories(${LIBMHD_INCLUDE_DIRS})
endif()

pkg_check_modules(AO QUIET ao)
if(AO_FOUND)
    message(STATUS "Found libao")
    include_directories(${AO_INCLUDE_DIRS})
endif()

pkg_check_modules(MPG123 QUIET libmpg123)
if(MPG123_FOUND)
    message(STATUS "Found libmpg123")
    include_directories(${MPG123_INCLUDE_DIRS})
endif()

# Create executable
add_executable(player-client ${SOURCES})

# Link libraries if found
if(GTK3_FOUND)
    target_link_libraries(player-client ${GTK3_LIBRARIES})
endif()

if(LIBNOTIFY_FOUND)
    target_link_libraries(player-client ${LIBNOTIFY_LIBRARIES})
endif()

if(JSONC_FOUND)
    target_link_libraries(player-client ${JSONC_LIBRARIES})
endif()

if(LIBMHD_FOUND)
    target_link_libraries(player-client ${LIBMHD_LIBRARIES})
endif()

if(AO_FOUND)
    target_link_libraries(player-client ${AO_LIBRARIES})
endif()

if(MPG123_FOUND)
    target_link_libraries(player-client ${MPG123_LIBRARIES})
endif()

# Always link pthread and math
target_link_libraries(player-client pthread m)

# Installation rules
install(TARGETS player-client DESTINATION /usr/local/bin)
install(FILES debian/player-client.service DESTINATION /lib/systemd/system)
install(FILES debian/player-client.desktop DESTINATION /usr/share/applications)
