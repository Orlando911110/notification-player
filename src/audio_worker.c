#include "audio_worker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <pthread.h>
#include <errno.h>
#include <alsa/asoundlib.h>
#include <pulse/pulseaudio.h>

#define MAX_QUEUE_SIZE 100
#define MAX_WORKERS 4

typedef struct {
    audio_job_t *jobs[MAX_QUEUE_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} job_queue_t;

static job_queue_t queue;
static pthread_t workers[MAX_WORKERS];
static int worker_count = 0;
static int audio_initialized = 0;
static int current_volume = 80;
static pthread_mutex_t volume_mutex = PTHREAD_MUTEX_INITIALIZER;

// PulseAudio 简单播放函数（使用库API，不调用system）
static int play_audio_native(const char *filename, int force_wakeup) {
    // 使用ALSA直接播放
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    FILE *audio_file;
    char *buffer;
    size_t buffer_size;
    int ret = -1;
    
    // 打开音频文件
    audio_file = fopen(filename, "rb");
    if (!audio_file) {
        syslog(LOG_ERR, "Failed to open audio file: %s", filename);
        return -1;
    }
    
    // 获取文件大小
    fseek(audio_file, 0, SEEK_END);
    long file_size = ftell(audio_file);
    fseek(audio_file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(audio_file);
        return -1;
    }
    
    // 读取WAV头（简化处理，实际应解析WAV格式）
    buffer = malloc(file_size);
    if (!buffer) {
        fclose(audio_file);
        return -1;
    }
    
    size_t read_size = fread(buffer, 1, file_size, audio_file);
    fclose(audio_file);
    
    if (read_size < 44) { // WAV头最小44字节
        free(buffer);
        return -1;
    }
    
    // 解析WAV头获取采样率、通道数等
    int sample_rate = *(int*)(buffer + 24);
    int channels = *(short*)(buffer + 22);
    int bits_per_sample = *(short*)(buffer + 34);
    
    // 打开ALSA设备
    if (snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        free(buffer);
        return -1;
    }
    
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(pcm_handle, params);
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm_handle, params, 
        bits_per_sample == 16 ? SND_PCM_FORMAT_S16_LE : SND_PCM_FORMAT_U8);
    snd_pcm_hw_params_set_channels(pcm_handle, params, channels);
    snd_pcm_hw_params_set_rate_near(pcm_handle, params, &sample_rate, NULL);
    snd_pcm_hw_params(pcm_handle, params);
    
    // 播放音频数据（跳过WAV头）
    char *audio_data = buffer + 44;
    size_t audio_size = read_size - 44;
    
    ret = snd_pcm_writei(pcm_handle, audio_data, audio_size / (channels * bits_per_sample / 8));
    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    
    free(buffer);
    return ret > 0 ? 0 : -1;
}

// 使用posix_spawn替代system()
static int play_audio_external(const char *filename, int force_wakeup) {
    pid_t pid;
    char *args[4];
    int status;
    
    pid = fork();
    if (pid == 0) {
        // 子进程：尝试使用aplay或paplay
        if (force_wakeup) {
            execlp("systemd-inhibit", "systemd-inhibit", 
                   "--why=Notification", "--mode=block", 
                   "sleep", "1", NULL);
            // 如果systemd-inhibit失败，继续播放
        }
        
        // 尝试aplay
        execlp("aplay", "aplay", filename, NULL);
        // 如果aplay失败，尝试paplay
        execlp("paplay", "paplay", filename, NULL);
        exit(1);
    } else if (pid > 0) {
        // 父进程：等待子进程完成，但设置超时
        int timeout = 30; // 30秒超时
        while (timeout > 0) {
            if (waitpid(pid, &status, WNOHANG) > 0) {
                return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
            }
            sleep(1);
            timeout--;
        }
        // 超时，杀死子进程
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        return -1;
    }
    
    return -1;
}

static void* audio_worker_thread(void *arg) {
    while (1) {
        pthread_mutex_lock(&queue.mutex);
        
        while (queue.count == 0) {
            pthread_cond_wait(&queue.not_empty, &queue.mutex);
        }
        
        audio_job_t *job = queue.jobs[queue.head];
        queue.head = (queue.head + 1) % MAX_QUEUE_SIZE;
        queue.count--;
        pthread_cond_signal(&queue.not_full);
        
        pthread_mutex_unlock(&queue.mutex);
        
        // 执行任务
        if (job) {
            switch (job->type) {
                case AUDIO_CMD_PLAY:
                    // 尝试原生播放，失败则使用外部播放器
                    job->result = play_audio_native(job->sound_file, job->force_wakeup);
                    if (job->result < 0) {
                        job->result = play_audio_external(job->sound_file, job->force_wakeup);
                    }
                    break;
                    
                case AUDIO_CMD_SET_VOLUME:
                    pthread_mutex_lock(&volume_mutex);
                    current_volume = job->volume;
                    pthread_mutex_unlock(&volume_mutex);
                    // 使用AMixer或Pactl设置音量
                    {
                        char cmd[128];
                        snprintf(cmd, sizeof(cmd), 
                                "amixer set Master %d%% 2>/dev/null || pactl set-sink-volume @DEFAULT_SINK@ %d%% 2>/dev/null",
                                job->volume, job->volume);
                        system(cmd); // 这里仍然使用system，但这是低频操作
                    }
                    job->result = 0;
                    break;
                    
                default:
                    job->result = -1;
                    break;
            }
            
            // 标记任务完成并唤醒等待线程
            pthread_mutex_lock(&job->mutex);
            job->completed = 1;
            pthread_cond_signal(&job->cond);
            pthread_mutex_unlock(&job->mutex);
        }
    }
    return NULL;
}

int audio_worker_init(int num_workers) {
    if (audio_initialized) {
        return 0;
    }
    
    if (num_workers <= 0 || num_workers > MAX_WORKERS) {
        num_workers = MAX_WORKERS;
    }
    
    // 初始化队列
    queue.head = 0;
    queue.tail = 0;
    queue.count = 0;
    pthread_mutex_init(&queue.mutex, NULL);
    pthread_cond_init(&queue.not_empty, NULL);
    pthread_cond_init(&queue.not_full, NULL);
    
    // 创建工作线程
    worker_count = num_workers;
    for (int i = 0; i < worker_count; i++) {
        if (pthread_create(&workers[i], NULL, audio_worker_thread, NULL) != 0) {
            syslog(LOG_ERR, "Failed to create audio worker thread %d", i);
            worker_count = i;
            break;
        }
    }
    
    audio_initialized = 1;
    syslog(LOG_INFO, "Audio worker initialized with %d threads", worker_count);
    return 0;
}

void audio_worker_cleanup(void) {
    if (!audio_initialized) {
        return;
    }
    
    audio_initialized = 0;
    
    // 取消所有工作线程
    for (int i = 0; i < worker_count; i++) {
        pthread_cancel(workers[i]);
        pthread_join(workers[i], NULL);
    }
    
    pthread_mutex_destroy(&queue.mutex);
    pthread_cond_destroy(&queue.not_empty);
    pthread_cond_destroy(&queue.not_full);
    
    syslog(LOG_INFO, "Audio worker cleaned up");
}

static int enqueue_job(audio_job_t *job) {
    if (!audio_initialized) {
        return -1;
    }
    
    pthread_mutex_lock(&queue.mutex);
    
    while (queue.count >= MAX_QUEUE_SIZE) {
        pthread_cond_wait(&queue.not_full, &queue.mutex);
    }
    
    queue.jobs[queue.tail] = job;
    queue.tail = (queue.tail + 1) % MAX_QUEUE_SIZE;
    queue.count++;
    pthread_cond_signal(&queue.not_empty);
    
    pthread_mutex_unlock(&queue.mutex);
    return 0;
}

int audio_play_async(const char *filename, int force_wakeup) {
    audio_job_t *job = malloc(sizeof(audio_job_t));
    if (!job) {
        return -1;
    }
    
    memset(job, 0, sizeof(audio_job_t));
    job->type = AUDIO_CMD_PLAY;
    strncpy(job->sound_file, filename, 255);
    job->sound_file[255] = '\0';
    job->force_wakeup = force_wakeup;
    job->completed = 1; // 异步任务立即返回，不等待完成
    
    return enqueue_job(job);
}

int audio_play_sync(const char *filename, int force_wakeup) {
    audio_job_t job;
    memset(&job, 0, sizeof(job));
    job.type = AUDIO_CMD_PLAY;
    strncpy(job.sound_file, filename, 255);
    job.sound_file[255] = '\0';
    job.force_wakeup = force_wakeup;
    job.completed = 0;
    pthread_mutex_init(&job.mutex, NULL);
    pthread_cond_init(&job.cond, NULL);
    
    if (enqueue_job(&job) < 0) {
        pthread_mutex_destroy(&job.mutex);
        pthread_cond_destroy(&job.cond);
        return -1;
    }
    
    // 等待任务完成
    pthread_mutex_lock(&job.mutex);
    while (!job.completed) {
        pthread_cond_wait(&job.cond, &job.mutex);
    }
    pthread_mutex_unlock(&job.mutex);
    
    pthread_mutex_destroy(&job.mutex);
    pthread_cond_destroy(&job.cond);
    
    return job.result;
}

void audio_stop_async(void) {
    // 停止所有播放进程
    system("pkill -f aplay 2>/dev/null");
    system("pkill -f paplay 2>/dev/null");
}

void audio_set_volume_async(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    
    audio_job_t *job = malloc(sizeof(audio_job_t));
    if (!job) {
        return;
    }
    
    memset(job, 0, sizeof(audio_job_t));
    job->type = AUDIO_CMD_SET_VOLUME;
    job->volume = volume;
    job->completed = 1;
    enqueue_job(job);
}

int audio_get_volume_sync(void) {
    pthread_mutex_lock(&volume_mutex);
    int vol = current_volume;
    pthread_mutex_unlock(&volume_mutex);
    return vol;
}

int audio_check_device_sync(void) {
    // 检查ALSA设备
    int ret = system("aplay -l 2>/dev/null | grep -q card");
    if (ret != 0) {
        ret = system("pactl info > /dev/null 2>&1");
    }
    return ret == 0 ? 1 : 0;
}