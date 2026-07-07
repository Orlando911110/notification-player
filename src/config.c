#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <json-c/json.h>
#include <errno.h>

static app_config_t config;
static int config_loaded = 0;

void set_default_config(void) {
    memset(&config, 0, sizeof(config));
    strcpy(config.default_sound, "/usr/share/notification-client/sounds/default.wav");
    config.default_volume = 80;
    config.http_port = 8080;
    strcpy(config.audio_device, "default");
    config.enable_wakeup = 1;
    config.custom_sound_count = 0;
}

int load_config(const char *path) {
    set_default_config();
    
    FILE *fp = fopen(path, "r");
    if (!fp) {
        syslog(LOG_WARNING, "Config file not found: %s", path);
        return -1;
    }
    
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char *data = malloc(len + 1);
    if (!data) {
        fclose(fp);
        return -1;
    }
    
    fread(data, 1, len, fp);
    data[len] = '\0';
    fclose(fp);
    
    json_object *obj = json_tokener_parse(data);
    free(data);
    
    if (!obj) {
        syslog(LOG_ERR, "Failed to parse config file: %s", path);
        return -1;
    }
    
    json_object *sound_obj, *volume_obj, *port_obj, *device_obj, *wakeup_obj;
    
    if (json_object_object_get_ex(obj, "default_sound", &sound_obj)) {
        strncpy(config.default_sound, json_object_get_string(sound_obj), 255);
    }
    
    if (json_object_object_get_ex(obj, "default_volume", &volume_obj)) {
        config.default_volume = json_object_get_int(volume_obj);
    }
    
    if (json_object_object_get_ex(obj, "http_port", &port_obj)) {
        config.http_port = json_object_get_int(port_obj);
    }
    
    if (json_object_object_get_ex(obj, "audio_device", &device_obj)) {
        strncpy(config.audio_device, json_object_get_string(device_obj), 127);
    }
    
    if (json_object_object_get_ex(obj, "enable_wakeup", &wakeup_obj)) {
        config.enable_wakeup = json_object_get_boolean(wakeup_obj);
    }
    
    // Load custom sounds
    json_object *sounds_obj;
    if (json_object_object_get_ex(obj, "custom_sounds", &sounds_obj)) {
        int count = json_object_array_length(sounds_obj);
        config.custom_sound_count = count > 10 ? 10 : count;
        for (int i = 0; i < config.custom_sound_count; i++) {
            json_object *snd = json_object_array_get_idx(sounds_obj, i);
            if (snd) {
                strncpy(config.custom_sounds[i], json_object_get_string(snd), 255);
            }
        }
    }
    
    json_object_put(obj);
    config_loaded = 1;
    
    syslog(LOG_INFO, "Configuration loaded from %s", path);
    return 0;
}

app_config_t* get_config(void) {
    if (!config_loaded) {
        set_default_config();
        config_loaded = 1;
    }
    return &config;
}

int save_config(const char *path) {
    json_object *obj = json_object_new_object();
    
    json_object_object_add(obj, "default_sound", json_object_new_string(config.default_sound));
    json_object_object_add(obj, "default_volume", json_object_new_int(config.default_volume));
    json_object_object_add(obj, "http_port", json_object_new_int(config.http_port));
    json_object_object_add(obj, "audio_device", json_object_new_string(config.audio_device));
    json_object_object_add(obj, "enable_wakeup", json_object_new_boolean(config.enable_wakeup));
    
    // Save custom sounds
    json_object *sounds_array = json_object_new_array();
    for (int i = 0; i < config.custom_sound_count; i++) {
        json_object_array_add(sounds_array, json_object_new_string(config.custom_sounds[i]));
    }
    json_object_object_add(obj, "custom_sounds", sounds_array);
    
    const char *json_str = json_object_to_json_string(obj);
    
    FILE *fp = fopen(path, "w");
    if (!fp) {
        syslog(LOG_ERR, "Failed to save config: %s", strerror(errno));
        json_object_put(obj);
        return -1;
    }
    
    fprintf(fp, "%s\n", json_str);
    fclose(fp);
    json_object_put(obj);
    
    return 0;
}