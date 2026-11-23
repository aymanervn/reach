#ifndef REACH_THEME_H
#define REACH_THEME_H

#include "reach/core/geometry.h"
#include "reach/support/motion.h"

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

    typedef struct reach_shadow
    {
        float offset_x;
        float offset_y;
        float blur;
        reach_color color;
    } reach_shadow;

    typedef struct reach_shadow_pad
    {
        float left;
        float top;
        float right;
        float bottom;
    } reach_shadow_pad;

    typedef enum reach_accent_preset
    {
        REACH_ACCENT_PRESET_BLUE = 0,
        REACH_ACCENT_PRESET_CYAN,
        REACH_ACCENT_PRESET_TEAL,
        REACH_ACCENT_PRESET_GREEN,
        REACH_ACCENT_PRESET_PURPLE,
        REACH_ACCENT_PRESET_LAVENDER,
        REACH_ACCENT_PRESET_ORANGE,
        REACH_ACCENT_PRESET_RED,
        REACH_ACCENT_PRESET_COUNT,
    } reach_accent_preset;

    typedef struct reach_accent_pair
    {
        reach_color deep;
        reach_color bright;
    } reach_accent_pair;

    typedef enum reach_motion_preset
    {
        REACH_MOTION_PRESET_SNAPPY = 0,
        REACH_MOTION_PRESET_SMOOTH,
        REACH_MOTION_PRESET_BOUNCY,
        REACH_MOTION_PRESET_PRESS,
        REACH_MOTION_PRESET_RELEASE,
        REACH_MOTION_PRESET_FADE_IN,
        REACH_MOTION_PRESET_FADE_OUT,
        REACH_MOTION_PRESET_CARD_FADE_IN,
        REACH_MOTION_PRESET_CARD_FADE_OUT,
        REACH_MOTION_PRESET_BAR_REVEAL,
        REACH_MOTION_PRESET_POPUP_RISE,
        REACH_MOTION_PRESET_POPUP_FADE_IN,
        REACH_MOTION_PRESET_COUNT,
    } reach_motion_preset;

    // floor and ceiling bound each sRGB-encoded channel of the composed glass surface where the
    // backdrop pushes it past them; painted colors outside the bounds are kept.
    typedef struct reach_glass
    {
        int32_t intensity;
        float floor;
        float ceiling;
    } reach_glass;

    typedef struct reach_theme
    {
        reach_theme_mode mode;
        reach_glass glass;

        reach_color dock_background;
        reach_color top_bar_background;
        reach_color top_bar_scrim;
        reach_color switcher_background;
        reach_color bar_border;
        reach_shadow bar_shadow;
        reach_color popup_background;
        reach_color top_bar_dropdown_background;
        reach_color popup_border;
        reach_color widget_background;
        reach_color widget_border;
        reach_color popup_separator;
        reach_color glass_popup_separator;
        reach_color system_hud_background;
        reach_color system_hud_border;
        reach_color system_hud_primary_text;
        reach_color system_hud_secondary_text;
        reach_color system_hud_icon_background;
        reach_color system_hud_glyph;
        reach_color system_hud_track;
        reach_color system_hud_muted_fill;
        reach_color toggle_track_off;
        reach_color toggle_knob;
        reach_shadow popup_shadow;
        reach_shadow widget_shadow;

        reach_color primary_text;
        reach_color inverse_text;

        reach_color icon_box_background;
        reach_color dock_fallback_icon_background;
        reach_color system_glyph;
        reach_color fallback_icon_text;
        reach_color bar_text_primary;
        reach_color bar_text_secondary;
        reach_color bar_button_background;
        reach_color bar_tray_background;
        reach_color bar_battery_background;
        reach_color bar_battery_fill;
        reach_color bar_battery_low;
        reach_color bar_battery_saver;
        reach_color bar_battery_charging;
        reach_color battery_press_overlay;
        reach_color bar_stats_cpu;
        reach_color bar_stats_memory;
        reach_color bar_stats_download;
        reach_color bar_stats_upload;
        reach_color bar_click_feedback;
        reach_color bar_separator_dot;

        reach_color quick_settings_slider_track;
        reach_color quick_settings_slider_muted_fill;
        reach_color quick_settings_button_background;
        reach_color quick_settings_secondary_text;
        reach_color quick_settings_toggle_track_off;
        reach_color quick_settings_scrollbar_track;
        reach_color quick_settings_scrollbar_thumb;
        reach_color quick_settings_section_label;
        reach_color quick_settings_toggle_divider;
        reach_color quick_settings_press_overlay;

        reach_color now_playing_background;
        reach_color now_playing_title;
        reach_color now_playing_control_text;
        reach_color now_playing_control_disabled_text;

        reach_color launcher_search_background;
        reach_color launcher_search_text;
        reach_color launcher_placeholder_text;
        reach_color launcher_border;
        reach_color launcher_scrollbar_track;
        reach_color launcher_scrollbar_thumb;
        reach_color launcher_row_icon_background;
        reach_color launcher_row_icon_glyph;
        reach_color launcher_row_title;
        reach_color launcher_row_title_selected;
        reach_color launcher_row_path;
        reach_color launcher_press_overlay;

        reach_color switcher_label_text;

        reach_color context_menu_text;
        reach_color context_menu_close_background;
        reach_color context_menu_close_glyph;

        reach_color stage_backdrop;
        reach_color stage_tile_placeholder;
        reach_color stage_tile_label;
        reach_color stage_tile_header_background;
        reach_color stage_close_background;
        reach_color stage_close_hover_background;
        reach_color stage_close_glyph;
        reach_color stage_close_hover_glyph;

        reach_color clipboard_background;
        reach_color clipboard_border;
        reach_color clipboard_primary_text;
        reach_color clipboard_secondary_text;
        reach_color clipboard_empty_text;
        reach_color clipboard_item_background;
        reach_color clipboard_clear_button_background;
        reach_color clipboard_clear_button_disabled_background;
        reach_color clipboard_scrollbar_track;
        reach_color clipboard_scrollbar_thumb;

        reach_color screenshot_scrim;
        reach_color screenshot_selection_border;
        reach_color screenshot_label_background;
        reach_color screenshot_label_text;

        reach_color settings_background;
        reach_color settings_nav_background;
        reach_color settings_text;
        reach_color settings_secondary_text;
        reach_color settings_card_background;
        reach_color settings_pill_background;
        reach_color settings_icon_box_background;
        reach_color settings_segmented_control_background;
        reach_color settings_input_background;
        reach_color settings_divider;
        reach_color settings_toggle_track_off;
        reach_color settings_toggle_knob;
        reach_color settings_button_text;
        reach_color settings_button_danger;
        reach_color settings_button_disabled_background;
        reach_color settings_status_success;
        reach_color settings_status_error;
        reach_color settings_window_button_background;
        reach_color settings_window_close_hover;
        reach_color settings_window_minimize_hover;
        reach_color settings_scrollbar_track;
        reach_color settings_scrollbar_thumb;
        reach_color accent;
        reach_color accent_text;
        reach_color accent_tint;
        reach_color accent_selection;

        float radius_small;
        float radius_large;
        float button_pressed_darken;
        float accent_tint_alpha;
        float dock_corner_radius_ratio;
        float border_thickness;
        float icon_box_height_ratio;
        float icon_max_box_ratio;
        float icon_box_corner_radius_ratio;
        float icon_box_corner_radius_max;
        float tray_slot_size_ratio;
        float bar_separator_dot_size;
        float bar_separator_dot_gap;
        float now_playing_width;
        float now_playing_padding;
        float now_playing_gap;
        float now_playing_control_gap;
        float now_playing_play_button_width;
        float now_playing_prev_next_button_width;
        float stage_animation_seconds;
        float popup_rise_distance;
        float popup_rise_scale;
        reach_motion_spec motion_presets[REACH_MOTION_PRESET_COUNT];
        float stage_motion_reference_seconds;
    } reach_theme;

    const reach_theme *reach_theme_default(void);
    const reach_theme *reach_theme_for_mode(reach_theme_mode mode);
    reach_accent_preset reach_accent_preset_clamp(int32_t preset);
    reach_accent_pair reach_accent_preset_pair(reach_accent_preset preset);
    void reach_theme_resolve(reach_theme_mode mode, const reach_accent_pair *accent,
                             reach_theme *out_theme);
    reach_color reach_theme_color_alpha(reach_color color, float alpha);
    static inline int32_t reach_glass_equal(reach_glass first, reach_glass second)
    {
        return first.intensity == second.intensity && first.floor == second.floor &&
               first.ceiling == second.ceiling;
    }
    reach_color reach_theme_color_mix(reach_color from, reach_color to, float t);
    float reach_theme_border_thickness(const reach_theme *theme, float dpi_scale);
    reach_rect_f32 reach_theme_border_content_rect(const reach_theme *theme, float dpi_scale,
                                                   reach_rect_f32 bounds);
    float reach_theme_shadow_extent(const reach_shadow *shadow, float dpi_scale);
    float reach_theme_shadow_visible_extent(const reach_shadow *shadow, float dpi_scale);
    reach_shadow_pad reach_theme_shadow_pad(const reach_shadow *shadow, float dpi_scale);
    float reach_theme_dock_corner_radius(const reach_theme *theme, float dock_height);
    float reach_theme_icon_box_size(const reach_theme *theme, float dock_height);
    float reach_theme_icon_size(const reach_theme *theme, float icon_box_size);
    float reach_theme_icon_box_corner_radius(const reach_theme *theme, float icon_box_size);
    float reach_theme_tray_slot_size(const reach_theme *theme, float dock_height);
    reach_motion_spec reach_theme_motion_preset(const reach_theme *theme,
                                                reach_motion_preset preset);
    float reach_theme_stage_motion_scale(const reach_theme *theme, float stage_seconds);

#ifdef __cplusplus
}
#endif

#endif
