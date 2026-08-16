#ifndef REACH_THEME_H
#define REACH_THEME_H

#include "reach/core/geometry.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_color
    {
        float r;
        float g;
        float b;
        float a;
    } reach_color;

    typedef enum reach_theme_mode
    {
        REACH_THEME_MODE_DARK = 0,
        REACH_THEME_MODE_LIGHT = 1,
    } reach_theme_mode;

    typedef enum reach_theme_accent
    {
        REACH_THEME_ACCENT_BLUE = 0,
        REACH_THEME_ACCENT_CYAN,
        REACH_THEME_ACCENT_TEAL,
        REACH_THEME_ACCENT_GREEN,
        REACH_THEME_ACCENT_PURPLE,
        REACH_THEME_ACCENT_LAVENDER,
        REACH_THEME_ACCENT_ORANGE,
        REACH_THEME_ACCENT_YELLOW,
        REACH_THEME_ACCENT_RED,
        REACH_THEME_ACCENT_COUNT,
    } reach_theme_accent;

    typedef struct reach_theme
    {
        reach_theme_mode mode;

        reach_color bar_background;
        reach_color bar_border;
        reach_color popup_background;
        reach_color popup_border;

        reach_color primary_text;
        reach_color inverse_text;

        reach_color icon_box_background;
        reach_color system_glyph;
        reach_color fallback_icon_text;
        reach_color bar_text_primary;
        reach_color bar_text_secondary;
        reach_color bar_button_background;
        reach_color bar_power_hover_background;
        reach_color bar_battery_low;
        reach_color dock_running_indicator;
        reach_color bar_click_feedback;

        reach_color tray_click_feedback;

        reach_color quick_settings_slider_track;
        reach_color quick_settings_slider_fill;
        reach_color quick_settings_slider_muted_fill;
        reach_color quick_settings_button_background;
        reach_color quick_settings_secondary_text;
        reach_color quick_settings_separator;
        reach_color quick_settings_app_volume_track;
        reach_color quick_settings_app_volume_fill;
        reach_color quick_settings_app_volume_muted_fill;
        reach_color quick_settings_section_label;

        reach_color now_playing_background;
        reach_color now_playing_title;
        reach_color now_playing_control_text;
        reach_color now_playing_artist_text;

        reach_color launcher_search_background;
        reach_color launcher_search_text;
        reach_color launcher_placeholder_text;
        reach_color launcher_selection_highlight;
        reach_color launcher_search_icon;
        reach_color launcher_border;
        reach_color launcher_scrollbar_track;
        reach_color launcher_scrollbar_thumb;
        reach_color launcher_row_selected_background;
        reach_color launcher_row_icon_background;
        reach_color launcher_row_icon_glyph;
        reach_color launcher_row_title;
        reach_color launcher_row_title_selected;
        reach_color launcher_row_path;
        reach_color launcher_row_path_selected;

        reach_color switcher_selection_background;
        reach_color switcher_label_text;

        reach_color context_menu_text;
        reach_color context_menu_hover_background;
        reach_color context_menu_close_background;
        reach_color context_menu_close_glyph;
        reach_color menu_accent_shutdown;
        reach_color menu_accent_sleep;
        reach_color menu_accent_restart;
        reach_color menu_accent_lock;
        reach_color menu_accent_settings;

        reach_color stage_backdrop;
        reach_color stage_tile_placeholder;
        reach_color stage_tile_highlight;
        reach_color stage_tile_label;
        reach_color stage_close_background;
        reach_color stage_close_hover_background;
        reach_color stage_close_glyph;
        reach_color stage_close_hover_glyph;

        reach_color clipboard_background;
        reach_color clipboard_border;
        reach_color clipboard_primary_text;
        reach_color clipboard_secondary_text;
        reach_color clipboard_item_background;
        reach_color clipboard_item_hover_background;
        reach_color clipboard_item_hover_border;
        reach_color clipboard_scrollbar_track;
        reach_color clipboard_scrollbar_thumb;

        reach_color settings_background;
        reach_color settings_nav_background;
        reach_color settings_text;
        reach_color settings_secondary_text;
        reach_color settings_card_background;
        reach_color settings_pill_background;
        reach_color settings_input_background;
        reach_color settings_divider;
        reach_color settings_toggle_track_off;
        reach_color settings_toggle_knob;
        reach_color settings_button_text;
        reach_color settings_button_primary;
        reach_color settings_button_success;
        reach_color settings_button_danger;
        reach_color settings_button_disabled_background;
        reach_color settings_status_success;
        reach_color settings_status_error;
        reach_color settings_window_button_background;
        reach_color settings_window_close_hover;
        reach_color settings_window_minimize_hover;
        reach_color settings_scrollbar_track;
        reach_color settings_scrollbar_thumb;
        reach_color accents[REACH_THEME_ACCENT_COUNT];

        float radius_small;
        float radius_large;
        float button_pressed_darken;
        float accent_tint_alpha;
        float now_playing_artist_text_size;
        float now_playing_text_gap;
        float dock_corner_radius_ratio;
        float border_thickness;
        float icon_box_height_ratio;
        float icon_max_box_ratio;
        float icon_box_corner_radius_ratio;
        float icon_box_corner_radius_max;
        float tray_slot_size_ratio;
        float now_playing_width;
        float now_playing_padding;
        float now_playing_gap;
        float now_playing_control_gap;
        float now_playing_play_button_width;
        float now_playing_prev_next_button_width;
        float now_playing_title_text_size;

        float surface_open_seconds;
        float surface_close_seconds;
        float bar_reveal_seconds;
        float stage_animation_seconds;
        float stage_close_hover_seconds;
        float stage_reflow_seconds;
    } reach_theme;

    const reach_theme *reach_theme_default(void);
    const reach_theme *reach_theme_for_mode(reach_theme_mode mode);
    reach_color reach_theme_accent_color(const reach_theme *theme, reach_theme_accent accent);
    reach_color reach_theme_color_alpha(reach_color color, float alpha);
    reach_color reach_theme_color_mix(reach_color from, reach_color to, float t);
    float reach_theme_dock_corner_radius(const reach_theme *theme, float dock_height);
    float reach_theme_icon_box_size(const reach_theme *theme, float dock_height);
    float reach_theme_icon_size(const reach_theme *theme, float icon_box_size);
    float reach_theme_icon_box_corner_radius(const reach_theme *theme, float icon_box_size);
    float reach_theme_tray_slot_size(const reach_theme *theme, float dock_height);

#ifdef __cplusplus
}
#endif

#endif
