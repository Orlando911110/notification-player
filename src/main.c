#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <gtk/gtk.h>
#include <libnotify/notify.h>
#include "config.h"
#include "http_server.h"
#include "audio_player.h"
#include "notification.h"

static GtkStatusIcon *tray_icon;
static int running = 1;
AppConfig config;

void signal_handler(int sig) {
    running = 0;
    gtk_main_quit();
}

void on_tray_icon_activate(GtkStatusIcon *status_icon, gpointer user_data) {
    show_notification("PlayerClient", "服务正在运行中...");
}

void on_quit_activate(GtkMenuItem *item, gpointer user_data) {
    running = 0;
    gtk_main_quit();
}

void create_tray_icon() {
    GtkWidget *menu;
    GtkWidget *quit_item;
    
    menu = gtk_menu_new();
    
    quit_item = gtk_menu_item_new_with_label("退出");
    g_signal_connect(G_OBJECT(quit_item), "activate", 
                    G_CALLBACK(on_quit_activate), NULL);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit_item);
    gtk_widget_show_all(menu);
    
    tray_icon = gtk_status_icon_new();
    gtk_status_icon_set_from_icon_name(tray_icon, "audio-volume-high");
    gtk_status_icon_set_tooltip_text(tray_icon, "PlayerClient - 消息通知播放客户端");
    
    g_signal_connect(G_OBJECT(tray_icon), "activate", 
                    G_CALLBACK(on_tray_icon_activate), NULL);
    g_signal_connect(G_OBJECT(tray_icon), "popup-menu", 
                    G_CALLBACK(gtk_status_icon_position_menu), menu);
    
    gtk_status_icon_set_visible(tray_icon, TRUE);
}

void *http_server_thread(void *arg) {
    start_http_server(config.port);
    return NULL;
}

int main(int argc, char *argv[]) {
    pthread_t http_thread;
    
    // Initialize configuration
    memset(&config, 0, sizeof(config));
    config.port = DEFAULT_PORT;
    strcpy(config.sound_dir, DEFAULT_SOUND_DIR);
    config.default_volume = DEFAULT_VOLUME;
    
    // Load configuration file
    load_config(&config);
    
    // Initialize GTK
    gtk_init(&argc, &argv);
    
    // Initialize notification system
    notify_init("PlayerClient");
    
    // Initialize audio system
    init_audio_player();
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create system tray icon
    create_tray_icon();
    
    // Start HTTP server in separate thread
    if (pthread_create(&http_thread, NULL, http_server_thread, NULL) != 0) {
        fprintf(stderr, "Failed to create HTTP server thread\n");
        return 1;
    }
    
    // Show startup notification
    char startup_msg[256];
    snprintf(startup_msg, sizeof(startup_msg), 
             "PlayerClient v%s 已启动\nHTTP API 端口: %d", VERSION, config.port);
    show_notification("PlayerClient", startup_msg);
    
    printf("PlayerClient v%s started\n", VERSION);
    printf("HTTP API listening on port %d\n", config.port);
    printf("Sound directory: %s\n", config.sound_dir);
    
    // Run GTK main loop
    gtk_main();
    
    // Cleanup
    running = 0;
    cleanup_audio_player();
    notify_uninit();
    
    // Wait for HTTP thread to finish
    pthread_join(http_thread, NULL);
    
    return 0;
}