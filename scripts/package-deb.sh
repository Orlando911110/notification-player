#!/bin/bash
# Deb package builder for notification-player

set -e

APP_NAME="notification-player"
VERSION=${1:-"1.0.0"}
ARCH="mips64el"
BUILD_DIR="$(pwd)/dist/${APP_NAME}_${VERSION}_${ARCH}"

echo "Building deb package for ${APP_NAME} v${VERSION} (${ARCH})"

# 清理旧的构建
rm -rf dist
mkdir -p "$BUILD_DIR"

# 复制deb文件
cp -r deb/* "$BUILD_DIR/"
mkdir -p "$BUILD_DIR/usr/local/bin"
mkdir -p "$BUILD_DIR/usr/share/doc/${APP_NAME}"
mkdir -p "$BUILD_DIR/usr/share/${APP_NAME}/sounds"

# 复制二进制文件
if [ -f "bin/${APP_NAME}" ]; then
    cp "bin/${APP_NAME}" "$BUILD_DIR/usr/local/bin/"
else
    echo "Error: Binary not found. Run 'make build' first."
    exit 1
fi

# 复制配置文件
cp config.yaml "$BUILD_DIR/etc/${APP_NAME}/"

# 复制文档
cp README.md "$BUILD_DIR/usr/share/doc/${APP_NAME}/"
cp LICENSE "$BUILD_DIR/usr/share/doc/${APP_NAME}/" 2>/dev/null || true

# 复制音频文件
if [ -d "sounds" ]; then
    cp -r sounds/* "$BUILD_DIR/usr/share/${APP_NAME}/sounds/" 2>/dev/null || true
fi

# 创建systemd服务文件
cat > "$BUILD_DIR/lib/systemd/system/${APP_NAME}.service" <<'EOF'
[Unit]
Description=Notification Player Service
After=network.target sound.target

[Service]
Type=simple
User=notification-player
Group=notification-player
WorkingDirectory=/var/lib/notification-player
ExecStart=/usr/local/bin/notification-player -config /etc/notification-player/config.yaml
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

# 设置权限
chmod 755 "$BUILD_DIR/DEBIAN/postinst"
chmod 755 "$BUILD_DIR/DEBIAN/prerm"
chmod 755 "$BUILD_DIR/DEBIAN/postrm"
chmod 755 "$BUILD_DIR/usr/local/bin/${APP_NAME}"

# 更新版本号
sed -i "s/Version: .*/Version: ${VERSION}/" "$BUILD_DIR/DEBIAN/control"

# 计算大小
INSTALLED_SIZE=$(du -sk "$BUILD_DIR" | cut -f1)
sed -i "/^Installed-Size:/d" "$BUILD_DIR/DEBIAN/control"
echo "Installed-Size: ${INSTALLED_SIZE}" >> "$BUILD_DIR/DEBIAN/control"

# 构建deb包
cd dist
dpkg-deb --root-owner-group --build "${APP_NAME}_${VERSION}_${ARCH}"
cd ..

echo "Deb package created: dist/${APP_NAME}_${VERSION}_${ARCH}.deb"
echo "Installed size: ${INSTALLED_SIZE}KB"