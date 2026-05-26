#ifndef REACH_PORTS_HOTKEYS_H
#define REACH_PORTS_HOTKEYS_H

#include "reach/core/hotkey.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_hotkeys reach_hotkeys;

typedef struct reach_hotkeys_ops {
    void (*destroy)(reach_hotkeys *hotkeys);
    reach_result (*register_hotkeys)(reach_hotkeys *hotkeys, const reach_hotkey_config *config, uint32_t count);
    reach_result (*unregister_all)(reach_hotkeys *hotkeys);
    reach_hotkey_command (*translate_registered)(const reach_hotkeys *hotkeys, uint32_t id);
} reach_hotkeys_ops;

typedef struct reach_hotkeys_port {
    reach_hotkeys *hotkeys;
    reach_hotkeys_ops ops;
} reach_hotkeys_port;

#ifdef __cplusplus
}
#endif

#endif
