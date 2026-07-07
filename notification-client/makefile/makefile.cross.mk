# Cross-compilation Makefile for MIPS64el on Kylin
CC = mips64el-linux-gnuabi64-gcc
CXX = mips64el-linux-gnuabi64-g++
AR = mips64el-linux-gnuabi64-ar
STRIP = mips64el-linux-gnuabi64-strip

CFLAGS = -Wall -O2 -march=mips64el -mtune=mips64el -D_GNU_SOURCE
LDFLAGS = -lpthread

PKG_CONFIG_PATH ?= /usr/lib/mips64el-linux-gnuabi64/pkgconfig
PKG_CONFIG_LIBDIR ?= /usr/lib/mips64el-linux-gnuabi64/pkgconfig

PULSE_LIBS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR) pkg-config --libs libpulse 2>/dev/null || echo "-lpulse")
ALSA_LIBS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR) pkg-config --libs alsa 2>/dev/null || echo "-lasound")
JSON_LIBS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR) pkg-config --libs json-c 2>/dev/null || echo "-ljson-c")

LDFLAGS += $(PULSE_LIBS) $(ALSA_LIBS) $(JSON_LIBS)

SOURCES = src/main.c src/audio.c src/http_server.c src/config.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = notification-client

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: $@"
	@file $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS)
	rm -rf $(TARGET)
	rm -f notification-client_*.deb
	rm -rf package/