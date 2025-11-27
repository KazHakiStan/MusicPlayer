#include "ui.h"
#include "ctype.h"
#include "direct.h"
#include "string.h"
#include "version.h"
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>

#define MAX_COMPONENTS 16
static int g_first_draw = 1;
static UiComponent g_components[MAX_COMPONENTS];
static int g_component_count = 0;

typedef struct {
  char name_display[MAX_PATH];
  char path_utf8[MAX_PATH];
  bool is_parent;
  bool is_drive;
} FolderItem;

// Clear rectangular region in the terminal.
static void clear_rect(UiRect area) {
  if (area.w <= 0 || area.h <= 0)
    return;

  for (int row = 0; row < area.h; ++row) {
    // Move cursor to the beginning of this row
    printf("\033[%d;%dH", area.y + 1 + row, area.x + 1);

    // Overwrite with spaces up to area.w
    for (int col = 0; col < area.w; ++col) {
      putchar(' ');
    }

    // Just in case, clear anything beyond that to end of line
    printf("\033[K");
  }
}

static UiComponent *register_component(UiSectionId section, const char *id,
                                       UiComponentDrawFn draw,
                                       UiComponentInputFn input,
                                       UiComponentResizeFn resize,
                                       int pref_height) {
  if (g_component_count >= MAX_COMPONENTS)
    return NULL;
  UiComponent *c = &g_components[g_component_count++];
  c->id = id;
  c->enabled = true;
  c->section = section;
  c->draw = draw;
  c->handle_input = input;
  c->resize = resize;
  c->min_height = 1;
  c->pref_height = pref_height;
  c->userdata = NULL;
  return c;
}

static UiComponent *find_component(const char *id) {
  for (int i = 0; i < g_component_count; ++i) {
    if (strcmp(g_components[i].id, id) == 0)
      return &g_components[i];
  }
  return NULL;
}

// Draw a solid horizontal separator using Unicode box-drawing characters.
static void draw_hline(int width) {
  if (width <= 0)
    return;

  // One UTF-8 char: '─' (U+2500)
  // In source file: make sure file is saved as UTF-8.
  for (int i = 0; i < width; ++i) {
    fputs("─", stdout);
  }
  printf("\033[K\n"); // clear the rest of line and newline
}

static void comp_banner_draw(UiComponent *, const Player *, const UIState *,
                             UiRect);
static void comp_now_playing_draw(UiComponent *, const Player *,
                                  const UIState *, UiRect);
static void comp_navigation_draw(UiComponent *, const Player *, const UIState *,
                                 UiRect);
static void comp_playlist_draw(UiComponent *, const Player *, const UIState *,
                               UiRect);
static void comp_volume_draw(UiComponent *, const Player *, const UIState *,
                             UiRect);
static void comp_progress_draw(UiComponent *, const Player *, const UIState *,
                               UiRect);
static void comp_footer_controls_draw(UiComponent *, const Player *,
                                      const UIState *, UiRect);

static void comp_banner_draw(UiComponent *self, const Player *player,
                             const UIState *ui, UiRect area) {
  (void)self;
  (void)player;
  (void)ui;

  printf("=== CLI Music Player (v%s) ===\033[K\n", MP_VERSION);
}

static void comp_now_playing_draw(UiComponent *self, const Player *player,
                                  const UIState *ui, UiRect area) {
  (void)self;
  (void)ui;
  (void)area;

  printf("Now Playing: %s by %s from %s\033[K\n",
         player->current_track.title[0] ? player->current_track.title : "-",
         player->current_track.artist[0] ? player->current_track.artist : "-",
         player->current_track.album[0] ? player->current_track.album : "-");
}

static void comp_navigation_draw(UiComponent *self, const Player *player,
                                 const UIState *ui, UiRect area) {
  (void)self;
  (void)ui;
  (void)player;
  (void)area;

  printf("Playlist\033[K\n");
}

static void comp_progress_draw(UiComponent *self, const Player *player,
                               const UIState *ui, UiRect area) {
  (void)self;
  (void)area;

  if (player->current_track.duration <= 0.0) {
    printf("\033[K\n");
    return;
  }

  double progress = player->position / player->current_track.duration;
  if (progress < 0.0)
    progress = 0.0;
  if (progress > 1.0)
    progress = 1.0;

  int bar_width = ui->width - 20;
  if (bar_width < 10)
    bar_width = 10;

  int pos = (int)(progress * (bar_width - 1));

  printf("[");
  for (int i = 0; i < bar_width; ++i) {
    putchar(i <= pos ? '=' : ' ');
  }
  printf("] %.1f/%.1f\033[K\n", player->position,
         player->current_track.duration);
}

static void comp_volume_draw(UiComponent *self, const Player *player,
                             const UIState *ui, UiRect area) {
  (void)self;
  (void)ui;
  (void)area;

  printf("Volume: %d%%\033[K\n", (int)(player->volume * 100));
}

static void comp_playlist_draw(UiComponent *self, const Player *player,
                               const UIState *ui, UiRect area) {
  (void)self;
  (void)player;

  int max_lines = area.h;
  if (max_lines <= 0)
    return;

  // header
  // printf("Playlist (A: add folder, ENTER: play)\033[K\n");
  // max_lines--;

  if (max_lines <= 0)
    return;

  printf("   %-25.25s | %-25.25s | %-30.30s\033[K\n", "Title", "Artist",
         "Album");
  max_lines--;

  if (max_lines <= 0)
    return;

  printf("   %-25.25s-+-%-25.25s-+-%-30.30s\033[K\n",
         "------------------------", "------------------------",
         "------------------------------");
  max_lines--;

  int start = ui->track_offset;
  for (int i = 0; i < max_lines; ++i) {
    int idx = start + i;
    if (idx >= ui->track_count) {
      printf("\033[K\n");
      continue;
    }

    const Track *t = &ui->tracks[idx];
    char marker = (idx == ui->selected_index) ? '>' : ' ';

    printf("%c %2d %-25.25s | %-25.25s | %-30.30s\033[K\n", marker, idx + 1,
           t->title[0] ? t->title : "-", t->artist[0] ? t->artist : "-",
           t->album[0] ? t->album : "-");
  }

  if (ui->track_count == 0) {
    printf("  (no tracks loaded)\033[K\n");
  }
}

static void comp_footer_controls_draw(UiComponent *self, const Player *player,
                                      const UIState *ui, UiRect area) {
  (void)self;
  (void)ui;
  (void)area;

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

  printf("Repeat: %s  |  Shuffle: %s\033[K\n", repeat_str,
         player->shuffle ? "ON" : "OFF");

  printf("Controls: [P] Play/Pause  [S] Stop  [Q] Quit\033[K\n");
  printf("          [+/-] Volume    [A] Add folder   [↑/↓] Select  [ENTER] "
         "Play\033[K\n");
  printf("          [R] Repeat mode  [F] Shuffle on/off\033");
}

static void draw_main_screen_components(const Player *player, UIState *ui) {
  if (g_first_draw) {
    printf("\033[2J");
    g_first_draw = 0;
  }

  ui_compute_layout(ui);

  // Move cursor home
  printf("\033[H");

  // Now draw sections via components
  for (int s = 0; s < UI_SECTION_COUNT; ++s) {
    UiSection *sec = &ui->sections[s];
    UiRect r = sec->rect;

    // clear_rect(r);

    int current_y = r.y;

    // Move cursor to the top of this section (row is 0-based, so +1)
    printf("\033[%d;1H", current_y + 1);

    for (int i = 0; i < sec->component_count; ++i) {
      UiComponent *c = sec->components[i];
      if (!c->enabled || !c->draw)
        continue;

      UiRect comp_rect = (UiRect){.x = r.x, .y = current_y, .w = r.w, .h = 1};

      // playlist uses remaining height
      if (strcmp(c->id, "playlist") == 0) {
        comp_rect.h = r.y + r.h - current_y;
      }

      printf("\033[%d;1H", comp_rect.y + 1);
      c->draw(c, player, ui, comp_rect);

      current_y += comp_rect.h;
      if (current_y >= r.y + r.h)
        break;
    }

    // After certain sections, draw separators
    if (s != UI_SECTION_FOOTER) {
      printf("\033[%d;1H", r.y + r.h + 1); // next line after section
      draw_hline(ui->width);
      // Sleep(4000);
    }
  }
}

// Truncate UTF-8 string to at most max_chars characters, safely.
// Returns the number of characters written to dst (not counting '\0').
static int utf8_truncate(const char *src, char *dst, int dst_size,
                         int max_chars) {
  if (!src || !dst || dst_size <= 0 || max_chars <= 0) {
    if (dst_size > 0)
      dst[0] = '\0';
    return 0;
  }

  int chars = 0;
  const unsigned char *s = (const unsigned char *)src;
  char *d = dst;
  char *d_end = dst + dst_size - 1; // leave space for '\0'

  while (*s && d < d_end && chars < max_chars) {
    unsigned char c = *s;
    int len = 1;

    if ((c & 0x80) == 0x00) {
      len = 1; // ASCII
    } else if ((c & 0xE0) == 0xC0) {
      len = 2; // 2-byte
    } else if ((c & 0xF0) == 0xE0) {
      len = 3; // 3-byte
    } else if ((c & 0xF8) == 0xF0) {
      len = 4; // 4-byte
    } else {
      // invalid byte, skip
      s++;
      continue;
    }

    if (d + len > d_end)
      break;

    for (int i = 0; i < len; ++i) {
      *d++ = *s++;
    }

    chars++;
  }

  *d = '\0';
  return chars;
}
/**
 * UTF-8 (char*) -> UTF-16 (wchar_t*)
 */
static int utf8_to_utf16(const char *src, wchar_t *dst, int dst_len) {
  if (!src || !dst || dst_len <= 0)
    return 0;

  int wlen = MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, dst_len);
  if (wlen <= 0) {
    dst[0] = L'\0';
    return 0;
  }
  return wlen;
}

/**
 * UTF-16 (wchar_t*) -> UTF-8 (char*)
 */
static int utf16_to_utf8(const wchar_t *src, char *dst, int dst_len) {
  if (!src || !dst || dst_len <= 0)
    return 0;

  int len = WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, dst_len, NULL, NULL);
  if (len <= 0) {
    dst[0] = '\0';
    return 0;
  }
  return len;
}

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

      // UTF-8 path like "C:\"
      snprintf(it->path_utf8, sizeof(it->path_utf8), "%c:\\", letter);
      snprintf(it->name_display, sizeof(it->name_display), "%c:\\", letter);

      it->is_parent = false;
      it->is_drive = true;
    }
  }
  return count;
}

static int list_subdirs2(const char *base_utf8, FolderItem *items,
                         int max_items) {
  int count = 0;

  // Parent entry ".."
  if (base_utf8 && base_utf8[0] != '\0') {
    FolderItem *p = &items[count++];
    strcpy(p->name_display, "..");
    p->path_utf8[0] = '\0'; // will be handled specially
    p->is_parent = true;
    p->is_drive = false;
  }

  // Build UTF-16 search path: base_utf8 + "\\*"
  wchar_t base_w[MAX_PATH];
  if (!utf8_to_utf16(base_utf8, base_w, MAX_PATH)) {
    return count;
  }

  wchar_t search_w[MAX_PATH];
  swprintf(search_w, MAX_PATH, L"%ls\\*", base_w);

  WIN32_FIND_DATAW ffd;
  HANDLE hFind = FindFirstFileW(search_w, &ffd);
  if (hFind == INVALID_HANDLE_VALUE)
    return count;

  do {
    if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0)
        continue;

      if (count >= max_items)
        break;

      FolderItem *it = &items[count++];

      // full UTF-16 path = base_w + "\" + ffd.cFileName
      wchar_t full_w[MAX_PATH];
      swprintf(full_w, MAX_PATH, L"%ls\\%ls", base_w, ffd.cFileName);

      // convert full path to UTF-8 for path_utf8
      utf16_to_utf8(full_w, it->path_utf8, sizeof(it->path_utf8));

      // convert folder name to UTF-8 for display
      utf16_to_utf8(ffd.cFileName, it->name_display, sizeof(it->name_display));

      it->is_parent = false;
      it->is_drive = false;
    }
  } while (FindNextFileW(hFind, &ffd));

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
static void add_folder_mp3s_recursive(UIState *ui_state,
                                      const char *folder_utf8) {
  WIN32_FIND_DATAW ffd;
  HANDLE hFind;
  wchar_t folder_w[MAX_PATH];
  wchar_t search_w[MAX_PATH];

  if (!utf8_to_utf16(folder_utf8, folder_w, MAX_PATH)) {
    return;
  }

  // 1) Add all *.mp3 in this folder
  swprintf(search_w, MAX_PATH, L"%ls\\*.mp3", folder_w);
  hFind = FindFirstFileW(search_w, &ffd);
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

      // full path in UTF-16
      wchar_t full_w[MAX_PATH];
      swprintf(full_w, MAX_PATH, L"%ls\\%ls", folder_w, ffd.cFileName);

      // convert full path to UTF-8 for Track.filepath
      utf16_to_utf8(full_w, t->filepath, sizeof(t->filepath));

      // default title from filename (UTF-8)
      utf16_to_utf8(ffd.cFileName, t->title, sizeof(t->title));

      strcpy(t->artist, "Unknown Artist");
      strcpy(t->album, "Unknown Album");
      t->duration = 0.0;

      // your existing metadata loader (still char*)
      player_fill_metadata_from_file(t->filepath, t);

      ui_state->track_count++;
    } while (FindNextFileW(hFind, &ffd));
    FindClose(hFind);
  }

  // 2) Recurse into subdirectories
  swprintf(search_w, MAX_PATH, L"%ls\\*", folder_w);
  hFind = FindFirstFileW(search_w, &ffd);
  if (hFind == INVALID_HANDLE_VALUE)
    return;

  do {
    if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0)
        continue;

      wchar_t sub_w[MAX_PATH];
      swprintf(sub_w, MAX_PATH, L"%ls\\%ls", folder_w, ffd.cFileName);

      char sub_utf8[MAX_PATH];
      utf16_to_utf8(sub_w, sub_utf8, sizeof(sub_utf8));

      add_folder_mp3s_recursive(ui_state, sub_utf8);
    }
  } while (FindNextFileW(hFind, &ffd));

  FindClose(hFind);
}

static void add_folder_mp3s(UIState *ui_state, const char *folder_utf8) {
  add_folder_mp3s_recursive(ui_state, folder_utf8);

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
      printf("Location: %s\033[K\n\n", current);
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
      ui_state->screen = SCREEN_MAIN;
      ui_state->dirty = true;
      break;
    }

    if (ch == 's' || ch == 'S') {
      if (current[0] != '\0') {
        add_folder_mp3s(ui_state, current); // current = UTF-8 path
      }
      ui_state->screen = SCREEN_MAIN;
      ui_state->dirty = true;
      break;
    }

    if (ch == '\r') { // ENTER
      if (count == 0)
        continue;

      FolderItem *sel = &items[selected];

      if (current[0] == '\0') {
        // selecting a drive
        strncpy(current, sel->path_utf8, sizeof(current) - 1);
        current[sizeof(current) - 1] = '\0';
        selected = 0;
        offset = 0;
      } else {
        if (sel->is_parent) {
          // go up one level in UTF-8 path
          char *last = strrchr(current, '\\');
          if (last && last > current + 2) { // e.g. "C:\folder"
            *last = '\0';
          } else {
            current[0] = '\0'; // back to drives
          }
          selected = 0;
          offset = 0;
        } else if (sel->path_utf8[0]) {
          strncpy(current, sel->path_utf8, sizeof(current) - 1);
          current[sizeof(current) - 1] = '\0';
          selected = 0;
          offset = 0;
        }
      }
    }
  }
}

static void draw_folder_picker(UIState *ui) {
  int w, h;
  ui_get_terminal_size(&w, &h);
  int max_lines = h - 8; // header + footer
  if (max_lines < 3)
    max_lines = 3;

  FolderItem items[256];
  int count = 0;

  if (ui->folder_current[0] == '\0') {
    // show drives
    count = list_drives(items, 256);
  } else {
    // show subdirectories of current folder
    count = list_subdirs2(ui->folder_current, items, 256);
  }

  // clamp selection
  if (ui->folder_selected >= count)
    ui->folder_selected = (count > 0 ? count - 1 : 0);
  if (ui->folder_selected < 0)
    ui->folder_selected = 0;

  // clamp offset
  if (ui->folder_offset > ui->folder_selected)
    ui->folder_offset = ui->folder_selected;
  if (ui->folder_selected >= ui->folder_offset + max_lines)
    ui->folder_offset = ui->folder_selected - max_lines + 1;
  if (ui->folder_offset < 0)
    ui->folder_offset = 0;
  if (ui->folder_offset > count - 1)
    ui->folder_offset = (count > 0 ? count - 1 : 0);

  // draw
  printf("\033[H"); // go to top-left
  printf("=== Select folder with MP3 files ===\033[K\n\n");

  if (ui->folder_current[0] == '\0')
    printf("Location: [drives]\033[K\n\n");
  else
    printf("Location: %s\033[K\n\n", ui->folder_current);

  if (count == 0) {
    printf("  (no subfolders)\033[K\n");
  }

  for (int i = 0; i < max_lines; ++i) {
    int idx = ui->folder_offset + i;
    if (idx >= count) {
      printf("\033[K\n");
      continue;
    }
    FolderItem *it = &items[idx];
    char mark = (idx == ui->folder_selected) ? '>' : ' ';
    printf("%c %s\033[K\n", mark, it->name_display);
  }

  printf("\nControls: ↑/↓ move  ENTER select  S = use this folder  Q = "
         "cancel\033[K\n");
  fflush(stdout);
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

void ui_draw(const Player *player, UIState *ui) {
  if (ui->screen == SCREEN_FOLDER_PICKER) {
    draw_folder_picker(ui); // what prompt_add_folder does now
    return;
  } else {
    draw_main_screen_components(player, ui);
  }

  fflush(stdout);
}

void ui_handle_input(Player *player, UIState *ui_state) {
  if (!_kbhit())
    return;

  int ch = _getch();

  // Folder picker mode
  if (ui_state->screen == SCREEN_FOLDER_PICKER) {
    FolderItem items[256];
    int count = 0;

    if (ui_state->folder_current[0] == '\0') {
      count = list_drives(items, 256);
    } else {
      count = list_subdirs2(ui_state->folder_current, items, 256);
    }

    // Arrow keys
    if (ch == 0 || ch == 0xE0) {
      int code = _getch();
      if (code == 72) { // UP
        if (ui_state->folder_selected > 0)
          ui_state->folder_selected--;
      } else if (code == 80) { // DOWN
        if (ui_state->folder_selected < count - 1)
          ui_state->folder_selected++;
      }

      ui_state->dirty = true;
      return;
    }

    // Other keys
    switch (ch) {
    case 'q':
    case 'Q':
    case 27: // ESC
      ui_state->screen = SCREEN_MAIN;
      ui_state->dirty = true;
      return;

    case 's':
    case 'S':
      if (ui_state->folder_current[0] != '\0') {
        add_folder_mp3s(ui_state, ui_state->folder_current);
      }
      ui_state->screen = SCREEN_MAIN;
      ui_state->dirty = true;
      return;

    case '\r': // ENTER
      if (count == 0)
        return;

      {
        FolderItem *sel = &items[ui_state->folder_selected];

        if (ui_state->folder_current[0] == '\0') {
          // selecting a drive
          strncpy(ui_state->folder_current, sel->path_utf8,
                  sizeof(ui_state->folder_current) - 1);
          ui_state->folder_current[sizeof(ui_state->folder_current) - 1] = '\0';
          ui_state->folder_selected = 0;
          ui_state->folder_offset = 0;
        } else {
          if (sel->is_parent) {
            // go up
            char *last = strrchr(ui_state->folder_current, '\\');
            if (last && last > ui_state->folder_current + 2) {
              *last = '\0';
            } else {
              ui_state->folder_current[0] = '\0'; // back to drives
            }
            ui_state->folder_selected = 0;
            ui_state->folder_offset = 0;
          } else if (sel->path_utf8[0]) {
            // go down into subfolder
            strncpy(ui_state->folder_current, sel->path_utf8,
                    sizeof(ui_state->folder_current) - 1);
            ui_state->folder_current[sizeof(ui_state->folder_current) - 1] =
                '\0';
            ui_state->folder_selected = 0;
            ui_state->folder_offset = 0;
          }
        }
      }
      ui_state->dirty = true;
      return;
    }

    return;
  }

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

        ui_state->dirty = true;
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
        ui_state->dirty = true;
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
    ui_state->dirty = true;
    break;

  case 's':
  case 'S':
    player_stop(player);
    ui_state->dirty = true;
    break;
  case 'r':
  case 'R':
    if (player->repeat_mode == REPEAT_NONE)
      player->repeat_mode = REPEAT_ONE;
    else if (player->repeat_mode == REPEAT_ONE)
      player->repeat_mode = REPEAT_ALL;
    else
      player->repeat_mode = REPEAT_NONE;
    ui_state->dirty = true;
    break;

  case 'f':
  case 'F': // F like "shuffle"
    player->shuffle = !player->shuffle;
    ui_state->dirty = true;
    break;
  case '+':
    player_set_volume(player, player->volume + 0.1);
    ui_state->dirty = true;
    break;
  case '-':
    player_set_volume(player, player->volume - 0.1);
    ui_state->dirty = true;
    break;

  // NEW: add folder
  case 'a':
  case 'A':
    ui_state->screen = SCREEN_FOLDER_PICKER;
    // prompt_add_folder(ui_state);
    ui_state->dirty = true;
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
      ui_state->dirty = true;
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
    ui_state->dirty = true;
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
    ui_state->dirty = true;
    break;
  }
  }
}

void ui_compute_layout(UIState *ui) {
  ui_get_terminal_size(&ui->width, &ui->height);
  int w = ui->width;
  int h = ui->height;

  const int MIN_W = 80;
  const int MIN_H = 24;

  UiMode old_mode = ui->mode;
  UiMode new_mode = (w >= MIN_W && h >= MIN_H) ? UI_MODE_FULL : UI_MODE_COMPACT;

  if (new_mode != old_mode) {
    ui->mode = new_mode;
    ui->dirty = true;

    UiComponent *header_progress = find_component("header_progress");
    UiComponent *footer_controls = find_component("footer_controls");
    UiComponent *playlist = find_component("playlist");

    if (ui->mode == UI_MODE_COMPACT) {
      // if (header_progress)
      //   header_progress->enabled = false;
      if (footer_controls)
        footer_controls->enabled = false;
      if (playlist)
        playlist->enabled = false;
    } else {
      // if (header_progress)
      //   header_progress->enabled = true;
      if (footer_controls)
        footer_controls->enabled = true;
      if (playlist)
        playlist->enabled = true;
    }
  }

  UiRect banner = (UiRect){0, 0, w, 1};
  UiRect header = (UiRect){0, 2, w, 3};
  UiRect nav = (UiRect){0, 6, w, 1};
  UiRect footer = (UiRect){0, h - 4, w, 4};
  UiRect main = (UiRect){0, banner.h + header.h + nav.h + 3, w,
                         h - banner.h - header.h - nav.h - footer.h - 4};

  if (ui->mode == UI_MODE_COMPACT) {
    header.h = 2;
    nav.h = 0;
    footer.h = 2;
    main.y = header.h;
    main.h = h - header.h - footer.h;
  }

  ui->sections[UI_SECTION_BANNER].rect = banner;
  ui->sections[UI_SECTION_HEADER].rect = header;
  ui->sections[UI_SECTION_NAV].rect = nav;
  ui->sections[UI_SECTION_MAIN].rect = main;
  ui->sections[UI_SECTION_FOOTER].rect = footer;
}

void ui_init_state(UIState *ui, UiMode mode) {
  ui->mode = mode;
  ui->dirty = true;
  ui->last_prog_tick = GetTickCount();

  g_component_count = 0;

  // banner components
  UiComponent *banner = register_component(UI_SECTION_BANNER, "banner",
                                           comp_banner_draw, NULL, NULL, 1);

  // header components
  UiComponent *now_playing = register_component(
      UI_SECTION_HEADER, "now_playing", comp_now_playing_draw, NULL, NULL, 1);

  UiComponent *header_progress = register_component(
      UI_SECTION_HEADER, "header_progress", comp_progress_draw, NULL, NULL, 1);

  UiComponent *volume = register_component(UI_SECTION_HEADER, "volume",
                                           comp_volume_draw, NULL, NULL, 1);

  // navifation components
  UiComponent *navigation = register_component(
      UI_SECTION_NAV, "navigation", comp_navigation_draw, NULL, NULL, 1);

  // main components
  UiComponent *playlist = register_component(UI_SECTION_MAIN, "playlist",
                                             comp_playlist_draw, NULL, NULL, 0);

  // footer
  UiComponent *footer_controls =
      register_component(UI_SECTION_FOOTER, "footer_controls",
                         comp_footer_controls_draw, NULL, NULL, 2);

  // attach components to sections
  for (int i = 0; i < UI_SECTION_COUNT; ++i) {
    ui->sections[i].component_count = 0;
  }
  for (int i = 0; i < g_component_count; ++i) {
    UiComponent *c = &g_components[i];
    UiSection *sec = &ui->sections[c->section];
    if (sec->component_count < MAX_COMPONENTS) {
      sec->components[sec->component_count++] = c;
    }
  }

  // mode-specific toggles
  if (ui->mode == UI_MODE_COMPACT) {
    header_progress->enabled = false;
    footer_controls->enabled = false;
    // maybe disable volume or footer_controls if you want extreme compact
  }
}
