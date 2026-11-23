#include "reach/core/theme.h"
#include "reach/core/config.h"
#include "reach/support/util.h"
#include "theme_palette.h"

#include <math.h>

#define REACH_THEME_STAGE_BACKDROP_ALPHA 1.0f
#define REACH_THEME_ACCENT_CONTRAST 4.5f
#define REACH_THEME_GLASS_PRIMARY_CONTRAST 4.5f
#define REACH_THEME_GLASS_SECONDARY_CONTRAST 3.0f

#define REACH_SHADOW_AMBIENT(blur_px)                                                              \
    {.offset_x = 0.0f, .offset_y = 0.0f, .blur = (blur_px), .color = REACH_PURE_BLACK(0.3f)}

#define REACH_THEME_SHADOWS                                                                        \
    .bar_shadow = REACH_SHADOW_AMBIENT(5.0f), .popup_shadow = REACH_SHADOW_AMBIENT(5.0f),          \
    .widgets_shadow = REACH_SHADOW_AMBIENT(5.0f)

#define REACH_THEME_METRICS                                                                        \
    .radius_small = 12.0f, .radius_large = 20.0f, .button_pressed_darken = 0.65f,                  \
    .settings_pill_selected_tint = REACH_ACCENT_TINT, .dock_corner_radius_ratio = 0.5f,            \
    .border_thickness = 1.0f, .icon_box_height_ratio = 0.60f, .icon_max_box_ratio = 0.76f,         \
    .icon_box_corner_radius_ratio = 0.28f, .icon_box_corner_radius_max = 14.0f,                    \
    .tray_slot_size_ratio = 0.605f, .bar_separator_dot_size = 4.0f, .bar_separator_dot_gap = 9.0f, \
    .now_playing_width = 220.0f, .now_playing_padding = 9.0f, .now_playing_gap = 6.0f,             \
    .now_playing_control_gap = 8.0f, .now_playing_play_button_width = 20.0f,                       \
    .now_playing_prev_next_button_width = 13.2f, .stage_animation_seconds = 0.30f,                 \
    .popup_rise_distance = 10.0f, .popup_rise_scale = 0.97f

#define REACH_THEME_SPRING(response, damping) {REACH_EASING_SPRING, (response), (damping), 0.0}
#define REACH_THEME_FADE(seconds) {REACH_EASING_EASE_OUT, 0.0f, 0.0f, (seconds)}

#define REACH_THEME_MOTION                                                                         \
    .motion_presets =                                                                              \
        {                                                                                          \
            [REACH_MOTION_PRESET_SNAPPY] = REACH_THEME_SPRING(0.24f, 1.0f),                        \
            [REACH_MOTION_PRESET_SMOOTH] = REACH_THEME_SPRING(0.38f, 1.0f),                        \
            [REACH_MOTION_PRESET_BOUNCY] = REACH_THEME_SPRING(0.34f, 0.78f),                       \
            [REACH_MOTION_PRESET_PRESS] = REACH_THEME_SPRING(0.16f, 1.0f),                         \
            [REACH_MOTION_PRESET_RELEASE] = REACH_THEME_SPRING(0.30f, 0.72f),                      \
            [REACH_MOTION_PRESET_FADE_IN] = REACH_THEME_FADE(0.14),                                \
            [REACH_MOTION_PRESET_FADE_OUT] = REACH_THEME_FADE(0.11),                               \
            [REACH_MOTION_PRESET_CARD_FADE_IN] = REACH_THEME_FADE(0.05),                           \
            [REACH_MOTION_PRESET_CARD_FADE_OUT] = REACH_THEME_FADE(0.18),                          \
            [REACH_MOTION_PRESET_BAR_REVEAL] = REACH_THEME_SPRING(0.28f, 1.0f),                    \
            [REACH_MOTION_PRESET_POPUP_RISE] = REACH_THEME_SPRING(0.30f, 1.0f),                    \
            [REACH_MOTION_PRESET_POPUP_FADE_IN] = REACH_THEME_FADE(0.10),                          \
        },                                                                                         \
    .stage_motion_reference_seconds = 0.38f

static const reach_theme reach_theme_dark = {
    .mode = REACH_THEME_MODE_DARK,
    .glass = {.intensity = REACH_CONFIG_GLASS_INTENSITY_DEFAULT, .floor = 0.0f, .ceiling = 1.0f},

    .dock_background = REACH_DARK_SURFACE_BASE(1.0f),
    .top_bar_background = REACH_DARK_SURFACE_BASE(1.0f),
    .top_bar_scrim = REACH_PURE_BLACK(0.65f),
    .switcher_background = REACH_DARK_SURFACE_BASE(1.0f),
    .shared_border = REACH_DARK_BORDER(1.0f),
    .popup_background = REACH_DARK_SURFACE_RAISED(1.0f),
    .bar_popup_background = REACH_DARK_SURFACE_BASE(1.0f),
    .popup_border = REACH_DARK_BORDER(1.0f),
    .widgets_background = REACH_DARK_SURFACE_RAISED(1.0f),
    .widgets_border = REACH_DARK_BORDER(1.0f),
    .widgets_unavailable_text = REACH_DARK_TEXT(REACH_TEXT_SECONDARY),
    .widgets_resize_grip_fill = REACH_DARK_TEXT(REACH_TEXT_TERTIARY),
    .widgets_artwork_overlay = REACH_DARK_SURFACE_RAISED(0.48f),
    .popup_divider = REACH_DARK_DIVIDER(1.0f),
    .popup_glass_divider = REACH_DARK_TEXT(REACH_TEXT_QUATERNARY),
    .system_hud_background = REACH_DARK_SURFACE_RAISED(1.0f),
    .system_hud_border = REACH_DARK_BORDER(1.0f),
    .system_hud_text = REACH_DARK_TEXT(REACH_TEXT_PRIMARY),
    .system_hud_secondary_text = REACH_DARK_TEXT(REACH_TEXT_SECONDARY),
    .system_hud_icon_background = REACH_DARK_SURFACE_ELEVATED(1.0f),
    .system_hud_glyph = REACH_DARK_TEXT(REACH_TEXT_PRIMARY),
    .system_hud_track = REACH_FILL(REACH_DARK_FILL_SECONDARY),
    .system_hud_muted_fill = REACH_DARK_TEXT(REACH_TEXT_TERTIARY),
    .toggle_off_track = REACH_DARK_TEXT(REACH_TEXT_TERTIARY),
    .toggle_knob = REACH_PURE_WHITE(1.0f),

    .shared_text = REACH_DARK_TEXT(REACH_TEXT_PRIMARY),
    .settings_window_button_glyph = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),

    .dock_fallback_icon_background = REACH_PURE_WHITE(0.80f),
    .shared_glyph = REACH_DARK_TEXT(REACH_TEXT_PRIMARY),
    .bar_fallback_icon_text = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),
    .top_bar_text = REACH_DARK_TEXT(REACH_TEXT_PRIMARY),
    .top_bar_secondary_text = REACH_DARK_TEXT(REACH_TEXT_SECONDARY),
    .shared_button_background = REACH_FILL(REACH_DARK_FILL_TERTIARY),
    .top_bar_tray_background = REACH_FILL(REACH_DARK_FILL_TERTIARY),
    .top_bar_battery_background = REACH_FILL(REACH_DARK_FILL_TERTIARY),
    .top_bar_battery_fill = REACH_GREEN_DEEP(1.0f),
    .top_bar_battery_low_fill = REACH_RED_DEEP(1.0f),
    .top_bar_battery_saver_fill = REACH_YELLOW_DEEP(1.0f),
    .top_bar_battery_charging_fill = REACH_BLUE_DEEP(1.0f),
    .battery_pressed_overlay = REACH_PURE_BLACK(0.12f),
    .top_bar_cpu_text = REACH_CYAN_BRIGHT(1.0f),
    .top_bar_memory_text = REACH_LAVENDER_BRIGHT(1.0f),
    .top_bar_download_text = REACH_GREEN_BRIGHT(1.0f),
    .top_bar_upload_text = REACH_ORANGE_BRIGHT(1.0f),
    .bar_pressed_background = REACH_DARK_SURFACE_ELEVATED(1.0f),
    .top_bar_separator_fill = REACH_DARK_TEXT(REACH_TEXT_SECONDARY),

    .quick_settings_slider_track = REACH_FILL(REACH_DARK_FILL_SECONDARY),
    .quick_settings_slider_muted_fill = REACH_DARK_TEXT(REACH_TEXT_TERTIARY),
    .quick_settings_button_background = REACH_FILL(REACH_DARK_FILL_TERTIARY),
    .quick_settings_secondary_text = REACH_DARK_TEXT(REACH_TEXT_SECONDARY),
    .quick_settings_toggle_off_track = REACH_DARK_TEXT(REACH_TEXT_TERTIARY),
    .quick_settings_scrollbar_track = REACH_FILL(REACH_DARK_FILL_SECONDARY),
    .quick_settings_scrollbar_thumb = REACH_DARK_TEXT(REACH_TEXT_SECONDARY),
    .quick_settings_section_text = REACH_DARK_TEXT(REACH_TEXT_SECONDARY),
    .quick_settings_toggle_divider = REACH_DARK_TEXT(REACH_TEXT_QUATERNARY),
    .quick_settings_pressed_overlay = REACH_PURE_BLACK(0.12f),

    .now_playing_background = REACH_DARK_SURFACE_BASE(0.35f),
    .now_playing_title_text = REACH_DARK_TEXT(REACH_TEXT_PRIMARY),
    .now_playing_control_glyph = REACH_DARK_TEXT(REACH_TEXT_PRIMARY),
    .now_playing_control_disabled_glyph = REACH_DARK_TEXT(REACH_TEXT_TERTIARY),

    .launcher_search_background = REACH_DARK_SURFACE_BASE(1.0f),
    .launcher_search_text = REACH_DARK_TEXT(REACH_TEXT_PRIMARY),
    .launcher_search_placeholder_text = REACH_DARK_TEXT(REACH_TEXT_TERTIARY),
    .launcher_border = REACH_DARK_BORDER(1.0f),
    .launcher_scrollbar_track = REACH_FILL(REACH_DARK_FILL_SECONDARY),
    .launcher_scrollbar_thumb = REACH_DARK_TEXT(REACH_TEXT_SECONDARY),
    .launcher_row_icon_background = REACH_DARK_SURFACE_ELEVATED(1.0f),
    .launcher_row_icon_glyph = REACH_DARK_TEXT(REACH_TEXT_SECONDARY),
    .launcher_row_text = REACH_DARK_TEXT(REACH_TEXT_PRIMARY),
    .launcher_row_selected_text = REACH_DARK_TEXT(REACH_TEXT_PRIMARY),
    .launcher_row_secondary_text = REACH_DARK_TEXT(REACH_TEXT_SECONDARY),
    .launcher_pressed_overlay = REACH_PURE_BLACK(0.12f),

    .switcher_label_text = REACH_DARK_TEXT(REACH_TEXT_PRIMARY),

    .context_menu_text = REACH_DARK_TEXT(REACH_TEXT_PRIMARY),
    .context_menu_close_background = REACH_RED_DEEP(1.0f),
    .context_menu_close_glyph = REACH_PURE_WHITE(1.0f),

    .stage_background = REACH_DARK_SURFACE_BASE(REACH_THEME_STAGE_BACKDROP_ALPHA),
    .stage_tile_placeholder_background = REACH_DARK_SURFACE_ELEVATED(1.0f),
    .stage_tile_text = REACH_DARK_TEXT(REACH_TEXT_PRIMARY),
    .stage_tile_header_background = REACH_DARK_SURFACE_ELEVATED(0.45f),
    .stage_close_background = REACH_DARK_TEXT(REACH_TEXT_SECONDARY),
    .stage_close_hover_background = REACH_RED_DEEP(1.0f),
    .stage_close_glyph = REACH_PURE_WHITE(0.80f),
    .stage_close_hover_glyph = REACH_PURE_WHITE(1.0f),

    .clipboard_background = REACH_DARK_SURFACE_BASE(1.0f),
    .clipboard_border = REACH_DARK_BORDER(1.0f),
    .clipboard_text = REACH_DARK_TEXT(REACH_TEXT_PRIMARY),
    .clipboard_secondary_text = REACH_DARK_TEXT(REACH_TEXT_SECONDARY),
    .clipboard_disabled_text = REACH_DARK_TEXT(REACH_TEXT_TERTIARY),
    .clipboard_empty_text = REACH_DARK_TEXT(REACH_TEXT_SECONDARY),
    .clipboard_item_background = REACH_DARK_SURFACE_ELEVATED(1.0f),
    .clipboard_clear_button_background = REACH_FILL(REACH_DARK_FILL_TERTIARY),
    .clipboard_clear_button_disabled_background = REACH_FILL(REACH_DARK_FILL_QUATERNARY),
    .clipboard_scrollbar_track = REACH_FILL(REACH_DARK_FILL_SECONDARY),
    .clipboard_scrollbar_thumb = REACH_DARK_TEXT(REACH_TEXT_SECONDARY),

    .screenshot_scrim = REACH_PURE_BLACK(0.45f),
    .screenshot_selection_border = REACH_PURE_WHITE(1.0f),
    .screenshot_label_background = REACH_DARK_SURFACE_BASE(0.92f),
    .screenshot_label_text = REACH_DARK_TEXT(REACH_TEXT_PRIMARY),

    .settings_background = REACH_DARK_SURFACE_BASE(1.0f),
    .settings_nav_background = REACH_DARK_SURFACE_RAISED(1.0f),
    .settings_text = REACH_DARK_TEXT(REACH_TEXT_PRIMARY),
    .settings_secondary_text = REACH_DARK_TEXT(REACH_TEXT_SECONDARY),
    .settings_tertiary_text = REACH_DARK_TEXT(REACH_TEXT_TERTIARY),
    .settings_card_background = REACH_DARK_SURFACE_ELEVATED(1.0f),
    .settings_pill_background = REACH_DARK_SURFACE_ELEVATED(1.0f),
    .settings_icon_box_background = REACH_DARK_SURFACE_ELEVATED(1.0f),
    .settings_segmented_control_background = REACH_FILL(REACH_DARK_FILL_TERTIARY),
    .settings_input_background = REACH_FILL(REACH_DARK_FILL_TERTIARY),
    .settings_divider = REACH_DARK_DIVIDER(1.0f),
    .settings_toggle_off_track = REACH_DARK_TEXT(REACH_TEXT_TERTIARY),
    .settings_toggle_knob = REACH_PURE_WHITE(1.0f),
    .settings_button_text = REACH_DARK_TEXT(REACH_TEXT_PRIMARY),
    .settings_button_danger_text = REACH_PURE_WHITE(1.0f),
    .settings_button_danger_background = REACH_RED_DEEP(1.0f),
    .settings_button_disabled_background = REACH_DARK_SURFACE_ELEVATED(1.0f),
    .settings_success_text = REACH_GREEN_BRIGHT(1.0f),
    .settings_error_text = REACH_RED_BRIGHT(1.0f),
    .settings_window_button_background = REACH_DARK_TEXT(REACH_TEXT_SECONDARY),
    .settings_window_close_hover_background = REACH_RED_DEEP(1.0f),
    .settings_window_minimize_hover_background = REACH_YELLOW_BRIGHT(1.0f),
    .settings_scrollbar_track = REACH_FILL(REACH_DARK_FILL_SECONDARY),
    .settings_scrollbar_thumb = REACH_DARK_TEXT(REACH_TEXT_SECONDARY),
    .accent_fill = REACH_BLUE_DEEP(1.0f),
    .accent_text = REACH_BLUE_BRIGHT(1.0f),
    .accent_tint = REACH_BLUE_DEEP(REACH_ACCENT_TINT),
    .accent_selection = REACH_BLUE_DEEP(REACH_ACCENT_SELECTION),
    .widgets_decoration_fill = REACH_BLUE_DEEP(REACH_ACCENT_TINT),
    .widgets_ribbon_fill = REACH_BLUE_DEEP(REACH_ACCENT_FAINT),
    .widgets_meter_empty_fill = REACH_BLUE_BRIGHT(REACH_ACCENT_TINT),
    .widgets_meter_muted_fill = REACH_BLUE_BRIGHT(REACH_ACCENT_MUTED),
    .widgets_section_divider = REACH_BLUE_BRIGHT(REACH_ACCENT_TINT),
    .widgets_drag_overlay = REACH_BLUE_DEEP(REACH_ACCENT_SELECTION),
    .widgets_primary_control_background = REACH_BLUE_DEEP(REACH_ACCENT_TINT),
    .widgets_control_background = REACH_BLUE_DEEP(REACH_ACCENT_FAINT),
    .settings_avatar_placeholder_background = REACH_BLUE_BRIGHT(REACH_ACCENT_TINT),
    // Opaque accent blends exist only once resolved; unresolved tables keep the plain surface.
    .widgets_accent_background = REACH_DARK_SURFACE_RAISED(1.0f),
    .settings_avatar_border = REACH_DARK_SURFACE_ELEVATED(1.0f),

    REACH_THEME_SHADOWS,
    REACH_THEME_METRICS,
    REACH_THEME_MOTION,
};

static const reach_theme reach_theme_light = {
    .mode = REACH_THEME_MODE_LIGHT,
    .glass = {.intensity = REACH_CONFIG_GLASS_INTENSITY_DEFAULT, .floor = 0.0f, .ceiling = 1.0f},

    .dock_background = REACH_LIGHT_SURFACE_BASE(1.0f),
    .top_bar_background = REACH_LIGHT_SURFACE_BASE(1.0f),
    .top_bar_scrim = REACH_PURE_WHITE(0.65f),
    .switcher_background = REACH_LIGHT_SURFACE_BASE(1.0f),
    .shared_border = REACH_LIGHT_BORDER(1.0f),
    .popup_background = REACH_LIGHT_SURFACE_BASE(1.0f),
    .bar_popup_background = REACH_LIGHT_SURFACE_BASE(1.0f),
    .popup_border = REACH_LIGHT_BORDER(1.0f),
    .widgets_background = REACH_LIGHT_SURFACE_BASE(1.0f),
    .widgets_border = REACH_LIGHT_BORDER(1.0f),
    .widgets_unavailable_text = REACH_LIGHT_TEXT(REACH_TEXT_SECONDARY),
    .widgets_resize_grip_fill = REACH_LIGHT_TEXT(REACH_TEXT_TERTIARY),
    .widgets_artwork_overlay = REACH_LIGHT_SURFACE_BASE(0.48f),
    .popup_divider = REACH_LIGHT_DIVIDER(1.0f),
    .popup_glass_divider = REACH_LIGHT_TEXT(REACH_TEXT_QUATERNARY),
    .system_hud_background = REACH_LIGHT_SURFACE_BASE(1.0f),
    .system_hud_border = REACH_LIGHT_BORDER(1.0f),
    .system_hud_text = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),
    .system_hud_secondary_text = REACH_LIGHT_TEXT(REACH_TEXT_SECONDARY),
    .system_hud_icon_background = REACH_LIGHT_SURFACE_ELEVATED(1.0f),
    .system_hud_glyph = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),
    .system_hud_track = REACH_FILL(REACH_LIGHT_FILL_SECONDARY),
    .system_hud_muted_fill = REACH_LIGHT_TEXT(REACH_TEXT_TERTIARY),
    .toggle_off_track = REACH_LIGHT_TEXT(REACH_TEXT_TERTIARY),
    .toggle_knob = REACH_PURE_WHITE(1.0f),

    .shared_text = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),
    .settings_window_button_glyph = REACH_DARK_TEXT(REACH_TEXT_PRIMARY),

    .dock_fallback_icon_background = REACH_LIGHT_SURFACE_RAISED(0.80f),
    .shared_glyph = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),
    .bar_fallback_icon_text = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),
    .top_bar_text = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),
    .top_bar_secondary_text = REACH_LIGHT_TEXT(REACH_TEXT_SECONDARY),
    .shared_button_background = REACH_FILL(REACH_LIGHT_FILL_TERTIARY),
    .top_bar_tray_background = REACH_FILL(REACH_LIGHT_FILL_TERTIARY),
    .top_bar_battery_background = REACH_FILL(REACH_LIGHT_FILL_TERTIARY),
    .top_bar_battery_fill = REACH_GREEN_BRIGHT(1.0f),
    .top_bar_battery_low_fill = REACH_RED_BRIGHT(1.0f),
    .top_bar_battery_saver_fill = REACH_YELLOW_BRIGHT(1.0f),
    .top_bar_battery_charging_fill = REACH_BLUE_BRIGHT(1.0f),
    .battery_pressed_overlay = REACH_PURE_BLACK(0.12f),
    .top_bar_cpu_text = REACH_CYAN_INK(1.0f),
    .top_bar_memory_text = REACH_LAVENDER_INK(1.0f),
    .top_bar_download_text = REACH_GREEN_INK(1.0f),
    .top_bar_upload_text = REACH_ORANGE_INK(1.0f),
    .bar_pressed_background = REACH_LIGHT_SURFACE_RAISED(1.0f),
    .top_bar_separator_fill = REACH_LIGHT_TEXT(REACH_TEXT_SECONDARY),

    .quick_settings_slider_track = REACH_FILL(REACH_LIGHT_FILL_SECONDARY),
    .quick_settings_slider_muted_fill = REACH_LIGHT_TEXT(REACH_TEXT_TERTIARY),
    .quick_settings_button_background = REACH_FILL(REACH_LIGHT_FILL_TERTIARY),
    .quick_settings_secondary_text = REACH_LIGHT_TEXT(REACH_TEXT_SECONDARY),
    .quick_settings_toggle_off_track = REACH_LIGHT_TEXT(REACH_TEXT_TERTIARY),
    .quick_settings_scrollbar_track = REACH_FILL(REACH_LIGHT_FILL_SECONDARY),
    .quick_settings_scrollbar_thumb = REACH_LIGHT_TEXT(REACH_TEXT_SECONDARY),
    .quick_settings_section_text = REACH_LIGHT_TEXT(REACH_TEXT_SECONDARY),
    .quick_settings_toggle_divider = REACH_LIGHT_TEXT(REACH_TEXT_QUATERNARY),
    .quick_settings_pressed_overlay = REACH_PURE_BLACK(0.12f),

    .now_playing_background = REACH_LIGHT_SURFACE_RAISED(0.35f),
    .now_playing_title_text = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),
    .now_playing_control_glyph = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),
    .now_playing_control_disabled_glyph = REACH_LIGHT_TEXT(REACH_TEXT_TERTIARY),

    .launcher_search_background = REACH_LIGHT_SURFACE_BASE(1.0f),
    .launcher_search_text = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),
    .launcher_search_placeholder_text = REACH_LIGHT_TEXT(REACH_TEXT_TERTIARY),
    .launcher_border = REACH_LIGHT_BORDER(1.0f),
    .launcher_scrollbar_track = REACH_FILL(REACH_LIGHT_FILL_SECONDARY),
    .launcher_scrollbar_thumb = REACH_LIGHT_TEXT(REACH_TEXT_SECONDARY),
    .launcher_row_icon_background = REACH_LIGHT_SURFACE_RAISED(1.0f),
    .launcher_row_icon_glyph = REACH_LIGHT_TEXT(REACH_TEXT_SECONDARY),
    .launcher_row_text = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),
    .launcher_row_selected_text = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),
    .launcher_row_secondary_text = REACH_LIGHT_TEXT(REACH_TEXT_SECONDARY),
    .launcher_pressed_overlay = REACH_PURE_BLACK(0.12f),

    .switcher_label_text = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),

    .context_menu_text = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),
    .context_menu_close_background = REACH_RED_DEEP(1.0f),
    .context_menu_close_glyph = REACH_PURE_WHITE(1.0f),

    .stage_background = REACH_LIGHT_SURFACE_BASE(REACH_THEME_STAGE_BACKDROP_ALPHA),
    .stage_tile_placeholder_background = REACH_LIGHT_SURFACE_RAISED(1.0f),
    .stage_tile_text = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),
    .stage_tile_header_background = REACH_LIGHT_SURFACE_ELEVATED(1.0f),
    .stage_close_background = REACH_LIGHT_TEXT(REACH_TEXT_SECONDARY),
    .stage_close_hover_background = REACH_RED_DEEP(1.0f),
    .stage_close_glyph = REACH_PURE_WHITE(0.80f),
    .stage_close_hover_glyph = REACH_PURE_WHITE(1.0f),

    .clipboard_background = REACH_LIGHT_SURFACE_BASE(1.0f),
    .clipboard_border = REACH_LIGHT_BORDER(1.0f),
    .clipboard_text = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),
    .clipboard_secondary_text = REACH_LIGHT_TEXT(REACH_TEXT_SECONDARY),
    .clipboard_disabled_text = REACH_LIGHT_TEXT(REACH_TEXT_TERTIARY),
    .clipboard_empty_text = REACH_LIGHT_TEXT(REACH_TEXT_SECONDARY),
    .clipboard_item_background = REACH_LIGHT_SURFACE_RAISED(1.0f),
    .clipboard_clear_button_background = REACH_FILL(REACH_LIGHT_FILL_TERTIARY),
    .clipboard_clear_button_disabled_background = REACH_FILL(REACH_LIGHT_FILL_QUATERNARY),
    .clipboard_scrollbar_track = REACH_FILL(REACH_LIGHT_FILL_SECONDARY),
    .clipboard_scrollbar_thumb = REACH_LIGHT_TEXT(REACH_TEXT_SECONDARY),

    .screenshot_scrim = REACH_PURE_BLACK(0.45f),
    .screenshot_selection_border = REACH_PURE_WHITE(1.0f),
    .screenshot_label_background = REACH_LIGHT_SURFACE_BASE(0.92f),
    .screenshot_label_text = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),

    .settings_background = REACH_LIGHT_SURFACE_RAISED(1.0f),
    .settings_nav_background = REACH_LIGHT_SURFACE_BASE(1.0f),
    .settings_text = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),
    .settings_secondary_text = REACH_LIGHT_TEXT(REACH_TEXT_SECONDARY),
    .settings_tertiary_text = REACH_LIGHT_TEXT(REACH_TEXT_TERTIARY),
    .settings_card_background = REACH_LIGHT_SURFACE_BASE(1.0f),
    .settings_pill_background = REACH_LIGHT_SURFACE_RAISED(1.0f),
    .settings_icon_box_background = REACH_LIGHT_SURFACE_ELEVATED(1.0f),
    .settings_segmented_control_background = REACH_FILL(REACH_LIGHT_FILL_TERTIARY),
    .settings_input_background = REACH_FILL(REACH_LIGHT_FILL_TERTIARY),
    .settings_divider = REACH_LIGHT_DIVIDER(1.0f),
    .settings_toggle_off_track = REACH_LIGHT_TEXT(REACH_TEXT_TERTIARY),
    .settings_toggle_knob = REACH_PURE_WHITE(1.0f),
    .settings_button_text = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),
    .settings_button_danger_text = REACH_PURE_WHITE(1.0f),
    .settings_button_danger_background = REACH_RED_DEEP(1.0f),
    .settings_button_disabled_background = REACH_LIGHT_SURFACE_ELEVATED(1.0f),
    .settings_success_text = REACH_GREEN_INK(1.0f),
    .settings_error_text = REACH_RED_INK(1.0f),
    .settings_window_button_background = REACH_LIGHT_TEXT(REACH_TEXT_SECONDARY),
    .settings_window_close_hover_background = REACH_RED_DEEP(1.0f),
    .settings_window_minimize_hover_background = REACH_YELLOW_DEEP(1.0f),
    .settings_scrollbar_track = REACH_FILL(REACH_LIGHT_FILL_SECONDARY),
    .settings_scrollbar_thumb = REACH_LIGHT_TEXT(REACH_TEXT_SECONDARY),
    .accent_fill = REACH_BLUE_BRIGHT(1.0f),
    .accent_text = REACH_BLUE_DEEP(1.0f),
    .accent_tint = REACH_BLUE_BRIGHT(REACH_ACCENT_TINT),
    .accent_selection = REACH_BLUE_BRIGHT(REACH_ACCENT_SELECTION),
    .widgets_decoration_fill = REACH_BLUE_BRIGHT(REACH_ACCENT_TINT),
    .widgets_ribbon_fill = REACH_BLUE_BRIGHT(REACH_ACCENT_FAINT),
    .widgets_meter_empty_fill = REACH_BLUE_DEEP(REACH_ACCENT_TINT),
    .widgets_meter_muted_fill = REACH_BLUE_DEEP(REACH_ACCENT_MUTED),
    .widgets_section_divider = REACH_BLUE_DEEP(REACH_ACCENT_TINT),
    .widgets_drag_overlay = REACH_BLUE_BRIGHT(REACH_ACCENT_SELECTION),
    .widgets_primary_control_background = REACH_BLUE_BRIGHT(REACH_ACCENT_TINT),
    .widgets_control_background = REACH_BLUE_BRIGHT(REACH_ACCENT_FAINT),
    .settings_avatar_placeholder_background = REACH_BLUE_DEEP(REACH_ACCENT_TINT),
    // Opaque accent blends exist only once resolved; unresolved tables keep the plain surface.
    .widgets_accent_background = REACH_LIGHT_SURFACE_BASE(1.0f),
    .settings_avatar_border = REACH_LIGHT_SURFACE_BASE(1.0f),

    REACH_THEME_SHADOWS,
    REACH_THEME_METRICS,
    REACH_THEME_MOTION,
};

static float reach_theme_min(float a, float b)
{
    return a < b ? a : b;
}

const reach_theme *reach_theme_default(void)
{
    return &reach_theme_dark;
}

const reach_theme *reach_theme_for_mode(reach_theme_mode mode)
{
    return mode == REACH_THEME_MODE_LIGHT ? &reach_theme_light : &reach_theme_dark;
}

static const reach_accent_pair reach_accent_presets[REACH_ACCENT_PRESET_COUNT] = {
    [REACH_ACCENT_PRESET_BLUE] = {REACH_BLUE_DEEP(1.0f), REACH_BLUE_BRIGHT(1.0f)},
    [REACH_ACCENT_PRESET_CYAN] = {REACH_CYAN_DEEP(1.0f), REACH_CYAN_BRIGHT(1.0f)},
    [REACH_ACCENT_PRESET_TEAL] = {REACH_TEAL_DEEP(1.0f), REACH_TEAL_BRIGHT(1.0f)},
    [REACH_ACCENT_PRESET_GREEN] = {REACH_GREEN_DEEP(1.0f), REACH_GREEN_BRIGHT(1.0f)},
    [REACH_ACCENT_PRESET_PURPLE] = {REACH_PURPLE_DEEP(1.0f), REACH_PURPLE_BRIGHT(1.0f)},
    [REACH_ACCENT_PRESET_LAVENDER] = {REACH_LAVENDER_DEEP(1.0f), REACH_LAVENDER_BRIGHT(1.0f)},
    [REACH_ACCENT_PRESET_ORANGE] = {REACH_ORANGE_DEEP(1.0f), REACH_ORANGE_BRIGHT(1.0f)},
    [REACH_ACCENT_PRESET_RED] = {REACH_RED_DEEP(1.0f), REACH_RED_BRIGHT(1.0f)},
};

reach_accent_preset reach_accent_preset_clamp(int32_t preset)
{
    return preset >= 0 && preset < REACH_ACCENT_PRESET_COUNT ? (reach_accent_preset)preset
                                                             : REACH_ACCENT_PRESET_BLUE;
}

reach_accent_pair reach_accent_preset_pair(reach_accent_preset preset)
{
    return reach_accent_presets[reach_accent_preset_clamp((int32_t)preset)];
}

static float reach_theme_linear_channel(float channel)
{
    return channel <= 0.04045f ? channel / 12.92f : powf((channel + 0.055f) / 1.055f, 2.4f);
}

static float reach_theme_luminance(reach_color color)
{
    return 0.2126f * reach_theme_linear_channel(color.r) +
           0.7152f * reach_theme_linear_channel(color.g) +
           0.0722f * reach_theme_linear_channel(color.b);
}

static float reach_theme_contrast(reach_color a, reach_color b)
{
    float first = reach_theme_luminance(a) + 0.05f;
    float second = reach_theme_luminance(b) + 0.05f;
    return first > second ? first / second : second / first;
}

// Translucent references are judged as drawn over the candidate, like text on an accent fill.
static int32_t reach_theme_contrast_met(reach_color color, reach_color reference)
{
    return reach_theme_contrast(color, reach_theme_color_mix(color, reference, reference.a)) >=
           REACH_THEME_ACCENT_CONTRAST;
}

static reach_color reach_theme_meet_contrast(reach_color color, reach_color reference,
                                             reach_color toward)
{
    color.a = 1.0f;
    if (reach_theme_contrast_met(color, reference))
    {
        return color;
    }
    float low = 0.0f;
    float high = 1.0f;
    for (int32_t step = 0; step < 24; ++step)
    {
        float middle = (low + high) * 0.5f;
        if (reach_theme_contrast_met(reach_theme_color_mix(color, toward, middle), reference))
        {
            high = middle;
        }
        else
        {
            low = middle;
        }
    }
    return reach_theme_color_mix(color, toward, high);
}

static int32_t reach_theme_glass_surface_readable(const reach_theme *theme, float level)
{
    const reach_color surface = {level, level, level, 1.0f};
    const reach_color primary[] = {
        theme->shared_text,          theme->top_bar_text,
        theme->context_menu_text,    theme->switcher_label_text,
        theme->launcher_search_text, theme->clipboard_text,
        theme->system_hud_text,      theme->now_playing_control_glyph,
    };
    const reach_color secondary[] = {
        theme->top_bar_secondary_text,      theme->quick_settings_secondary_text,
        theme->quick_settings_section_text, theme->launcher_row_secondary_text,
        theme->clipboard_secondary_text,    theme->system_hud_secondary_text,
    };
    for (size_t index = 0; index < sizeof(primary) / sizeof(primary[0]); ++index)
    {
        reach_color text = reach_theme_color_mix(surface, primary[index], primary[index].a);
        if (reach_theme_contrast(text, surface) < REACH_THEME_GLASS_PRIMARY_CONTRAST)
            return 0;
    }
    for (size_t index = 0; index < sizeof(secondary) / sizeof(secondary[0]); ++index)
    {
        reach_color text = reach_theme_color_mix(surface, secondary[index], secondary[index].a);
        if (reach_theme_contrast(text, surface) < REACH_THEME_GLASS_SECONDARY_CONTRAST)
            return 0;
    }
    return 1;
}

// The furthest gray level from the theme's own surfaces that keeps every glass text token
// readable. Dark themes bound how light glass gets, light themes how dark.
static float reach_theme_glass_limit(const reach_theme *theme, int32_t light)
{
    float readable = light ? 1.0f : 0.0f;
    float unreadable = light ? 0.0f : 1.0f;
    if (reach_theme_glass_surface_readable(theme, unreadable))
        return unreadable;
    for (int32_t step = 0; step < 24; ++step)
    {
        float middle = (readable + unreadable) * 0.5f;
        if (reach_theme_glass_surface_readable(theme, middle))
            readable = middle;
        else
            unreadable = middle;
    }
    return readable;
}

static int32_t reach_theme_color_close(reach_color a, reach_color b)
{
    const float tolerance = 0.5f / 255.0f;
    return fabsf(a.r - b.r) <= tolerance && fabsf(a.g - b.g) <= tolerance &&
           fabsf(a.b - b.b) <= tolerance && fabsf(a.a - b.a) <= tolerance;
}

void reach_theme_resolve(reach_theme_mode mode, const reach_accent_pair *accent,
                         reach_theme *out_theme)
{
    if (out_theme == 0)
    {
        return;
    }
    *out_theme = *reach_theme_for_mode(mode);
    reach_accent_pair pair =
        accent != 0 ? *accent : reach_accent_preset_pair(REACH_ACCENT_PRESET_BLUE);
    const reach_color black = REACH_PURE_BLACK(1.0f);
    const reach_color white = REACH_PURE_WHITE(1.0f);
    const int32_t light = mode == REACH_THEME_MODE_LIGHT;
    const reach_color surface = light ? (reach_color)REACH_LIGHT_SURFACE_ELEVATED(1.0f)
                                      : (reach_color)REACH_DARK_SURFACE_ELEVATED(1.0f);
    reach_color fill = reach_theme_meet_contrast(light ? pair.bright : pair.deep,
                                                 out_theme->shared_text, light ? white : black);
    out_theme->accent_fill = fill;
    out_theme->accent_text =
        reach_theme_meet_contrast(light ? pair.deep : pair.bright, surface, light ? black : white);
    const reach_color text = out_theme->accent_text;
    out_theme->accent_tint = reach_theme_color_alpha(fill, REACH_ACCENT_TINT);
    out_theme->accent_selection = reach_theme_color_alpha(fill, REACH_ACCENT_SELECTION);
    out_theme->widgets_accent_background =
        reach_theme_color_mix(out_theme->widgets_background, fill, REACH_ACCENT_FAINT);
    out_theme->widgets_decoration_fill = reach_theme_color_alpha(fill, REACH_ACCENT_TINT);
    out_theme->widgets_ribbon_fill = reach_theme_color_alpha(fill, REACH_ACCENT_FAINT);
    out_theme->widgets_meter_empty_fill = reach_theme_color_alpha(text, REACH_ACCENT_TINT);
    out_theme->widgets_meter_muted_fill = reach_theme_color_alpha(text, REACH_ACCENT_MUTED);
    out_theme->widgets_section_divider = reach_theme_color_alpha(text, REACH_ACCENT_TINT);
    out_theme->widgets_drag_overlay = reach_theme_color_alpha(fill, REACH_ACCENT_SELECTION);
    out_theme->widgets_primary_control_background =
        reach_theme_color_alpha(fill, REACH_ACCENT_TINT);
    out_theme->widgets_control_background = reach_theme_color_alpha(fill, REACH_ACCENT_FAINT);
    out_theme->settings_avatar_border =
        reach_theme_color_mix(out_theme->settings_card_background, text, REACH_ACCENT_MUTED);
    out_theme->settings_avatar_placeholder_background =
        reach_theme_color_alpha(text, REACH_ACCENT_TINT);
    // The static tables hold the blue preset already resolved, so an unresolved theme matches.
    const reach_accent_pair blue = reach_accent_preset_pair(REACH_ACCENT_PRESET_BLUE);
    const reach_theme *base = reach_theme_for_mode(mode);
    REACH_ASSERT(
        !reach_theme_color_close(pair.deep, blue.deep) ||
        !reach_theme_color_close(pair.bright, blue.bright) ||
        (reach_theme_color_close(out_theme->accent_fill, base->accent_fill) &&
         reach_theme_color_close(out_theme->accent_text, base->accent_text) &&
         reach_theme_color_close(out_theme->accent_tint, base->accent_tint) &&
         reach_theme_color_close(out_theme->accent_selection, base->accent_selection) &&
         reach_theme_color_close(out_theme->widgets_meter_muted_fill,
                                 base->widgets_meter_muted_fill) &&
         reach_theme_color_close(out_theme->widgets_drag_overlay, base->widgets_drag_overlay)));
    float limit = reach_theme_glass_limit(out_theme, light);
    out_theme->glass.floor = light ? limit : 0.0f;
    out_theme->glass.ceiling = light ? 1.0f : limit;
}

reach_color reach_theme_color_alpha(reach_color color, float alpha)
{
    color.a = alpha;
    return color;
}

reach_color reach_theme_color_mix(reach_color from, reach_color to, float t)
{
    reach_color color;
    color.r = from.r + (to.r - from.r) * t;
    color.g = from.g + (to.g - from.g) * t;
    color.b = from.b + (to.b - from.b) * t;
    color.a = from.a + (to.a - from.a) * t;
    return color;
}

float reach_theme_border_thickness(const reach_theme *theme, float dpi_scale)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    return reach_line_thickness(actual->border_thickness * (dpi_scale > 0.0f ? dpi_scale : 1.0f));
}

reach_rect_f32 reach_theme_border_content_rect(const reach_theme *theme, float dpi_scale,
                                               reach_rect_f32 bounds)
{
    float inset = reach_theme_border_thickness(theme, dpi_scale);
    inset = inset > 0.0f ? inset : 0.0f;
    float width = bounds.width > 0.0f ? bounds.width : 0.0f;
    float height = bounds.height > 0.0f ? bounds.height : 0.0f;
    float horizontal = inset < width * 0.5f ? inset : width * 0.5f;
    float vertical = inset < height * 0.5f ? inset : height * 0.5f;
    bounds.x += horizontal;
    bounds.y += vertical;
    bounds.width = width - horizontal * 2.0f;
    bounds.height = height - vertical * 2.0f;
    return bounds;
}

static float reach_shadow_ceil(float value)
{
    float truncated = (float)(int)value;
    return value > truncated ? truncated + 1.0f : truncated;
}

static float reach_shadow_edge(float extent, float offset)
{
    float edge = extent - offset;
    return edge > 0.0f ? reach_shadow_ceil(edge) : 0.0f;
}

float reach_theme_shadow_extent(const reach_shadow *shadow, float dpi_scale)
{
    float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    float sigma = (shadow != 0 ? shadow->blur : 0.0f) * scale * 0.5f;
    return sigma * 3.0f;
}

float reach_theme_shadow_visible_extent(const reach_shadow *shadow, float dpi_scale)
{
    float extent = reach_theme_shadow_extent(shadow, dpi_scale);
    if (shadow == 0 || extent <= 0.0f || shadow->color.a * 0.5f <= 1.0f / 255.0f)
    {
        return 0.0f;
    }
    float spread = extent / 3.0f * 1.41421356f;
    float visible = 0.0f;
    float hidden = extent;
    for (int32_t step = 0; step < 24; ++step)
    {
        float distance = (visible + hidden) * 0.5f;
        if (shadow->color.a * 0.5f * erfcf(distance / spread) > 1.0f / 255.0f)
        {
            visible = distance;
        }
        else
        {
            hidden = distance;
        }
    }
    return hidden;
}

reach_shadow_pad reach_theme_shadow_pad(const reach_shadow *shadow, float dpi_scale)
{
    reach_shadow_pad pad = {0.0f, 0.0f, 0.0f, 0.0f};
    if (shadow == 0 || shadow->color.a <= 0.0f)
    {
        return pad;
    }

    float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    float extent = reach_theme_shadow_extent(shadow, dpi_scale);
    float offset_x = shadow->offset_x * scale;
    float offset_y = shadow->offset_y * scale;

    pad.left = reach_shadow_edge(extent, offset_x);
    pad.right = reach_shadow_edge(extent, -offset_x);
    pad.top = reach_shadow_edge(extent, offset_y);
    pad.bottom = reach_shadow_edge(extent, -offset_y);
    return pad;
}

float reach_theme_dock_corner_radius(const reach_theme *theme, float dock_height)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    return dock_height * actual->dock_corner_radius_ratio;
}

float reach_theme_icon_box_size(const reach_theme *theme, float dock_height)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    return dock_height * actual->icon_box_height_ratio;
}

float reach_theme_icon_size(const reach_theme *theme, float icon_box_size)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    return icon_box_size * actual->icon_max_box_ratio;
}

float reach_theme_icon_box_corner_radius(const reach_theme *theme, float icon_box_size)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    return reach_theme_min(icon_box_size * actual->icon_box_corner_radius_ratio,
                           actual->icon_box_corner_radius_max);
}

float reach_theme_tray_slot_size(const reach_theme *theme, float dock_height)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    return dock_height * actual->tray_slot_size_ratio;
}

reach_motion_spec reach_theme_motion_preset(const reach_theme *theme, reach_motion_preset preset)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    if (preset < 0 || preset >= REACH_MOTION_PRESET_COUNT)
    {
        preset = REACH_MOTION_PRESET_SMOOTH;
    }
    return actual->motion_presets[preset];
}

float reach_theme_stage_motion_scale(const reach_theme *theme, float stage_seconds)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    float reference = actual->stage_motion_reference_seconds;
    float scale = stage_seconds / reference;
    return reference > 0.0f && stage_seconds > 0.0f && scale < 1e30f ? scale : 1.0f;
}
