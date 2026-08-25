#include "reach/core/theme.h"

#define REACH_RGB8(r, g, b, a) {(r) / 255.0f, (g) / 255.0f, (b) / 255.0f, (a)}

#define REACH_THEME_STAGE_BACKDROP_ALPHA 0.99f

#define REACH_PURE_WHITE(a) REACH_RGB8(255, 255, 255, a)
#define REACH_CRITICAL_RED(a) REACH_RGB8(217, 61, 61, a)
#define REACH_BUTTON_PRIMARY(a) REACH_RGB8(31, 110, 158, a)
#define REACH_BUTTON_SUCCESS(a) REACH_RGB8(41, 148, 77, a)

#define REACH_DARK_DEEP(a) REACH_RGB8(17, 18, 17, a)
#define REACH_DARK_BASE(a) REACH_RGB8(30, 31, 42, a)
#define REACH_DARK_RAISED(a) REACH_RGB8(34, 36, 45, a)
#define REACH_DARK_STRUCTURE(a) REACH_RGB8(37, 38, 49, a)
#define REACH_DARK_MIDTONE(a) REACH_RGB8(44, 45, 54, a)
#define REACH_DARK_MID_CONTRAST_GRAY(a) REACH_RGB8(148, 150, 153, a)
#define REACH_DARK_HIGH_CONTRAST_WHITE(a) REACH_RGB8(203, 203, 214, a)
#define REACH_DARK_EDGE_ON_DEEP(a) REACH_RGB8(101, 101, 106, a)
#define REACH_DARK_EDGE_ON_BASE(a) REACH_RGB8(108, 108, 119, a)

#define REACH_DARK_ACCENT_BLUE(a) REACH_RGB8(61, 148, 255, a)
#define REACH_DARK_ACCENT_CYAN(a) REACH_RGB8(51, 184, 245, a)
#define REACH_DARK_ACCENT_TEAL(a) REACH_RGB8(79, 199, 219, a)
#define REACH_DARK_ACCENT_GREEN(a) REACH_RGB8(56, 209, 110, a)
#define REACH_DARK_ACCENT_PURPLE(a) REACH_RGB8(179, 97, 242, a)
#define REACH_DARK_ACCENT_LAVENDER(a) REACH_RGB8(179, 140, 242, a)
#define REACH_DARK_ACCENT_ORANGE(a) REACH_RGB8(242, 140, 56, a)
#define REACH_DARK_ACCENT_YELLOW(a) REACH_RGB8(247, 191, 56, a)
#define REACH_DARK_ACCENT_RED(a) REACH_RGB8(245, 115, 89, a)
#define REACH_DARK_BATTERY_BLUE(a) REACH_RGB8(38, 102, 168, a)
#define REACH_DARK_BATTERY_GREEN(a) REACH_RGB8(29, 128, 66, a)
#define REACH_DARK_BATTERY_YELLOW(a) REACH_RGB8(137, 97, 7, a)
#define REACH_DARK_BATTERY_RED(a) REACH_RGB8(174, 55, 55, a)

#define REACH_LIGHT_BASE(a) REACH_RGB8(255, 255, 255, a)
#define REACH_LIGHT_RAISED(a) REACH_RGB8(245, 245, 244, a)
#define REACH_LIGHT_STRUCTURE(a) REACH_RGB8(239, 239, 238, a)
#define REACH_LIGHT_MIDTONE(a) REACH_RGB8(220, 220, 220, a)
#define REACH_LIGHT_MID_CONTRAST_GRAY(a) REACH_RGB8(110, 111, 109, a)
#define REACH_LIGHT_HIGH_CONTRAST_BLACK(a) REACH_RGB8(18, 23, 31, a)
#define REACH_LIGHT_EDGE_ON_BASE(a) REACH_RGB8(148, 151, 154, a)

#define REACH_LIGHT_ACCENT_BLUE(a) REACH_RGB8(31, 97, 209, a)
#define REACH_LIGHT_ACCENT_CYAN(a) REACH_RGB8(20, 128, 184, a)
#define REACH_LIGHT_ACCENT_TEAL(a) REACH_RGB8(26, 128, 143, a)
#define REACH_LIGHT_ACCENT_GREEN(a) REACH_RGB8(26, 143, 71, a)
#define REACH_LIGHT_ACCENT_PURPLE(a) REACH_RGB8(122, 51, 189, a)
#define REACH_LIGHT_ACCENT_LAVENDER(a) REACH_RGB8(112, 77, 194, a)
#define REACH_LIGHT_ACCENT_ORANGE(a) REACH_RGB8(194, 97, 20, a)
#define REACH_LIGHT_ACCENT_YELLOW(a) REACH_RGB8(168, 122, 15, a)
#define REACH_LIGHT_ACCENT_RED(a) REACH_RGB8(199, 61, 46, a)
#define REACH_LIGHT_BATTERY_BLUE(a) REACH_RGB8(126, 190, 245, a)
#define REACH_LIGHT_BATTERY_GREEN(a) REACH_RGB8(106, 214, 143, a)
#define REACH_LIGHT_BATTERY_YELLOW(a) REACH_RGB8(242, 207, 98, a)
#define REACH_LIGHT_BATTERY_RED(a) REACH_RGB8(244, 151, 142, a)

#define REACH_SHADOW_AMBIENT(blur_px)                                                              \
    {.offset_x = 0.0f, .offset_y = 0.0f, .blur = (blur_px), .color = REACH_RGB8(0, 0, 0, 0.3f)}

#define REACH_THEME_SHADOWS                                                                        \
    .bar_shadow = REACH_SHADOW_AMBIENT(5.0f), .popup_shadow = REACH_SHADOW_AMBIENT(5.0f)

#define REACH_THEME_METRICS                                                                        \
    .radius_small = 12.0f, .radius_large = 20.0f, .button_pressed_darken = 0.65f,                  \
    .accent_tint_alpha = 0.20f, .dock_corner_radius_ratio = 0.5f, .border_thickness = 1.0f,        \
    .icon_box_height_ratio = 0.60f, .icon_max_box_ratio = 0.76f,                                   \
    .icon_box_corner_radius_ratio = 0.28f, .icon_box_corner_radius_max = 14.0f,                    \
    .tray_slot_size_ratio = 0.3f, .bar_separator_dot_size = 4.0f, .bar_separator_dot_gap = 9.0f,   \
    .now_playing_width = 220.0f, .now_playing_padding = 9.0f, .now_playing_gap = 6.0f,             \
    .now_playing_control_gap = 8.0f, .now_playing_play_button_width = 20.0f,                       \
    .now_playing_prev_next_button_width = 13.2f, .now_playing_title_text_size = 13.5f,             \
    .surface_open_seconds = 0.16f, .surface_close_seconds = 0.12f, .bar_reveal_seconds = 0.25f,    \
    .stage_animation_seconds = 0.28f, .stage_close_hover_seconds = 0.14f,                          \
    .stage_reflow_seconds = 0.34f

static const reach_theme reach_theme_dark = {
    .mode = REACH_THEME_MODE_DARK,

    .dock_background = REACH_DARK_DEEP(1.0f),
    .top_bar_background = REACH_DARK_DEEP(1.0f),
    .switcher_background = REACH_DARK_DEEP(1.0f),
    .bar_border = REACH_DARK_EDGE_ON_DEEP(1.0f),
    .popup_background = REACH_DARK_BASE(1.0f),
    .popup_border = REACH_DARK_EDGE_ON_BASE(1.0f),
    .popup_separator = REACH_DARK_STRUCTURE(1.0f),
    .system_hud_background = REACH_DARK_BASE(1.0f),
    .system_hud_border = REACH_DARK_EDGE_ON_BASE(1.0f),
    .system_hud_primary_text = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),
    .system_hud_secondary_text = REACH_DARK_MID_CONTRAST_GRAY(1.0f),
    .system_hud_icon_background = REACH_DARK_STRUCTURE(1.0f),
    .system_hud_glyph = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),
    .system_hud_track = REACH_DARK_MIDTONE(1.0f),
    .system_hud_fill = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),
    .system_hud_muted_fill = REACH_DARK_HIGH_CONTRAST_WHITE(0.42f),
    .toggle_track_off = REACH_DARK_DEEP(1.0f),
    .toggle_knob = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),

    .primary_text = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),
    .inverse_text = REACH_DARK_BASE(0.92f),

    .icon_box_background = REACH_PURE_WHITE(1.0f),
    .system_glyph = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),
    .fallback_icon_text = REACH_DARK_BASE(0.92f),
    .bar_text_primary = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),
    .bar_text_secondary = REACH_DARK_MID_CONTRAST_GRAY(1.0f),
    .bar_button_background = REACH_DARK_HIGH_CONTRAST_WHITE(0.10f),
    .bar_tray_background = REACH_DARK_STRUCTURE(1.0f),
    .bar_power_hover_background = REACH_DARK_HIGH_CONTRAST_WHITE(0.18f),
    .bar_battery_background = REACH_DARK_HIGH_CONTRAST_WHITE(0.10f),
    .bar_battery_fill = REACH_DARK_BATTERY_GREEN(1.0f),
    .bar_battery_low = REACH_DARK_BATTERY_RED(1.0f),
    .bar_battery_saver = REACH_DARK_BATTERY_YELLOW(1.0f),
    .bar_battery_charging = REACH_DARK_BATTERY_BLUE(1.0f),
    .bar_stats_cpu = REACH_DARK_ACCENT_CYAN(1.0f),
    .bar_stats_memory = REACH_DARK_ACCENT_LAVENDER(1.0f),
    .bar_stats_download = REACH_DARK_ACCENT_GREEN(1.0f),
    .bar_stats_upload = REACH_DARK_ACCENT_ORANGE(1.0f),
    .dock_running_indicator = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),
    .bar_click_feedback = REACH_DARK_RAISED(1.0f),
    .bar_separator_dot = REACH_DARK_MID_CONTRAST_GRAY(1.0f),

    .tray_click_feedback = REACH_DARK_RAISED(1.0f),

    .quick_settings_slider_track = REACH_DARK_MIDTONE(1.0f),
    .quick_settings_slider_fill = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),
    .quick_settings_slider_muted_fill = REACH_DARK_HIGH_CONTRAST_WHITE(0.38f),
    .quick_settings_button_background = REACH_DARK_MIDTONE(1.0f),
    .quick_settings_secondary_text = REACH_DARK_MID_CONTRAST_GRAY(1.0f),
    .quick_settings_separator = REACH_DARK_MIDTONE(1.0f),
    .quick_settings_app_volume_track = REACH_DARK_HIGH_CONTRAST_WHITE(0.32f),
    .quick_settings_app_volume_fill = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),
    .quick_settings_app_volume_muted_fill = REACH_DARK_HIGH_CONTRAST_WHITE(0.42f),
    .quick_settings_section_label = REACH_DARK_MID_CONTRAST_GRAY(1.0f),

    .now_playing_background = REACH_DARK_DEEP(0.35f),
    .now_playing_title = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),
    .now_playing_control_text = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),

    .launcher_search_background = REACH_DARK_DEEP(1.0f),
    .launcher_search_text = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),
    .launcher_placeholder_text = REACH_DARK_MID_CONTRAST_GRAY(1.0f),
    .launcher_selection_highlight = REACH_DARK_HIGH_CONTRAST_WHITE(0.25f),
    .launcher_search_icon = REACH_DARK_HIGH_CONTRAST_WHITE(0.32f),
    .launcher_border = REACH_DARK_EDGE_ON_DEEP(1.0f),
    .launcher_scrollbar_track = REACH_DARK_STRUCTURE(1.0f),
    .launcher_scrollbar_thumb = REACH_DARK_MID_CONTRAST_GRAY(1.0f),
    .launcher_row_selected_background = REACH_DARK_RAISED(1.0f),
    .launcher_row_icon_background = REACH_DARK_RAISED(1.0f),
    .launcher_row_icon_glyph = REACH_DARK_MID_CONTRAST_GRAY(1.0f),
    .launcher_row_title = REACH_DARK_HIGH_CONTRAST_WHITE(0.86f),
    .launcher_row_title_selected = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),
    .launcher_row_path = REACH_DARK_MID_CONTRAST_GRAY(1.0f),
    .launcher_row_path_selected = REACH_DARK_MID_CONTRAST_GRAY(1.0f),

    .switcher_selection_background = REACH_DARK_RAISED(1.0f),
    .switcher_label_text = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),

    .context_menu_text = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),
    .context_menu_hover_background = REACH_DARK_MIDTONE(1.0f),
    .context_menu_close_background = REACH_CRITICAL_RED(1.0f),
    .context_menu_close_glyph = REACH_PURE_WHITE(1.0f),
    .menu_accent_shutdown = REACH_DARK_ACCENT_RED(1.0f),
    .menu_accent_sleep = REACH_DARK_ACCENT_LAVENDER(1.0f),
    .menu_accent_restart = REACH_DARK_ACCENT_GREEN(1.0f),
    .menu_accent_lock = REACH_DARK_ACCENT_YELLOW(1.0f),

    .stage_backdrop = REACH_DARK_DEEP(REACH_THEME_STAGE_BACKDROP_ALPHA),
    .stage_tile_placeholder = REACH_DARK_RAISED(1.0f),
    .stage_tile_highlight = REACH_DARK_HIGH_CONTRAST_WHITE(0.85f),
    .stage_tile_label = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),
    .stage_close_background = REACH_DARK_STRUCTURE(0.45f),
    .stage_close_hover_background = REACH_CRITICAL_RED(1.0f),
    .stage_close_glyph = REACH_PURE_WHITE(0.80f),
    .stage_close_hover_glyph = REACH_PURE_WHITE(1.0f),

    .clipboard_background = REACH_DARK_DEEP(1.0f),
    .clipboard_border = REACH_DARK_EDGE_ON_DEEP(1.0f),
    .clipboard_primary_text = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),
    .clipboard_secondary_text = REACH_DARK_MID_CONTRAST_GRAY(1.0f),
    .clipboard_item_background = REACH_DARK_RAISED(1.0f),
    .clipboard_item_hover_background = REACH_DARK_STRUCTURE(1.0f),
    .clipboard_item_hover_border = REACH_DARK_MID_CONTRAST_GRAY(0.30f),
    .clipboard_scrollbar_track = REACH_DARK_STRUCTURE(1.0f),
    .clipboard_scrollbar_thumb = REACH_DARK_MID_CONTRAST_GRAY(1.0f),

    .settings_background = REACH_DARK_DEEP(1.0f),
    .settings_nav_background = REACH_DARK_BASE(1.0f),
    .settings_text = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),
    .settings_secondary_text = REACH_DARK_MID_CONTRAST_GRAY(1.0f),
    .settings_card_background = REACH_DARK_RAISED(1.0f),
    .settings_pill_background = REACH_DARK_RAISED(1.0f),
    .settings_power_custom_time_background = REACH_DARK_MIDTONE(1.0f),
    .settings_input_background = REACH_DARK_STRUCTURE(1.0f),
    .settings_divider = REACH_DARK_STRUCTURE(1.0f),
    .settings_toggle_track_off = REACH_DARK_DEEP(1.0f),
    .settings_toggle_knob = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),
    .settings_button_text = REACH_DARK_HIGH_CONTRAST_WHITE(1.0f),
    .settings_button_primary = REACH_BUTTON_PRIMARY(1.0f),
    .settings_button_success = REACH_BUTTON_SUCCESS(1.0f),
    .settings_button_danger = REACH_CRITICAL_RED(1.0f),
    .settings_button_disabled_background = REACH_DARK_STRUCTURE(1.0f),
    .settings_status_success = REACH_DARK_ACCENT_GREEN(1.0f),
    .settings_status_error = REACH_DARK_ACCENT_RED(1.0f),
    .settings_window_button_background = REACH_DARK_MID_CONTRAST_GRAY(1.0f),
    .settings_window_close_hover = REACH_CRITICAL_RED(1.0f),
    .settings_window_minimize_hover = REACH_DARK_ACCENT_YELLOW(1.0f),
    .settings_scrollbar_track = REACH_DARK_STRUCTURE(1.0f),
    .settings_scrollbar_thumb = REACH_DARK_MID_CONTRAST_GRAY(1.0f),
    .accents =
        {
            [REACH_THEME_ACCENT_BLUE] = REACH_DARK_ACCENT_BLUE(1.0f),
            [REACH_THEME_ACCENT_CYAN] = REACH_DARK_ACCENT_CYAN(1.0f),
            [REACH_THEME_ACCENT_TEAL] = REACH_DARK_ACCENT_TEAL(1.0f),
            [REACH_THEME_ACCENT_GREEN] = REACH_DARK_ACCENT_GREEN(1.0f),
            [REACH_THEME_ACCENT_PURPLE] = REACH_DARK_ACCENT_PURPLE(1.0f),
            [REACH_THEME_ACCENT_LAVENDER] = REACH_DARK_ACCENT_LAVENDER(1.0f),
            [REACH_THEME_ACCENT_ORANGE] = REACH_DARK_ACCENT_ORANGE(1.0f),
            [REACH_THEME_ACCENT_YELLOW] = REACH_DARK_ACCENT_YELLOW(1.0f),
            [REACH_THEME_ACCENT_RED] = REACH_DARK_ACCENT_RED(1.0f),
        },

    REACH_THEME_SHADOWS,
    REACH_THEME_METRICS,
};

static const reach_theme reach_theme_light = {
    .mode = REACH_THEME_MODE_LIGHT,

    .dock_background = REACH_LIGHT_BASE(1.0f),
    .top_bar_background = REACH_LIGHT_BASE(1.0f),
    .switcher_background = REACH_LIGHT_BASE(1.0f),
    .bar_border = REACH_LIGHT_EDGE_ON_BASE(1.0f),
    .popup_background = REACH_LIGHT_BASE(1.0f),
    .popup_border = REACH_LIGHT_EDGE_ON_BASE(1.0f),
    .popup_separator = REACH_LIGHT_STRUCTURE(1.0f),
    .system_hud_background = REACH_LIGHT_BASE(1.0f),
    .system_hud_border = REACH_LIGHT_EDGE_ON_BASE(1.0f),
    .system_hud_primary_text = REACH_LIGHT_HIGH_CONTRAST_BLACK(1.0f),
    .system_hud_secondary_text = REACH_LIGHT_MID_CONTRAST_GRAY(1.0f),
    .system_hud_icon_background = REACH_LIGHT_STRUCTURE(1.0f),
    .system_hud_glyph = REACH_LIGHT_HIGH_CONTRAST_BLACK(1.0f),
    .system_hud_track = REACH_LIGHT_MIDTONE(1.0f),
    .system_hud_fill = REACH_LIGHT_HIGH_CONTRAST_BLACK(1.0f),
    .system_hud_muted_fill = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.42f),
    .toggle_track_off = REACH_LIGHT_MID_CONTRAST_GRAY(0.8f),
    .toggle_knob = REACH_LIGHT_BASE(1.0f),

    .primary_text = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.92f),
    .inverse_text = REACH_PURE_WHITE(0.95f),

    .icon_box_background = REACH_LIGHT_RAISED(1.0f),
    .system_glyph = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.92f),
    .fallback_icon_text = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.92f),
    .bar_text_primary = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.94f),
    .bar_text_secondary = REACH_LIGHT_MID_CONTRAST_GRAY(1.0f),
    .bar_button_background = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.07f),
    .bar_tray_background = REACH_LIGHT_MIDTONE(1.0f),
    .bar_power_hover_background = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.14f),
    .bar_battery_background = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.07f),
    .bar_battery_fill = REACH_LIGHT_BATTERY_GREEN(1.0f),
    .bar_battery_low = REACH_LIGHT_BATTERY_RED(1.0f),
    .bar_battery_saver = REACH_LIGHT_BATTERY_YELLOW(1.0f),
    .bar_battery_charging = REACH_LIGHT_BATTERY_BLUE(1.0f),
    .bar_stats_cpu = REACH_LIGHT_ACCENT_CYAN(1.0f),
    .bar_stats_memory = REACH_LIGHT_ACCENT_LAVENDER(1.0f),
    .bar_stats_download = REACH_LIGHT_ACCENT_GREEN(1.0f),
    .bar_stats_upload = REACH_LIGHT_ACCENT_ORANGE(1.0f),
    .dock_running_indicator = REACH_LIGHT_MID_CONTRAST_GRAY(1.0f),
    .bar_click_feedback = REACH_LIGHT_RAISED(1.0f),
    .bar_separator_dot = REACH_LIGHT_MID_CONTRAST_GRAY(1.0f),

    .tray_click_feedback = REACH_LIGHT_RAISED(1.0f),

    .quick_settings_slider_track = REACH_LIGHT_MIDTONE(1.0f),
    .quick_settings_slider_fill = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.90f),
    .quick_settings_slider_muted_fill = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.36f),
    .quick_settings_button_background = REACH_LIGHT_MIDTONE(1.0f),
    .quick_settings_secondary_text = REACH_LIGHT_MID_CONTRAST_GRAY(1.0f),
    .quick_settings_separator = REACH_LIGHT_STRUCTURE(1.0f),
    .quick_settings_app_volume_track = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.26f),
    .quick_settings_app_volume_fill = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.88f),
    .quick_settings_app_volume_muted_fill = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.38f),
    .quick_settings_section_label = REACH_LIGHT_MID_CONTRAST_GRAY(1.0f),

    .now_playing_background = REACH_LIGHT_RAISED(0.35f),
    .now_playing_title = REACH_LIGHT_HIGH_CONTRAST_BLACK(1.0f),
    .now_playing_control_text = REACH_LIGHT_HIGH_CONTRAST_BLACK(1.0f),

    .launcher_search_background = REACH_LIGHT_BASE(1.0f),
    .launcher_search_text = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.95f),
    .launcher_placeholder_text = REACH_LIGHT_MID_CONTRAST_GRAY(1.0f),
    .launcher_selection_highlight = REACH_LIGHT_ACCENT_BLUE(0.28f),
    .launcher_search_icon = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.36f),
    .launcher_border = REACH_LIGHT_EDGE_ON_BASE(1.0f),
    .launcher_scrollbar_track = REACH_LIGHT_MIDTONE(1.0f),
    .launcher_scrollbar_thumb = REACH_LIGHT_MID_CONTRAST_GRAY(1.0f),
    .launcher_row_selected_background = REACH_LIGHT_MIDTONE(1.0f),
    .launcher_row_icon_background = REACH_LIGHT_RAISED(1.0f),
    .launcher_row_icon_glyph = REACH_LIGHT_MID_CONTRAST_GRAY(1.0f),
    .launcher_row_title = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.86f),
    .launcher_row_title_selected = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.96f),
    .launcher_row_path = REACH_LIGHT_MID_CONTRAST_GRAY(1.0f),
    .launcher_row_path_selected = REACH_LIGHT_MID_CONTRAST_GRAY(1.0f),

    .switcher_selection_background = REACH_LIGHT_MIDTONE(1.0f),
    .switcher_label_text = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.96f),

    .context_menu_text = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.96f),
    .context_menu_hover_background = REACH_LIGHT_MIDTONE(1.0f),
    .context_menu_close_background = REACH_CRITICAL_RED(1.0f),
    .context_menu_close_glyph = REACH_PURE_WHITE(1.0f),
    .menu_accent_shutdown = REACH_LIGHT_ACCENT_RED(1.0f),
    .menu_accent_sleep = REACH_LIGHT_ACCENT_LAVENDER(1.0f),
    .menu_accent_restart = REACH_LIGHT_ACCENT_GREEN(1.0f),
    .menu_accent_lock = REACH_LIGHT_ACCENT_YELLOW(1.0f),

    .stage_backdrop = REACH_LIGHT_BASE(REACH_THEME_STAGE_BACKDROP_ALPHA),
    .stage_tile_placeholder = REACH_LIGHT_RAISED(1.0f),
    .stage_tile_highlight = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.85f),
    .stage_tile_label = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.95f),
    .stage_close_background = REACH_LIGHT_MID_CONTRAST_GRAY(0.55f),
    .stage_close_hover_background = REACH_CRITICAL_RED(1.0f),
    .stage_close_glyph = REACH_PURE_WHITE(0.80f),
    .stage_close_hover_glyph = REACH_PURE_WHITE(1.0f),

    .clipboard_background = REACH_LIGHT_BASE(1.0f),
    .clipboard_border = REACH_LIGHT_EDGE_ON_BASE(1.0f),
    .clipboard_primary_text = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.90f),
    .clipboard_secondary_text = REACH_LIGHT_MID_CONTRAST_GRAY(1.0f),
    .clipboard_item_background = REACH_LIGHT_RAISED(1.0f),
    .clipboard_item_hover_background = REACH_LIGHT_STRUCTURE(1.0f),
    .clipboard_item_hover_border = REACH_LIGHT_MID_CONTRAST_GRAY(0.30f),
    .clipboard_scrollbar_track = REACH_LIGHT_MIDTONE(1.0f),
    .clipboard_scrollbar_thumb = REACH_LIGHT_MID_CONTRAST_GRAY(1.0f),

    .settings_background = REACH_LIGHT_RAISED(1.0f),
    .settings_nav_background = REACH_LIGHT_BASE(1.0f),
    .settings_text = REACH_LIGHT_HIGH_CONTRAST_BLACK(0.96f),
    .settings_secondary_text = REACH_LIGHT_MID_CONTRAST_GRAY(1.0f),
    .settings_card_background = REACH_LIGHT_RAISED(1.0f),
    .settings_pill_background = REACH_LIGHT_RAISED(1.0f),
    .settings_power_custom_time_background = REACH_LIGHT_STRUCTURE(1.0f),
    .settings_input_background = REACH_LIGHT_STRUCTURE(1.0f),
    .settings_divider = REACH_LIGHT_STRUCTURE(1.0f),
    .settings_toggle_track_off = REACH_LIGHT_MID_CONTRAST_GRAY(0.8f),
    .settings_toggle_knob = REACH_LIGHT_BASE(1.0f),
    .settings_button_text = REACH_PURE_WHITE(0.96f),
    .settings_button_primary = REACH_BUTTON_PRIMARY(1.0f),
    .settings_button_success = REACH_BUTTON_SUCCESS(1.0f),
    .settings_button_danger = REACH_CRITICAL_RED(1.0f),
    .settings_button_disabled_background = REACH_LIGHT_STRUCTURE(1.0f),
    .settings_status_success = REACH_LIGHT_ACCENT_GREEN(1.0f),
    .settings_status_error = REACH_LIGHT_ACCENT_RED(1.0f),
    .settings_window_button_background = REACH_LIGHT_MID_CONTRAST_GRAY(1.0f),
    .settings_window_close_hover = REACH_CRITICAL_RED(1.0f),
    .settings_window_minimize_hover = REACH_LIGHT_ACCENT_YELLOW(1.0f),
    .settings_scrollbar_track = REACH_LIGHT_MIDTONE(1.0f),
    .settings_scrollbar_thumb = REACH_LIGHT_MID_CONTRAST_GRAY(1.0f),
    .accents =
        {
            [REACH_THEME_ACCENT_BLUE] = REACH_LIGHT_ACCENT_BLUE(1.0f),
            [REACH_THEME_ACCENT_CYAN] = REACH_LIGHT_ACCENT_CYAN(1.0f),
            [REACH_THEME_ACCENT_TEAL] = REACH_LIGHT_ACCENT_TEAL(1.0f),
            [REACH_THEME_ACCENT_GREEN] = REACH_LIGHT_ACCENT_GREEN(1.0f),
            [REACH_THEME_ACCENT_PURPLE] = REACH_LIGHT_ACCENT_PURPLE(1.0f),
            [REACH_THEME_ACCENT_LAVENDER] = REACH_LIGHT_ACCENT_LAVENDER(1.0f),
            [REACH_THEME_ACCENT_ORANGE] = REACH_LIGHT_ACCENT_ORANGE(1.0f),
            [REACH_THEME_ACCENT_YELLOW] = REACH_LIGHT_ACCENT_YELLOW(1.0f),
            [REACH_THEME_ACCENT_RED] = REACH_LIGHT_ACCENT_RED(1.0f),
        },

    REACH_THEME_SHADOWS,
    REACH_THEME_METRICS,
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

reach_color reach_theme_accent_color(const reach_theme *theme, reach_theme_accent accent)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    if (accent < 0 || accent >= REACH_THEME_ACCENT_COUNT)
    {
        accent = REACH_THEME_ACCENT_BLUE;
    }
    return actual->accents[accent];
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
    return actual->border_thickness * (dpi_scale > 0.0f ? dpi_scale : 1.0f);
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
