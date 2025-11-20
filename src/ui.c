#include "ui.h"
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

static int g_first_draw = 1;

// Strip trailing newline from fgets
static void trim_newline(char *s) {
  size_t len = strlen(s);
  while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
    s[--len] = '\0';
  }
}

// Load all *.mp3 files from a folder into ui_state->tracks
static void add_folder_mp3s(UIState *ui_state, const char *folder) {
  char search_path[MAX_PATH];
  snprintf(search_path, sizeof(search_path), "%s\\*.mp3", folder);

  WIN32_FIND_DATAA ffd;
  HANDLE hFind = FindFirstFileA(search_path, &ffd);
  if (hFind == INVALID_HANDLE_VALUE) {
    printf("\nNo MP3 files found in %s\n", folder);
    Sleep(1000);
    return;
  }

  do {
    if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      continue;

    int idx = ui_state->track_count;
    Track *new_arr = realloc(ui_state->tracks, (idx + 1) * sizeof(Track));
    if (!new_arr) {
      printf("\nOut of memory while adding tracks.\n");
      break;
    }
    ui_state->tracks = new_arr;

    Track *t = &ui_state->tracks[idx];
    memset(t, 0, sizeof(Track));

    // full path
    snprintf(t->filepath, sizeof(t->filepath), "%s\\%s", folder, ffd.cFileName);
    // title from filename
    strncpy(t->title, ffd.cFileName, sizeof(t->title) - 1);
    strcpy(t->artist, "Unknown");
    strcpy(t->album, "Unknown");
    t->duration = 0.0; // will be filled when we actually load/play

    ui_state->track_count++;
  } while (FindNextFileA(hFind, &ffd));

  FindClose(hFind);

  if (ui_state->track_count > 0 &&
      ui_state->selected_index >= ui_state->track_count) {
    ui_state->selected_index = ui_state->track_count - 1;
  }
}

// Prompt user for folder and add mp3s from there
static void prompt_add_folder(UIState *ui_state) {
  char folder[MAX_PATH];

  // Move cursor to bottom to avoid messing UI too much
  printf("\nEnter folder path with MP3 files: ");
  fflush(stdout);

  if (!fgets(folder, sizeof(folder), stdin)) {
    return;
  }
  trim_newline(folder);
  if (folder[0] == '\0')
    return;

  add_folder_mp3s(ui_state, folder);
}

void ui_init(void) {
  // Enable UTF-8 and ANSI escape sequences in Windows terminal
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD dwMode = 0;
  GetConsoleMode(hOut, &dwMode);
  dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  SetConsoleMode(hOut, dwMode);

  // Optional: UTF-8 output
  SetConsoleOutputCP(CP_UTF8);

  // Hide cursor
  printf("\033[?25l");
  fflush(stdout);
}

void ui_cleanup(void) {
  // Show cursor
  printf("\033[?25h");
  fflush(stdout);
}

void ui_get_terminal_size(int *width, int *height) {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  *width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
  *height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
}

void ui_draw(const Player *player, const UIState *ui_state) {
  // Clear once on the very first frame
  if (g_first_draw) {
    printf("\033[2J"); // clear whole screen once
    g_first_draw = 0;
  }

  // Move cursor to top-left (home)
  printf("\033[H");

  printf("=== CLI Music Player ===\n\n");

  // Current track info
  printf("Now Playing:\n");
  printf("Title:  %s\n", player->current_track.title);
  printf("Artist: %s\n", player->current_track.artist);
  printf("Album:  %s\n\n", player->current_track.album);

  // Player state
  const char *state_str = "UNKNOWN";
  switch (player->state) {
  case PLAYER_PLAYING:
    state_str = "PLAYING";
    break;
  case PLAYER_PAUSED:
    state_str = "PAUSED";
    break;
  case PLAYER_STOPPED:
    state_str = "STOPPED";
    break;
  }
  printf("State: %s\n", state_str);

  // Progress bar
  if (player->current_track.duration > 0) {
    double progress = player->position / player->current_track.duration;
    if (progress < 0.0)
      progress = 0.0;
    if (progress > 1.0)
      progress = 1.0;

    int bar_width = ui_state->width - 20;
    if (bar_width < 10)
      bar_width = 10;

    int pos = (int)(progress * (bar_width - 1));

    printf("\n[");
    for (int i = 0; i < bar_width; i++) {
      putchar(i <= pos ? '=' : ' ');
    }
    printf("] %.1f/%.1f\n", player->position, player->current_track.duration);
  }

  // Volume
  printf("Volume: %d%%\n\n", (int)(player->volume * 100));

  // NEW: Playlist
  printf("Playlist (A: add folder, ENTER: play):\n");
  int max_lines = ui_state->height - 14; // leave space for header/footer
  if (max_lines < 3)
    max_lines = 3;

  for (int i = 0; i < ui_state->track_count && i < max_lines; ++i) {
    const Track *t = &ui_state->tracks[i];
    printf("%c %2d. %s\n", (i == ui_state->selected_index ? '>' : ' '), i + 1,
           t->title[0] ? t->title : "(untitled)");
  }

  if (ui_state->track_count == 0) {
    printf("  (no tracks loaded)\n");
  }

  // Controls help
  printf("Controls: [P] Play/Pause [S] Stop [Q] Quit\n");
  printf("          [+] Volume Up [-] Volume Down\n");

  fflush(stdout);
}

void ui_handle_input(Player *player, UIState *ui_state) {
  if (!_kbhit())
    return;

  int ch = _getch();

  // Arrow keys etc.
  if (ch == 0 || ch == 0xE0) {
    int code = _getch();
    switch (code) {
    case 72: // UP
      if (ui_state->track_count > 0 && ui_state->selected_index > 0)
        ui_state->selected_index--;
      break;
    case 80: // DOWN
      if (ui_state->track_count > 0 &&
          ui_state->selected_index < ui_state->track_count - 1)
        ui_state->selected_index++;
      break;
    }
    return;
  }

  switch (ch) {
  case 'q':
  case 'Q':
    ui_state->should_quit = true;
    break;

  case 'p':
  case 'P':
    if (player->state == PLAYER_PLAYING) {
      player_pause(player);
    } else {
      player_play(player);
    }
    break;

  case 's':
  case 'S':
    player_stop(player);
    break;

  case '+':
    player_set_volume(player, player->volume + 0.1);
    break;
  case '-':
    player_set_volume(player, player->volume - 0.1);
    break;

  // NEW: add folder
  case 'a':
  case 'A':
    prompt_add_folder(ui_state);
    break;

  // NEW: ENTER = play selected track
  case '\r': // Enter
    if (ui_state->track_count > 0) {
      Track *t = &ui_state->tracks[ui_state->selected_index];

      if (player_load_track(player, t->filepath)) {
        // update playlist track with duration & cleaned title
        *t = player->current_track;
        player_play(player);
      }
    }
    break;
  }
}
