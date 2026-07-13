#ifndef REACH_FEATURES_SWITCHER_H
#define REACH_FEATURES_SWITCHER_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/render_commands.h"
#include "reach/features/feature_capsule.h"
#include "reach/services/icon_service.h"
#include "reach/services/window_tracking.h"
#include "reach/core/theme.h"
#include "reach/core/pinned_app.h"
#include "reach/core/ui_events.h"
#include "reach/ports/window_manager.h"
#include "reach/ports/icon_provider.h"
#include "reach/support/animation.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_SWITCHER_VISIBLE_MAX 12

    typedef struct reach_switcher_model
    {
        size_t window_count;
        size_t selected_index;
        size_t visible_start;
    } reach_switcher_model;

    typedef struct reach_switcher_render_item
    {
        uint64_t icon_id;
        uint16_t label[260];
    } reach_switcher_render_item;

    typedef struct reach_switcher_render_input
    {
        const reach_theme *theme;
        reach_rect_f32 bounds;
        const reach_switcher_model *model;
        const reach_switcher_render_item *items;
        size_t item_count;
        float dpi_scale;
        int32_t text_alignment_center;
        int32_t text_weight_demi_bold;
    } reach_switcher_render_input;

    size_t reach_switcher_visible_count(size_t window_count);
    reach_rect_f32 reach_switcher_bounds_for_count(reach_rect_f32 monitor_bounds,
                                                   size_t visible_count);
    reach_rect_f32 reach_switcher_bounds_for_count_scaled(reach_rect_f32 monitor_bounds,
                                                          size_t visible_count, float dpi_scale);
    void reach_switcher_update_visible_start(reach_switcher_model *model);
    reach_result reach_switcher_build_render_commands(const reach_switcher_render_input *input,
                                                      reach_render_command_buffer *out_commands);

    typedef struct reach_switcher_state
    {
        int32_t open;
        size_t selected_index;
        size_t visible_start;
        uintptr_t windows[REACH_MAX_PINNED_APPS];
        size_t window_count;
    } reach_switcher_state;

    typedef struct reach_switcher reach_switcher;

    reach_result reach_switcher_create(reach_switcher **out_switcher);

    void reach_switcher_attach_services(reach_switcher *switcher, reach_icon_service *icons,
                                        reach_window_tracking *windows);

    const reach_switcher_state *reach_switcher_state_ptr(reach_switcher *switcher);

    int32_t reach_switcher_is_open(const reach_switcher *switcher);

    const reach_feature_capsule_ops *reach_switcher_capsule_ops(void);

    const reach_ui_event_type *reach_switcher_routed_events(size_t *out_count);
    void reach_switcher_destroy(reach_switcher *switcher);

    void reach_switcher_tick(reach_switcher *switcher, double delta_seconds);

    void reach_switcher_reset_width_animation(reach_switcher *switcher);

    int32_t reach_switcher_width_animation_active(const reach_switcher *switcher);

    reach_rect_f32 reach_switcher_apply_width_animation(
        reach_switcher *switcher, int32_t transition_visible, int32_t open, int32_t bounds_valid,
        float last_bounds_width, reach_rect_f32 target, int32_t *out_request_redraw);

    typedef enum reach_switcher_action_type
    {
        REACH_SWITCHER_ACTION_NONE = 0,
        REACH_SWITCHER_ACTION_OPENED = 1,
        REACH_SWITCHER_ACTION_CHANGED = 2,
        REACH_SWITCHER_ACTION_CLOSED = 3,
        REACH_SWITCHER_ACTION_COMMITTED = 4
    } reach_switcher_action_type;

    typedef struct reach_switcher_action
    {
        reach_switcher_action_type type;
        uintptr_t window;
    } reach_switcher_action;

    typedef struct reach_switcher_render_context
    {
        const reach_theme *theme;
        reach_rect_f32 bounds;
        float dpi_scale;
        int32_t icon_size_px;
    } reach_switcher_render_context;

    void reach_switcher_force_close(reach_switcher *switcher);

    reach_switcher_action reach_switcher_handle_event(reach_switcher *switcher,
                                                      const reach_ui_event *event);

    reach_switcher_action reach_switcher_sync_windows(reach_switcher *switcher);

    reach_result reach_switcher_append_render_commands(reach_switcher *switcher,
                                                       const reach_switcher_render_context *ctx,
                                                       reach_render_command_buffer *out_commands);

#ifdef __cplusplus
}
#endif

#endif
