#ifndef REACH_FEATURES_STAGE_H
#define REACH_FEATURES_STAGE_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/geometry.h"
#include "reach/core/limits.h"
#include "reach/core/render_commands.h"
#include "reach/core/theme.h"
#include "reach/core/ui_events.h"
#include "reach/features/feature_capsule.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_STAGE_MAX_TILES (REACH_MAX_OPEN_WINDOWS + 1)

    typedef struct reach_stage reach_stage;

    typedef struct reach_stage_tile
    {
        uintptr_t window;
        uint64_t icon_id;
        uint16_t label[260];
        int32_t minimized;
        int32_t desktop;
        uint32_t monitor_index;
        int32_t monitor_portrait;
        int32_t departing;
        float presence;
        float presence_from;
        float bar_height;
        reach_rect_f32 source_rect;
        reach_rect_f32 target_rect;
        reach_rect_f32 reflow_from;
        reach_rect_f32 current_rect;
        reach_rect_f32 current_bar;
    } reach_stage_tile;

    typedef struct reach_stage_state
    {
        int32_t open;
        int32_t closing;
        float progress;
        float reflow;
        float close_hover;
        float animation_seconds;
        reach_rect_f32 bounds;
        float dpi_scale;
        size_t tile_count;
        size_t tile_generation;
        size_t hover_index;
        int32_t has_hover;
        size_t close_hover_index;
        size_t selected_index;
        int32_t has_selection;
        reach_stage_tile tiles[REACH_STAGE_MAX_TILES];
    } reach_stage_state;

    typedef struct reach_stage_open_window
    {
        uintptr_t window;
        uint64_t icon_id;
        const uint16_t *label;
        int32_t minimized;
        int32_t desktop;
        uint32_t monitor_index;
        int32_t monitor_portrait;
        reach_rect_f32 frame;
    } reach_stage_open_window;

    typedef struct reach_stage_thumbnail_placement
    {
        uintptr_t window;
        reach_rect_f32 destination;
        reach_rect_f32 source_screen;
        float opacity;
        int32_t visible;
        int32_t source_screen_valid;
        int32_t minimized;
        int32_t desktop;
    } reach_stage_thumbnail_placement;

    typedef enum reach_stage_action_type
    {
        REACH_STAGE_ACTION_NONE = 0,
        REACH_STAGE_ACTION_ACTIVATE_WINDOW = 1,
        REACH_STAGE_ACTION_SHOW_DESKTOP = 2,
        REACH_STAGE_ACTION_CLOSE_WINDOW = 3
    } reach_stage_action_type;

    reach_result reach_stage_create(reach_stage **out_stage);
    void reach_stage_destroy(reach_stage *stage);

    const reach_stage_state *reach_stage_state_ptr(const reach_stage *stage);

    int32_t reach_stage_is_open(const reach_stage *stage);
    int32_t reach_stage_animation_active(const reach_stage *stage);

    void reach_stage_set_animation_seconds(reach_stage *stage, float seconds);

    reach_result reach_stage_open(reach_stage *stage, reach_rect_f32 monitor_bounds,
                                  float dpi_scale, const reach_stage_open_window *windows,
                                  size_t window_count);
    void reach_stage_begin_close(reach_stage *stage);
    void reach_stage_force_close(reach_stage *stage);

    int32_t reach_stage_update_windows(reach_stage *stage, const reach_stage_open_window *windows,
                                       size_t window_count);
    size_t reach_stage_tile_generation(const reach_stage *stage);

    size_t reach_stage_thumbnail_count(const reach_stage *stage);
    reach_result reach_stage_thumbnail_at(const reach_stage *stage, size_t index,
                                          reach_stage_thumbnail_placement *out_placement);

    int32_t reach_stage_tile_at_point(const reach_stage *stage, reach_point_f32 point,
                                      size_t *out_index);
    int32_t reach_stage_close_button_at_point(const reach_stage *stage, reach_point_f32 point,
                                              size_t *out_index);
    reach_rect_f32 reach_stage_tile_close_button_rect(const reach_stage *stage, size_t index);

    const reach_feature_capsule_ops *reach_stage_capsule_ops(void);
    typedef struct reach_stage_render_context
    {
        const reach_theme *theme;
        reach_rect_f32 bounds;
        float dpi_scale;
    } reach_stage_render_context;

    reach_result reach_stage_append_render_commands(reach_stage *stage,
                                                    const reach_stage_render_context *ctx,
                                                    reach_render_command_buffer *out_commands);

#ifdef __cplusplus
}
#endif

#endif
