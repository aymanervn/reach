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
    } reach_top_bar_layout;

    enum reach_top_bar_animation_id
    {
        REACH_TOP_BAR_ANIM_Y = 0,
        REACH_TOP_BAR_ANIM_COUNT
    };

    typedef struct reach_top_bar_state
    {
        reach_top_bar_layout layout;
        reach_bar_visibility_state visibility;
        int32_t pointer_sequence_active;
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

    size_t reach_top_bar_input_region_count(const reach_top_bar *top_bar);
    reach_rect_f32 reach_top_bar_input_region_at(const reach_top_bar *top_bar, size_t index);

    void reach_top_bar_begin_reveal_session(reach_top_bar *top_bar);
    reach_bar_visibility_result
    reach_top_bar_update_visibility(reach_top_bar *top_bar,
                                    const reach_bar_visibility_request *request);

    typedef struct reach_top_bar_render_context
    {
        const reach_theme *theme;
        float dpi_scale;
    } reach_top_bar_render_context;

    reach_result reach_top_bar_append_render_commands(reach_top_bar *top_bar,
                                                      const reach_top_bar_render_context *ctx,
                                                      reach_render_command_buffer *out_commands);

#ifdef __cplusplus
}
#endif

#endif
