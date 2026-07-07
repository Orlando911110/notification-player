#ifndef AUDIO_WORKER_H
#define AUDIO_WORKER_H

#include <stdint.h>
#include <pthread.h>

typedef enum {
    AUDIO_CMD_PLAY,
    AUDIO_CMD_STOP,
    AUDIO_CMD_SET_VOLUME,
    AUDIO_CMD_GET_VOLUME,
    AUDIO_CMD_CHECK_DEVICE
} audio_command_type_t;

typedef struct {
    audio_command_type_t type;
    char sound_file[256];
    int force_wakeup;
    int volume;
    int result;
    int completed;
    pthread_cond_t cond;
    pthread_mutex_t mutex;
} audio_job_t;

// 初始化音频工作线程池
int audio_worker_init(int num_workers);

// 清理音频工作线程池
void audio_worker_cleanup(void);

// 异步播放音频（立即返回）
int audio_play_async(const char *filename, int force_wakeup);

// 同步播放音频（阻塞等待完成）
int audio_play_sync(const char *filename, int force_wakeup);

// 停止播放
void audio_stop_async(void);

// 设置音量（异步）
void audio_set_volume_async(int volume);

// 获取音量（同步）
int audio_get_volume_sync(void);

// 检查音频设备（同步）
int audio_check_device_sync(void);

#endif