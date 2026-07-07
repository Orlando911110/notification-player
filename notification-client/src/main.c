#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <syslog.h>
#include <getopt.h>
#include <json-c/json.h>
#include "http_server.h"
#include "audio.h"
#include "config.h"

#define VERSION "1.0.0"
#define DEFAULT_CONFIG "/etc/notification-client/config.json"

static int running = 1;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        syslog(LOG_INFO, "Received termination signal, shutting down...");
        running = 0;
    }
}

void daemonize() {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    if (pid > 0) {
        exit(0);
    }
    
    umask(0);
    
    if (setsid() < 0) {
        perror("setsid");
        exit(1);
    }
    
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    openlog("notification-client", LOG_PID, LOG_DAEMON);
}

void print_usage(char *prog) {
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("Options:\n");
    printf("  -c, --config FILE   Configuration file path\n");
    printf("  -d, --daemon        Run as daemon\n");
    printf("  -p, --port PORT     HTTP server port (default: 8080)\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -v, --version       Show version information\n");
}

int main(int argc, char *argv[]) {
    char *config_path = DEFAULT_CONFIG;
    int daemon_mode = 0;
    int http_port = 8080;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"daemon", no_argument, 0, 'd'},
        {"port", required_argument, 0, 'p'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "c:dp:hv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                config_path = optarg;
                break;
            case 'd':
                daemon_mode = 1;
                break;
            case 'p':
                http_port = atoi(optarg);
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'v':
                printf("Notification Client v%s\n", VERSION);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    if (daemon_mode) {
        daemonize();
    }
    
    // Initialize audio system
    if (audio_init() < 0) {
        syslog(LOG_ERR, "Failed to initialize audio system");
        return 1;
    }
    
    // Load configuration
    if (load_config(config_path) < 0) {
        syslog(LOG_WARNING, "Using default configuration");
    }
    
    // Register signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    
    // Start HTTP server
    syslog(LOG_INFO, "Starting notification client v%s on port %d", VERSION, http_port);
    
    http_server_t *server = http_server_create(http_port);
    if (!server) {
        syslog(LOG_ERR, "Failed to create HTTP server");
        audio_cleanup();
        return 1;
    }
    
    // Main loop
    while (running) {
        http_server_poll(server, 100);
    }
    
    // Cleanup
    http_server_destroy(server);
    audio_cleanup();
    syslog(LOG_INFO, "Notification client stopped");
    closelog();
    
    return 0;
}