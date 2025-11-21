#include "player.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

int main(int argc, char *argv[]) {
  printf("Starting main, argc = %d\n", argc);
  fflush(stdout);

  // Initialize components
  ui_init();
  printf("Loaded UI");

  Player player;
  player_init(&player);
  printf("Loaded player");

  UIState ui_state = {0};
  ui_get_terminal_size(&ui_state.width, &ui_state.height);

  // NEW: playlist fields start empty
  ui_state.tracks = NULL;
  ui_state.track_count = 0;
  ui_state.selected_index = 0;
  ui_state.track_offset = 0;

  // OPTIONAL: if user *does* pass an mp3, pre-load it
  if (argc >= 2) {
    if (!player_load_track(&player, argv[1])) {
      printf("Failed to load audio file: %s\n", argv[1]);
      Sleep(2000);
    } else {
      printf("Loaded: %s\n", argv[1]);
      Sleep(1000);
    }
  }

  // Main loop
  while (!ui_state.should_quit) {
    // Update terminal size
    ui_get_terminal_size(&ui_state.width, &ui_state.height);

    // Update player state
    bool finished = player_update(&player);
    if (finished) {
      ui_handle_track_end(&player, &ui_state);
    }

    // Draw UI
    ui_draw(&player, &ui_state);

    // Handle input
    ui_handle_input(&player, &ui_state);

    // Small delay to prevent high CPU usage
    Sleep(50);
  }

  // Cleanup
  printf("Goodbye!\n");
  ui_cleanup();
  free(ui_state.tracks);

  return 0;
}
