#include "http_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <syslog.h>
#include <json-c/json.h>
#include "audio.h"
#include "config.h"

#define BUFFER_SIZE 8192
#define MAX_HEADERS 64

struct http_server_s {
    int socket_fd;
    int port;
    int running;
};

typedef struct {
    char method[16];
    char path[256];
    char version[16];
    char headers[MAX_HEADERS][256];
    int header_count;
    char body[2048];
    int content_length;
} http_request_t;

typedef struct {
    int status_code;
    char *content_type;
    char *body;
    int body_length;
} http_response_t;

// Parse HTTP request
static int parse_request(char *buffer, http_request_t *req) {
    char *line = strtok(buffer, "\r\n");
    if (!line) return -1;
    
    // Parse request line
    sscanf(line, "%15s %255s %15s", req->method, req->path, req->version);
    
    req->header_count = 0;
    req->content_length = 0;
    
    // Parse headers
    while ((line = strtok(NULL, "\r\n")) != NULL) {
        if (strlen(line) == 0) break;
        if (req->header_count < MAX_HEADERS) {
            strncpy(req->headers[req->header_count], line, 255);
            req->header_count++;
        }
        
        // Check for Content-Length
        if (strncasecmp(line, "Content-Length:", 15) == 0) {
            req->content_length = atoi(line + 15);
        }
    }
    
    // Parse body if present
    if (req->content_length > 0 && req->content_length < sizeof(req->body)) {
        char *body_start = strstr(buffer, "\r\n\r\n");
        if (body_start) {
            body_start += 4;
            memcpy(req->body, body_start, req->content_length);
            req->body[req->content_length] = '\0';
        }
    }
    
    return 0;
}

// Send HTTP response
static void send_response(int client_fd, http_response_t *resp) {
    char header[512];
    snprintf(header, sizeof(header),
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n",
            resp->status_code,
            resp->status_code == 200 ? "OK" :
            resp->status_code == 400 ? "Bad Request" :
            resp->status_code == 404 ? "Not Found" :
            "Internal Server Error",
            resp->content_type ? resp->content_type : "application/json",
            resp->body ? strlen(resp->body) : 0);
    
    send(client_fd, header, strlen(header), 0);
    if (resp->body) {
        send(client_fd, resp->body, strlen(resp->body), 0);
    }
}

// Handle API request
static void handle_api_request(int client_fd, const char *path, const char *body) {
    http_response_t resp;
    resp.content_type = "application/json";
    
    json_object *response = json_object_new_object();
    json_object_object_add(response, "timestamp", json_object_new_int64(time(NULL)));
    
    // Parse request body for POST requests
    json_object *request = NULL;
    if (body && strlen(body) > 0) {
        request = json_tokener_parse(body);
    }
    
    if (strcmp(path, "/api/play") == 0) {
        // Play notification sound
        const char *sound_file = "/usr/share/notification-client/sounds/default.wav";
        int force_wakeup = 0;
        
        if (request) {
            json_object *sound_obj, *wakeup_obj;
            if (json_object_object_get_ex(request, "sound", &sound_obj)) {
                sound_file = json_object_get_string(sound_obj);
            }
            if (json_object_object_get_ex(request, "force_wakeup", &wakeup_obj)) {
                force_wakeup = json_object_get_boolean(wakeup_obj);
            }
        }
        
        int result = audio_play(sound_file, force_wakeup);
        
        json_object_object_add(response, "action", json_object_new_string("play"));
        json_object_object_add(response, "success", json_object_new_boolean(result == 0));
        json_object_object_add(response, "sound", json_object_new_string(sound_file));
        json_object_object_add(response, "force_wakeup", json_object_new_boolean(force_wakeup));
        
        if (result == 0) {
            resp.status_code = 200;
        } else {
            resp.status_code = 500;
            json_object_object_add(response, "error", json_object_new_string("Failed to play audio"));
        }
        
    } else if (strcmp(path, "/api/stop") == 0) {
        audio_stop();
        json_object_object_add(response, "action", json_object_new_string("stop"));
        json_object_object_add(response, "success", json_object_new_boolean(1));
        resp.status_code = 200;
        
    } else if (strcmp(path, "/api/volume") == 0) {
        if (request) {
            json_object *vol_obj;
            if (json_object_object_get_ex(request, "volume", &vol_obj)) {
                int volume = json_object_get_int(vol_obj);
                audio_set_volume(volume);
                json_object_object_add(response, "volume", json_object_new_int(volume));
                resp.status_code = 200;
            } else {
                resp.status_code = 400;
                json_object_object_add(response, "error", json_object_new_string("Missing volume parameter"));
            }
        } else {
            // GET request - return current volume
            int volume = audio_get_volume();
            json_object_object_add(response, "volume", json_object_new_int(volume));
            resp.status_code = 200;
        }
        
    } else if (strcmp(path, "/api/status") == 0) {
        int device_available = audio_check_device();
        json_object_object_add(response, "status", json_object_new_string("running"));
        json_object_object_add(response, "device_available", json_object_new_boolean(device_available));
        json_object_object_add(response, "volume", json_object_new_int(audio_get_volume()));
        resp.status_code = 200;
        
    } else if (strcmp(path, "/api/info") == 0) {
        json_object *info = json_object_new_object();
        json_object_object_add(info, "name", json_object_new_string("Notification Client"));
        json_object_object_add(info, "version", json_object_new_string("1.0.0"));
        json_object_object_add(info, "arch", json_object_new_string("mips64el"));
        
        json_object_object_add(response, "info", info);
        resp.status_code = 200;
        
    } else {
        resp.status_code = 404;
        json_object_object_add(response, "error", json_object_new_string("Endpoint not found"));
    }
    
    if (request) {
        json_object_put(request);
    }
    
    resp.body = (char*)json_object_to_json_string(response);
    send_response(client_fd, &resp);
    json_object_put(response);
}

// Client handler thread
static void* client_handler(void *arg) {
    int client_fd = *(int*)arg;
    free(arg);
    
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    
    int bytes = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
    if (bytes > 0) {
        http_request_t req;
        memset(&req, 0, sizeof(req));
        
        if (parse_request(buffer, &req) == 0) {
            if (strcmp(req.method, "GET") == 0 || strcmp(req.method, "POST") == 0) {
                handle_api_request(client_fd, req.path, req.body);
            } else {
                http_response_t resp = {405, "text/plain", "Method Not Allowed", 0};
                send_response(client_fd, &resp);
            }
        }
    }
    
    close(client_fd);
    return NULL;
}

http_server_t* http_server_create(int port) {
    http_server_t *server = malloc(sizeof(http_server_t));
    if (!server) return NULL;
    
    server->port = port;
    server->running = 1;
    
    server->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->socket_fd < 0) {
        perror("socket");
        free(server);
        return NULL;
    }
    
    int opt = 1;
    setsockopt(server->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(server->socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server->socket_fd);
        free(server);
        return NULL;
    }
    
    if (listen(server->socket_fd, 10) < 0) {
        perror("listen");
        close(server->socket_fd);
        free(server);
        return NULL;
    }
    
    syslog(LOG_INFO, "HTTP server listening on port %d", port);
    
    return server;
}

void http_server_destroy(http_server_t *server) {
    if (!server) return;
    
    server->running = 0;
    close(server->socket_fd);
    free(server);
}

void http_server_poll(http_server_t *server, int timeout_ms) {
    if (!server || !server->running) return;
    
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(server->socket_fd, &read_fds);
    
    struct timeval tv = {
        .tv_sec = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000
    };
    
    int ready = select(server->socket_fd + 1, &read_fds, NULL, NULL, &tv);
    if (ready > 0 && FD_ISSET(server->socket_fd, &read_fds)) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server->socket_fd, (struct sockaddr*)&client_addr, &addr_len);
        
        if (client_fd > 0) {
            pthread_t thread;
            int *fd_ptr = malloc(sizeof(int));
            *fd_ptr = client_fd;
            pthread_create(&thread, NULL, client_handler, fd_ptr);
            pthread_detach(thread);
        }
    }
}

int http_server_get_port(http_server_t *server) {
    return server ? server->port : -1;
}