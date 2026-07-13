#ifndef REACH_FEATURES_TRAY_H
#define REACH_FEATURES_TRAY_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/render_commands.h"
#include "reach/ports/tray_provider.h"
#include "reach/core/theme.h"
#include "reach/features/feature_capsule.h"
#include "reach/features/popup.h"
#include "reach/support/animation.h"
#include "reach/support/util.h"

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

    typedef enum reach_tray_pointer_action_kind
    {
        REACH_TRAY_POINTER_ACTION_NONE = 0,
        REACH_TRAY_POINTER_ACTION_ACTIVATE_LEFT = 1,
        REACH_TRAY_POINTER_ACTION_ACTIVATE_RIGHT = 2
    } reach_tray_pointer_action_kind;

    void reach_tray_model_init(reach_tray_model *model);
    reach_result reach_tray_model_refresh(reach_tray_model *model,
                                          reach_tray_provider_port *provider);
    void reach_tray_compute_popup_layout(reach_tray_model *model, const reach_theme *theme,
                                         const reach_dock_layout *dock_layout, float dpi_scale,
                                         reach_rect_f32 *out_bounds);
    reach_result reach_tray_build_render_commands(const reach_tray_render_input *input,
                                                  reach_render_command_buffer *out_commands);

    enum reach_tray_animation_id
    {
        REACH_TRAY_ANIM_FEEDBACK_OPACITY = 0,
        REACH_TRAY_ANIM_COUNT
    };

    typedef struct reach_tray_state
    {
        int32_t popup_open;
        reach_tray_model model;

        size_t feedback_index;
        int32_t feedback_pressed;
    } reach_tray_state;

    typedef struct reach_tray reach_tray;

    reach_result reach_tray_create(reach_tray **out_tray);
    void reach_tray_destroy(reach_tray *tray);

    reach_animation_manager *reach_tray_animation_manager(reach_tray *tray);

    const reach_feature_capsule_ops *reach_tray_capsule_ops(void);

    int32_t reach_tray_popup_is_open(const reach_tray *tray);
    int32_t reach_tray_set_popup_open(reach_tray *tray, int32_t open);
    reach_result reach_tray_refresh(reach_tray *tray, reach_tray_provider_port *provider);
    void reach_tray_layout_popup(reach_tray *tray, const reach_theme *theme,
                                 const reach_dock_layout *dock_layout, float dpi_scale,
                                 reach_rect_f32 *out_bounds);

    const reach_tray_state *reach_tray_state_ptr(reach_tray *tray);

    size_t reach_tray_item_count(reach_tray *tray);
    uint64_t reach_tray_item_icon_id(reach_tray *tray, size_t index);

    typedef struct reach_tray_render_context
    {
        const reach_theme *theme;
        reach_rect_f32 bounds;
        float dock_height;
        float dpi_scale;
    } reach_tray_render_context;

    reach_result reach_tray_append_render_commands(reach_tray *tray,
                                                   const reach_tray_render_context *ctx,
                                                   reach_render_command_buffer *out_commands);

#ifdef __cplusplus
}
#endif

#endif
