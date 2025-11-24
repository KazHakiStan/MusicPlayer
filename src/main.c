#include "player.h"
#include "ui.h"
#include "version.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

static void run_uninstaller(void) {
  // Let cmd.exe / PowerShell expand $env:LOCALAPPDATA
  char cmd[1024];
  snprintf(cmd, sizeof(cmd),
           "powershell -ExecutionPolicy Bypass -NoProfile -File "
           "\"%s\\MusicPlayer\\uninstall.ps1\"",
           "%LOCALAPPDATA%");

  system(cmd);
}

static int check_for_update(UIState *ui_state) {
  char cmd[512];

  // use %LOCALAPPDATA%\MusicPlayer\update.ps1 -CheckOnly
  // let cmd.exe expand %LOCALAPPDATA%
  snprintf(cmd, sizeof(cmd),
           "powershell -ExecutionPolicy Bypass -NoProfile -File "
           "\"$env:LOCALAPPDATA\\MusicPlayer\\update.ps1\" -CheckOnly");

  FILE *pipe = _popen(cmd, "r");
  if (!pipe) {
    return 0;
  }

  char buf[256];
  char latest[64] = {0};

  // read last line from output (should be just "v0.x.y" or empty)
  while (fgets(buf, sizeof(buf), pipe)) {
    strncpy(latest, buf, sizeof(latest) - 1);
    latest[sizeof(latest) - 1] = '\0';
  }
  _pclose(pipe);

  // trim whitespace
  char *p = latest;
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
    p++;
  char *end = p + strlen(p);
  while (end > p && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' ||
                     end[-1] == '\n')) {
    *--end = '\0';
  }

  if (p[0] == '\0') {
    // no new version
    ui_state->has_update = false;
    ui_state->latest_version[0] = '\0';
    return 0;
  }

  ui_state->has_update = true;
  strncpy(ui_state->latest_version, p, sizeof(ui_state->latest_version) - 1);
  ui_state->latest_version[sizeof(ui_state->latest_version) - 1] = '\0';
  return 1;
}

static void run_updater(void) {
  char cmd[1024];

  // use %LOCALAPPDATA%\MusicPlayer\update.ps1
  // We let cmd.exe expand %LOCALAPPDATA%
  snprintf(cmd, sizeof(cmd),
           "powershell -ExecutionPolicy Bypass -NoProfile -File "
           "\"%s\\MusicPlayer\\update.ps1\"",
           "%LOCALAPPDATA%");

  // system() will go through cmd.exe, so %LOCALAPPDATA% expands
  int rc = system(cmd);
  (void)rc;
}

int main(int argc, char *argv[]) {
  if (argc > 1 && strcmp(argv[1], "update") == 0) {
    // run the updater script and exit
    // (see below)
    run_updater();
    return 0;
  }

  if (argc > 1 && strcmp(argv[1], "--version") == 0) {
    printf("%s\n", MP_VERSION);
    return 0;
  }

  if (argc > 1 && strcmp(argv[1], "uninstall") == 0) {
    run_uninstaller();
    return 0;
  }

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
  ui_state.has_update = false;
  ui_state.latest_version[0] = '\0';

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

  check_for_update(&ui_state);

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
