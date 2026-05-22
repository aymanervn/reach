#ifndef REACH_FEATURES_TRAY_H
#define REACH_FEATURES_TRAY_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/render_commands.h"
#include "reach/ports/tray_provider.h"
#include "reach/theme.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_tray_model {
    reach_tray_item items[REACH_MAX_TRAY_ITEMS];
    reach_rect_f32 item_slots[REACH_MAX_TRAY_ITEMS];
    size_t item_count;
} reach_tray_model;

typedef struct reach_tray_render_input {
    const reach_theme *theme;
    const reach_tray_model *model;
    reach_rect_f32 bounds;
    float dock_height;
    size_t click_feedback_index;
    float click_feedback_opacity;
    int32_t text_alignment_center;
} reach_tray_render_input;

void reach_tray_model_init(reach_tray_model *model);
reach_result reach_tray_model_refresh(reach_tray_model *model, reach_tray_provider_port *provider);
void reach_tray_compute_popup_layout(
    reach_tray_model *model,
    const reach_theme *theme,
    const reach_dock_layout *dock_layout,
    reach_rect_f32 *out_bounds);
reach_result reach_tray_build_render_commands(const reach_tray_render_input *input, reach_render_command_buffer *out_commands);

#ifdef __cplusplus
}
#endif

#endif
