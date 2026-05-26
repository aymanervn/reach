#ifndef REACH_CORE_HOTKEY_H
#define REACH_CORE_HOTKEY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum reach_hotkey_command {
    REACH_HOTKEY_NONE = 0,
    REACH_HOTKEY_SNAP_LEFT = 1,
    REACH_HOTKEY_SNAP_RIGHT = 2,
    REACH_HOTKEY_SNAP_TOP = 3,
    REACH_HOTKEY_SNAP_BOTTOM = 4,
    REACH_HOTKEY_SNAP_FULL = 5,
    REACH_HOTKEY_SEARCH = 6
} reach_hotkey_command;

typedef struct reach_hotkey_config {
    uint32_t modifiers;
    uint32_t key;
    uint32_t command;
} reach_hotkey_config;

#ifdef __cplusplus
}
#endif

#endif
