#include "reach/features/clipboard.h"

#include "clipboard_common.h"

/*
 * Clipboard pointer/wheel interaction (moved out of composition's shell_input.cpp
 * in the behavior-encapsulation phase). Each entry point mutates only the capsule's
 * own state and reports semantic actions + side effects; composition executes them
 * through the ports/policy it owns.
 */

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
        state->model.pressed_index = hit.index;
        state->model.pressed_hit_type = hit.type;
        state->model.pressed_item_id =
            hit.index < state->model.count ? state->model.items[hit.index].id : 0;
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
    size_t pressed = state->model.pressed_index;
    reach_clipboard_hit_type pressed_hit_type =
        (reach_clipboard_hit_type)state->model.pressed_hit_type;
    uint64_t pressed_item_id = state->model.pressed_item_id;
    reach_clipboard_model_clear_press(&state->model);

    if (hit.type == REACH_CLIPBOARD_HIT_CLEAR && pressed_hit_type == REACH_CLIPBOARD_HIT_CLEAR)
    {
        out->handled = 1;
        out->action = REACH_CLIPBOARD_ACTION_CLEAR_ALL;
        return;
    }

    if (hit.type == REACH_CLIPBOARD_HIT_ITEM_CLOSE &&
        pressed_hit_type == REACH_CLIPBOARD_HIT_ITEM_CLOSE && hit.index == pressed &&
        hit.index < state->model.count && state->model.items[hit.index].id == pressed_item_id)
    {
        out->handled = 1;
        out->action = REACH_CLIPBOARD_ACTION_REMOVE_ITEM;
        out->item_index = hit.index;
        out->item_id = pressed_item_id;
        out->redraw = 1;
        out->relayout = 1;
        out->request_update = 1;
        return;
    }

    if (hit.type == REACH_CLIPBOARD_HIT_ITEM && pressed_hit_type == REACH_CLIPBOARD_HIT_ITEM &&
        hit.index == pressed && hit.index < state->model.count &&
        state->model.items[hit.index].id == pressed_item_id)
    {
        out->handled = 1;
        out->action = REACH_CLIPBOARD_ACTION_RESTORE_ITEM;
        out->item_index = hit.index;
        out->item_id = pressed_item_id;
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
    size_t next = (hit.type == REACH_CLIPBOARD_HIT_ITEM || hit.type == REACH_CLIPBOARD_HIT_ITEM_CLOSE)
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

void reach_clipboard_clear_press_state(reach_clipboard_feature *clipboard)
{
    if (clipboard != nullptr)
    {
        reach_clipboard_model_clear_press(&reach_clipboard_feature_state_mut(clipboard)->model);
    }
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
