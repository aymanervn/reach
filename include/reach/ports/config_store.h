#ifndef REACH_PORTS_CONFIG_STORE_H
#define REACH_PORTS_CONFIG_STORE_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/pinned_app.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_config_store reach_config_store;

#define REACH_MAX_WALLPAPER_MONITORS 8

    typedef struct reach_config_snapshot
    {
        uint16_t version[32];
        reach_pinned_app_model pinned_apps[REACH_MAX_PINNED_APPS];
        size_t pinned_app_count;
        uint16_t wallpaper_path[260];
        uint16_t monitor_wallpaper_paths[REACH_MAX_WALLPAPER_MONITORS][260];
        float dock_height;
        float dock_width;
        float dock_icon_size;
        int32_t power_sleep_minutes;
        int32_t power_lock_minutes;
        int32_t power_shutdown_minutes;
        int32_t power_restart_minutes;
        int32_t power_sleep_wait_apps;
        int32_t power_shutdown_wait_apps;
        int32_t power_restart_wait_apps;
        int32_t high_refresh_rate;
    } reach_config_snapshot;

    typedef struct reach_config_store_ops
    {
        reach_result (*load)(reach_config_store *store, reach_config_snapshot *out_snapshot);
        reach_result (*save)(reach_config_store *store, const reach_config_snapshot *snapshot);
        void (*destroy)(reach_config_store *store);
    } reach_config_store_ops;

    typedef struct reach_config_store_port
    {
        reach_config_store *store;
        reach_config_store_ops ops;
    } reach_config_store_port;

#ifdef __cplusplus
}
#endif

#endif
