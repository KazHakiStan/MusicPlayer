#include "ui.h"
#include "ctype.h"
#include "direct.h"
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>

static int g_first_draw = 1;

typedef struct {
  char name_display[MAX_PATH];
  char path_fs[MAX_PATH];
  bool is_parent;
  bool is_drive;
} FolderItem;

static void ansi_to_utf8(const char *src, char *dst, int dst_size) {
  if (!src || !dst || dst_size <= 0)
    return;

  // First convert from ANSI (CP_ACP) to UTF-16
  int wlen = MultiByteToWideChar(CP_ACP, 0, src, -1, NULL, 0);
  if (wlen <= 0) {
    dst[0] = '\0';
    return;
  }

  wchar_t *wbuf = (wchar_t *)malloc(wlen * sizeof(wchar_t));
  if (!wbuf) {
    dst[0] = '\0';
    return;
  }

  MultiByteToWideChar(CP_ACP, 0, src, -1, wbuf, wlen);

  // Now convert UTF-16 → UTF-8
  int u8len =
      WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, dst, dst_size, NULL, NULL);
  free(wbuf);

  if (u8len <= 0) {
    dst[0] = '\0';
  }
}

static int list_drives(FolderItem *items, int max_items) {
  DWORD mask = GetLogicalDrives();
  int count = 0;

  for (char letter = 'A'; letter <= 'Z'; ++letter) {
    if (mask & (1 << (letter - 'A'))) {
      if (count >= max_items)
        break;
      FolderItem *it = &items[count++];
      snprintf(it->path_fs, sizeof(it->path_fs), "%c:\\", letter);
      ansi_to_utf8(it->path_fs, it->name_display, sizeof(it->name_display));
      it->is_parent = false;
      it->is_drive = true;
    }
  }
  return count;
}

static int list_subdirs2(const char *base, FolderItem *items, int max_items) {
  int count = 0;

  // parent entry
  if (base && base[0] != '\0') {
    FolderItem *p = &items[count++];
    strcpy(p->name_display, "..");
    p->path_fs[0] = '\0';
    p->is_parent = true;
    p->is_drive = false;
  }

  char search[MAX_PATH];
  snprintf(search, sizeof(search), "%s\\*", base);

  // TODO: Change to Wide format to support UTF8 (Cyrillic)

  WIN32_FIND_DATAA ffd;
  HANDLE hFind = FindFirstFileA(search, &ffd);
  if (hFind == INVALID_HANDLE_VALUE)
    return count;

  do {
    if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0)
        continue;

      if (count >= max_items)
        break;
      FolderItem *it = &items[count++];
      // filesystem path (still ANSI)
      snprintf(it->path_fs, sizeof(it->path_fs), "%s\\%s", base, ffd.cFileName);

      // display name: ffd.cFileName converted to UTF-8
      ansi_to_utf8(ffd.cFileName, it->name_display, sizeof(it->name_display));

      it->is_parent = false;
      it->is_drive = false;
    }
  } while (FindNextFileA(hFind, &ffd));

  FindClose(hFind);
  return count;
}

static void play_track_at_index(Player *player, UIState *ui_state, int index) {
  if (index < 0 || index >= ui_state->track_count)
    return;

  ui_state->selected_index = index;

  Track *t = &ui_state->tracks[index];
  if (player_load_track(player, t->filepath)) {
    // refresh metadata & duration in playlist from player
    *t = player->current_track;
    player_play(player);
  }
}

// Load all *.mp3 files from a folder into ui_state->tracks
static void add_folder_mp3s_recursive(UIState *ui_state, const char *folder) {
  WIN32_FIND_DATAA ffd;
  HANDLE hFind;
  char search[MAX_PATH];

  // 1) Add all *.mp3 in this folder
  snprintf(search, sizeof(search), "%s\\*.mp3", folder);
  hFind = FindFirstFileA(search, &ffd);
  if (hFind != INVALID_HANDLE_VALUE) {
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
      snprintf(t->filepath, sizeof(t->filepath), "%s\\%s", folder,
               ffd.cFileName);

      // defaults from filename
      ansi_to_utf8(ffd.cFileName, t->title, sizeof(t->title));
      strcpy(t->artist, "Unknown Artist");
      strcpy(t->album, "Unknown Album");
      t->duration = 0.0;

      // fill metadata (title/artist/album/duration) if available
      player_fill_metadata_from_file(t->filepath, t);

      ui_state->track_count++;
    } while (FindNextFileA(hFind, &ffd));
    FindClose(hFind);
  }

  // 2) Recurse into subdirectories
  snprintf(search, sizeof(search), "%s\\*", folder);
  hFind = FindFirstFileA(search, &ffd);
  if (hFind == INVALID_HANDLE_VALUE)
    return;

  do {
    if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0)
        continue;

      char sub[MAX_PATH];
      snprintf(sub, sizeof(sub), "%s\\%s", folder, ffd.cFileName);

      // You might want to skip huge system dirs if scanning root drives
      // e.g. skip "Windows", "Program Files" here if needed.

      add_folder_mp3s_recursive(ui_state, sub);
    }
  } while (FindNextFileA(hFind, &ffd));

  FindClose(hFind);
}

static void add_folder_mp3s(UIState *ui_state, const char *folder) {
  add_folder_mp3s_recursive(ui_state, folder);

  if (ui_state->track_count > 0 &&
      ui_state->selected_index >= ui_state->track_count) {
    ui_state->selected_index = ui_state->track_count - 1;
  }
}

// Prompt user for folder and add mp3s from there
static void prompt_add_folder(UIState *ui_state) {
  char current[MAX_PATH] = "";
  int selected = 0;
  int offset = 0;

  for (;;) {
    FolderItem items[256];
    int count = 0;

    if (current[0] == '\0') {
      count = list_drives(items, 256);
    } else {
      count = list_subdirs2(current, items, 256);
    }

    if (selected >= count)
      selected = count > 0 ? count - 1 : 0;
    if (selected < 0)
      selected = 0;

    // get current terminal size
    int w, h;
    ui_get_terminal_size(&w, &h);
    int max_lines = h - 8; // header + footer
    if (max_lines < 3)
      max_lines = 3;

    // clamp offset
    if (offset > selected)
      offset = selected;
    if (selected >= offset + max_lines)
      offset = selected - max_lines + 1;
    if (offset < 0)
      offset = 0;
    if (offset > count - 1)
      offset = count > 0 ? count - 1 : 0;

    // draw picker
    printf("\033[H"); // home only, no full clear
    printf("=== Select folder with MP3 files ===\033[K\n\n");
    if (current[0] == '\0')
      printf("Location: [drives]\033[K\n\n");
    else {
      char current_display[MAX_PATH];
      ansi_to_utf8(current, current_display, sizeof(current_display));
      printf("Location: %s\033[K\n\n", current_display);
    }
    if (count == 0) {
      printf("  (no subfolders)\033[K\n");
    }

    for (int i = 0; i < max_lines; ++i) {
      int idx = offset + i;
      if (idx >= count) {
        printf("\033[K\n");
        continue;
      }
      FolderItem *it = &items[idx];
      char mark = (idx == selected) ? '>' : ' ';
      printf("%c %s\033[K\n", mark, it->name_display);
    }

    printf("\nControls: ↑/↓ move  ENTER select  S = use this folder  Q = "
           "cancel\033[K\n");
    fflush(stdout);

    int ch = _getch();

    if (ch == 0 || ch == 0xE0) {
      int code = _getch();
      if (code == 72) { // UP
        if (selected > 0)
          selected--;
      } else if (code == 80) { // DOWN
        if (selected < count - 1)
          selected++;
      }
      continue;
    }

    if (ch == 'q' || ch == 'Q' || ch == 27) {
      break;
    }

    if (ch == 's' || ch == 'S') {
      if (current[0] != '\0') {
        add_folder_mp3s(ui_state, current);
      }
      break;
    }

    if (ch == '\r') { // ENTER
      if (count == 0)
        continue;

      FolderItem *sel = &items[selected];

      if (current[0] == '\0') {
        strncpy(current, sel->path_fs, sizeof(current) - 1);
        current[sizeof(current) - 1] = '\0';
        selected = 0;
        offset = 0;
      } else {
        if (sel->is_parent) {
          char *last = strrchr(current, '\\');
          if (last && last > current + 2) {
            *last = '\0';
          } else {
            current[0] = '\0';
          }
          selected = 0;
          offset = 0;
        } else if (sel->path_fs[0]) {
          strncpy(current, sel->path_fs, sizeof(current) - 1);
          current[sizeof(current) - 1] = '\0';
          selected = 0;
          offset = 0;
        }
      }
    }
  }
}

void ui_init(void) {
  // Enable UTF-8 and ANSI escape sequences in Windows terminal
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD dwMode = 0;
  GetConsoleMode(hOut, &dwMode);
  dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  SetConsoleMode(hOut, dwMode);

  srand((unsigned)time(NULL));

  // Optional: UTF-8 output
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);

  // Enter alternate screen buffer
  printf("\x1b[?1049h");

  // Hide cursor
  printf("\x1b[?25l");
  fflush(stdout);
}

void ui_cleanup(void) {
  // Show cursor
  printf("\x1b[?25h");

  // Leave alternate screen (restores original terminal content)
  printf("\x1b[?1049l");
  fflush(stdout);
}

void ui_get_terminal_size(int *width, int *height) {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  *width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
  *height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
}

void ui_draw(const Player *player, UIState *ui_state) {
  // Clear once on the very first frame
  if (g_first_draw) {
    printf("\033[2J"); // clear whole screen once
    g_first_draw = 0;
  }

  // Move cursor to top-left (home)
  printf("\033[H");
  int w = ui_state->width;
  int h = ui_state->height;

  const int MIN_W = 80;
  const int MIN_H = 24;

  // // Move cursor to home and clear from cursor to end of screen
  // printf("\033[H\033[J");

  // If terminal is too small, just show a simple message
  if (w < MIN_W || h < MIN_H) {
    printf("=== CLI Music Player ===\033[K\n\033[K\n");
    printf("Terminal window is too small.\033[K\n");
    printf("Please resize to at least %d x %d characters.\033[K\n", MIN_W,
           MIN_H);
    fflush(stdout);
    return;
  }

  printf("=== CLI Music Player ===\033[K\n\033[K\n");

  // Current track info
  printf("Now Playing: ");
  printf("%s", player->current_track.title);
  printf(" by %s", player->current_track.artist);
  printf(" from %s\033[K\n", player->current_track.album);

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
  printf("State: %s\033[K\n", state_str);

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

    printf("\033[K\n[");
    for (int i = 0; i < bar_width; i++) {
      putchar(i <= pos ? '=' : ' ');
    }
    printf("] %.1f/%.1f\033[K\n", player->position,
           player->current_track.duration);
  }

  // Volume
  printf("Volume: %d%%\033[K\n\033[K\n", (int)(player->volume * 100));

  const char *repeat_str = "None";
  switch (player->repeat_mode) {
  case REPEAT_ONE:
    repeat_str = "One";
    break;
  case REPEAT_ALL:
    repeat_str = "Playlist";
    break;
  default:
    break;
  }

  printf("Repeat: %s  |  Shuffle: %s\033[K\n\033[K\n", repeat_str,
         player->shuffle ? "ON" : "OFF");

  // NEW: Playlist
  printf("Playlist (A: add folder, ENTER: play):\033[K\n");
  printf("   %-25.25s | %-25.25s | %-30.30s\033[K\n", "Title", "Artist",
         "Album");
  printf("   %-25.25s-+-%-25.25s-+-%-30.30s\033[K\n",
         "------------------------", "------------------------",
         "------------------------------");

  // how many lines we can show, leaving some space for header/footer
  int max_lines = ui_state->height - 18;
  if (max_lines < 3)
    max_lines = 3;

  // clamp offset
  if (ui_state->track_offset < 0)
    ui_state->track_offset = 0;
  if (ui_state->track_offset > ui_state->track_count - 1)
    ui_state->track_offset =
        ui_state->track_count > 0 ? ui_state->track_count - 1 : 0;

  for (int i = 0; i < max_lines; ++i) {
    int idx = ui_state->track_offset + i;
    if (idx >= ui_state->track_count) {
      printf("\033[K\n");
      continue;
    }

    const Track *t = &ui_state->tracks[idx];
    char marker = (idx == ui_state->selected_index ? '>' : ' ');

    printf("%c %2d %-25.25s | %-25.25s | %-30.30s\033[K\n", marker, idx + 1,
           t->title[0] ? t->title : "-", t->artist[0] ? t->artist : "-",
           t->album[0] ? t->album : "-");
  }
  if (ui_state->track_count == 0) {
    printf("  (no tracks loaded)\033[K\n");
  }

  // Controls help
  printf("Controls: [P] Play/Pause  [S] Stop  [Q] Quit\033[K\n");
  printf("          [+/-] Volume    [A] Add folder   [↑/↓] Select  [ENTER] "
         "Play\033[K\n");
  printf("          [R] Repeat mode  [F] Shuffle on/off\033[K\n");

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
      if (ui_state->track_count > 0 && ui_state->selected_index > 0) {
        ui_state->selected_index--;

        // keep selection within window; scroll up if needed
        if (ui_state->selected_index < ui_state->track_offset) {
          ui_state->track_offset = ui_state->selected_index;
        }
      }
      break;
    case 80: // DOWN
      if (ui_state->track_count > 0 &&
          ui_state->selected_index < ui_state->track_count - 1) {

        ui_state->selected_index++;

        int max_lines = ui_state->height - 18;
        if (max_lines < 3)
          max_lines = 3;

        // if selection moved below visible window, scroll down
        if (ui_state->selected_index >= ui_state->track_offset + max_lines) {

          ui_state->track_offset = ui_state->selected_index - max_lines + 1;
        }
      }
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
  case 'r':
  case 'R':
    if (player->repeat_mode == REPEAT_NONE)
      player->repeat_mode = REPEAT_ONE;
    else if (player->repeat_mode == REPEAT_ONE)
      player->repeat_mode = REPEAT_ALL;
    else
      player->repeat_mode = REPEAT_NONE;
    break;

  case 'f':
  case 'F': // F like "shuffle"
    player->shuffle = !player->shuffle;
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

void ui_handle_track_end(Player *player, UIState *ui_state) {
  if (ui_state->track_count == 0)
    return;

  // find current index in playlist (by filepath)
  int current = ui_state->selected_index;
  for (int i = 0; i < ui_state->track_count; ++i) {
    if (strcmp(ui_state->tracks[i].filepath, player->current_track.filepath) ==
        0) {
      current = i;
      break;
    }
  }

  switch (player->repeat_mode) {
  case REPEAT_ONE:
    // simply restart current track
    play_track_at_index(player, ui_state, current);
    break;

  case REPEAT_ALL:
  case REPEAT_NONE: {
    int next = current;

    if (player->shuffle && ui_state->track_count > 1) {
      // random next (different from current)
      do {
        next = rand() % ui_state->track_count;
      } while (next == current);
    } else {
      // sequential
      next = current + 1;
    }

    // end-of-playlist behavior
    if (next >= ui_state->track_count) {
      if (player->repeat_mode == REPEAT_ALL) {
        next = 0; // loop playlist
      } else {
        // REPEAT_NONE and no more tracks -> just stop
        return;
      }
    }

    play_track_at_index(player, ui_state, next);
    break;
  }
  }
}
