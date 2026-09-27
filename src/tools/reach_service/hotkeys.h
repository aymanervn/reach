#ifndef REACH_SERVICE_HOTKEYS_H
#define REACH_SERVICE_HOTKEYS_H

#include "reach/protocol/reach_service_protocol.h"

#include <stdint.h>

typedef int32_t (*reach_helper_game_mode_active_fn)(void);

struct reach_helper_hotkey_callbacks
{
    reach_helper_game_mode_active_fn game_mode_active;
};

void reach_helper_hotkeys_configure(const reach_helper_hotkey_callbacks *callbacks);
reach_result reach_helper_start_hotkeys(void);
void reach_helper_stop_hotkeys(void);
void reach_helper_clear_hotkey_state(void);

#endif
