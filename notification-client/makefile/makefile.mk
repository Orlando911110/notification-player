CC = gcc
CFLAGS = -Wall -O2 -march=mips64el -mtune=mips64el
LDFLAGS = -lpulse -lasound -ljson-c -lpthread

PREFIX = /usr
BINDIR = $(PREFIX)/bin
SYSCONFDIR = /etc
DATADIR = $(PREFIX)/share/notification-client
SYSTEMDDIR = /lib/systemd/system

SOURCES = src/main.c src/audio.c src/http_server.c src/config.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = notification-client

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)
	
	install -d $(DESTDIR)$(SYSCONFDIR)/notification-client
	install -m 644 config/config.json $(DESTDIR)$(SYSCONFDIR)/notification-client/
	
	install -d $(DESTDIR)$(DATADIR)/sounds
	install -m 644 sounds/default.wav $(DESTDIR)$(DATADIR)/sounds/ 2>/dev/null || true
	
	install -d $(DESTDIR)$(SYSTEMDDIR)
	install -m 644 systemd/notification-client.service $(DESTDIR)$(SYSTEMDDIR)/

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -rf $(DESTDIR)$(SYSCONFDIR)/notification-client
	rm -rf $(DESTDIR)$(DATADIR)
	rm -f $(DESTDIR)$(SYSTEMDDIR)/notification-client.service

clean:
	rm -f $(OBJECTS) $(TARGET)

distclean: clean
	rm -f debian/*.deb debian/*.buildinfo debian/*.changes

.PHONY: all install uninstall clean distclean