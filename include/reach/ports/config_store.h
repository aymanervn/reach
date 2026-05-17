#ifndef REACH_PORTS_CONFIG_STORE_H
#define REACH_PORTS_CONFIG_STORE_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/ui_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_config_store reach_config_store;

typedef struct reach_config_snapshot {
    reach_pinned_app_model pinned_apps[REACH_MAX_PINNED_APPS];
    size_t pinned_app_count;
    uint16_t wallpaper_path[260];
    float dock_height;
    float dock_width;
    float dock_icon_size;
} reach_config_snapshot;

typedef struct reach_config_store_ops {
    reach_result (*load)(reach_config_store *store, reach_config_snapshot *out_snapshot);
    reach_result (*save)(reach_config_store *store, const reach_config_snapshot *snapshot);
    void (*destroy)(reach_config_store *store);
} reach_config_store_ops;

typedef struct reach_config_store_port {
    reach_config_store *store;
    reach_config_store_ops ops;
} reach_config_store_port;

#ifdef __cplusplus
}
#endif

#endif
