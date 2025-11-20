#include "player.h"
#include "audio.h"
#include <stdio.h>
#include <string.h>

static AudioEngine *audio_engine = NULL;

void player_init(Player *player) {
  memset(player, 0, sizeof(Player));
  player->state = PLAYER_STOPPED;
  player->volume = 0.7; // 70% default volume

  audio_engine = audio_init();
  if (audio_engine) {
    audio_set_volume(audio_engine, player->volume);
  }
}

bool player_load_track(Player *player, const char *filepath) {
  if (!audio_engine)
    return false;

  if (audio_load_file(audio_engine, filepath)) {
    strncpy(player->current_track.filepath, filepath,
            sizeof(player->current_track.filepath) - 1);

    // Extract filename as title for now
    const char *filename = strrchr(filepath, '\\');
    if (!filename)
      filename = strrchr(filepath, '/');
    if (filename)
      filename++;
    else
      filename = filepath;

    strncpy(player->current_track.title, filename,
            sizeof(player->current_track.title) - 1);
    strcpy(player->current_track.artist, "Unknown Artist");
    strcpy(player->current_track.album, "Unknown Album");

    player->current_track.duration = audio_get_duration(audio_engine);
    player->position = 0.0;

    return true;
  }
  return false;
}

void player_play(Player *player) {
  if (!audio_engine)
    return;

  // If track is stopped and at the end, restart from beginning
  if (player->state == PLAYER_STOPPED &&
      player->current_track.filepath[0] != '\0' &&
      player->position >= player->current_track.duration - 0.01) {

    // reload audio data (reopens file, resets cursor)
    if (audio_load_file(audio_engine, player->current_track.filepath)) {
      player->position = 0.0;
    }
  }

  if (audio_is_playing(audio_engine)) {
    audio_pause(audio_engine);
    player->state = PLAYER_PAUSED;
  } else {
    audio_play(audio_engine);
    player->state = PLAYER_PLAYING;
  }
}

void player_pause(Player *player) {
  if (audio_engine) {
    audio_pause(audio_engine);
    player->state = PLAYER_PAUSED;
  }
}

void player_stop(Player *player) {
  if (audio_engine) {
    audio_stop(audio_engine);
    player->state = PLAYER_STOPPED;
    player->position = 0.0;
  }
}

void player_seek(Player *player, double position) {
  // To be implemented with audio backend
  player->position = position;
}

void player_set_volume(Player *player, double volume) {
  if (volume < 0.0)
    volume = 0.0;
  if (volume > 1.0)
    volume = 1.0;

  player->volume = volume;
  if (audio_engine) {
    audio_set_volume(audio_engine, volume);
  }
}

void player_update(Player *player) {
  if (audio_engine && player->state == PLAYER_PLAYING) {
    player->position = audio_get_position(audio_engine);

    if (player->position >= player->current_track.duration) {
      player->position = player->current_track.duration;
      player->state = PLAYER_STOPPED;
      // do NOT call player_stop() here (it resets and loses "end" state)
    }
  }
}

void player_cleanup(void) {
  if (audio_engine) {
    audio_cleanup(audio_engine);
    audio_engine = NULL;
  }
}
