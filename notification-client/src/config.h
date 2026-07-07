#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

typedef struct {
    char default_sound[256];
    int default_volume;
    int http_port;
    char audio_device[128];
    int enable_wakeup;
    char custom_sounds[10][256];
    int custom_sound_count;
} app_config_t;

// 加载配置
int load_config(const char *path);

// 获取配置
app_config_t* get_config(void);

// 保存配置
int save_config(const char *path);

// 设置默认配置
void set_default_config(void);

#endif