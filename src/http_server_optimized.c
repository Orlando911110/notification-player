#include "http_server_optimized.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <poll.h>
#include "audio_worker.h"

#define MAX_EVENTS 64
#define BUFFER_SIZE 8192
#define READ_TIMEOUT_MS 5000
#define MAX_REQUEST_SIZE 65536

typedef struct {
    int fd;
    char read_buffer[BUFFER_SIZE];
    char write_buffer[BUFFER_SIZE];
    size_t read_len;
    size_t write_len;
    size_t write_sent;
    int keep_alive;
    struct sockaddr_in addr;
    time_t last_active;
    int state; // 0: reading, 1: writing, 2: closed
} client_connection_t;

typedef struct {
    int epoll_fd;
    int server_fd;
    int port;
    int running;
    int max_connections;
    int current_connections;
    pthread_t *worker_threads;
    int worker_count;
    pthread_mutex_t conn_mutex;
    pthread_cond_t conn_cond;
    client_connection_t **connections;
    int conn_capacity;
} http_server_opt_t;

// HTTP请求结构
typedef struct {
    char method[16];
    char path[256];
    char version[16];
    int content_length;
    char body[MAX_REQUEST_SIZE];
    int body_read;
} http_request_t;

// 线程安全的队列
typedef struct task_s {
    int client_fd;
    struct task_s *next;
} task_t;

static task_t *task_queue_head = NULL;
static task_t *task_queue_tail = NULL;
static pthread_mutex_t task_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t task_cond = PTHREAD_COND_INITIALIZER;

static void enqueue_task(int client_fd) {
    task_t *task = malloc(sizeof(task_t));
    if (!task) {
        close(client_fd);
        return;
    }
    task->client_fd = client_fd;
    task->next = NULL;
    
    pthread_mutex_lock(&task_mutex);
    if (task_queue_tail) {
        task_queue_tail->next = task;
    } else {
        task_queue_head = task;
    }
    task_queue_tail = task;
    pthread_cond_signal(&task_cond);
    pthread_mutex_unlock(&task_mutex);
}

static int dequeue_task(void) {
    pthread_mutex_lock(&task_mutex);
    while (!task_queue_head) {
        pthread_cond_wait(&task_cond, &task_mutex);
    }
    task_t *task = task_queue_head;
    task_queue_head = task->next;
    if (!task_queue_head) {
        task_queue_tail = NULL;
    }
    int fd = task->client_fd;
    free(task);
    pthread_mutex_unlock(&task_mutex);
    return fd;
}

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int read_all(int fd, char *buffer, size_t *len, size_t max_len, int timeout_ms) {
    struct pollfd pfd = {.fd = fd, .events = POLLIN};
    size_t total = 0;
    
    while (total < max_len) {
        int ret = poll(&pfd, 1, timeout_ms);
        if (ret < 0) {
            return -1;
        }
        if (ret == 0) {
            return -1; // 超时
        }
        
        ssize_t n = read(fd, buffer + total, max_len - total);
        if (n <= 0) {
            return n < 0 ? -1 : 0;
        }
        total += n;
        
        // 检查是否收到完整头部
        if (total >= 4 && strstr(buffer, "\r\n\r\n")) {
            break;
        }
    }
    
    *len = total;
    return 0;
}

static int write_all(int fd, const char *buffer, size_t len, int timeout_ms) {
    size_t sent = 0;
    struct pollfd pfd = {.fd = fd, .events = POLLOUT};
    
    while (sent < len) {
        int ret = poll(&pfd, 1, timeout_ms);
        if (ret < 0) {
            return -1;
        }
        if (ret == 0) {
            return -1; // 超时
        }
        
        ssize_t n = write(fd, buffer + sent, len - sent);
        if (n <= 0) {
            return -1;
        }
        sent += n;
    }
    
    return 0;
}

static void handle_request(client_connection_t *conn) {
    http_request_t req = {0};
    char *body_start;
    
    // 解析请求
    char *line = strtok(conn->read_buffer, "\r\n");
    if (!line) {
        goto error;
    }
    
    sscanf(line, "%15s %255s %15s", req.method, req.path, req.version);
    
    // 解析头部
    while ((line = strtok(NULL, "\r\n")) != NULL) {
        if (strlen(line) == 0) break;
        if (strncasecmp(line, "Content-Length:", 15) == 0) {
            req.content_length = atoi(line + 15);
        }
    }
    
    // 读取body
    body_start = strstr(conn->read_buffer, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        req.body_read = conn->read_len - (body_start - conn->read_buffer);
        if (req.body_read > 0) {
            memcpy(req.body, body_start, req.body_read);
            req.body[req.body_read] = '\0';
        }
    }
    
    // 处理请求
    char response[2048] = {0};
    int status = 200;
    
    if (strcmp(req.path, "/api/play") == 0) {
        const char *sound = "/usr/share/notification-client/sounds/default.wav";
        int force = strstr(req.body, "force_wakeup") != NULL;
        
        // 异步播放，立即返回
        int result = audio_play_async(sound, force);
        snprintf(response, sizeof(response),
                "{\"action\":\"play\",\"success\":%s,\"async\":true}",
                result == 0 ? "true" : "false");
        status = result == 0 ? 200 : 500;
        
    } else if (strcmp(req.path, "/api/stop") == 0) {
        audio_stop_async();
        snprintf(response, sizeof(response),
                "{\"action\":\"stop\",\"success\":true}");
                
    } else if (strcmp(req.path, "/api/volume") == 0) {
        if (req.body_read > 0) {
            char *vol_str = strstr(req.body, "volume");
            if (vol_str) {
                int vol = atoi(vol_str + 7);
                audio_set_volume_async(vol);
                snprintf(response, sizeof(response),
                        "{\"volume\":%d,\"success\":true}", vol);
            }
        } else {
            int vol = audio_get_volume_sync();
            snprintf(response, sizeof(response),
                    "{\"volume\":%d}", vol);
        }
        
    } else if (strcmp(req.path, "/api/status") == 0) {
        int device = audio_check_device_sync();
        snprintf(response, sizeof(response),
                "{\"status\":\"running\",\"device_available\":%s,\"volume\":%d}",
                device ? "true" : "false", audio_get_volume_sync());
                
    } else if (strcmp(req.path, "/api/info") == 0) {
        snprintf(response, sizeof(response),
                "{\"name\":\"Notification Client\",\"version\":\"1.0.0\",\"arch\":\"mips64el\",\"async\":true}");
                
    } else {
        status = 404;
        snprintf(response, sizeof(response),
                "{\"error\":\"Endpoint not found\"}");
    }
    
    // 发送响应
    char response_header[512];
    snprintf(response_header, sizeof(response_header),
            "HTTP/1.1 %d OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n",
            status, strlen(response));
    
    char full_response[4096];
    snprintf(full_response, sizeof(full_response), "%s%s", response_header, response);
    
    write_all(conn->fd, full_response, strlen(full_response), 5000);
    
error:
    close(conn->fd);
    free(conn);
}

static void* worker_thread(void *arg) {
    http_server_opt_t *server = (http_server_opt_t*)arg;
    
    while (server->running) {
        int fd = dequeue_task();
        if (fd < 0) continue;
        
        client_connection_t *conn = malloc(sizeof(client_connection_t));
        if (!conn) {
            close(fd);
            continue;
        }
        
        memset(conn, 0, sizeof(client_connection_t));
        conn->fd = fd;
        conn->last_active = time(NULL);
        
        // 读取请求
        if (read_all(fd, conn->read_buffer, &conn->read_len, 
                     sizeof(conn->read_buffer) - 1, READ_TIMEOUT_MS) == 0) {
            handle_request(conn);
        } else {
            close(fd);
            free(conn);
        }
    }
    
    return NULL;
}

http_server_opt_t* http_server_opt_create(int port, int max_workers, int max_connections) {
    if (max_workers <= 0) max_workers = 4;
    if (max_connections <= 0) max_connections = 100;
    
    http_server_opt_t *server = calloc(1, sizeof(http_server_opt_t));
    if (!server) {
        return NULL;
    }
    
    server->port = port;
    server->max_connections = max_connections;
    server->worker_count = max_workers;
    server->running = 0;
    server->current_connections = 0;
    
    pthread_mutex_init(&server->conn_mutex, NULL);
    pthread_cond_init(&server->conn_cond, NULL);
    
    server->connections = malloc(sizeof(client_connection_t*) * max_connections);
    server->conn_capacity = max_connections;
    
    // 创建socket
    server->server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server->server_fd < 0) {
        free(server->connections);
        free(server);
        return NULL;
    }
    
    int opt = 1;
    setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(server->server_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(server->server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server->server_fd);
        free(server->connections);
        free(server);
        return NULL;
    }
    
    if (listen(server->server_fd, max_connections) < 0) {
        close(server->server_fd);
        free(server->connections);
        free(server);
        return NULL;
    }
    
    // 初始化音频工作线程
    audio_worker_init(2);
    
    // 创建工作线程
    server->worker_threads = malloc(sizeof(pthread_t) * max_workers);
    for (int i = 0; i < max_workers; i++) {
        pthread_create(&server->worker_threads[i], NULL, worker_thread, server);
    }
    
    syslog(LOG_INFO, "HTTP server created on port %d with %d workers", port, max_workers);
    return server;
}

void http_server_opt_destroy(http_server_opt_t *server) {
    if (!server) return;
    
    server->running = 0;
    
    // 等待所有工作线程结束
    for (int i = 0; i < server->worker_count; i++) {
        pthread_join(server->worker_threads[i], NULL);
    }
    
    close(server->server_fd);
    
    pthread_mutex_destroy(&server->conn_mutex);
    pthread_cond_destroy(&server->conn_cond);
    
    free(server->connections);
    free(server->worker_threads);
    free(server);
    
    audio_worker_cleanup();
    
    syslog(LOG_INFO, "HTTP server destroyed");
}

void http_server_opt_run(http_server_opt_t *server) {
    if (!server) return;
    
    server->running = 1;
    
    struct epoll_event ev, events[MAX_EVENTS];
    int epoll_fd = epoll_create1(0);
    
    ev.events = EPOLLIN;
    ev.data.fd = server->server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server->server_fd, &ev);
    
    syslog(LOG_INFO, "HTTP server running on port %d", server->port);
    
    while (server->running) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 100);
        
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server->server_fd) {
                // 新连接
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int client_fd = accept4(server->server_fd, 
                                        (struct sockaddr*)&client_addr, 
                                        &addr_len, SOCK_NONBLOCK);
                
                if (client_fd < 0) continue;
                
                pthread_mutex_lock(&server->conn_mutex);
                if (server->current_connections < server->max_connections) {
                    server->current_connections++;
                    // 将任务加入队列
                    enqueue_task(client_fd);
                } else {
                    // 拒绝连接
                    close(client_fd);
                }
                pthread_mutex_unlock(&server->conn_mutex);
            }
        }
    }
    
    close(epoll_fd);
    syslog(LOG_INFO, "HTTP server stopped");
}

void http_server_opt_stop(http_server_opt_t *server) {
    if (server) {
        server->running = 0;
    }
}