CC = mips64el-linux-gnuabi64-gcc
CXX = mips64el-linux-gnuabi64-g++
AR = mips64el-linux-gnuabi64-ar
STRIP = mips64el-linux-gnuabi64-strip

CFLAGS = -Wall -O2 -march=mips64el -mtune=mips64el -D_GNU_SOURCE -pthread
LDFLAGS = -lpthread -lasound -lpulse

SOURCES = src/main_optimized.c src/audio_worker.c src/http_server_optimized.c
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