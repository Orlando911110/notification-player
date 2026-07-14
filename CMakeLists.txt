cmake_minimum_required(VERSION 3.10)
project(player-client VERSION 1.0.2 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# MIPS64 specific
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mabi=64 -Wall -Wextra -O2")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -lpthread")

# Source files
set(SOURCES
    src/main.c
)

# Include directories
include_directories(src/)

# Create executable
add_executable(player-client ${SOURCES})

# Link with pthread
target_link_libraries(player-client pthread)

# Installation
install(TARGETS player-client DESTINATION /usr/local/bin)
install(FILES debian/player-client.service DESTINATION /lib/systemd/system)
