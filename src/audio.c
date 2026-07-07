#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <pulse/pulseaudio.h>
#include <alsa/asoundlib.h>
#include <syslog.h>

static pa_context *pa_ctx = NULL;
static pa_mainloop *pa_ml = NULL;
static int audio_initialized = 0;
static int current_volume = 80;
static pthread_mutex_t audio_mutex = PTHREAD_MUTEX_INITIALIZER;
static pa_stream *current_stream = NULL;

// PulseAudio callback
static void pa_context_state_cb(pa_context *c, void *userdata) {
    pa_context_state_t state = pa_context_get_state(c);
    if (state == PA_CONTEXT_READY) {
        syslog(LOG_INFO, "PulseAudio context ready");
    } else if (state == PA_CONTEXT_FAILED) {
        syslog(LOG_ERR, "PulseAudio connection failed");
    }
}

static void pa_stream_state_cb(pa_stream *s, void *userdata) {
    // Stream state callback
}

static void pa_stream_write_cb(pa_stream *s, size_t length, void *userdata) {
    // Write callback
}

int audio_init(void) {
    pthread_mutex_lock(&audio_mutex);
    
    if (audio_initialized) {
        pthread_mutex_unlock(&audio_mutex);
        return 0;
    }
    
    // Initialize PulseAudio
    pa_ml = pa_mainloop_new();
    if (!pa_ml) {
        syslog(LOG_ERR, "Failed to create PulseAudio mainloop");
        pthread_mutex_unlock(&audio_mutex);
        return -1;
    }
    
    pa_mainloop_api *api = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(api, "notification-client");
    if (!pa_ctx) {
        syslog(LOG_ERR, "Failed to create PulseAudio context");
        pa_mainloop_free(pa_ml);
        pthread_mutex_unlock(&audio_mutex);
        return -1;
    }
    
    pa_context_set_state_callback(pa_ctx, pa_context_state_cb, NULL);
    
    if (pa_context_connect(pa_ctx, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0) {
        syslog(LOG_ERR, "Failed to connect to PulseAudio server");
        pa_context_unref(pa_ctx);
        pa_mainloop_free(pa_ml);
        pthread_mutex_unlock(&audio_mutex);
        return -1;
    }
    
    audio_initialized = 1;
    pthread_mutex_unlock(&audio_mutex);
    
    return 0;
}

void audio_cleanup(void) {
    pthread_mutex_lock(&audio_mutex);
    
    if (pa_ctx) {
        pa_context_disconnect(pa_ctx);
        pa_context_unref(pa_ctx);
        pa_ctx = NULL;
    }
    
    if (pa_ml) {
        pa_mainloop_free(pa_ml);
        pa_ml = NULL;
    }
    
    audio_initialized = 0;
    pthread_mutex_unlock(&audio_mutex);
}

int audio_play(const char *filename, int force_wakeup) {
    if (!filename) {
        syslog(LOG_ERR, "No audio file specified");
        return -1;
    }
    
    // Check if file exists
    if (access(filename, R_OK) != 0) {
        syslog(LOG_ERR, "Audio file not found: %s", filename);
        return -1;
    }
    
    pthread_mutex_lock(&audio_mutex);
    
    if (!audio_initialized) {
        syslog(LOG_ERR, "Audio system not initialized");
        pthread_mutex_unlock(&audio_mutex);
        return -1;
    }
    
    // For PulseAudio, we use paplay as a simple solution
    // In production, you'd implement proper streaming
    char command[512];
    
    if (force_wakeup) {
        // Force wakeup: increase volume and use system wakeup
        snprintf(command, sizeof(command), 
                "paplay --volume=%d --client-name=notification-client \"%s\"",
                current_volume * 65536 / 100, filename);
        
        // Send system notification for wakeup
        system("systemd-inhibit --why=\"Notification\" --mode=block sleep 1");
    } else {
        snprintf(command, sizeof(command), 
                "paplay --volume=%d \"%s\"", 
                current_volume * 65536 / 100, filename);
    }
    
    // Run in background
    command[strlen(command)] = '&';
    
    syslog(LOG_INFO, "Playing audio: %s (force_wakeup=%d)", filename, force_wakeup);
    int result = system(command);
    
    pthread_mutex_unlock(&audio_mutex);
    
    return (result == 0) ? 0 : -1;
}

void audio_stop(void) {
    pthread_mutex_lock(&audio_mutex);
    system("pkill -f \"paplay.*notification-client\"");
    pthread_mutex_unlock(&audio_mutex);
}

void audio_set_volume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    current_volume = volume;
    
    // Update system volume
    char command[128];
    snprintf(command, sizeof(command), "pactl set-sink-volume @DEFAULT_SINK@ %d%%", volume);
    system(command);
}

int audio_get_volume(void) {
    return current_volume;
}

int audio_check_device(void) {
    // Check if PulseAudio is running
    int result = system("pactl info > /dev/null 2>&1");
    return (result == 0) ? 1 : 0;
}