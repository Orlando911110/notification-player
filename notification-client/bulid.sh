#!/bin/bash

set -e

# Build the binary
make clean
make

# Create debian directory structure
mkdir -p debian/tmp
mkdir -p debian/usr/bin
mkdir -p debian/etc/notification-client
mkdir -p debian/usr/share/notification-client/sounds
mkdir -p debian/lib/systemd/system
mkdir -p debian/DEBIAN

# Copy files
cp notification-client debian/usr/bin/
cp config/config.json debian/etc/notification-client/
cp systemd/notification-client.service debian/lib/systemd/system/

# Create a default sound file (8-bit WAV)
cat > debian/usr/share/notification-client/sounds/default.wav << 'EOF'
# This would be a binary WAV file in production
# For now, it's a placeholder
EOF

# Copy control files
cp debian/control debian/DEBIAN/
cp debian/postinst debian/DEBIAN/
cp debian/prerm debian/DEBIAN/
chmod 755 debian/DEBIAN/postinst
chmod 755 debian/DEBIAN/prerm

# Build the package
dpkg-deb --build debian notification-client_1.0.0_mips64el.deb

echo "Package built: notification-client_1.0.0_mips64el.deb"