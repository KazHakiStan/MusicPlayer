#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

typedef struct AudioEngine AudioEngine;

// Audio engine functions
AudioEngine *audio_init(void);
void audio_cleanup(AudioEngine *engine);
bool audio_load_file(AudioEngine *engine, const char *filename);
void audio_play(AudioEngine *engine);
void audio_pause(AudioEngine *engine);
void audio_stop(AudioEngine *engine);
void audio_set_volume(AudioEngine *engine, float volume);
double audio_get_position(AudioEngine *engine);
double audio_get_duration(AudioEngine *engine);
bool audio_is_playing(AudioEngine *engine);

#endif
