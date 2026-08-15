#ifndef REACH_FEATURES_TOP_BAR_H
#define REACH_FEATURES_TOP_BAR_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/geometry.h"
#include "reach/core/render_commands.h"
#include "reach/core/theme.h"
#include "reach/features/common/bar_visibility.h"
#include "reach/features/feature_capsule.h"
#include "reach/support/animation.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_top_bar_pill
    {
        REACH_TOP_BAR_PILL_POWER_CLOCK = 0,
        REACH_TOP_BAR_PILL_NOW_PLAYING = 1,
        REACH_TOP_BAR_PILL_CURRENT_APP = 2,
        REACH_TOP_BAR_PILL_TRAY = 3,
        REACH_TOP_BAR_PILL_QUICK_SETTINGS = 4,
        REACH_TOP_BAR_PILL_COUNT = 5
    } reach_top_bar_pill;

    typedef struct reach_top_bar_layout
    {
        reach_rect_f32 bounds;
        reach_rect_f32 pills[REACH_TOP_BAR_PILL_COUNT];
        int32_t pill_visible[REACH_TOP_BAR_PILL_COUNT];
        reach_rect_f32 power_button;
        reach_rect_f32 clock;
    } reach_top_bar_layout;

    enum reach_top_bar_animation_id
    {
        REACH_TOP_BAR_ANIM_Y = 0,
        REACH_TOP_BAR_ANIM_POWER_HOVER,
        REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY,
        REACH_TOP_BAR_ANIM_COUNT
    };

    enum reach_top_bar_feedback_slot
    {
        REACH_TOP_BAR_FEEDBACK_POWER_BUTTON = 0,
        REACH_TOP_BAR_FEEDBACK_NONE
    };

    typedef enum reach_top_bar_pointer_region
    {
        REACH_TOP_BAR_POINTER_REGION_NONE = 0,
        REACH_TOP_BAR_POINTER_REGION_POWER_BUTTON = 1
    } reach_top_bar_pointer_region;

    typedef enum reach_top_bar_pointer_action_kind
    {
        REACH_TOP_BAR_POINTER_ACTION_NONE = 0,
        REACH_TOP_BAR_POINTER_ACTION_PRESS_POWER = 1,
        REACH_TOP_BAR_POINTER_ACTION_TOGGLE_POWER = 2
    } reach_top_bar_pointer_action_kind;

    typedef struct reach_top_bar_state
    {
        reach_top_bar_layout layout;
        reach_bar_visibility_state visibility;
        int32_t pointer_sequence_active;

        int32_t pressed_control;
        size_t feedback_index;
        int32_t feedback_pressed;
        int32_t power_hovered;
        int32_t power_release_suppressed;

        uint16_t clock_time_text[32];
        uint16_t clock_date_text[64];
        int32_t clock_initialized;
        int64_t clock_last_minute;
    } reach_top_bar_state;

    typedef struct reach_top_bar reach_top_bar;

    reach_result reach_top_bar_create(reach_top_bar **out_top_bar);
    void reach_top_bar_destroy(reach_top_bar *top_bar);

    const reach_feature_capsule_ops *reach_top_bar_capsule_ops(void);
    const reach_top_bar_state *reach_top_bar_state_ptr(const reach_top_bar *top_bar);
    reach_animation_manager *reach_top_bar_manager(reach_top_bar *top_bar);

    float reach_top_bar_height(const reach_theme *theme, float dock_height);

    typedef struct reach_top_bar_build_context
    {
        const reach_theme *theme;
        reach_rect_f32 monitor_bounds;
        float dpi_scale;
        float dock_height;
    } reach_top_bar_build_context;

    void reach_top_bar_build_layout(reach_top_bar *top_bar,
                                    const reach_top_bar_build_context *ctx);

    reach_point_i32 reach_top_bar_local_point(const reach_top_bar_layout *layout, int32_t x,
                                              int32_t y);
    reach_rect_f32 reach_top_bar_rect_to_screen(const reach_top_bar_layout *layout,
                                                reach_rect_f32 rect);

    size_t reach_top_bar_input_region_count(const reach_top_bar *top_bar);
    reach_rect_f32 reach_top_bar_input_region_at(const reach_top_bar *top_bar, size_t index);

    reach_top_bar_pointer_region reach_top_bar_pointer_region_at(const reach_top_bar *top_bar,
                                                                 int32_t local_x, int32_t local_y);
    int32_t reach_top_bar_pointer_sequence_active(const reach_top_bar *top_bar);
    void reach_top_bar_suppress_power_release(reach_top_bar *top_bar);

    int32_t reach_top_bar_update_clock(reach_top_bar *top_bar);

    void reach_top_bar_begin_reveal_session(reach_top_bar *top_bar);
    reach_bar_visibility_result
    reach_top_bar_update_visibility(reach_top_bar *top_bar,
                                    const reach_bar_visibility_request *request);

    typedef struct reach_top_bar_render_context
    {
        const reach_theme *theme;
        float dpi_scale;
        int32_t battery_valid;
        int32_t battery_percent;
    } reach_top_bar_render_context;

    reach_result reach_top_bar_append_render_commands(reach_top_bar *top_bar,
                                                      const reach_top_bar_render_context *ctx,
                                                      reach_render_command_buffer *out_commands);

#ifdef __cplusplus
}
#endif

#endif
