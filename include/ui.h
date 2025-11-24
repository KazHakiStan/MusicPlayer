#ifndef UI_H
#define UI_H

#include "player.h"

typedef struct {
  int width;
  int height;
  bool should_quit;

  Track *tracks;
  int track_count;
  int selected_index;

  int track_offset;

  bool has_update;
  char latest_version[32];
} UIState;

// UI functions
void ui_init(void);
void ui_cleanup(void);
void ui_draw(const Player *player, UIState *ui_state);
void ui_handle_input(Player *player, UIState *ui_state);
void ui_get_terminal_size(int *width, int *height);
void ui_handle_track_end(Player *player, UIState *ui_state);

#endif
