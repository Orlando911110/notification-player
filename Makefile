.PHONY: all clean install uninstall deb

CC = gcc
CFLAGS = -Wall -Wextra -O2 `pkg-config --cflags gtk+-3.0 libnotify json-c libmicrohttpd ao libmpg123`
LDFLAGS = `pkg-config --libs gtk+-3.0 libnotify json-c libmicrohttpd ao libmpg123` -lpthread -lm

SRCDIR = src
OBJDIR = obj
BINDIR = bin

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))
TARGET = $(BINDIR)/player-client

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BINDIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -rf $(OBJDIR) $(BINDIR)

install: $(TARGET)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/
	install -d $(DESTDIR)/usr/share/player-client/sounds
	install -d $(DESTDIR)/etc/player-client
	echo '{"port": 8080, "sound_dir": "/usr/share/player-client/sounds", "default_volume": 80}' > $(DESTDIR)/etc/player-client/config.json
	install -d $(DESTDIR)/lib/systemd/system
	install -m 644 debian/player-client.service $(DESTDIR)/lib/systemd/system/
	install -d $(DESTDIR)/usr/share/applications
	install -m 644 debian/player-client.desktop $(DESTDIR)/usr/share/applications/

deb: $(TARGET)
	dpkg-buildpackage -b -us -uc -a mips64

test:
	$(TARGET) --test