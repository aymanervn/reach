#ifndef REACH_FEATURES_TRAY_H
#define REACH_FEATURES_TRAY_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/render_commands.h"
#include "reach/ports/tray_provider.h"
#include "reach/core/theme.h"
#include "reach/features/popup.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_tray_model
    {
        reach_tray_item items[REACH_MAX_TRAY_ITEMS];
        reach_rect_f32 item_slots[REACH_MAX_TRAY_ITEMS];
        size_t item_count;
    } reach_tray_model;

    typedef struct reach_tray_render_input
    {
        const reach_theme *theme;
        const reach_tray_model *model;
        reach_rect_f32 bounds;
        float dock_height;
        float dpi_scale;
        size_t click_feedback_index;
        float click_feedback_opacity;
        int32_t text_alignment_center;
    } reach_tray_render_input;

    typedef enum reach_tray_hit_type
    {
        REACH_TRAY_HIT_NONE = 0,
        REACH_TRAY_HIT_ITEM = 1,
        REACH_TRAY_HIT_POPUP = 2
    } reach_tray_hit_type;

    typedef struct reach_tray_hit_result
    {
        reach_tray_hit_type type;
        size_t index;
    } reach_tray_hit_result;

    typedef enum reach_tray_action_type
    {
        REACH_TRAY_FEATURE_ACTION_NONE = 0,
        REACH_TRAY_FEATURE_ACTION_ACTIVATE = 1
    } reach_tray_action_type;

    typedef struct reach_tray_feature_action
    {
        reach_tray_action_type type;
        size_t item_index;
        uint32_t item_id;
        reach_tray_action provider_action;
    } reach_tray_feature_action;

    void reach_tray_model_init(reach_tray_model *model);
    reach_result reach_tray_model_refresh(reach_tray_model *model,
                                          reach_tray_provider_port *provider);
    void reach_tray_compute_popup_layout(reach_tray_model *model, const reach_theme *theme,
                                         const reach_dock_layout *dock_layout,
                                         float dpi_scale, reach_rect_f32 *out_bounds);
    reach_result reach_tray_build_render_commands(const reach_tray_render_input *input,
                                                  reach_render_command_buffer *out_commands);
    reach_tray_hit_result reach_tray_hit_test_popup(const reach_tray_model *model,
                                                    reach_rect_f32 popup_bounds, int32_t x,
                                                    int32_t y);
    reach_tray_feature_action reach_tray_action_for_hit(const reach_tray_model *model,
                                                        reach_tray_hit_result hit,
                                                        reach_tray_action provider_action);

#ifdef __cplusplus
}
#endif

#endif
