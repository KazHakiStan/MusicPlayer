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
           "\"%s\\MusicPlayer\\update.ps1\" -CheckOnly",
           "%LOCALAPPDATA%");

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
  ui_state.dirty = true;
  ui_state.last_prog_tick = GetTickCount();
  ui_state.screen = SCREEN_MAIN;
  ui_state.folder_current[0] = '\0';
  ui_state.folder_selected = 0;
  ui_state.folder_offset = 0;

  ui_init_state(&ui_state, UI_MODE_FULL);

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

  DWORD last_prog_tick = GetTickCount();

  // Main loop
  while (!ui_state.should_quit) {
    int old_w = ui_state.width;
    int old_h = ui_state.height;
    ui_get_terminal_size(&ui_state.width, &ui_state.height);
    if (ui_state.width != old_w || ui_state.height != old_h) {
      ui_state.dirty = true;
    }

    PlayerState old_state = player.state;
    double old_pos = player.position;
    bool finished = player_update(&player);
    if (finished) {
      ui_handle_track_end(&player, &ui_state);
    }

    if (player.state != old_state) {
      ui_state.dirty = true;
    }

    DWORD now = GetTickCount();
    if (player.state == PLAYER_PLAYING &&
        now - ui_state.last_prog_tick >= 100) {
      ui_state.dirty = true;
      ui_state.last_prog_tick = now;
    }

    ui_handle_input(&player, &ui_state);

    if (ui_state.dirty) {
      ui_draw(&player, &ui_state);
      ui_state.dirty = false;
    }

    Sleep(10);
  }
  // Cleanup
  printf("Goodbye!\n");
  ui_cleanup();
  free(ui_state.tracks);

  return 0;
}
