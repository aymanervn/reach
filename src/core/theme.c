#include "reach/core/theme.h"

static float reach_theme_min(float a, float b)
{
    return a < b ? a : b;
}

const reach_theme *reach_theme_default(void)
{
    static const reach_theme theme = {
        .light_background = {0.36f, 0.43f, 0.49f, 0.56f},
        .light_border = {1.00f, 1.00f, 1.00f, 0.30f},
        .dark_background = {0.10f, 0.13f, 0.16f, 0.88f},
        .dark_border = {0.10f, 0.13f, 0.16f, 0.88f},
        .dock_shadow = {0.55f, 0.57f, 0.60f, 0.16f},
        .icon_box_background = {1.0f, 1.0f, 1.0f, 1.0f},
        .icon_backplate_background = {1.0f, 0.97f, 0.91f, 0.96f},
        .fallback_icon_background = {0.18f, 0.20f, 0.24f, 1.0f},
        .fallback_icon_text = {0.12f, 0.14f, 0.17f, 0.92f},
        .dock_button_background = {0.52f, 0.59f, 0.65f, 0.66f},
        .dock_system_separator = {0.72f, 0.72f, 0.72f, 1.0f},
        .dock_clock_time = {1.0f, 0.97f, 0.91f, 0.96f},
        .dock_clock_date = {1.0f, 0.97f, 0.91f, 0.56f},
        .dock_power_glyph = {1.0f, 1.0f, 1.0f, 1.0f},

        .quick_settings_slider_track_color = {0.70f, 0.78f, 0.84f, 0.24f},
        .quick_settings_slider_fill_color = {1.0f, 0.97f, 0.91f, 0.92f},
        .quick_settings_slider_muted_fill_color = {1.0f, 0.97f, 0.91f, 0.38f},
        .quick_settings_button_color = {0.70f, 0.78f, 0.84f, 0.24f},
        .now_playing_background = {0.52f, 0.59f, 0.65f, 0.66f},
        .launcher_search_background = {0.10f, 0.13f, 0.16f, 1.0f},
        .launcher_search_border = {0.10f, 0.13f, 0.16f, 0.88f},
        .launcher_search_text = {1.0f, 1.0f, 1.0f, 0.95f},
        .clipboard_background = {0.10f, 0.13f, 0.16f, 0.96f},
        .clipboard_border = {0.30f, 0.36f, 0.41f, 0.42f},
        .clipboard_primary_text = {1.0f, 1.0f, 1.0f, 0.88f},
        .clipboard_secondary_text = {1.0f, 1.0f, 1.0f, 0.52f},
        .clipboard_item_background = {1.0f, 1.0f, 1.0f, 0.035f},
        .clipboard_item_hover_background = {1.0f, 1.0f, 1.0f, 0.075f},
        .clipboard_item_hover_border = {1.0f, 1.0f, 1.0f, 0.24f},
        .clipboard_scrollbar_track = {1.0f, 1.0f, 1.0f, 0.14f},
        .clipboard_scrollbar_thumb = {1.0f, 1.0f, 1.0f, 0.62f},
        .clipboard_panel_radius = 10.0f,
        .clipboard_item_radius = 8.0f,
        .clipboard_title_text_size = 15.0f,
        .clipboard_body_text_size = 12.0f,
        .now_playing_title = {1.0f, 0.97f, 0.91f, 0.94f},
        .now_playing_control_text = {1.0f, 0.97f, 0.91f, 0.90f},
        .now_playing_control_background = {0.70f, 0.78f, 0.84f, 0.0f},
        .now_playing_artist_text = {1.0f, 0.97f, 0.91f, 0.55f},
        .now_playing_artist_text_size = 9.0f,
        .now_playing_text_gap = 4.0f,
        .now_playing_control_gap = 4.0f,
        .settings_selected_nav_background = {0.08f, 0.22f, 0.42f, 0.92f},
        .settings_text = {1.0f, 1.0f, 1.0f, 0.96f},
        .settings_secondary_text = {1.0f, 1.0f, 1.0f, 0.64f},
        .light_text = {1.0f, 0.97f, 0.91f, 0.88f},
        .dark_text = {0.08f, 0.11f, 0.14f, 0.88f},
        .dock_corner_radius_ratio = 0.32f,
        .dock_corner_radius_max = 22.0f,
        .border_thickness = 1.0f,
        .dock_shadow_alpha = 0.16f,
        .icon_box_height_ratio = 0.60f,
        .icon_max_box_ratio = 0.76f,
        .icon_box_corner_radius_ratio = 0.28f,
        .icon_box_corner_radius_max = 14.0f,
        .tray_slot_size_ratio = 0.3f,
        .dock_power_button_corner_radius = 100.0f,
        .dock_system_separator_width = 0.75f,
        .dock_system_separator_height_ratio = 0.56f,
        .dock_clock_width = 92.0f,
        .now_playing_width = 220.0f,
        .now_playing_left_margin = 4.0f,
        .now_playing_height_ratio = 0.85f,
        .now_playing_corner_radius_ratio = 0.32f,
        .now_playing_corner_radius_max = 18.0f,
        .now_playing_padding = 6.0f,
        .now_playing_gap = 6.0f,
        .now_playing_play_button_width = 20.0f,
        .now_playing_prev_next_button_width = 13.2f,
        .now_playing_title_text_size = 12.0f,
    };
    return &theme;
}

float reach_theme_dock_corner_radius(const reach_theme *theme, float dock_height)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    return reach_theme_min(dock_height * actual->dock_corner_radius_ratio,
                           actual->dock_corner_radius_max);
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

float reach_theme_now_playing_height(const reach_theme *theme, float dock_height)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    return dock_height * actual->now_playing_height_ratio;
}

float reach_theme_now_playing_corner_radius(const reach_theme *theme, float widget_height)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    return reach_theme_min(widget_height * actual->now_playing_corner_radius_ratio,
                           actual->now_playing_corner_radius_max);
}
