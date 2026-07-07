#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

// 初始化音频系统
int audio_init(void);

// 清理音频系统
void audio_cleanup(void);

// 播放音频文件（支持强制唤醒）
int audio_play(const char *filename, int force_wakeup);

// 停止当前播放
void audio_stop(void);

// 设置音量 (0-100)
void audio_set_volume(int volume);

// 获取当前音量
int audio_get_volume(void);

// 检查音频设备是否可用
int audio_check_device(void);

#endif