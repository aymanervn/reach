#ifndef REACH_FEATURES_BATTERY_H
#define REACH_FEATURES_BATTERY_H

#include "reach/core/render_commands.h"
#include "reach/core/theme.h"
#include "reach/features/feature_capsule.h"
#include "reach/features/popup.h"
#include "reach/support/util.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_battery_pointer_action_kind
    {
        REACH_BATTERY_POINTER_ACTION_NONE = 0,
        REACH_BATTERY_POINTER_ACTION_DISMISS = 1,
        REACH_BATTERY_POINTER_ACTION_TOGGLE_SAVER = 2
    } reach_battery_pointer_action_kind;

    typedef struct reach_battery_model
    {
        int32_t percent;
        int32_t saver_on;
        int32_t saver_pending;
        int32_t saver_pending_enabled;
    } reach_battery_model;

    typedef struct reach_battery_state
    {
        int32_t open;
        reach_battery_model model;
        reach_rect_f32 bounds;
        reach_rect_f32 percent_label;
        reach_rect_f32 separator;
        reach_rect_f32 saver_row;
        reach_rect_f32 saver_label;
        reach_rect_f32 saver_toggle;
        float notch_anchor_x;
        int32_t drop_direction;
    } reach_battery_state;

    typedef struct reach_battery_open_context
    {
        const reach_theme *theme;
        reach_rect_f32 monitor;
        reach_rect_f32 anchor_button;
        float bar_edge_y;
        float dpi_scale;
        int32_t drop_direction;
    } reach_battery_open_context;

    typedef struct reach_battery_render_context
    {
        const reach_theme *theme;
        float dpi_scale;
    } reach_battery_render_context;

    typedef struct reach_battery reach_battery;

    reach_result reach_battery_create(reach_battery **out_battery);
    void reach_battery_destroy(reach_battery *battery);

    const reach_feature_capsule_ops *reach_battery_capsule_ops(void);

    const reach_battery_state *reach_battery_state_ptr(const reach_battery *battery);
    int32_t reach_battery_is_open(const reach_battery *battery);
    void reach_battery_force_close(reach_battery *battery);
    void reach_battery_reset(reach_battery *battery);

    void reach_battery_open(reach_battery *battery, const reach_battery_open_context *ctx);
    void reach_battery_relayout(reach_battery *battery, const reach_battery_open_context *ctx);

    int32_t reach_battery_set_power(reach_battery *battery, int32_t percent, int32_t saver_on);
    void reach_battery_set_saver_pending(reach_battery *battery, int32_t pending,
                                         int32_t pending_enabled);
    int32_t reach_battery_saver_pending(const reach_battery *battery);

    int32_t reach_battery_model_saver_effective(const reach_battery_model *model);
    void reach_battery_format_percent(uint16_t *dst, size_t dst_count, int32_t percent);

    reach_battery_pointer_action_kind reach_battery_hit_test(const reach_battery_state *state,
                                                             int32_t x, int32_t y);

    reach_result reach_battery_append_render_commands(const reach_battery *battery,
                                                      const reach_battery_render_context *ctx,
                                                      reach_render_command_buffer *out_commands);

#ifdef __cplusplus
}
#endif

#endif
