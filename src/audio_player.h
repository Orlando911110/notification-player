#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

void init_audio_player();
void cleanup_audio_player();
int play_audio(const char *file_path, int volume);
void stop_audio();
void set_volume(int volume);
int get_player_status();

#endif