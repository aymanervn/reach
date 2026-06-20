#ifndef REACH_THEME_H
#define REACH_THEME_H

#include "reach/core/geometry.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_theme
    {
        reach_color light_background;
        reach_color dark_background;
        reach_color light_border;
        reach_color dark_border;
        reach_color dock_shadow;
        reach_color icon_box_background;
        reach_color icon_backplate_background;
        reach_color fallback_icon_background;
        reach_color fallback_icon_text;
        reach_color dock_system_separator;
        reach_color dock_clock_time;
        reach_color dock_clock_date;
        reach_color dock_button_background;
        reach_color dock_power_glyph;
        reach_color quick_settings_slider_track_color;
        reach_color quick_settings_slider_fill_color;
        reach_color quick_settings_slider_muted_fill_color;
        reach_color quick_settings_button_color;
        reach_color music_widget_background;
        reach_color launcher_search_background;
        reach_color launcher_search_border;
        reach_color launcher_search_text;
        reach_color clipboard_background;
        reach_color clipboard_border;
        reach_color clipboard_primary_text;
        reach_color clipboard_secondary_text;
        reach_color clipboard_item_background;
        reach_color clipboard_item_hover_background;
        reach_color clipboard_item_hover_border;
        reach_color clipboard_scrollbar_track;
        reach_color clipboard_scrollbar_thumb;
        float clipboard_panel_radius;
        float clipboard_item_radius;
        float clipboard_title_text_size;
        float clipboard_body_text_size;
        reach_color music_widget_cover_background;
        reach_color music_widget_title;
        reach_color music_widget_control_text;
        reach_color music_widget_control_background;
        reach_color music_widget_artist_text;
        float music_widget_artist_text_size;
        float music_widget_text_gap;
        reach_color settings_selected_nav_background;
        reach_color settings_text;
        reach_color settings_secondary_text;
        float dock_corner_radius_ratio;
        reach_color light_text;
        reach_color dark_text;
        float dock_corner_radius_max;
        float border_thickness;
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
        float music_widget_width;
        float music_widget_left_margin;
        float music_widget_height_ratio;
        float music_widget_corner_radius_ratio;
        float music_widget_corner_radius_max;
        float music_widget_padding;
        float music_widget_gap;
        float music_widget_control_gap;
        float music_widget_play_button_width;
        float music_widget_prev_next_button_width;
        float music_widget_title_text_size;
    } reach_theme;

    const reach_theme *reach_theme_default(void);
    float reach_theme_dock_corner_radius(const reach_theme *theme, float dock_height);
    float reach_theme_icon_box_size(const reach_theme *theme, float dock_height);
    float reach_theme_icon_size(const reach_theme *theme, float icon_box_size);
    float reach_theme_icon_box_corner_radius(const reach_theme *theme, float icon_box_size);
    float reach_theme_tray_slot_size(const reach_theme *theme, float dock_height);
    float reach_theme_music_widget_height(const reach_theme *theme, float dock_height);
    float reach_theme_music_widget_corner_radius(const reach_theme *theme, float widget_height);

#ifdef __cplusplus
}
#endif

#endif
