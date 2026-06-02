#ifndef REACH_THEME_H
#define REACH_THEME_H

#include "reach/core/geometry.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_theme
    {
        reach_color dock_background;
        reach_color switcher_background;
        reach_color dock_border;
        reach_color dock_shadow;
        reach_color icon_box_background;
        reach_color icon_backplate_background;
        reach_color fallback_icon_background;
        reach_color fallback_icon_text;
        reach_color tray_button_background;
        reach_color tray_popup_background;
        reach_color dock_system_separator;
        reach_color dock_clock_time;
        reach_color dock_clock_date;
        reach_color dock_power_button_background;
        reach_color dock_power_glyph;
        reach_color quick_settings_slider_track_color;
        reach_color quick_settings_slider_fill_color;
        reach_color quick_settings_slider_muted_fill_color;
        reach_color quick_settings_expand_button_color;
        reach_color quick_settings_expand_text_color;
        float dock_corner_radius_ratio;
        float dock_corner_radius_max;
        float dock_border_thickness;
        float dock_shadow_alpha;
        float icon_box_height_ratio;
        float icon_max_box_ratio;
        float icon_box_corner_radius_ratio;
        float icon_box_corner_radius_max;
        float tray_slot_size_ratio;
        float dock_power_button_corner_radius;
        float dock_system_separator_width;
        float dock_system_separator_height_ratio;
        float dock_clock_width;
    } reach_theme;

    const reach_theme *reach_theme_default(void);
    float reach_theme_dock_corner_radius(const reach_theme *theme, float dock_height);
    float reach_theme_icon_box_size(const reach_theme *theme, float dock_height);
    float reach_theme_icon_size(const reach_theme *theme, float icon_box_size);
    float reach_theme_icon_box_corner_radius(const reach_theme *theme, float icon_box_size);
    float reach_theme_tray_slot_size(const reach_theme *theme, float dock_height);

#ifdef __cplusplus
}
#endif

#endif
