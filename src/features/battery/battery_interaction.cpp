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
    if (!reach_battery_point_in_rect(state->bounds, x, y))
    {
        return REACH_BATTERY_POINTER_ACTION_DISMISS;
    }
    reach_rect_f32 saver_row = state->saver_row;
    saver_row.x += state->bounds.x;
    saver_row.y += state->bounds.y;
    if (reach_battery_point_in_rect(saver_row, x, y))
    {
        return REACH_BATTERY_POINTER_ACTION_TOGGLE_SAVER;
    }
    return REACH_BATTERY_POINTER_ACTION_NONE;
}
