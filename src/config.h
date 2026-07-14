#ifndef CONFIG_H
#define CONFIG_H

#define VERSION "1.0.2"
#define DEFAULT_PORT 8080
#define DEFAULT_SOUND_DIR "/usr/share/player-client/sounds"
#define DEFAULT_VOLUME 80
#define CONFIG_FILE "/etc/player-client/config.json"
#define BUFFER_SIZE 4096
#define MAX_PATH 1024

typedef struct {
    int port;
    char sound_dir[MAX_PATH];
    int default_volume;
} AppConfig;

// Function declarations
int load_config(AppConfig *config);
int save_config(AppConfig *config);

#endif