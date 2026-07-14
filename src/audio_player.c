#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ao/ao.h>
#include <mpg123.h>
#include <pthread.h>
#include "audio_player.h"

static ao_device *device = NULL;
static ao_sample_format format;
static int default_driver;
static mpg123_handle *mh = NULL;
static pthread_t play_thread;
static int is_playing = 0;
static int current_volume = 80;

void init_audio_player() {
    ao_initialize();
    default_driver = ao_default_driver_id();
    
    mpg123_init();
    mh = mpg123_new(NULL, NULL);
    
    printf("Audio player initialized\n");
}

void cleanup_audio_player() {
    stop_audio();
    
    if (mh != NULL) {
        mpg123_close(mh);
        mpg123_delete(mh);
    }
    mpg123_exit();
    ao_shutdown();
    
    printf("Audio player cleaned up\n");
}

void set_volume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    current_volume = volume;
}

void *play_thread_func(void *arg) {
    char *file_path = (char *)arg;
    unsigned char buffer[4096];
    size_t done;
    
    // Try MP3 first
    if (mpg123_open(mh, file_path) == MPG123_OK) {
        long rate;
        int channels, encoding;
        
        mpg123_getformat(mh, &rate, &channels, &encoding);
        
        format.bits = mpg123_encsize(encoding) * 8;
        format.rate = rate;
        format.channels = channels;
        format.byte_format = AO_FMT_NATIVE;
        format.matrix = NULL;
        
        device = ao_open_live(default_driver, &format, NULL);
        if (device == NULL) {
            fprintf(stderr, "Error opening audio device\n");
            mpg123_close(mh);
            free(file_path);
            is_playing = 0;
            return NULL;
        }
        
        while (mpg123_read(mh, buffer, sizeof(buffer), &done) == MPG123_OK && is_playing) {
            // Apply volume adjustment
            if (current_volume < 100) {
                for (size_t i = 0; i < done; i++) {
                    buffer[i] = (buffer[i] * current_volume) / 100;
                }
            }
            ao_play(device, (char *)buffer, done);
        }
        
        mpg123_close(mh);
        ao_close(device);
    } else {
        // Try WAV fallback
        fprintf(stderr, "Failed to open MP3 file: %s\n", file_path);
    }
    
    free(file_path);
    is_playing = 0;
    return NULL;
}

int play_audio(const char *file_path, int volume) {
    if (is_playing) {
        stop_audio();
        // Wait for thread to finish
        usleep(100000); // 100ms
    }
    
    if (volume >= 0 && volume <= 100) {
        current_volume = volume;
    }
    
    char *path_copy = strdup(file_path);
    if (path_copy == NULL) {
        return -1;
    }
    
    is_playing = 1;
    
    if (pthread_create(&play_thread, NULL, play_thread_func, path_copy) != 0) {
        fprintf(stderr, "Failed to create play thread\n");
        free(path_copy);
        is_playing = 0;
        return -1;
    }
    
    return 0;
}

void stop_audio() {
    is_playing = 0;
    // Give time for thread to cleanup
    usleep(100000);
}

int get_player_status() {
    return is_playing;
}