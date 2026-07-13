#include "reach/features/tray.h"

#include "tray_common.h"

static int32_t reach_tray_rect_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return (float)x >= rect.x && (float)x <= rect.x + rect.width && (float)y >= rect.y &&
           (float)y <= rect.y + rect.height;
}

reach_tray_hit_result reach_tray_hit_test_popup(const reach_tray_model *model,
                                                reach_rect_f32 popup_bounds, int32_t x, int32_t y)
{
    reach_tray_hit_result hit = {};
    hit.type = REACH_TRAY_HIT_NONE;
    hit.index = REACH_MAX_TRAY_ITEMS;
    if (model == nullptr)
    {
        return hit;
    }

    for (size_t index = 0; index < model->item_count; ++index)
    {
        if (reach_tray_rect_contains(model->item_slots[index], x, y))
        {
            hit.type = REACH_TRAY_HIT_ITEM;
            hit.index = index;
            return hit;
        }
    }

    if (reach_tray_rect_contains(popup_bounds, x, y))
    {
        hit.type = REACH_TRAY_HIT_POPUP;
    }
    return hit;
}

reach_tray_feature_action reach_tray_action_for_hit(const reach_tray_model *model,
                                                    reach_tray_hit_result hit,
                                                    reach_tray_action provider_action)
{
    reach_tray_feature_action action = {};
    action.type = REACH_TRAY_FEATURE_ACTION_NONE;
    action.item_index = hit.index;
    if (model == nullptr || hit.type != REACH_TRAY_HIT_ITEM || hit.index >= model->item_count)
    {
        return action;
    }

    action.type = REACH_TRAY_FEATURE_ACTION_ACTIVATE;
    action.item_id = model->items[hit.index].id;
    action.provider_action = provider_action;
    return action;
}
