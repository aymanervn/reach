#include "reach/core/theme.h"

static float reach_theme_min(float a, float b)
{
    return a < b ? a : b;
}

const reach_theme *reach_theme_default(void)
{
    static const reach_theme theme = {
        { 1.0f, 1.0f, 1.0f, 0.50f },
        { 0.13f, 0.12f, 0.11f, 0.85f },
        { 1.0f, 1.0f, 1.0f, 0.70f },
        { 0.55f, 0.57f, 0.60f, 0.16f },
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 0.97f, 0.91f, 0.96f },
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 0.18f, 0.20f, 0.24f, 1.0f },
        { 0.12f, 0.14f, 0.17f, 0.92f },
        { 0.22f, 0.24f, 0.28f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 0.72f, 0.72f, 0.72f, 0.80f },
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 0.72f, 0.72f, 0.72f, 0.90f },
        { 0.22f, 0.24f, 0.28f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 0.96f },
        0.32f,
        22.0f,
        1.0f,
        0.16f,
        0.60f,
        0.76f,
        0.68f,
        0.28f,
        14.0f,
        220.0f,
        120.0f,
        18.0f,
        0.3f,
        1.0f,
        0.56f,
        92.0f
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
