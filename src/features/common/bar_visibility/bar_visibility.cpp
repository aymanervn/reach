#include "reach/features/common/bar_visibility.h"

static int32_t reach_bar_point_in_rect(reach_point_i32 point, reach_rect_f32 rect)
{
    return (float)point.x >= rect.x && (float)point.x < rect.x + rect.width &&
           (float)point.y >= rect.y && (float)point.y < rect.y + rect.height;
}

static float reach_bar_hidden_y(reach_bar_edge edge, reach_rect_f32 shown_bounds,
                                reach_rect_f32 monitor_bounds)
{
    if (edge == REACH_BAR_EDGE_TOP)
    {
        return monitor_bounds.y - shown_bounds.height - 4.0f;
    }
    return monitor_bounds.y + monitor_bounds.height + 4.0f;
}

reach_rect_f32 reach_bar_reveal_edge_bounds(reach_bar_edge edge, int32_t mode,
                                            reach_rect_f32 shown_bounds,
                                            reach_rect_f32 monitor_bounds)
{
    reach_rect_f32 bounds = {};
    bounds.x = shown_bounds.x;
    bounds.width = shown_bounds.width;

    if (edge == REACH_BAR_EDGE_TOP)
    {
        if (mode == REACH_BAR_REVEAL_EDGE_BRIDGE)
        {
            bounds.y = monitor_bounds.y;
            bounds.height = shown_bounds.y + shown_bounds.height - monitor_bounds.y;
        }
        else
        {
            bounds.y = monitor_bounds.y - 1.0f;
            bounds.height = 3.0f;
        }
        return bounds;
    }

    float monitor_bottom = monitor_bounds.y + monitor_bounds.height;
    if (mode == REACH_BAR_REVEAL_EDGE_BRIDGE)
    {
        bounds.y = shown_bounds.y;
        bounds.height = monitor_bottom - shown_bounds.y;
    }
    else
    {
        bounds.y = monitor_bottom - 2.0f;
        bounds.height = 3.0f;
    }
    return bounds;
}

void reach_bar_visibility_reset(reach_bar_visibility_state *state)
{
    if (state != nullptr)
    {
        *state = {};
    }
}

void reach_bar_begin_reveal_session(reach_bar_visibility_state *state)
{
    if (state != nullptr)
    {
        state->reveal_session_active = 1;
    }
}

reach_bar_visibility_result reach_bar_update_visibility(reach_bar_visibility_state *state,
                                                        reach_animation_manager *manager,
                                                        size_t y_track,
                                                        const reach_bar_visibility_request *request)
{
    reach_bar_visibility_result result = {};
    if (state == nullptr || manager == nullptr || request == nullptr)
    {
        return result;
    }

    float hidden_y =
        reach_bar_hidden_y(request->edge, request->shown_bounds, request->monitor_bounds);
    reach_rect_f32 current_bounds = request->shown_bounds;
    if (state->animation_initialized)
    {
        current_bounds.y = reach_animation_manager_value(manager, y_track);
    }
    reach_rect_f32 bridge_bounds =
        reach_bar_reveal_edge_bounds(request->edge, REACH_BAR_REVEAL_EDGE_BRIDGE,
                                     request->shown_bounds, request->monitor_bounds);
    int32_t pointer_over_bar =
        request->pointer_valid && reach_bar_point_in_rect(request->pointer, current_bounds);
    int32_t pointer_in_bridge =
        request->pointer_valid && reach_bar_point_in_rect(request->pointer, bridge_bounds);

    int32_t target_hidden = 0;
    int32_t edge_mode = REACH_BAR_REVEAL_EDGE_DISABLED;

    if (request->game_mode)
    {
        state->reveal_session_active = 0;
        target_hidden = 1;
        edge_mode = REACH_BAR_REVEAL_EDGE_DISABLED;
    }
    else if (!request->can_hide)
    {
        state->reveal_session_active = 0;
        target_hidden = 0;
        edge_mode = REACH_BAR_REVEAL_EDGE_BRIDGE;
    }
    else if (request->pointer_sequence_active || request->transient_open)
    {
        target_hidden = 0;
        edge_mode = REACH_BAR_REVEAL_EDGE_BRIDGE;
    }
    else if (state->reveal_session_active)
    {
        if (pointer_in_bridge || pointer_over_bar)
        {
            edge_mode = REACH_BAR_REVEAL_EDGE_BRIDGE;
        }
        else
        {
            state->reveal_session_active = 0;
            target_hidden = 1;
            edge_mode = REACH_BAR_REVEAL_EDGE_THIN;
        }
    }
    else if (pointer_over_bar)
    {
        target_hidden = 0;
    }
    else
    {
        target_hidden = 1;
        edge_mode = REACH_BAR_REVEAL_EDGE_THIN;
    }

    float target_y = target_hidden ? hidden_y : request->shown_bounds.y;
    if (target_hidden && request->sticky_feedback)
    {
        result.clear_sticky_feedback = 1;
    }

    if (!state->animation_initialized)
    {
        state->animation_initialized = 1;
        state->target_hidden = target_hidden;
        reach_animation_manager_set(manager, y_track, target_y);
    }

    if (state->target_hidden != target_hidden)
    {
        state->target_hidden = target_hidden;
        reach_animation_manager_animate_to(manager, y_track, target_y, 0.25,
                                           REACH_EASING_EASE_IN_OUT);
    }

    reach_rect_f32 animated = request->shown_bounds;
    animated.y = reach_animation_manager_value(manager, y_track);
    result.animated_bounds = animated;
    result.edge_mode = edge_mode;
    result.visible = target_hidden ? 0 : 1;
    return result;
}
