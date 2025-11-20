#include "player.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

int main(int argc, char *argv[]) {
  printf("Starting main, argc = %d\n", argc);
  fflush(stdout);
  if (argc < 2) {
    printf("Usage: %s <audio_file>\n", argv[0]);
    return 1;
  }
  printf("Check main");

  // Initialize components
  ui_init();
  printf("Loaded UI");

  Player player;
  player_init(&player);
  printf("Loaded player");

  UIState ui_state = {0};
  ui_get_terminal_size(&ui_state.width, &ui_state.height);

  // Load the track
  if (!player_load_track(&player, argv[1])) {
    printf("Failed to load audio file: %s\n", argv[1]);
    player_cleanup();
    ui_cleanup();
    return 1;
  }

  printf("Loaded: %s\n", argv[1]);
  Sleep(2000); // Brief pause

  // Main loop
  while (!ui_state.should_quit) {
    // Update terminal size
    ui_get_terminal_size(&ui_state.width, &ui_state.height);

    // Update player state
    player_update(&player);

    // Draw UI
    ui_draw(&player, &ui_state);

    // Handle input
    ui_handle_input(&player, &ui_state);

    // Small delay to prevent high CPU usage
    Sleep(50);
  }

  // Cleanup
  ui_cleanup();
  printf("Goodbye!\n");

  return 0;
}
