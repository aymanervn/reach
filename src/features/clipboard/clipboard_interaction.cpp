#include "reach/features/clipboard.h"

#include "clipboard_common.h"

static uint64_t reach_clipboard_pressable_target(reach_clipboard_hit_result hit)
{
    if (hit.type != REACH_CLIPBOARD_HIT_ITEM && hit.type != REACH_CLIPBOARD_HIT_ITEM_CLOSE &&
        hit.type != REACH_CLIPBOARD_HIT_CLEAR)
    {
        return REACH_PRESSABLE_TARGET_NONE;
    }
    return ((uint64_t)hit.type << 32) | (uint64_t)hit.index;
}

static reach_clipboard_hit_result reach_clipboard_pressable_hit(uint64_t target)
{
    reach_clipboard_hit_result hit = {};
    hit.type = (reach_clipboard_hit_type)(target >> 32);
    hit.index = (size_t)(target & UINT32_MAX);
    return hit;
}

static void reach_clipboard_apply_pressable_result(const reach_pressable_result *result,
                                                   reach_clipboard_event_result *out)
{
    if (result == nullptr || out == nullptr)
    {
        return;
    }
    out->redraw |= result->redraw;
    if (result->capture != 0)
    {
        out->capture_pointer = result->capture;
    }
    out->sync_pointer_subscriptions |= result->sync_pointer_subscriptions;
}

void reach_clipboard_pointer_down(reach_clipboard_feature *clipboard, int32_t x, int32_t y,
                                  reach_clipboard_event_result *out)
{
    if (clipboard == nullptr || out == nullptr)
    {
        return;
    }

    reach_clipboard_state *state = reach_clipboard_feature_state_mut(clipboard);

    if (!state->model.open)
    {
        return;
    }

    reach_clipboard_hit_result hit = reach_clipboard_hit_test(&state->model, &state->layout, x, y);

    if (hit.type == REACH_CLIPBOARD_HIT_SCROLLBAR_THUMB ||
        hit.type == REACH_CLIPBOARD_HIT_SCROLLBAR_TRACK)
    {
        reach_scrollbar_begin_drag(&state->model.scrollbar, &state->scrollbar_drag,
                                   &state->layout.scrollbar, (float)y,
                                   hit.type == REACH_CLIPBOARD_HIT_SCROLLBAR_THUMB);
        out->handled = 1;
        out->capture_pointer = 1;
        out->redraw = 1;
        out->relayout = 1;
        return;
    }

    if (hit.type == REACH_CLIPBOARD_HIT_ITEM || hit.type == REACH_CLIPBOARD_HIT_ITEM_CLOSE ||
        hit.type == REACH_CLIPBOARD_HIT_CLEAR)
    {
        state->press_identity =
            hit.index < state->model.count ? state->model.items[hit.index].id : 0;
        reach_pressable_result result = {};
        reach_pressable_press(&state->pressable, REACH_POINTER_BUTTON_PRIMARY,
                              reach_clipboard_pressable_target(hit), REACH_PRESSABLE_FEEDBACK_NONE,
                              nullptr, &result);
        reach_clipboard_apply_pressable_result(&result, out);
        out->handled = 1;
    }
}

void reach_clipboard_pointer_up(reach_clipboard_feature *clipboard, int32_t x, int32_t y,
                                reach_clipboard_event_result *out)
{
    if (clipboard == nullptr || out == nullptr)
    {
        return;
    }

    reach_clipboard_state *state = reach_clipboard_feature_state_mut(clipboard);

    if (!state->model.open)
    {
        return;
    }

    reach_clipboard_hit_result hit = reach_clipboard_hit_test(&state->model, &state->layout, x, y);
    uint64_t press_identity = state->press_identity;
    int32_t was_tracking = reach_pressable_tracking(&state->pressable);
    reach_pressable_result result = {};
    reach_pressable_release(&state->pressable, REACH_POINTER_BUTTON_PRIMARY,
                            reach_clipboard_pressable_target(hit), nullptr, &result);
    reach_clipboard_apply_pressable_result(&result, out);
    state->press_identity = 0;
    if (!result.activated)
    {
        out->handled = was_tracking;
        return;
    }
    hit = reach_clipboard_pressable_hit(result.activated_target);

    if (hit.type == REACH_CLIPBOARD_HIT_CLEAR)
    {
        out->handled = 1;
        out->action = REACH_CLIPBOARD_ACTION_CLEAR_ALL;
        return;
    }

    if (hit.type == REACH_CLIPBOARD_HIT_ITEM_CLOSE && hit.index < state->model.count &&
        state->model.items[hit.index].id == press_identity)
    {
        out->handled = 1;
        out->action = REACH_CLIPBOARD_ACTION_REMOVE_ITEM;
        out->item_index = hit.index;
        out->item_id = press_identity;
        out->redraw = 1;
        out->relayout = 1;
        out->request_update = 1;
        return;
    }

    if (hit.type == REACH_CLIPBOARD_HIT_ITEM && hit.index < state->model.count &&
        state->model.items[hit.index].id == press_identity)
    {
        out->handled = 1;
        out->action = REACH_CLIPBOARD_ACTION_RESTORE_ITEM;
        out->item_index = hit.index;
        out->item_id = press_identity;
    }
}

void reach_clipboard_scrollbar_release(reach_clipboard_feature *clipboard,
                                       reach_clipboard_event_result *out)
{
    if (clipboard == nullptr || out == nullptr)
    {
        return;
    }

    reach_clipboard_state *state = reach_clipboard_feature_state_mut(clipboard);

    if (!state->scrollbar_drag.active)
    {
        return;
    }

    reach_scrollbar_end_drag(&state->scrollbar_drag);
    out->handled = 1;
    out->capture_pointer = -1;
}

void reach_clipboard_scrollbar_drag_move(reach_clipboard_feature *clipboard, int32_t y,
                                         reach_clipboard_event_result *out)
{
    if (clipboard == nullptr || out == nullptr)
    {
        return;
    }

    reach_clipboard_state *state = reach_clipboard_feature_state_mut(clipboard);

    if (!state->scrollbar_drag.active)
    {
        return;
    }

    reach_scrollbar_update_drag(&state->model.scrollbar, &state->scrollbar_drag,
                                &state->layout.scrollbar, (float)y);
    state->model.hovered_index = REACH_CLIPBOARD_MAX_ITEMS;
    out->handled = 1;
    out->redraw = 1;
    out->relayout = 1;
}

void reach_clipboard_pointer_move(reach_clipboard_feature *clipboard, int32_t x, int32_t y,
                                  reach_clipboard_event_result *out)
{
    if (clipboard == nullptr || out == nullptr)
    {
        return;
    }

    reach_clipboard_state *state = reach_clipboard_feature_state_mut(clipboard);

    if (!state->model.open)
    {
        return;
    }

    reach_clipboard_hit_result hit = reach_clipboard_hit_test(&state->model, &state->layout, x, y);
    if (reach_pressable_tracking(&state->pressable))
    {
        reach_pressable_result result = {};
        reach_pressable_update(&state->pressable, reach_clipboard_pressable_target(hit), &result);
        reach_clipboard_apply_pressable_result(&result, out);
    }
    size_t next =
        (hit.type == REACH_CLIPBOARD_HIT_ITEM || hit.type == REACH_CLIPBOARD_HIT_ITEM_CLOSE)
            ? hit.index
            : REACH_CLIPBOARD_MAX_ITEMS;
    if (next != state->model.hovered_index)
    {
        size_t previous = state->model.hovered_index;
        state->model.hovered_index = next;
        reach_clipboard_feature_move_hover(clipboard, previous, next);
        out->redraw = 1;
        out->request_update = 1;
    }
    out->handled = 1;
}

void reach_clipboard_wheel(reach_clipboard_feature *clipboard, int32_t x, int32_t y,
                           int32_t wheel_delta, reach_clipboard_event_result *out)
{
    if (clipboard == nullptr || out == nullptr)
    {
        return;
    }

    reach_clipboard_state *state = reach_clipboard_feature_state_mut(clipboard);

    if (!state->model.open || wheel_delta == 0 ||
        !((float)x >= state->layout.viewport.x &&
          (float)x <= state->layout.viewport.x + state->layout.viewport.width &&
          (float)y >= state->layout.viewport.y &&
          (float)y <= state->layout.viewport.y + state->layout.viewport.height))
    {
        return;
    }

    reach_scrollbar_scroll(&state->model.scrollbar, wheel_delta > 0 ? -72.0f : 72.0f);
    state->model.hovered_index = REACH_CLIPBOARD_MAX_ITEMS;
    reach_clipboard_feature_collapse_all_hover(clipboard);
    out->handled = 1;
    out->redraw = 1;
    out->relayout = 1;
    out->request_update = 1;
}

void reach_clipboard_cancel_press(reach_clipboard_feature *clipboard,
                                  reach_clipboard_event_result *out)
{
    if (clipboard == nullptr || out == nullptr)
    {
        return;
    }
    reach_clipboard_state *state = reach_clipboard_feature_state_mut(clipboard);
    int32_t was_tracking = reach_pressable_tracking(&state->pressable);
    reach_pressable_result result = {};
    reach_pressable_cancel(&state->pressable, nullptr, &result);
    reach_clipboard_apply_pressable_result(&result, out);
    state->press_identity = 0;
    out->handled |= was_tracking;
}

int32_t reach_clipboard_remove_item(reach_clipboard_feature *clipboard, size_t index,
                                    uint64_t item_id)
{
    if (clipboard == nullptr)
    {
        return 0;
    }
    reach_clipboard_model *model = &reach_clipboard_feature_state_mut(clipboard)->model;
    if (index >= model->count || model->items[index].id != item_id)
    {
        return 0;
    }
    reach_clipboard_model_remove(model, index);
    return 1;
}

void reach_clipboard_confirm_restore(reach_clipboard_feature *clipboard, size_t index)
{
    if (clipboard == nullptr)
    {
        return;
    }

    reach_clipboard_state *state = reach_clipboard_feature_state_mut(clipboard);
    (void)reach_clipboard_model_promote(&state->model, index);
    state->model.scrollbar.offset = 0.0f;
    state->model.scrollbar.target = 0.0f;
}
