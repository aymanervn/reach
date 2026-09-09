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

    typedef enum reach_config_theme_preference
    {
        REACH_CONFIG_THEME_FOLLOW_REACH = 0,
        REACH_CONFIG_THEME_LIGHT = 1,
        REACH_CONFIG_THEME_DARK = 2
    } reach_config_theme_preference;

    typedef enum reach_config_top_bar_style
    {
        REACH_CONFIG_TOP_BAR_STYLE_SEGMENTED = 0,
        REACH_CONFIG_TOP_BAR_STYLE_UNIFIED = 1
    } reach_config_top_bar_style;

    typedef enum reach_config_top_bar_mode
    {
        REACH_CONFIG_TOP_BAR_MODE_DYNAMIC = 0,
        REACH_CONFIG_TOP_BAR_MODE_STATIC = 1
    } reach_config_top_bar_mode;

    typedef struct reach_config_snapshot
    {
        uint16_t version[32];
        reach_pinned_app_model pinned_apps[REACH_MAX_PINNED_APPS];
        size_t pinned_app_count;
        uint16_t wallpaper_path[260];
        uint16_t monitor_wallpaper_paths[REACH_MAX_WALLPAPER_MONITORS][260];
        float dock_height;
        reach_config_top_bar_style top_bar_style;
        reach_config_top_bar_mode top_bar_mode;
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
        reach_config_theme_preference windows_system_theme;
        reach_config_theme_preference windows_app_theme;
        int32_t stage_animation_ms;
    } reach_config_snapshot;

#ifdef __cplusplus
}
#endif

#endif
