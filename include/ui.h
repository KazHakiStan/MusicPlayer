#ifndef UI_H
#define UI_H

#ifndef UI_MAX_PATH
#define UI_MAX_PATH 260
#endif

#include "player.h"
#include <minwindef.h>

typedef enum { UI_MODE_FULL, UI_MODE_COMPACT } UiMode;
typedef enum { SCREEN_MAIN, SCREEN_FOLDER_PICKER } UiScreen;

typedef struct {
  int x;
  int y;
  int w;
  int h;
} UiRect;

typedef enum {
  UI_SECTION_BANNER,
  UI_SECTION_HEADER,
  UI_SECTION_NAV,
  UI_SECTION_MAIN,
  UI_SECTION_FOOTER,
  UI_SECTION_COUNT
} UiSectionId;

typedef struct UIState UIState;

typedef struct UiComponent UiComponent;

typedef void (*UiComponentDrawFn)(UiComponent *self, const Player *player,
                                  const UIState *ui, UiRect area);

typedef void (*UiComponentInputFn)(UiComponent *self, Player *player,
                                   UIState *ui, int key);

typedef void (*UiComponentResizeFn)(UiComponent *self, const UIState *ui,
                                    UiRect area);

struct UiComponent {
  const char *id;
  bool enabled;

  UiSectionId section;

  int min_height;
  int pref_height;

  UiComponentDrawFn draw;
  UiComponentInputFn handle_input;
  UiComponentResizeFn resize;

  void *userdata;
};

typedef struct {
  UiRect rect;
  int component_count;
  UiComponent *components[16];
} UiSection;

struct UIState {
  int width;
  int height;
  bool should_quit;

  Track *tracks;
  int track_count;
  int selected_index;
  int track_offset;

  bool has_update;
  char latest_version[32];

  bool dirty;
  DWORD last_prog_tick;

  UiMode mode;
  UiSection sections[UI_SECTION_COUNT];
  UiScreen screen;

  char folder_current[UI_MAX_PATH];
  int folder_selected;
  int folder_offset;
};

// UI functions
void ui_init(void);
void ui_cleanup(void);
void ui_draw(const Player *player, UIState *ui_state);
void ui_handle_input(Player *player, UIState *ui_state);
void ui_get_terminal_size(int *width, int *height);
void ui_handle_track_end(Player *player, UIState *ui_state);

void ui_init_state(UIState *ui, UiMode mode);
void ui_compute_layout(UIState *ui);

#endif
