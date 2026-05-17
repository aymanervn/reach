#ifndef REACH_CONFIG_H
#define REACH_CONFIG_H

#include <stdint.h>

#include "reach/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_dock_config {
    int32_t height_px;
    int32_t icon_size_px;
    int32_t auto_hide;
    double animation_seconds;
} reach_dock_config;

typedef struct reach_hotkey_config {
    uint32_t modifiers;
    uint32_t key;
    uint32_t command;
} reach_hotkey_config;

typedef struct reach_config reach_config;

reach_result reach_config_create(reach_config **out_config);
void reach_config_destroy(reach_config *config);
reach_result reach_config_load_ini(reach_config *config, const uint16_t *path);
reach_result reach_config_save_ini(const reach_config *config, const uint16_t *path);
reach_result reach_config_get_dock(const reach_config *config, reach_dock_config *out_config);

#ifdef __cplusplus
}
#endif

#endif
