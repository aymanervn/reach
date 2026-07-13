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
    result.index = REACH_MAX_PINNED_APPS;

    if (layout == nullptr)
    {
        return result;
    }

    if (reach_dock_rect_contains(layout->tray_button, x, y))
    {
        result.type = REACH_DOCK_HIT_TRAY_BUTTON;
        return result;
    }

    if (reach_dock_rect_contains(layout->quick_settings_button, x, y))
    {
        result.type = REACH_DOCK_HIT_QUICK_SETTINGS_BUTTON;
        return result;
    }

    if (reach_dock_rect_contains(layout->power_button, x, y))
    {
        result.type = REACH_DOCK_HIT_POWER_BUTTON;
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
    action.pinned_index = REACH_MAX_PINNED_APPS;

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
        action.pin_id = item->pin_id;
        return action;
    }

    if (item->pinned)
    {
        action.type = REACH_DOCK_ITEM_ACTION_LAUNCH_PINNED;
        action.pinned_index = item->pinned_index;
        action.pin_id = item->pin_id;
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

/* Animate a feedback slot's opacity toward target_opacity (the shared feedback
 * track). Returns 1 when the animation started (the dock needs a redraw). */
static int32_t reach_dock_feedback_start(reach_dock *dock, size_t slot, float target_opacity)
{
    if (dock == nullptr || slot > REACH_DOCK_FEEDBACK_POWER_BUTTON)
    {
        return 0;
    }

    reach_dock_state_mut(dock)->feedback_index = slot;

    reach_animation_manager_animate_to(reach_dock_manager(dock), REACH_DOCK_ANIM_FEEDBACK_OPACITY,
                                       target_opacity, 0.055, REACH_EASING_EASE_IN_OUT);

    return 1;
}

int32_t reach_dock_feedback_press(reach_dock *dock, size_t slot)
{
    if (dock == nullptr)
    {
        return 0;
    }

    reach_dock_state_mut(dock)->feedback_pressed = 1;
    reach_dock_state_mut(dock)->feedback_sticky = 0;

    return reach_dock_feedback_start(dock, slot, 0.50f);
}

int32_t reach_dock_feedback_press_immediate(reach_dock *dock, size_t slot, float opacity)
{
    if (dock == nullptr)
    {
        return 0;
    }
    reach_dock_state_mut(dock)->feedback_pressed = 1;
    reach_dock_state_mut(dock)->feedback_sticky = 0;
    return reach_dock_feedback_set_immediate(dock, slot, opacity);
}

int32_t reach_dock_feedback_set_immediate(reach_dock *dock, size_t slot, float opacity)
{
    if (dock == nullptr || slot > REACH_DOCK_FEEDBACK_POWER_BUTTON)
    {
        return 0;
    }

    reach_dock_state_mut(dock)->feedback_index = slot;
    reach_animation_manager_set(reach_dock_manager(dock), REACH_DOCK_ANIM_FEEDBACK_OPACITY,
                                opacity);
    return 1;
}

int32_t reach_dock_feedback_stick(reach_dock *dock)
{
    if (dock == nullptr || (!reach_dock_state_mut(dock)->feedback_pressed &&
                            reach_dock_state_mut(dock)->feedback_index == REACH_DOCK_FEEDBACK_NONE))
    {
        return 0;
    }

    reach_dock_state_mut(dock)->feedback_pressed = 0;
    reach_dock_state_mut(dock)->feedback_sticky =
        reach_dock_state_mut(dock)->feedback_index != REACH_DOCK_FEEDBACK_NONE;

    if (reach_dock_state_mut(dock)->feedback_sticky)
    {
        return reach_dock_feedback_set_immediate(dock, reach_dock_state_mut(dock)->feedback_index,
                                                 0.50f);
    }
    return 0;
}

int32_t reach_dock_feedback_release(reach_dock *dock)
{
    if (dock == nullptr || (!reach_dock_state_mut(dock)->feedback_pressed &&
                            reach_dock_state_mut(dock)->feedback_index == REACH_DOCK_FEEDBACK_NONE))
    {
        return 0;
    }

    reach_dock_state_mut(dock)->feedback_pressed = 0;
    reach_dock_state_mut(dock)->feedback_sticky = 0;

    if (reach_dock_state_mut(dock)->feedback_index != REACH_DOCK_FEEDBACK_NONE)
    {
        return reach_dock_feedback_start(dock, reach_dock_state_mut(dock)->feedback_index, 0.0f);
    }
    return 0;
}

int32_t reach_dock_feedback_clear_sticky(reach_dock *dock)
{
    if (dock != nullptr && reach_dock_state_mut(dock)->feedback_sticky)
    {
        return reach_dock_feedback_release(dock);
    }
    return 0;
}

static void reach_dock_drag_begin(reach_dock *dock, size_t index, int32_t x, int32_t y,
                                  const reach_dock_interaction_context *ctx,
                                  reach_dock_interaction_result *out)
{
    reach_dock_state *state = reach_dock_state_mut(dock);

    if (index >= state->model.item_count)
    {
        return;
    }

    state->drag.active = 1;

    state->drag.moved = 0;
    state->drag.source_index = index;
    state->drag.target_index = index;
    state->drag.pinned = state->model.items[index].pinned;
    state->drag.pin_id = 0;

    if (state->model.items[index].pinned &&
        state->model.items[index].pinned_index < ctx->pinned_app_count)
    {
        state->drag.pin_id = ctx->pinned_apps[state->model.items[index].pinned_index].id;
    }

    state->drag.window = state->model.items[index].window;
    state->drag.start_x = x;
    state->drag.start_y = y;

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

    reach_dock_state_mut(dock)->pressed_index = index;

    if (reach_dock_feedback_press(dock, index))
    {
        out->redraw = 1;
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

    if (!state->drag.active)
    {
        return;
    }

    int32_t dx = x - state->drag.start_x;
    int32_t dy = y - state->drag.start_y;

    if (!state->drag.moved && (dx * dx + dy * dy) >= 36)
    {
        state->drag.moved = 1;
    }

    if (!state->drag.moved)
    {
        return;
    }

    int32_t local_cursor_x = static_cast<int32_t>((float)x - ctx->layout->bounds.x);
    float next_drag_x =
        reach_dock_drag_clamped_x(ctx->theme, ctx->layout, local_cursor_x, state->drag.grab_offset_x);

    if (fabsf(next_drag_x - state->drag.x) >= 0.5f)
    {
        state->drag.x = next_drag_x;
        out->redraw = 1;
    }

    float dragged_box_x = state->drag.x;

    reach_dock_order_key drag_key = {state->drag.pinned, state->drag.pin_id, state->drag.window};
    size_t current = reach_dock_feature_model_find_order_key(&state->model, drag_key);

    size_t target = reach_dock_reorder_target(&state->model, ctx->layout, current, dragged_box_x);

    if (target != REACH_MAX_PINNED_APPS && target != state->drag.target_index)
    {
        if (current != REACH_MAX_PINNED_APPS)
        {
            reach_dock_feature_model_move_order(&state->model, current, target);
            out->rebuild_items = 1;
        }

        state->drag.target_index = target;
        state->feedback_index = target;
        out->redraw = 1;
        return;
    }

    current = reach_dock_feature_model_find_order_key(&state->model, drag_key);

    if (current != REACH_MAX_PINNED_APPS && state->feedback_index != current)
    {
        state->feedback_index = current;
        out->redraw = 1;
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

    if (!state->drag.active)
    {
        return;
    }

    uint32_t pin_id = state->drag.pin_id;
    int32_t dragged_pinned = state->drag.pinned;
    int32_t moved = state->drag.moved;
    size_t previous_pressed_dock_index = state->pressed_index;

    size_t target_pinned_index =
        dragged_pinned ? reach_dock_feature_model_pinned_order_index(&state->model, pin_id)
                       : REACH_MAX_PINNED_APPS;

    reach_dock_order_key drag_key = {state->drag.pinned, state->drag.pin_id, state->drag.window};
    size_t target_index = reach_dock_feature_model_find_item_key(&state->model, drag_key);

    state->drag.active = 0;
    state->drag.moved = 0;
    state->pressed_index = moved ? REACH_MAX_PINNED_APPS : previous_pressed_dock_index;
    out->redraw = 1;

    (void)reach_dock_feedback_release(dock);

    if (moved && target_index < ctx->layout->app_slot_count)
    {
        float target_x = reach_dock_slot_box_x(ctx->theme, ctx->layout, target_index);

        reach_animation_manager_start(reach_dock_manager(dock), REACH_DOCK_ANIM_DRAG_SNAP,
                                      state->drag.x, target_x, 0.12, REACH_EASING_EASE_IN_OUT);
    }
    else
    {
        state->drag.source_index = REACH_MAX_PINNED_APPS;
        state->drag.target_index = REACH_MAX_PINNED_APPS;
        state->drag.pinned = 0;
        state->drag.pin_id = 0;
        state->drag.window = 0;
        reach_animation_manager_reset(reach_dock_manager(dock), REACH_DOCK_ANIM_DRAG_SNAP);
    }

    if (moved && dragged_pinned && target_pinned_index != REACH_MAX_PINNED_APPS)
    {
        out->move_pin = 1;
        out->move_pin_id = pin_id;
        out->move_pin_target = target_pinned_index;
    }
}

int32_t reach_dock_item_release(reach_dock *dock, size_t index,
                                reach_dock_item_action *out_action,
                                reach_dock_interaction_result *out)
{
    if (dock == nullptr || out_action == nullptr || out == nullptr)
    {
        return 0;
    }

    reach_dock_state *state = reach_dock_state_mut(dock);

    if (state->pressed_index != index)
    {
        return 0;
    }

    state->pressed_index = REACH_MAX_PINNED_APPS;

    *out_action = reach_dock_item_action_for_index(&state->model, index);

    (void)reach_dock_feedback_release(dock);
    out->redraw = 1;
    return 1;
}

void reach_dock_clear_pressed(reach_dock *dock)
{
    if (dock != nullptr)
    {
        reach_dock_state_mut(dock)->pressed_index = REACH_MAX_PINNED_APPS;
    }
}

size_t reach_dock_reorder_target(const reach_dock_feature_model *model,
                                 const reach_dock_layout *layout, size_t current_index,
                                 float dragged_box_x)
{
    if (model == nullptr || layout == nullptr || model->item_count == 0 ||
        layout->app_slot_count == 0)
    {
        return REACH_MAX_PINNED_APPS;
    }

    size_t count =
        model->item_count < layout->app_slot_count ? model->item_count : layout->app_slot_count;

    if (current_index >= count)
    {
        return REACH_MAX_PINNED_APPS;
    }

    size_t target = current_index;

    while (target > 0)
    {
        float threshold = layout->app_slots[target - 1].x +
                          layout->app_slots[target - 1].width *
                              reach_dock_metrics_values.reorder_neighbor_threshold_ratio;
        if (dragged_box_x > threshold)
        {
            break;
        }
        --target;
    }

    while (target + 1 < count)
    {
        float threshold = layout->app_slots[target + 1].x -
                          layout->app_slots[target + 1].width *
                              reach_dock_metrics_values.reorder_neighbor_threshold_ratio;
        if (dragged_box_x < threshold)
        {
            break;
        }
        ++target;
    }

    return target;
}
