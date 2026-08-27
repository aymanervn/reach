#include "battery_common.h"

static int32_t reach_battery_point_in_rect(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return (float)x >= rect.x && (float)x <= rect.x + rect.width && (float)y >= rect.y &&
           (float)y <= rect.y + rect.height;
}

reach_battery_pointer_action_kind reach_battery_hit_test(const reach_battery_state *state,
                                                         int32_t x, int32_t y)
{
    if (state == nullptr || !state->open)
    {
        return REACH_BATTERY_POINTER_ACTION_NONE;
    }
    reach_rect_f32 surface_bounds = {0.0f, 0.0f, state->bounds.width, state->bounds.height};
    if (!reach_battery_point_in_rect(surface_bounds, x, y))
    {
        return REACH_BATTERY_POINTER_ACTION_DISMISS;
    }
    if (reach_battery_point_in_rect(state->saver_row, x, y))
    {
        return REACH_BATTERY_POINTER_ACTION_TOGGLE_SAVER;
    }
    return REACH_BATTERY_POINTER_ACTION_NONE;
}
