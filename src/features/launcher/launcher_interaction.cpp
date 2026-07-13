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

static size_t reach_launcher_visible_result_count(const reach_launcher_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    return model->result_count < REACH_SEARCH_VISIBLE_RESULTS ? model->result_count
                                                              : REACH_SEARCH_VISIBLE_RESULTS;
}

reach_launcher_hit_result reach_launcher_hit_test(const reach_launcher_model *model,
                                                  const reach_launcher_layout *layout, int32_t x,
                                                  int32_t y)
{
    reach_launcher_hit_result hit = {};
    hit.type = REACH_LAUNCHER_HIT_NONE;
    hit.index = REACH_MAX_PINNED_APPS;
    if (model == nullptr || layout == nullptr || !model->open)
    {
        return hit;
    }

    for (size_t index = 0; index < layout->pinned_app_slot_count; ++index)
    {
        if (reach_launcher_rect_contains(layout->pinned_app_slots[index], x, y))
        {
            hit.type = REACH_LAUNCHER_HIT_PINNED_APP;
            hit.index = index;
            return hit;
        }
    }

    if (reach_launcher_rect_contains(layout->search_result_scrollbar_thumb, x, y))
    {
        hit.type = REACH_LAUNCHER_HIT_SCROLLBAR_THUMB;
        hit.index = reach_launcher_model_result_scroll_offset(model);
        return hit;
    }

    if (reach_launcher_rect_contains(layout->search_result_scrollbar_track, x, y))
    {
        hit.type = REACH_LAUNCHER_HIT_SCROLLBAR_TRACK;
        hit.index = reach_launcher_model_result_scroll_offset(model);
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
        size_t index = reach_launcher_model_result_scroll_offset(model) + visible_index;
        hit.index = index < model->result_count ? index : REACH_SEARCH_MAX_RESULTS;
    }
    return hit;
}

/*
 * Pointer/wheel interaction entry points (moved out of composition's
 * shell_input.cpp in the behavior-encapsulation phase). The capsule mutates only
 * its own state; composition executes the reported side effects and the
 * completed click action through the ports/policy it owns.
 */

void reach_launcher_pointer_down(reach_launcher *launcher, int32_t x, int32_t y,
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

    reach_launcher_hit_result hit = reach_launcher_hit_test(&state->model, ctx->layout, x, y);

    if (hit.type == REACH_LAUNCHER_HIT_NONE &&
        !reach_launcher_rect_contains(ctx->layout->bounds, x, y))
    {
        return;
    }

    if (hit.type == REACH_LAUNCHER_HIT_SCROLLBAR_THUMB ||
        hit.type == REACH_LAUNCHER_HIT_SCROLLBAR_TRACK)
    {
        size_t old_offset = reach_launcher_result_scroll_offset_state(state);
        reach_scrollbar_layout scrollbar_layout = {ctx->layout->search_result_scrollbar_track,
                                                   ctx->layout->search_result_scrollbar_thumb};
        reach_scrollbar_begin_drag(&state->model.result_scrollbar, &state->launcher_scrollbar_drag,
                                   &scrollbar_layout, (float)y,
                                   hit.type == REACH_LAUNCHER_HIT_SCROLLBAR_THUMB);
        if (old_offset != reach_launcher_result_scroll_offset_state(state))
        {
            out->viewport_changed = 1;
        }
        out->sync_pointer_subscriptions = 1;
        out->capture_pointer = 1;
        out->handled = 1;
        return;
    }

    if (hit.type == REACH_LAUNCHER_HIT_SEARCH_RESULT && hit.index < state->model.result_count)
    {
        (void)reach_launcher_set_selected_result_state(state, hit.index);
        out->redraw = 1;
    }

    state->pressed_launcher_hit_type = hit.type;
    state->pressed_launcher_index = hit.index;
    out->handled = 1;
}

void reach_launcher_pointer_up(reach_launcher *launcher, int32_t x, int32_t y,
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

    reach_launcher_hit_result hit = reach_launcher_hit_test(&state->model, ctx->layout, x, y);

    reach_launcher_action action = reach_launcher_action_for_hit(&state->model, ctx->pinned_apps,
                                                                 ctx->pinned_app_count, hit);

    int32_t pressed_match = state->pressed_launcher_hit_type == hit.type &&
                            state->pressed_launcher_index == hit.index;

    state->pressed_launcher_hit_type = REACH_LAUNCHER_HIT_NONE;
    state->pressed_launcher_index = REACH_MAX_PINNED_APPS;

    if (pressed_match && action.type != REACH_LAUNCHER_ACTION_NONE)
    {
        out->action = action;
        out->handled = 1;
    }
}

void reach_launcher_scrollbar_drag_move(reach_launcher *launcher, int32_t y,
                                        const reach_launcher_event_context *ctx,
                                        reach_launcher_event_result *out)
{
    if (launcher == nullptr || ctx == nullptr || ctx->layout == nullptr || out == nullptr)
    {
        return;
    }

    reach_launcher_state *state = reach_launcher_state_mut(launcher);

    if (!state->launcher_scrollbar_drag.active)
    {
        return;
    }

    size_t old_offset = reach_launcher_result_scroll_offset_state(state);
    reach_scrollbar_layout scrollbar_layout = {ctx->layout->search_result_scrollbar_track,
                                               ctx->layout->search_result_scrollbar_thumb};
    reach_scrollbar_update_drag(&state->model.result_scrollbar, &state->launcher_scrollbar_drag,
                                &scrollbar_layout, (float)y);
    if (old_offset != reach_launcher_result_scroll_offset_state(state))
    {
        out->viewport_changed = 1;
    }
    out->handled = 1;
}

void reach_launcher_scrollbar_release(reach_launcher *launcher, reach_launcher_event_result *out)
{
    if (launcher == nullptr || out == nullptr)
    {
        return;
    }

    reach_launcher_state *state = reach_launcher_state_mut(launcher);

    if (!state->launcher_scrollbar_drag.active)
    {
        return;
    }

    reach_scrollbar_end_drag(&state->launcher_scrollbar_drag);
    out->sync_pointer_subscriptions = 1;
    out->capture_pointer = -1;
    out->handled = 1;
}

void reach_launcher_wheel(reach_launcher *launcher, int32_t x, int32_t y, int32_t wheel_delta,
                          const reach_launcher_event_context *ctx,
                          reach_launcher_event_result *out)
{
    if (launcher == nullptr || ctx == nullptr || ctx->layout == nullptr || out == nullptr)
    {
        return;
    }

    reach_launcher_state *state = reach_launcher_state_mut(launcher);

    if (!state->model.open || wheel_delta == 0 ||
        !reach_launcher_rect_contains(ctx->layout->search_results, x, y))
    {
        return;
    }

    int32_t direction = wheel_delta > 0 ? -1 : 1;
    size_t old_offset = reach_launcher_result_scroll_offset_state(state);
    (void)reach_launcher_scroll_results_state(state, direction * REACH_SEARCH_SCROLL_STEP);
    if (old_offset != reach_launcher_result_scroll_offset_state(state))
    {
        out->viewport_changed = 1;
    }
    out->handled = 1;
}
