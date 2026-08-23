#include "reach/features/context_menu.h"

#include "context_menu_common.h"

static int32_t reach_context_menu_rect_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return (float)x >= rect.x && (float)x <= rect.x + rect.width && (float)y >= rect.y &&
           (float)y <= rect.y + rect.height;
}

reach_rect_f32 reach_context_menu_close_button_rect(const reach_context_menu_metrics *metrics,
                                                    reach_rect_f32 item_slot, float dpi_scale)
{
    float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    float size = metrics->close_button_size * scale;
    float inset = metrics->close_button_inset * scale;
    reach_rect_f32 button = {};
    button.width = size;
    button.height = size;
    button.x = item_slot.x + item_slot.width - inset - size;
    button.y = item_slot.y + (item_slot.height - size) * 0.5f;
    return button;
}

reach_context_menu_hit_result
reach_context_menu_hit_test_close_buttons(const reach_context_menu_state *state, int32_t x,
                                          int32_t y)
{
    reach_context_menu_hit_result hit = {};
    hit.hit = 0;
    hit.index = REACH_CONTEXT_MENU_MAX_ITEMS;
    if (state == nullptr || !state->window_list_open)
    {
        return hit;
    }
    for (size_t index = 0; index < state->item_count; ++index)
    {
        if (state->item_windows[index] != 0 &&
            reach_context_menu_rect_contains(
                reach_context_menu_close_button_rect(
                    reach_context_menu_metrics_for(state->power_open, state->window_list_open),
                    state->item_slots[index], state->dpi_scale),
                x, y))
        {
            hit.hit = 1;
            hit.index = index;
            return hit;
        }
    }
    return hit;
}

reach_context_menu_hit_result reach_context_menu_hit_test_items(const reach_rect_f32 *item_slots,
                                                                size_t item_count, int32_t x,
                                                                int32_t y)
{
    reach_context_menu_hit_result hit = {};
    hit.hit = 0;
    hit.index = REACH_CONTEXT_MENU_MAX_ITEMS;
    if (item_slots == nullptr)
    {
        return hit;
    }
    for (size_t index = 0; index < item_count; ++index)
    {
        if (reach_context_menu_rect_contains(item_slots[index], x, y))
        {
            hit.hit = 1;
            hit.index = index;
            return hit;
        }
    }
    return hit;
}

reach_context_menu_action reach_context_menu_action_for_hit(const uint32_t *item_commands,
                                                            size_t item_count,
                                                            reach_context_menu_hit_result hit)
{
    reach_context_menu_action action = {};
    if (item_commands == nullptr || !hit.hit || hit.index >= item_count)
    {
        return action;
    }
    action.command = item_commands[hit.index];
    return action;
}
