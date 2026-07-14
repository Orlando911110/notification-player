#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <microhttpd.h>
#include <json-c/json.h>
#include "http_server.h"
#include "audio_player.h"
#include "notification.h"

static struct MHD_Daemon *daemon = NULL;
extern AppConfig config;

static int answer_to_connection(void *cls, struct MHD_Connection *connection,
                              const char *url, const char *method,
                              const char *version, const char *upload_data,
                              size_t *upload_data_size, void **con_cls) {
    
    const char *response_str;
    int status_code = MHD_HTTP_OK;
    struct MHD_Response *response;
    int ret;
    char response_buffer[1024];
    
    // Handle CORS
    if (strcmp(method, "OPTIONS") == 0) {
        response_str = "";
        response = MHD_create_response_from_buffer(strlen(response_str),
                                                  (void*)response_str,
                                                  MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type");
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    }
    
    // API endpoints
    if (strcmp(url, "/api/play") == 0 && strcmp(method, "POST") == 0) {
        // Parse JSON body to get sound file and volume
        const char *sound_file = "default.wav";
        int volume = config.default_volume;
        
        if (*con_cls == NULL) {
            *con_cls = malloc(1);
            *((int*)*con_cls) = 0;
            return MHD_YES;
        }
        
        if (*upload_data_size > 0) {
            // Parse JSON
            struct json_object *jobj = json_tokener_parse(upload_data);
            if (jobj != NULL) {
                struct json_object *jfile, *jvol;
                if (json_object_object_get_ex(jobj, "file", &jfile)) {
                    sound_file = json_object_get_string(jfile);
                }
                if (json_object_object_get_ex(jobj, "volume", &jvol)) {
                    volume = json_object_get_int(jvol);
                }
                json_object_put(jobj);
            }
            *upload_data_size = 0;
        }
        
        // Play sound
        char sound_path[1024];
        snprintf(sound_path, sizeof(sound_path), "%s/%s", config.sound_dir, sound_file);
        int result = play_audio(sound_path, volume);
        
        if (result == 0) {
            snprintf(response_buffer, sizeof(response_buffer),
                    "{\"status\": \"success\", \"message\": \"播放成功\", \"file\": \"%s\"}", sound_file);
            show_notification("PlayerClient", "音频播放成功");
        } else {
            snprintf(response_buffer, sizeof(response_buffer),
                    "{\"status\": \"error\", \"message\": \"播放失败\"}");
            status_code = MHD_HTTP_INTERNAL_SERVER_ERROR;
        }
        
        response_str = response_buffer;
    }
    else if (strcmp(url, "/api/status") == 0) {
        snprintf(response_buffer, sizeof(response_buffer),
                "{\"status\": \"running\", \"version\": \"%s\", \"port\": %d}", 
                VERSION, config.port);
        response_str = response_buffer;
    }
    else if (strcmp(url, "/api/stop") == 0) {
        stop_audio();
        snprintf(response_buffer, sizeof(response_buffer),
                "{\"status\": \"success\", \"message\": \"停止播放\"}");
        response_str = response_buffer;
    }
    else if (strcmp(url, "/api/volume") == 0 && strcmp(method, "POST") == 0) {
        if (*upload_data_size > 0) {
            struct json_object *jobj = json_tokener_parse(upload_data);
            if (jobj != NULL) {
                struct json_object *jvol;
                if (json_object_object_get_ex(jobj, "volume", &jvol)) {
                    int volume = json_object_get_int(jvol);
                    set_volume(volume);
                    snprintf(response_buffer, sizeof(response_buffer),
                            "{\"status\": \"success\", \"volume\": %d}", volume);
                }
                json_object_put(jobj);
            }
            *upload_data_size = 0;
        }
        response_str = response_buffer;
    }
    else {
        snprintf(response_buffer, sizeof(response_buffer),
                "{\"status\": \"error\", \"message\": \"未知的API端点\"}");
        response_str = response_buffer;
        status_code = MHD_HTTP_NOT_FOUND;
    }
    
    response = MHD_create_response_from_buffer(strlen(response_str),
                                              (void*)response_str,
                                              MHD_RESPMEM_MUST_COPY);
    
    MHD_add_response_header(response, "Content-Type", "application/json");
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    
    ret = MHD_queue_response(connection, status_code, response);
    MHD_destroy_response(response);
    
    return ret;
}

void start_http_server(int port) {
    daemon = MHD_start_daemon(MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD,
                             port, NULL, NULL,
                             &answer_to_connection, NULL,
                             MHD_OPTION_END);
    
    if (daemon == NULL) {
        fprintf(stderr, "Failed to start HTTP server on port %d\n", port);
        return;
    }
    
    printf("HTTP server started on port %d\n", port);
}

void stop_http_server() {
    if (daemon != NULL) {
        MHD_stop_daemon(daemon);
        daemon = NULL;
    }
}