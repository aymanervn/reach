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

    // Panels by elevation, each with its own dark and light value.
    typedef struct reach_theme_surfaces
    {
        reach_color base;
        reach_color raised;
        reach_color card;
        reach_color item;
        reach_color well;
    } reach_theme_surfaces;

    // Text strengths; shapes that read like glyphs use them too. inverse is the other mode's
    // primary.
    typedef struct reach_theme_text
    {
        reach_color primary;
        reach_color secondary;
        reach_color tertiary;
        reach_color quaternary;
        reach_color inverse;
    } reach_theme_text;

    // Areas behind content: tracks (primary), controls (secondary), disabled controls (tertiary).
    typedef struct reach_theme_fills
    {
        reach_color primary;
        reach_color secondary;
        reach_color tertiary;
    } reach_theme_fills;

    typedef struct reach_theme_status
    {
        reach_color success;
        reach_color error;
    } reach_theme_status;

    // Resolved from the chosen preset by reach_theme_resolve. tint_amount is the share of accent
    // text a selected control blends into its background.
    typedef struct reach_theme_accent
    {
        reach_color fill;
        reach_color text;
        reach_color tint;
        reach_color selection;
        reach_color faint;
        reach_color text_tint;
        reach_color text_muted;
        float tint_amount;
    } reach_theme_accent;

    typedef struct reach_theme
    {
        reach_theme_mode mode;
        reach_glass glass;

        reach_theme_surfaces surface;
        reach_theme_text text;
        reach_theme_fills fill;
        reach_color border;
        reach_color divider;
        // Glyphs, knobs and text drawn on colored or dimmed fills.
        reach_color on_color;
        reach_color danger;
        reach_color pressed_overlay;
        reach_color artwork_scrim;
        reach_theme_status status;
        reach_theme_accent accent;
        reach_shadow shadow;

        // Consumer exceptions: values no semantic role covers.
        reach_color top_bar_scrim;
        reach_color top_bar_battery_fill;
        reach_color top_bar_battery_low_fill;
        reach_color top_bar_battery_saver_fill;
        reach_color top_bar_battery_charging_fill;
        reach_color top_bar_cpu_text;
        reach_color top_bar_memory_text;
        reach_color top_bar_download_text;
        reach_color top_bar_upload_text;
        reach_color dock_fallback_icon_background;
        reach_color bar_fallback_icon_text;
        reach_color widgets_artwork_overlay;
        reach_color widgets_accent_background;
        reach_color stage_background;
        reach_color stage_tile_header_background;
        reach_color stage_close_glyph;
        reach_color screenshot_scrim;
        reach_color screenshot_label_background;
        reach_color settings_background;
        reach_color settings_window_minimize_hover_background;
        reach_color settings_avatar_border;

        float radius_small;
        float radius_large;
        float button_pressed_darken;
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
