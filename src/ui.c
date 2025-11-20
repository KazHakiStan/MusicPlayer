#include "ui.h"
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

static int g_first_draw = 1;

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

  // Controls help
  printf("Controls: [P] Play/Pause [S] Stop [Q] Quit\n");
  printf("          [+] Volume Up [-] Volume Down\n");

  fflush(stdout);
}

void ui_handle_input(Player *player, UIState *ui_state) {
  if (_kbhit()) {
    int ch = _getch();
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
    }
  }
}
