#include "reach/core/theme.h"

static float reach_theme_min(float a, float b)
{
    return a < b ? a : b;
}

const reach_theme *reach_theme_default(void)
{
    static const reach_theme theme = {
        .dock_background =                { 1.0f, 1.0f, 1.0f, 0.50f },
        .switcher_background =            { 0.13f, 0.12f, 0.11f, 0.85f },
        .dock_border =                    { 1.0f, 1.0f, 1.0f, 0.70f },
        .dock_shadow =                    { 0.55f, 0.57f, 0.60f, 0.16f },
        .icon_box_background =            { 1.0f, 1.0f, 1.0f, 1.0f },
        .icon_backplate_background =      { 1.0f, 0.97f, 0.91f, 0.96f },
        .icon_backplate_edge =            { 1.0f, 1.0f, 1.0f, 1.0f },
        .fallback_icon_background =       { 0.18f, 0.20f, 0.24f, 1.0f },
        .fallback_icon_text =             { 0.12f, 0.14f, 0.17f, 0.92f },
        .tray_button_background =         { 1.0f, 0.97f, 0.91f, 0.36f },
        .tray_popup_background =          { 0.13f, 0.12f, 0.11f, 0.85f },
        .tray_glyph =                     {  1.0f, 1.0f, 1.0f, 1.0f },
        .dock_system_separator =          { 0.72f, 0.72f, 0.72f, 1.0f },
        .dock_clock_time =                { 1.0f, 0.97f, 0.91f, 0.96f },
        .dock_clock_date =                { 1.0f, 0.97f, 0.91f, 0.56f },
        .dock_power_button_background =   { 1.0f, 0.97f, 0.91f, 0.36f },
        .dock_power_glyph =               { 1.0f, 1.0f, 1.0f, 1.0f },
        .dock_corner_radius_ratio =       0.32f,
        .dock_corner_radius_max =         22.0f,
        .dock_border_thickness =          1.0f,
        .dock_shadow_alpha =              0.16f,
        .icon_box_height_ratio =          0.60f,
        .icon_max_box_ratio =             0.76f,
        .icon_backplate_scale =           0.68f,
        .icon_box_corner_radius_ratio =   0.28f,
        .icon_box_corner_radius_max =     14.0f,
        .tray_popup_width =               220.0f,
        .tray_popup_height =              120.0f,
        .tray_popup_corner_radius =       18.0f,
        .tray_slot_size_ratio =           0.3f,
        .dock_power_button_corner_radius = 100.0f,
        .dock_system_separator_width =    0.75f,
        .dock_system_separator_height_ratio = 0.56f,
        .dock_clock_width =               92.0f,
    };
    return &theme;
}

float reach_theme_dock_corner_radius(const reach_theme *theme, float dock_height)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    return reach_theme_min(dock_height * actual->dock_corner_radius_ratio, actual->dock_corner_radius_max);
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
    return reach_theme_min(icon_box_size * actual->icon_box_corner_radius_ratio, actual->icon_box_corner_radius_max);
}

float reach_theme_tray_slot_size(const reach_theme *theme, float dock_height)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    return dock_height * actual->tray_slot_size_ratio;
}
