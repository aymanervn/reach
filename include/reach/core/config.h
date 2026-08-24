#ifndef REACH_CORE_CONFIG_H
#define REACH_CORE_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/pinned_app.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_MAX_WALLPAPER_MONITORS 8

    typedef struct reach_config_snapshot
    {
        uint16_t version[32];
        reach_pinned_app_model pinned_apps[REACH_MAX_PINNED_APPS];
        size_t pinned_app_count;
        uint16_t wallpaper_path[260];
        uint16_t monitor_wallpaper_paths[REACH_MAX_WALLPAPER_MONITORS][260];
        float dock_height;
        int32_t power_screen_off_minutes;
        int32_t power_sleep_minutes;
        int32_t power_lock_minutes;
        int32_t power_shutdown_minutes;
        int32_t power_restart_minutes;
        int32_t power_sleep_wait_apps;
        int32_t power_shutdown_wait_apps;
        int32_t power_restart_wait_apps;
        int32_t high_refresh_rate;
        int32_t bundled_font;
        int32_t light_theme;
        int32_t stage_animation_ms;
    } reach_config_snapshot;

#ifdef __cplusplus
}
#endif

#endif
