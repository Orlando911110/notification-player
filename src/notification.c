#include <stdio.h>
#include <libnotify/notify.h>
#include "notification.h"

void show_notification(const char *title, const char *message) {
    if (!notify_is_initted()) {
        notify_init("PlayerClient");
    }
    
    NotifyNotification *notification = notify_notification_new(
        title,
        message,
        "audio-volume-high"
    );
    
    notify_notification_set_timeout(notification, 3000); // 3 seconds
    notify_notification_set_app_name(notification, "PlayerClient");
    
    if (!notify_notification_show(notification, NULL)) {
        fprintf(stderr, "Failed to show notification\n");
    }
    
    g_object_unref(G_OBJECT(notification));
}