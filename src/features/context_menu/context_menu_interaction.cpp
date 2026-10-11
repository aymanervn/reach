#include "reach/features/context_menu.h"

#include "context_menu_common.h"

static int32_t reach_context_menu_rect_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return rect.width > 0.0f && rect.height > 0.0f && (float)x >= rect.x &&
           (float)x <= rect.x + rect.width && (float)y >= rect.y &&
           (float)y <= rect.y + rect.height;
}

reach_context_menu_hit_result
reach_context_menu_hit_test_close_buttons(const reach_context_menu *menu, int32_t x, int32_t y)
{
    reach_context_menu_hit_result hit = {};
    hit.index = REACH_CONTEXT_MENU_NO_ITEM;
    if (menu == nullptr || !menu->state.window_list_open)
    {
        return hit;
    }
    for (size_t visible = 0; visible < menu->previews.count; ++visible)
    {
        const reach_context_menu_preview_card *card = &menu->previews.cards[visible];
        if (reach_context_menu_rect_contains(card->close, x, y))
        {
            hit.hit = 1;
            hit.index = card->index;
            break;
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
    hit.index = REACH_CONTEXT_MENU_NO_ITEM;
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
