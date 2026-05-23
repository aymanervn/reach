#ifndef REACH_HOTKEYS_H
#define REACH_HOTKEYS_H

#include <stdint.h>

#include "reach/app/config.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_hotkeys reach_hotkeys;

typedef enum reach_hotkey_command {
    REACH_HOTKEY_NONE = 0,
    REACH_HOTKEY_SNAP_LEFT = 1,
    REACH_HOTKEY_SNAP_RIGHT = 2,
    REACH_HOTKEY_SNAP_TOP = 3,
    REACH_HOTKEY_SNAP_BOTTOM = 4,
    REACH_HOTKEY_SNAP_FULL = 5,
    REACH_HOTKEY_SEARCH = 6
} reach_hotkey_command;

reach_result reach_hotkeys_create(reach_hotkeys **out_hotkeys);
void reach_hotkeys_destroy(reach_hotkeys *hotkeys);
reach_result reach_hotkeys_register(reach_hotkeys *hotkeys, const reach_hotkey_config *config, uint32_t count);
reach_result reach_hotkeys_unregister_all(reach_hotkeys *hotkeys);
reach_hotkey_command reach_hotkeys_translate(uint32_t id);
reach_hotkey_command reach_hotkeys_translate_registered(const reach_hotkeys *hotkeys, uint32_t id);

#ifdef __cplusplus
}
#endif

#endif
