#ifndef REACH_THEME_H
#define REACH_THEME_H

#include "reach/core/ui_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_theme {
    reach_color dock_background;
    reach_color dock_border;
    reach_color dock_shadow;
    reach_color icon_box_background;
    reach_color icon_backplate_background;
    reach_color icon_backplate_edge;
    reach_color fallback_icon_background;
    reach_color fallback_icon_text;
    reach_color tray_button_background;
    reach_color tray_glyph;
    float dock_corner_radius_ratio;
    float dock_corner_radius_max;
    float dock_border_thickness;
    float dock_shadow_alpha;
    float icon_box_height_ratio;
    float icon_max_box_ratio;
    float icon_backplate_scale;
    float icon_box_corner_radius_ratio;
    float icon_box_corner_radius_max;
    float tray_popup_width;
    float tray_popup_height;
    float tray_popup_corner_radius;
    float tray_slot_size_ratio;
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
