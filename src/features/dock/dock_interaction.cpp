#include "reach/features/dock.h"

static int32_t reach_dock_rect_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return (float)x >= rect.x && (float)x <= rect.x + rect.width && (float)y >= rect.y && (float)y <= rect.y + rect.height;
}

reach_dock_hit_result reach_dock_hit_test(const reach_dock_layout *layout, int32_t x, int32_t y)
{
    reach_dock_hit_result result = {};
    result.type = REACH_DOCK_HIT_NONE;
    result.index = REACH_MAX_PINNED_APPS;
    if (layout == nullptr) {
        return result;
    }

    if (reach_dock_rect_contains(layout->tray_button, x, y)) {
        result.type = REACH_DOCK_HIT_TRAY_BUTTON;
        return result;
    }

    for (size_t index = 0; index < layout->app_slot_count; ++index) {
        if (reach_dock_rect_contains(layout->app_slots[index], x, y)) {
            result.type = REACH_DOCK_HIT_ITEM;
            result.index = index;
            return result;
        }
    }

    return result;
}

reach_dock_item_action reach_dock_item_action_for_index(const reach_dock_feature_model *model, size_t item_index)
{
    reach_dock_item_action action = {};
    action.type = REACH_DOCK_ITEM_ACTION_NONE;
    action.item_index = item_index;
    action.pinned_index = REACH_MAX_PINNED_APPS;
    if (model == nullptr || item_index >= model->item_count) {
        return action;
    }

    const reach_dock_item_model *item = &model->items[item_index];
    if (item->window != 0) {
        action.type = REACH_DOCK_ITEM_ACTION_FOCUS_WINDOW;
        action.window = item->window;
        action.pinned_index = item->pinned_index;
        action.pin_id = item->pin_id;
        return action;
    }
    if (item->pinned) {
        action.type = REACH_DOCK_ITEM_ACTION_LAUNCH_PINNED;
        action.pinned_index = item->pinned_index;
        action.pin_id = item->pin_id;
    }
    return action;
}

float reach_dock_slot_box_x(const reach_theme *theme, const reach_dock_layout *layout, size_t index)
{
    if (theme == nullptr || layout == nullptr || index >= layout->app_slot_count) {
        return 0.0f;
    }
    float icon_box_size = reach_theme_icon_box_size(theme, layout->bounds.height);
    return layout->app_slots[index].x - layout->bounds.x + (layout->app_slots[index].width - icon_box_size) * 0.5f;
}

float reach_dock_drag_clamped_x(const reach_theme *theme, const reach_dock_layout *layout, int32_t cursor_x, float grab_offset_x)
{
    if (theme == nullptr || layout == nullptr || layout->app_slot_count == 0) {
        return 0.0f;
    }
    float min_x = reach_dock_slot_box_x(theme, layout, 0);
    float max_x = reach_dock_slot_box_x(theme, layout, layout->app_slot_count - 1);
    float wanted_screen_x = (float)cursor_x - grab_offset_x;
    float wanted_local_x = wanted_screen_x - layout->bounds.x;
    if (wanted_local_x < min_x) {
        return min_x;
    }
    if (wanted_local_x > max_x) {
        return max_x;
    }
    return wanted_local_x;
}

size_t reach_dock_reorder_target(const reach_dock_feature_model *model, const reach_dock_layout *layout, size_t current_index, float dragged_box_x)
{
    if (model == nullptr || layout == nullptr || model->item_count == 0 || layout->app_slot_count == 0) {
        return REACH_MAX_PINNED_APPS;
    }

    size_t count = model->item_count < layout->app_slot_count ? model->item_count : layout->app_slot_count;
    if (current_index >= count) {
        return REACH_MAX_PINNED_APPS;
    }

    size_t target = current_index;
    while (target > 0) {
        float threshold = layout->app_slots[target - 1].x + layout->app_slots[target - 1].width * 0.25f;
        if (dragged_box_x > threshold) {
            break;
        }
        --target;
    }
    while (target + 1 < count) {
        float threshold = layout->app_slots[target + 1].x - layout->app_slots[target + 1].width * 0.25f;
        if (dragged_box_x < threshold) {
            break;
        }
        ++target;
    }
    return target;
}
