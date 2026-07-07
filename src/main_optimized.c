#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <getopt.h>
#include "http_server_optimized.h"
#include "audio_worker.h"

#define VERSION "2.0.0"
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
    printf("  -w, --workers N     Number of worker threads (default: 4)\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -v, --version       Show version information\n");
}

int main(int argc, char *argv[]) {
    int http_port = 8080;
    int daemon_mode = 0;
    int num_workers = 4;
    int max_connections = 100;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"daemon", no_argument, 0, 'd'},
        {"port", required_argument, 0, 'p'},
        {"workers", required_argument, 0, 'w'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "c:dp:w:hv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                // 配置文件路径（忽略）
                break;
            case 'd':
                daemon_mode = 1;
                break;
            case 'p':
                http_port = atoi(optarg);
                break;
            case 'w':
                num_workers = atoi(optarg);
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
    
    // 注册信号
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    
    // 创建HTTP服务器
    http_server_opt_t *server = http_server_opt_create(http_port, num_workers, max_connections);
    if (!server) {
        syslog(LOG_ERR, "Failed to create HTTP server");
        return 1;
    }
    
    syslog(LOG_INFO, "Notification Client v%s started (optimized version)", VERSION);
    
    // 运行服务器
    http_server_opt_run(server);
    
    // 清理
    http_server_opt_destroy(server);
    syslog(LOG_INFO, "Notification client stopped");
    closelog();
    
    return 0;
}