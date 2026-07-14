#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>

#define VERSION "1.0.2"
#define DEFAULT_PORT 8080
#define BUFFER_SIZE 4096
#define MAX_PATH 1024
#define MAX_BODY 16384

typedef struct {
    int port;
    char sound_dir[MAX_PATH];
    int default_volume;
    volatile int running;
} AppConfig;

static AppConfig config;

// Signal handler
static void signal_handler(int sig) {
    (void)sig;
    config.running = 0;
    printf("\nReceived signal, shutting down...\n");
}

// URL decode function
static void url_decode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && 
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= 'A' - 10;
            else a -= '0';
            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= 'A' - 10;
            else b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

// Simple JSON string value extractor
static int json_get_string(const char *json, const char *key, char *value, size_t max_len) {
    char search[512];
    snprintf(search, sizeof(search), "\"%s\"", key);
    
    const char *p = strstr(json, search);
    if (!p) return -1;
    
    p = strchr(p + strlen(search), ':');
    if (!p) return -1;
    
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    
    if (*p == '"') {
        p++;
        size_t i = 0;
        while (*p && *p != '"' && i < max_len - 1) {
            if (*p == '\\' && *(p+1)) {
                p++;
                switch (*p) {
                    case '"': value[i++] = '"'; break;
                    case '\\': value[i++] = '\\'; break;
                    case '/': value[i++] = '/'; break;
                    case 'n': value[i++] = '\n'; break;
                    case 't': value[i++] = '\t'; break;
                    default: value[i++] = *p; break;
                }
            } else {
                value[i++] = *p;
            }
            p++;
        }
        value[i] = '\0';
        return 0;
    }
    
    return -1;
}

// Simple JSON number extractor
static int json_get_int(const char *json, const char *key, int *value) {
    char search[512];
    snprintf(search, sizeof(search), "\"%s\"", key);
    
    const char *p = strstr(json, search);
    if (!p) return -1;
    
    p = strchr(p + strlen(search), ':');
    if (!p) return -1;
    
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    
    if (*p >= '0' && *p <= '9' || *p == '-') {
        *value = atoi(p);
        return 0;
    }
    
    return -1;
}

// Send HTTP response
static void send_response(int client_fd, int status_code, const char *status_text, 
                         const char *content_type, const char *body) {
    char header[BUFFER_SIZE];
    size_t body_len = strlen(body);
    
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Server: PlayerClient/%s\r\n"
        "Content-Type: %s; charset=utf-8\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "Connection: close\r\n"
        "Content-Length: %zu\r\n"
        "\r\n",
        status_code, status_text, VERSION, content_type, body_len);
    
    send(client_fd, header, header_len, 0);
    send(client_fd, body, body_len, 0);
}

// Parse HTTP request
static void parse_request(const char *request, char *method, char *url, char *body) {
    // Initialize
    method[0] = '\0';
    url[0] = '\0';
    body[0] = '\0';
    
    // Parse first line
    const char *line_end = strstr(request, "\r\n");
    if (!line_end) return;
    
    sscanf(request, "%15s %1023s", method, url);
    
    // Find body
    const char *body_start = strstr(request, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        // Check for Content-Length
        const char *cl = strcasestr(request, "Content-Length:");
        if (cl) {
            int content_length = atoi(cl + 15);
            strncpy(body, body_start, content_length);
            body[content_length] = '\0';
        } else {
            strncpy(body, body_start, MAX_BODY - 1);
            body[MAX_BODY - 1] = '\0';
        }
    }
}

// Route and handle API requests
static void handle_request(int client_fd, const char *method, const char *url, const char *body) {
    char decoded_url[1024];
    url_decode(decoded_url, url);
    
    // CORS preflight
    if (strcmp(method, "OPTIONS") == 0) {
        send_response(client_fd, 204, "No Content", "text/plain", "");
        return;
    }
    
    // API: GET /api/status
    if (strcmp(decoded_url, "/api/status") == 0) {
        time_t now = time(NULL);
        char json[1024];
        snprintf(json, sizeof(json),
            "{"
            "\"status\":\"running\","
            "\"version\":\"%s\","
            "\"port\":%d,"
            "\"sound_dir\":\"%s\","
            "\"default_volume\":%d,"
            "\"timestamp\":%ld"
            "}",
            VERSION, config.port, config.sound_dir, config.default_volume, (long)now);
        send_response(client_fd, 200, "OK", "application/json", json);
        return;
    }
    
    // API: POST /api/play
    if (strcmp(decoded_url, "/api/play") == 0 && strcmp(method, "POST") == 0) {
        char file[256] = "default.wav";
        int volume = config.default_volume;
        
        json_get_string(body, "file", file, sizeof(file));
        json_get_int(body, "volume", &volume);
        
        // Validate volume
        if (volume < 0) volume = 0;
        if (volume > 100) volume = 100;
        
        // Validate filename (prevent directory traversal)
        if (strchr(file, '/') || strchr(file, '\\') || strstr(file, "..")) {
            send_response(client_fd, 400, "Bad Request", "application/json",
                "{\"status\":\"error\",\"message\":\"Invalid filename\"}");
            return;
        }
        
        // Build full path
        char sound_path[MAX_PATH];
        snprintf(sound_path, sizeof(sound_path), "%s/%s", config.sound_dir, file);
        
        // Check if file exists
        if (access(sound_path, F_OK) != 0) {
            char json[512];
            snprintf(json, sizeof(json),
                "{\"status\":\"error\",\"message\":\"File not found: %s\"}", file);
            send_response(client_fd, 404, "Not Found", "application/json", json);
            return;
        }
        
        // Try to play using system commands
        char command[1024];
        int played = 0;
        
        // Try aplay (ALSA)
        snprintf(command, sizeof(command), "aplay -q \"%s\" > /dev/null 2>&1 &", sound_path);
        if (system(command) == 0) played = 1;
        
        // Try paplay (PulseAudio)
        if (!played) {
            snprintf(command, sizeof(command), "paplay \"%s\" > /dev/null 2>&1 &", sound_path);
            if (system(command) == 0) played = 1;
        }
        
        // Try play (SoX)
        if (!played) {
            float vol_factor = volume / 100.0;
            snprintf(command, sizeof(command), "play -q \"%s\" vol %.2f > /dev/null 2>&1 &", 
                    sound_path, vol_factor);
            if (system(command) == 0) played = 1;
        }
        
        char json[512];
        if (played) {
            snprintf(json, sizeof(json),
                "{\"status\":\"success\",\"message\":\"Playback started\",\"file\":\"%s\",\"volume\":%d}",
                file, volume);
            send_response(client_fd, 200, "OK", "application/json", json);
        } else {
            send_response(client_fd, 500, "Internal Server Error", "application/json",
                "{\"status\":\"error\",\"message\":\"No audio player available\"}");
        }
        return;
    }
    
    // API: POST /api/stop
    if (strcmp(decoded_url, "/api/stop") == 0) {
        system("pkill -f 'aplay.*player-client' 2>/dev/null");
        system("pkill -f 'paplay.*player-client' 2>/dev/null");
        system("pkill -f 'play.*player-client' 2>/dev/null");
        
        send_response(client_fd, 200, "OK", "application/json",
            "{\"status\":\"success\",\"message\":\"Playback stopped\"}");
        return;
    }
    
    // API: POST /api/volume
    if (strcmp(decoded_url, "/api/volume") == 0 && strcmp(method, "POST") == 0) {
        int volume = config.default_volume;
        json_get_int(body, "volume", &volume);
        
        if (volume < 0) volume = 0;
        if (volume > 100) volume = 100;
        
        config.default_volume = volume;
        
        // Try to set system volume
        char command[256];
        snprintf(command, sizeof(command), "amixer set Master %d%% > /dev/null 2>&1", volume);
        system(command);
        
        char json[256];
        snprintf(json, sizeof(json),
            "{\"status\":\"success\",\"volume\":%d}", volume);
        send_response(client_fd, 200, "OK", "application/json", json);
        return;
    }
    
    // 404 for unknown endpoints
    send_response(client_fd, 404, "Not Found", "application/json",
        "{\"status\":\"error\",\"message\":\"Endpoint not found\"}");
}

// HTTP server thread
static void *http_server_thread(void *arg) {
    (void)arg;
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        fprintf(stderr, "Failed to create socket: %s\n", strerror(errno));
        return NULL;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        fprintf(stderr, "Failed to set socket options: %s\n", strerror(errno));
        close(server_fd);
        return NULL;
    }
    
    // Bind
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(config.port);
    
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Failed to bind to port %d: %s\n", config.port, strerror(errno));
        close(server_fd);
        return NULL;
    }
    
    // Listen
    if (listen(server_fd, SOMAXCONN) < 0) {
        fprintf(stderr, "Failed to listen: %s\n", strerror(errno));
        close(server_fd);
        return NULL;
    }
    
    printf("✓ HTTP server started on http://0.0.0.0:%d\n", config.port);
    printf("  API endpoints:\n");
    printf("    GET  /api/status  - Get server status\n");
    printf("    POST /api/play    - Play audio file\n");
    printf("    GET  /api/stop    - Stop playback\n");
    printf("    POST /api/volume  - Set volume\n");
    printf("\n");
    
    // Accept connections
    while (config.running) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (config.running) {
                fprintf(stderr, "Accept failed: %s\n", strerror(errno));
            }
            continue;
        }
        
        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        
        // Read request
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            
            char method[16], url[1024], body[MAX_BODY];
            parse_request(buffer, method, url, body);
            
            // Log request
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
            printf("[%s] %s %s\n", client_ip, method, url);
            
            handle_request(client_fd, method, url, body);
        }
        
        close(client_fd);
    }
    
    close(server_fd);
    printf("HTTP server stopped.\n");
    return NULL;
}

// Load configuration from file
static int load_config(const char *config_file) {
    FILE *fp = fopen(config_file, "r");
    if (!fp) {
        printf("No config file found, using defaults\n");
        return -1;
    }
    
    char buffer[4096];
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, fp);
    buffer[len] = '\0';
    fclose(fp);
    
    json_get_int(buffer, "port", &config.port);
    json_get_string(buffer, "sound_dir", config.sound_dir, sizeof(config.sound_dir));
    json_get_int(buffer, "default_volume", &config.default_volume);
    
    return 0;
}

// Print usage
static void print_usage(const char *prog) {
    printf("PlayerClient v%s - HTTP Audio Playback Server\n\n", VERSION);
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  -p, --port PORT       Set HTTP port (default: %d)\n", DEFAULT_PORT);
    printf("  -c, --config FILE     Set config file path\n");
    printf("  -d, --sound-dir DIR   Set sound directory\n");
    printf("  -v, --version         Show version\n");
    printf("  -h, --help            Show this help\n");
    printf("\n");
    printf("API Endpoints:\n");
    printf("  GET  /api/status      Get server status\n");
    printf("  POST /api/play        Play audio (body: {\"file\":\"name.wav\",\"volume\":80})\n");
    printf("  GET  /api/stop        Stop audio playback\n");
    printf("  POST /api/volume      Set volume (body: {\"volume\":75})\n");
}

int main(int argc, char *argv[]) {
    pthread_t http_thread;
    
    // Set defaults
    memset(&config, 0, sizeof(config));
    config.port = DEFAULT_PORT;
    strcpy(config.sound_dir, "/usr/share/player-client/sounds");
    config.default_volume = 80;
    config.running = 1;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("PlayerClient v%s\n", VERSION);
            return 0;
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) config.port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc) load_config(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--sound-dir") == 0) {
            if (i + 1 < argc) strncpy(config.sound_dir, argv[++i], sizeof(config.sound_dir) - 1);
        }
    }
    
    // Try to load default config
    load_config("/etc/player-client/config.json");
    
    printf("╔══════════════════════════════════════╗\n");
    printf("║      PlayerClient v%s            ║\n", VERSION);
    printf("║      HTTP Audio Playback Server      ║\n");
    printf("╚══════════════════════════════════════╝\n");
    printf("\n");
    printf("Configuration:\n");
    printf("  Port:          %d\n", config.port);
    printf("  Sound dir:     %s\n", config.sound_dir);
    printf("  Default volume: %d%%\n", config.default_volume);
    printf("\n");
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create sound directory if it doesn't exist
    mkdir(config.sound_dir, 0755);
    
    // Start HTTP server
    if (pthread_create(&http_thread, NULL, http_server_thread, NULL) != 0) {
        fprintf(stderr, "Failed to create HTTP server thread\n");
        return 1;
    }
    
    // Wait for shutdown signal
    while (config.running) {
        sleep(1);
    }
    
    // Cleanup
    printf("Shutting down...\n");
    pthread_join(http_thread, NULL);
    printf("PlayerClient stopped.\n");
    
    return 0;
}
