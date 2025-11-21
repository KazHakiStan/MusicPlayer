#include "player.h"
#include "audio.h"
#include "mpg123.h"
#include <stdio.h>
#include <string.h>

static AudioEngine *audio_engine = NULL;

void player_fill_metadata_from_file(const char *filepath, Track *track) {
  static int mpg_inited = 0;
  if (!mpg_inited) {
    if (mpg123_init() != MPG123_OK) {
      printf("ERROR");
      return; // leave defaults if init fails
    }
    mpg_inited = 1;
  }

  int err = 0;
  mpg123_handle *mh = mpg123_new(NULL, &err);
  if (!mh)
    return;

  if (mpg123_open(mh, filepath) != MPG123_OK) {
    mpg123_delete(mh);
    return;
  }

  // Make sure all tags (incl. ID3v1 at end) are parsed
  mpg123_scan(mh);

  mpg123_id3v1 *v1 = NULL;
  mpg123_id3v2 *v2 = NULL;

  if (mpg123_id3(mh, &v1, &v2) == MPG123_OK) {
    // Prefer ID3v2 if present
    if (v2) {
      if (v2->title && v2->title->p && v2->title->p[0]) {
        strncpy(track->title, v2->title->p, sizeof(track->title) - 1);
        track->title[sizeof(track->title) - 1] = '\0';
      }
      if (v2->artist && v2->artist->p && v2->artist->p[0]) {
        strncpy(track->artist, v2->artist->p, sizeof(track->artist) - 1);
        track->artist[sizeof(track->artist) - 1] = '\0';
      }
      if (v2->album && v2->album->p && v2->album->p[0]) {
        strncpy(track->album, v2->album->p, sizeof(track->album) - 1);
        track->album[sizeof(track->album) - 1] = '\0';
      }
    }
    // Fallback to ID3v1 if v2 missing / empty
    else if (v1) {
      if (v1->title[0]) {
        strncpy(track->title, v1->title, sizeof(track->title) - 1);
        track->title[sizeof(track->title) - 1] = '\0';
      }
      if (v1->artist[0]) {
        strncpy(track->artist, v1->artist, sizeof(track->artist) - 1);
        track->artist[sizeof(track->artist) - 1] = '\0';
      }
      if (v1->album[0]) {
        strncpy(track->album, v1->album, sizeof(track->album) - 1);
        track->album[sizeof(track->album) - 1] = '\0';
      }
    }
  }

  mpg123_close(mh);
  mpg123_delete(mh);
}

void player_init(Player *player) {
  memset(player, 0, sizeof(Player));
  player->state = PLAYER_STOPPED;
  player->volume = 0.7; // 70% default volume
  player->shuffle = false;
  player->repeat_mode = REPEAT_NONE;

  audio_engine = audio_init();
  if (audio_engine) {
    audio_set_volume(audio_engine, player->volume);
  }
}

bool player_load_track(Player *player, const char *filepath) {
  if (!audio_engine)
    return false;

  if (audio_load_file(audio_engine, filepath)) {
    memset(&player->current_track, 0, sizeof(Track));

    strncpy(player->current_track.filepath, filepath,
            sizeof(player->current_track.filepath) - 1);

    // defaults
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

    // try to overwrite with real metadata
    player_fill_metadata_from_file(filepath, &player->current_track);

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

bool player_update(Player *player) {
  bool finished = false;

  if (audio_engine && player->state == PLAYER_PLAYING) {
    player->position = audio_get_position(audio_engine);

    if (player->position >= player->current_track.duration - 0.01) {
      player->position = player->current_track.duration;
      player->state = PLAYER_STOPPED;
      finished = true;
      // do NOT call player_stop() here (it resets and loses "end" state)
    }
  }

  return finished;
}

void player_cleanup(void) {
  if (audio_engine) {
    audio_cleanup(audio_engine);
    audio_engine = NULL;
  }
}
