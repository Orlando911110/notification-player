#!/bin/bash
set -e

case "$1" in
    remove)
        # Stop and disable service
        systemctl stop notification-client.service
        systemctl disable notification-client.service
        ;;
esac

exit 0