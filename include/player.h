#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>

typedef enum { PLAYER_STOPPED, PLAYER_PLAYING, PLAYER_PAUSED } PlayerState;

typedef enum { REPEAT_NONE = 0, REPEAT_ONE, REPEAT_ALL } RepeatMode;

typedef struct {
  char title[256];
  char artist[256];
  char album[256];
  double duration;
  char filepath[1024];
} Track;

typedef struct {
  PlayerState state;
  Track current_track;
  double position;
  double volume;
  RepeatMode repeat_mode;
  bool shuffle;
} Player;

// Player control functions
void player_init(Player *player);
bool player_load_track(Player *player, const char *filepath);
void player_play(Player *player);
void player_pause(Player *player);
void player_stop(Player *player);
void player_seek(Player *player, double position);
void player_set_volume(Player *player, double volume);
bool player_update(Player *player);
void player_cleanup(void);

void player_fill_metadata_from_file(const char *filepath, Track *track);

#endif
