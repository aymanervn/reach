#ifndef REACH_ELEVATION_HELPER_HOTKEYS_H
#define REACH_ELEVATION_HELPER_HOTKEYS_H

#include "../../adapters/windows/window_management/elevation_helper_protocol.h"

#include <stdint.h>
#include <windows.h>

typedef int32_t (*reach_helper_game_mode_active_fn)(void);
typedef int32_t (*reach_helper_minimize_game_fn)(HWND hwnd);

struct reach_helper_hotkey_callbacks
{
    reach_helper_game_mode_active_fn game_mode_active;
    reach_helper_minimize_game_fn minimize_game;
};

void reach_helper_hotkeys_configure(const reach_helper_hotkey_callbacks *callbacks);
reach_result reach_helper_start_hotkeys(void);
void reach_helper_stop_hotkeys(void);
void reach_helper_clear_hotkey_state(void);

#endif
