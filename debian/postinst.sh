#!/bin/bash
set -e

case "$1" in
    configure)
        # Create configuration directory
        mkdir -p /etc/notification-client
        mkdir -p /usr/share/notification-client/sounds
        
        # Copy default configuration if not exists
        if [ ! -f /etc/notification-client/config.json ]; then
            cp /usr/share/notification-client/config.json /etc/notification-client/
        fi
        
        # Create default sound if not exists
        if [ ! -f /usr/share/notification-client/sounds/default.wav ]; then
            # Generate a simple beep sound using sox if available
            if command -v sox > /dev/null; then
                sox -n /usr/share/notification-client/sounds/default.wav synth 0.5 sine 800
            fi
        fi
        
        # Reload systemd
        systemctl daemon-reload
        
        # Enable and start service
        systemctl enable notification-client.service
        systemctl start notification-client.service
        
        # Add to startup
        update-rc.d notification-client defaults > /dev/null 2>&1 || true
        ;;
esac

exit 0