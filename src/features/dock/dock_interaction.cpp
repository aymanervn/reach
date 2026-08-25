#include "reach/features/dock.h"

#include "dock_common_state.h"
#include "dock_interaction.h"

#include "dock_common.h"
#include "dock_metrics.h"

#include <math.h>

static int32_t reach_dock_rect_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return (float)x >= rect.x && (float)x <= rect.x + rect.width && (float)y >= rect.y &&
           (float)y <= rect.y + rect.height;
}

reach_dock_hit_result reach_dock_hit_test(const reach_dock_layout *layout, int32_t x, int32_t y)
{
    reach_dock_hit_result result = {};
    result.type = REACH_DOCK_HIT_NONE;
    result.index = REACH_MAX_DOCK_ITEMS;

    if (layout == nullptr)
    {
        return result;
    }

    if (reach_dock_rect_contains(layout->trigger_button, x, y))
    {
        result.type = REACH_DOCK_HIT_TRIGGER;
        return result;
    }

    for (size_t index = 0; index < layout->app_slot_count; ++index)
    {
        if (reach_dock_rect_contains(layout->app_slots[index], x, y))
        {
            result.type = REACH_DOCK_HIT_ITEM;
            result.index = index;
            return result;
        }
    }

    return result;
}

reach_dock_item_action reach_dock_item_action_for_index(const reach_dock_feature_model *model,
                                                        size_t item_index)
{
    reach_dock_item_action action = {};
    action.type = REACH_DOCK_ITEM_ACTION_NONE;
    action.item_index = item_index;
    action.pinned_index = REACH_MAX_DOCK_ITEMS;

    if (model == nullptr || item_index >= model->item_count)
    {
        return action;
    }

    const reach_dock_item_model *item = &model->items[item_index];

    if (item->window != 0)
    {
        action.type = REACH_DOCK_ITEM_ACTION_FOCUS_WINDOW;
        action.window = item->window;
        action.pinned_index = item->pinned_index;
        action.pin_id = item->pinned ? item->app_id : 0;
        return action;
    }

    if (item->pinned)
    {
        action.type = REACH_DOCK_ITEM_ACTION_LAUNCH_PINNED;
        action.pinned_index = item->pinned_index;
        action.pin_id = item->app_id;
    }

    return action;
}

float reach_dock_slot_box_x(const reach_theme *theme, const reach_dock_layout *layout, size_t index)
{
    if (theme == nullptr || layout == nullptr || index >= layout->app_slot_count)
    {
        return 0.0f;
    }

    float icon_box_size = reach_theme_icon_box_size(theme, layout->bounds.height);
    return reach_dock_icon_box_for_slot(layout->app_slots[index], icon_box_size).x;
}

float reach_dock_drag_clamped_x(const reach_theme *theme, const reach_dock_layout *layout,
                                int32_t local_cursor_x, float grab_offset_x)
{
    if (theme == nullptr || layout == nullptr || layout->app_slot_count == 0)
    {
        return 0.0f;
    }

    float min_x = reach_dock_slot_box_x(theme, layout, 0);
    float max_x = reach_dock_slot_box_x(theme, layout, layout->app_slot_count - 1);
    float wanted_local_x = (float)local_cursor_x - grab_offset_x;

    if (wanted_local_x < min_x)
    {
        return min_x;
    }
    if (wanted_local_x > max_x)
    {
        return max_x;
    }

    return wanted_local_x;
}

static void reach_dock_drag_begin(reach_dock *dock, size_t index, int32_t x, int32_t y,
                                  const reach_dock_interaction_context *ctx,
                                  reach_dock_interaction_result *out)
{
    reach_dock_state *state = reach_dock_state_mut(dock);

    if (index >= state->model.item_count || reach_draggable_tracking(&state->drag.gesture))
    {
        return;
    }

    reach_dock_order_key key = reach_dock_item_key_at(&state->model, index);
    uint64_t target = ((uint64_t)(key.pinned ? 1 : 0) << 32) | key.app_id;
    reach_draggable_begin(&state->drag.gesture, target, x, y);
    state->drag.target_index = index;
    state->drag.key = key;

    float box_x = reach_dock_slot_box_x(ctx->theme, ctx->layout, index);
    state->drag.grab_offset_x = (float)x - (ctx->layout->bounds.x + box_x);

    state->drag.x = box_x;
    reach_animation_manager_reset(reach_dock_manager(dock), REACH_DOCK_ANIM_DRAG_SNAP);
}

void reach_dock_item_press(reach_dock *dock, size_t index, int32_t x, int32_t y,
                           const reach_dock_interaction_context *ctx,
                           reach_dock_interaction_result *out)
{
    if (dock == nullptr || ctx == nullptr || out == nullptr || ctx->theme == nullptr ||
        ctx->layout == nullptr)
    {
        return;
    }

    reach_dock_drag_begin(dock, index, x, y, ctx, out);
}

void reach_dock_drag_update(reach_dock *dock, int32_t x, int32_t y,
                            const reach_dock_interaction_context *ctx,
                            reach_dock_interaction_result *out)
{
    if (dock == nullptr || ctx == nullptr || out == nullptr || ctx->theme == nullptr ||
        ctx->layout == nullptr)
    {
        return;
    }

    reach_dock_state *state = reach_dock_state_mut(dock);

    if (!reach_draggable_tracking(&state->drag.gesture))
    {
        return;
    }

    reach_draggable_result gesture = {};
    reach_draggable_update(&state->drag.gesture, x, y, 36, &gesture);
    if (!gesture.moved)
    {
        return;
    }

    int32_t local_cursor_x = static_cast<int32_t>((float)x - ctx->layout->bounds.x);
    float next_drag_x = reach_dock_drag_clamped_x(ctx->theme, ctx->layout, local_cursor_x,
                                                  state->drag.grab_offset_x);

    if (fabsf(next_drag_x - state->drag.x) >= 0.5f)
    {
        state->drag.x = next_drag_x;
        out->redraw = 1;
    }

    float dragged_box_x = state->drag.x;

    size_t current = reach_dock_feature_model_find_order_key(&state->model, state->drag.key);

    size_t target = reach_dock_reorder_target(&state->model, ctx->layout, current, dragged_box_x);

    if (target != REACH_MAX_DOCK_ITEMS && target != state->drag.target_index)
    {
        if (current != REACH_MAX_DOCK_ITEMS)
        {
            reach_dock_feature_model_move_order(&state->model, current, target);
            out->rebuild_items = 1;
        }

        state->drag.target_index = target;
        out->redraw = 1;
        return;
    }
}

void reach_dock_drag_end(reach_dock *dock, const reach_dock_interaction_context *ctx,
                         reach_dock_interaction_result *out)
{
    if (dock == nullptr || ctx == nullptr || out == nullptr || ctx->theme == nullptr ||
        ctx->layout == nullptr)
    {
        return;
    }

    reach_dock_state *state = reach_dock_state_mut(dock);

    if (!reach_draggable_tracking(&state->drag.gesture))
    {
        return;
    }

    uint32_t pin_id = state->drag.key.pinned ? state->drag.key.app_id : 0;
    int32_t dragged_pinned = state->drag.key.pinned;
    int32_t moved = reach_draggable_moved(&state->drag.gesture);
    size_t target_pinned_index =
        dragged_pinned ? reach_dock_feature_model_pinned_order_index(&state->model, pin_id)
                       : REACH_MAX_DOCK_ITEMS;

    size_t target_index = reach_dock_feature_model_find_item_key(&state->model, state->drag.key);

    reach_draggable_end(&state->drag.gesture, nullptr);
    out->redraw = 1;

    if (moved && target_index < ctx->layout->app_slot_count)
    {
        float target_x = reach_dock_slot_box_x(ctx->theme, ctx->layout, target_index);

        reach_animation_manager_start(reach_dock_manager(dock), REACH_DOCK_ANIM_DRAG_SNAP,
                                      state->drag.x, target_x, 0.12, REACH_EASING_EASE_IN_OUT);
    }
    else
    {
        state->drag.target_index = REACH_MAX_DOCK_ITEMS;
        state->drag.key = {};
        reach_animation_manager_reset(reach_dock_manager(dock), REACH_DOCK_ANIM_DRAG_SNAP);
    }

    if (moved && dragged_pinned && target_pinned_index != REACH_MAX_DOCK_ITEMS)
    {
        out->move_pin = 1;
        out->move_pin_id = pin_id;
        out->move_pin_target = target_pinned_index;
    }
}

size_t reach_dock_reorder_target(const reach_dock_feature_model *model,
                                 const reach_dock_layout *layout, size_t current_index,
                                 float dragged_box_x)
{
    if (model == nullptr || layout == nullptr || model->item_count == 0 ||
        layout->app_slot_count == 0)
    {
        return REACH_MAX_DOCK_ITEMS;
    }

    size_t count =
        model->item_count < layout->app_slot_count ? model->item_count : layout->app_slot_count;

    if (current_index >= count)
    {
        return REACH_MAX_DOCK_ITEMS;
    }

    size_t target =
        reach_horizontal_reorder_target(layout->app_slots, count, current_index, dragged_box_x,
                                        reach_dock_metrics_values.reorder_neighbor_threshold_ratio);
    return target == SIZE_MAX ? REACH_MAX_DOCK_ITEMS : target;
}
