#include "reach/features/launcher.h"

#include "launcher_common.h"

static int32_t reach_launcher_rect_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    if (rect.width <= 0.0f || rect.height <= 0.0f)
    {
        return 0;
    }
    return (float)x >= rect.x && (float)x <= rect.x + rect.width && (float)y >= rect.y &&
           (float)y <= rect.y + rect.height;
}

reach_launcher_hit_result reach_launcher_hit_test(const reach_launcher_state *state,
                                                  const reach_launcher_layout *layout, int32_t x,
                                                  int32_t y)
{
    const reach_launcher_model *model = state != nullptr ? &state->model : nullptr;
    reach_launcher_hit_result hit = {};
    hit.type = REACH_LAUNCHER_HIT_NONE;
    hit.index = REACH_MAX_PINNED_APPS;
    if (model == nullptr || layout == nullptr || !model->open)
    {
        return hit;
    }

    if (!reach_launcher_rect_contains(layout->search_results, x, y))
        return hit;

    if (reach_scroll_view_hit(&state->scroll, (float)x, (float)y))
    {
        hit.type = REACH_LAUNCHER_HIT_SCROLLBAR;
        hit.index = reach_launcher_result_scroll_offset_state(state);
        return hit;
    }

    if (reach_launcher_rect_contains(layout->search_result_items, x, y))
    {
        hit.type = REACH_LAUNCHER_HIT_SEARCH_RESULT;
        float local_y = (float)y - layout->search_result_items.y;
        size_t visible_count = reach_launcher_visible_result_count(model);
        float row_height =
            visible_count > 0 ? layout->search_result_items.height / (float)visible_count : 0.0f;
        size_t visible_index =
            row_height > 0.0f && local_y > 0.0f ? (size_t)(local_y / row_height) : 0;
        size_t index = reach_launcher_result_scroll_offset_state(state) + visible_index;
        hit.index = index < model->result_count ? index : REACH_SEARCH_MAX_RESULTS;
    }
    return hit;
}

static uint64_t reach_launcher_pressable_target(const reach_launcher *launcher,
                                                const reach_launcher_state *state,
                                                const reach_launcher_event_context *ctx,
                                                reach_launcher_hit_result hit,
                                                reach_pointer_button button)
{
    if (state != nullptr && hit.type == REACH_LAUNCHER_HIT_SEARCH_RESULT &&
        hit.index < state->model.result_count &&
        (button == REACH_POINTER_BUTTON_PRIMARY || button == REACH_POINTER_BUTTON_SECONDARY))
    {
        if (button == REACH_POINTER_BUTTON_SECONDARY &&
            state->model.results[hit.index].action != REACH_LAUNCHER_RESULT_OPEN_SEARCH)
        {
            return REACH_PRESSABLE_TARGET_NONE;
        }
        return (static_cast<uint64_t>(hit.type) << 56) |
               (static_cast<uint64_t>(reach_launcher_search_generation(launcher)) << 24) |
               static_cast<uint32_t>(hit.index);
    }
    return REACH_PRESSABLE_TARGET_NONE;
}

static void reach_launcher_apply_pressable_result(const reach_pressable_result *pressable,
                                                  reach_launcher_event_result *out)
{
    if (pressable == nullptr || out == nullptr)
    {
        return;
    }
    out->redraw = out->redraw || pressable->redraw;
    if (pressable->capture != 0)
    {
        out->capture_pointer = pressable->capture;
    }
    out->sync_pointer_subscriptions =
        out->sync_pointer_subscriptions || pressable->sync_pointer_subscriptions;
}

void reach_launcher_pointer_down(reach_launcher *launcher, int32_t x, int32_t y,
                                 reach_pointer_button button,
                                 const reach_launcher_event_context *ctx,
                                 reach_launcher_event_result *out)
{
    if (launcher == nullptr || ctx == nullptr || ctx->layout == nullptr || out == nullptr)
    {
        return;
    }

    reach_launcher_state *state = reach_launcher_state_mut(launcher);

    if (!state->model.open)
    {
        return;
    }

    reach_launcher_hit_result hit = reach_launcher_hit_test(state, ctx->layout, x, y);

    if (hit.type == REACH_LAUNCHER_HIT_NONE &&
        !reach_launcher_rect_contains(ctx->layout->bounds, x, y))
    {
        return;
    }

    if (button == REACH_POINTER_BUTTON_PRIMARY && hit.type == REACH_LAUNCHER_HIT_SCROLLBAR)
    {
        uint32_t scroll = reach_scroll_view_pointer_down(&state->scroll, (float)x, (float)y,
                                                         reach_motion_now_seconds());
        out->viewport_changed = (scroll & REACH_SCROLL_VIEW_CHANGED) != 0;
        out->sync_pointer_subscriptions = 1;
        out->capture_pointer = 1;
        out->handled = 1;
        return;
    }

    if (button == REACH_POINTER_BUTTON_PRIMARY && hit.type == REACH_LAUNCHER_HIT_SEARCH_RESULT &&
        hit.index < state->model.result_count)
    {
        (void)reach_launcher_set_selected_result_state(state, hit.index);
        out->redraw = 1;
    }

    uint64_t target = reach_launcher_pressable_target(launcher, state, ctx, hit, button);
    if (target != REACH_PRESSABLE_TARGET_NONE)
    {
        reach_pressable_result pressable = {};
        reach_pressable_feedback_style feedback = reach_launcher_pressable_feedback(launcher);
        reach_pressable_press(&state->pressable, button, target, hit.index, &feedback, &pressable);
        reach_launcher_apply_pressable_result(&pressable, out);
        out->handled = 1;
    }
    else
    {
        out->handled = 1;
    }
}

void reach_launcher_pointer_up(reach_launcher *launcher, int32_t x, int32_t y,
                               reach_pointer_button button, const reach_launcher_event_context *ctx,
                               reach_launcher_event_result *out)
{
    if (launcher == nullptr || ctx == nullptr || ctx->layout == nullptr || out == nullptr)
    {
        return;
    }

    reach_launcher_state *state = reach_launcher_state_mut(launcher);

    if (!state->model.open)
    {
        return;
    }

    reach_launcher_hit_result hit = reach_launcher_hit_test(state, ctx->layout, x, y);
    reach_pressable_result pressable = {};
    reach_pressable_feedback_style feedback = reach_launcher_pressable_feedback(launcher);
    reach_pressable_release(&state->pressable, button,
                            reach_launcher_pressable_target(launcher, state, ctx, hit, button),
                            &feedback, &pressable);
    reach_launcher_apply_pressable_result(&pressable, out);
    if (pressable.activated)
    {
        reach_launcher_action action = {};
        if (button == REACH_POINTER_BUTTON_SECONDARY)
        {
            action.type = REACH_LAUNCHER_ACTION_REVEAL_RESULT;
            action.result_index = hit.index;
        }
        else
        {
            action = reach_launcher_action_for_hit(&state->model, hit);
        }
        if (action.type != REACH_LAUNCHER_ACTION_NONE)
        {
            out->action = action;
            out->handled = 1;
        }
    }
}

void reach_launcher_pointer_move(reach_launcher *launcher, int32_t x, int32_t y,
                                 const reach_launcher_event_context *ctx,
                                 reach_launcher_event_result *out)
{
    if (launcher == nullptr || ctx == nullptr || ctx->layout == nullptr || out == nullptr)
    {
        return;
    }

    reach_launcher_state *state = reach_launcher_state_mut(launcher);

    reach_launcher_hit_result hit = reach_launcher_hit_test(state, ctx->layout, x, y);
    reach_pressable_result pressable = {};
    reach_pressable_update(
        &state->pressable,
        reach_launcher_pressable_target(launcher, state, ctx, hit,
                                        reach_pressable_button(&state->pressable)),
        &pressable);
    reach_launcher_apply_pressable_result(&pressable, out);

    uint32_t scroll =
        reach_scroll_view_pointer_move(&state->scroll, (float)y, reach_motion_now_seconds());
    if ((scroll & REACH_SCROLL_VIEW_HANDLED) == 0)
    {
        return;
    }
    out->viewport_changed = (scroll & REACH_SCROLL_VIEW_CHANGED) != 0;
    out->handled = 1;
}

void reach_launcher_scrollbar_release(reach_launcher *launcher, reach_launcher_event_result *out)
{
    if (launcher == nullptr || out == nullptr)
    {
        return;
    }

    reach_launcher_state *state = reach_launcher_state_mut(launcher);

    if ((reach_scroll_view_pointer_up(&state->scroll) & REACH_SCROLL_VIEW_RELEASE) == 0)
    {
        return;
    }

    out->sync_pointer_subscriptions = 1;
    out->capture_pointer = -1;
    out->handled = 1;
}

void reach_launcher_pointer_leave(reach_launcher *launcher, reach_launcher_event_result *out)
{
    if (launcher == nullptr || out == nullptr)
    {
        return;
    }
    reach_pressable_result pressable = {};
    reach_pressable_update(&reach_launcher_state_mut(launcher)->pressable,
                           REACH_PRESSABLE_TARGET_NONE, &pressable);
    reach_launcher_apply_pressable_result(&pressable, out);
}

void reach_launcher_pointer_cancel(reach_launcher *launcher, reach_launcher_event_result *out)
{
    if (launcher == nullptr || out == nullptr)
    {
        return;
    }
    reach_pressable_result pressable = {};
    reach_pressable_feedback_style feedback = reach_launcher_pressable_feedback(launcher);
    reach_pressable_cancel(&reach_launcher_state_mut(launcher)->pressable, &feedback, &pressable);
    reach_launcher_apply_pressable_result(&pressable, out);
    reach_launcher_scrollbar_release(launcher, out);
}

void reach_launcher_wheel(reach_launcher *launcher, int32_t x, int32_t y, int32_t wheel_delta,
                          const reach_launcher_event_context *ctx, reach_launcher_event_result *out)
{
    if (launcher == nullptr || ctx == nullptr || ctx->layout == nullptr || out == nullptr)
    {
        return;
    }

    reach_launcher_state *state = reach_launcher_state_mut(launcher);

    if (!state->model.open)
    {
        return;
    }

    uint32_t scroll = reach_scroll_view_wheel(&state->scroll, (float)x, (float)y, wheel_delta,
                                              reach_motion_now_seconds());
    out->viewport_changed = (scroll & REACH_SCROLL_VIEW_CHANGED) != 0;
    out->handled = (scroll & REACH_SCROLL_VIEW_HANDLED) != 0;
}
