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
#define REACH_CONFIG_DOCK_SIZE_MIN 50
#define REACH_CONFIG_DOCK_SIZE_MAX 100
#define REACH_CONFIG_GLASS_INTENSITY_MAX 100
#define REACH_CONFIG_GLASS_INTENSITY_DEFAULT 64

    typedef enum reach_desktop_widget
    {
        REACH_WIDGET_CLOCK,
        REACH_WIDGET_SYSTEM,
        REACH_WIDGET_NOW_PLAYING,
        REACH_WIDGET_CALENDAR,
        REACH_WIDGET_STOPWATCH,
        REACH_WIDGET_POMODORO,
        REACH_MAX_DESKTOP_WIDGETS
    } reach_desktop_widget;

#define REACH_DESKTOP_WIDGET_MASK ((1u << REACH_MAX_DESKTOP_WIDGETS) - 1u)

    typedef struct reach_config_widget_placement
    {
        int32_t x;
        int32_t y;
        int32_t width;
        int32_t height;
    } reach_config_widget_placement;

    typedef struct reach_config_widget_limits
    {
        int32_t min_width;
        int32_t min_height;
        int32_t max_width;
        int32_t max_height;
    } reach_config_widget_limits;

    typedef struct reach_widget_definition
    {
        const uint16_t *label;
        size_t ini_slot;
        int32_t default_width;
        int32_t default_height;
        reach_config_widget_limits limits;
    } reach_widget_definition;

    const reach_widget_definition *reach_widget_definition_at(size_t index);

    typedef enum reach_config_theme_preference
    {
        REACH_CONFIG_THEME_FOLLOW_REACH = 0,
        REACH_CONFIG_THEME_LIGHT = 1,
        REACH_CONFIG_THEME_DARK = 2
    } reach_config_theme_preference;

    typedef enum reach_config_top_bar_style
    {
        REACH_CONFIG_TOP_BAR_STYLE_SPLIT = 0,
        REACH_CONFIG_TOP_BAR_STYLE_UNIFIED = 1,
        REACH_CONFIG_TOP_BAR_STYLE_TRANSPARENT = 2
    } reach_config_top_bar_style;

    typedef enum reach_config_top_bar_mode
    {
        REACH_CONFIG_TOP_BAR_MODE_DYNAMIC = 0,
        REACH_CONFIG_TOP_BAR_MODE_STATIC = 1
    } reach_config_top_bar_mode;

    typedef struct reach_config_glass_settings
    {
        int32_t enabled;
        int32_t intensity;
    } reach_config_glass_settings;

    typedef struct reach_config_snapshot
    {
        uint16_t version[32];
        reach_pinned_app_model pinned_apps[REACH_MAX_PINNED_APPS];
        size_t pinned_app_count;
        uint16_t wallpaper_path[260];
        uint16_t monitor_wallpaper_paths[REACH_MAX_WALLPAPER_MONITORS][260];
        float dock_height;
        uint32_t desktop_widgets;
        reach_config_widget_placement widget_placements[REACH_MAX_DESKTOP_WIDGETS];
        int32_t dock_size;
        reach_config_top_bar_style top_bar_style;
        reach_config_top_bar_mode top_bar_mode;
        int32_t top_bar_icon_only;
        int32_t top_bar_overlay;
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
        int32_t accent_preset;
        reach_config_glass_settings glass;
        reach_config_theme_preference windows_system_theme;
        reach_config_theme_preference windows_app_theme;
        int32_t stage_animation_ms;
    } reach_config_snapshot;

#ifdef __cplusplus
}
#endif

#endif
