#include "reach/features/stage.h"

#include "stage_common.h"

#define REACH_STAGE_CLOSE_BUTTON_RATIO 1.05f
#define REACH_STAGE_CLOSE_BUTTON_NUDGE_RATIO 0.18f

static int32_t reach_stage_rect_contains(reach_rect_f32 rect, reach_point_f32 point)
{
    return rect.width > 0.0f && rect.height > 0.0f && point.x >= rect.x &&
           point.x < rect.x + rect.width && point.y >= rect.y && point.y < rect.y + rect.height;
}

static int32_t reach_stage_tile_takes_pointer(const reach_stage_tile *tile)
{
    return !tile->departing && tile->presence > 0.5f;
}

enum
{
    REACH_STAGE_PRESSABLE_TILE = 1,
    REACH_STAGE_PRESSABLE_CLOSE = 2
};

static uint64_t reach_stage_pressable_target(const reach_stage *stage, reach_point_f32 point)
{
    size_t index = 0;
    if (reach_stage_close_button_at_point(stage, point, &index))
    {
        return ((uint64_t)REACH_STAGE_PRESSABLE_CLOSE << 32) | (uint64_t)index;
    }
    if (reach_stage_tile_at_point(stage, point, &index))
    {
        return ((uint64_t)REACH_STAGE_PRESSABLE_TILE << 32) | (uint64_t)index;
    }
    return REACH_PRESSABLE_TARGET_NONE;
}

static void reach_stage_apply_pressable_result(const reach_pressable_result *result,
                                               reach_capsule_pointer_result *out)
{
    if (result == nullptr || out == nullptr)
    {
        return;
    }
    out->redraw |= result->redraw;
    out->capture = result->capture;
    out->sync_pointer_subscriptions = result->sync_pointer_subscriptions;
}

reach_rect_f32 reach_stage_tile_close_button_rect(const reach_stage *stage, size_t index)
{
    reach_rect_f32 rect = {};
    if (stage == nullptr || index >= stage->state.tile_count)
    {
        return rect;
    }

    const reach_stage_tile *tile = &stage->state.tiles[index];
    if (tile->desktop || tile->current_bar.height <= 0.0f)
    {
        return rect;
    }

    float size = tile->current_bar.height * REACH_STAGE_CLOSE_BUTTON_RATIO;
    float nudge = tile->current_bar.height * REACH_STAGE_CLOSE_BUTTON_NUDGE_RATIO;
    if (size <= 0.0f || size > tile->current_bar.width)
    {
        return rect;
    }

    rect.x = tile->current_bar.x + tile->current_bar.width - size + nudge;
    rect.y = tile->current_bar.y + (tile->current_bar.height - size) * 0.5f - nudge;
    rect.width = size;
    rect.height = size;
    return rect;
}

int32_t reach_stage_tile_at_point(const reach_stage *stage, reach_point_f32 point,
                                  size_t *out_index)
{
    if (stage == nullptr || !stage->state.open)
    {
        return 0;
    }

    for (size_t index = 0; index < stage->state.tile_count; ++index)
    {
        const reach_stage_tile *tile = &stage->state.tiles[index];
        if (!reach_stage_tile_takes_pointer(tile))
        {
            continue;
        }
        if (reach_stage_rect_contains(tile->current_rect, point) ||
            reach_stage_rect_contains(tile->current_bar, point) ||
            reach_stage_rect_contains(reach_stage_tile_close_button_rect(stage, index), point))
        {
            if (out_index != nullptr)
            {
                *out_index = index;
            }
            return 1;
        }
    }
    return 0;
}

int32_t reach_stage_close_button_at_point(const reach_stage *stage, reach_point_f32 point,
                                          size_t *out_index)
{
    if (stage == nullptr || !stage->state.open)
    {
        return 0;
    }

    for (size_t index = 0; index < stage->state.tile_count; ++index)
    {
        if (!reach_stage_tile_takes_pointer(&stage->state.tiles[index]))
        {
            continue;
        }
        if (!reach_stage_rect_contains(reach_stage_tile_close_button_rect(stage, index), point))
        {
            continue;
        }

        if (out_index != nullptr)
        {
            *out_index = index;
        }
        return 1;
    }
    return 0;
}

void reach_stage_handle_pointer(void *capsule, const reach_pointer_event *event,
                                reach_capsule_pointer_result *out)
{
    reach_stage *stage = static_cast<reach_stage *>(capsule);
    if (out == nullptr)
    {
        return;
    }

    *out = {};

    if (stage == nullptr || event == nullptr || !stage->state.open || stage->state.closing)
    {
        return;
    }

    reach_stage_state *state = &stage->state;
    reach_point_f32 point = {(float)event->x, (float)event->y};

    switch (event->kind)
    {
    case REACH_POINTER_EVENT_MOVE:
    {
        reach_pressable_result pressable_result = {};
        reach_pressable_update(&stage->pressable, reach_stage_pressable_target(stage, point),
                               &pressable_result);
        reach_stage_apply_pressable_result(&pressable_result, out);
        size_t index = 0;
        int32_t hit = reach_stage_tile_at_point(stage, point, &index);
        int32_t changed = hit != state->has_hover || (hit && index != state->hover_index);
        state->has_hover = hit;
        if (hit)
        {
            state->hover_index = index;
        }

        size_t close_index = 0;
        int32_t close_hit = reach_stage_close_button_at_point(stage, point, &close_index);
        if (close_hit)
        {
            state->close_hover_index = close_index;
        }

        reach_animation_manager_animate_to(
            &stage->animations, REACH_STAGE_ANIMATION_CLOSE_HOVER, close_hit ? 1.0f : 0.0f,
            reach_stage_close_hover_seconds(), REACH_EASING_EASE_OUT);

        out->handled = 1;
        out->redraw = changed;
        return;
    }

    case REACH_POINTER_EVENT_DOWN:
    {
        if (event->button != REACH_POINTER_BUTTON_PRIMARY)
        {
            return;
        }
        uint64_t target = reach_stage_pressable_target(stage, point);
        reach_pressable_result result = {};
        reach_pressable_press(&stage->pressable, REACH_POINTER_BUTTON_PRIMARY, target,
                              REACH_PRESSABLE_FEEDBACK_NONE, nullptr, &result);
        reach_stage_apply_pressable_result(&result, out);
        stage->pressable_generation =
            target != REACH_PRESSABLE_TARGET_NONE ? state->tile_generation : 0;
        out->handled = target != REACH_PRESSABLE_TARGET_NONE;
        return;
    }

    case REACH_POINTER_EVENT_UP:
    {
        if (event->button != REACH_POINTER_BUTTON_PRIMARY)
        {
            return;
        }
        int32_t was_tracking = reach_pressable_tracking(&stage->pressable);
        size_t pressed_generation = stage->pressable_generation;
        reach_pressable_result result = {};
        reach_pressable_release(&stage->pressable, REACH_POINTER_BUTTON_PRIMARY,
                                reach_stage_pressable_target(stage, point), nullptr, &result);
        reach_stage_apply_pressable_result(&result, out);
        stage->pressable_generation = 0;
        out->handled = was_tracking;
        if (!result.activated || pressed_generation != state->tile_generation)
        {
            return;
        }
        size_t index = (size_t)(result.activated_target & UINT32_MAX);
        uint64_t kind = result.activated_target >> 32;
        if (index >= state->tile_count)
        {
            return;
        }
        if (kind == REACH_STAGE_PRESSABLE_CLOSE)
        {
            out->redraw = 1;
            out->action.kind = REACH_FEATURE_ACTION_CLOSE_WINDOW;
            out->action.index = index;
            out->action.window = state->tiles[index].window;
            reach_stage_depart_tile(stage, index);
            reach_animation_manager_animate_to(
                &stage->animations, REACH_STAGE_ANIMATION_CLOSE_HOVER, 0.0f,
                reach_stage_close_hover_seconds(), REACH_EASING_EASE_OUT);
            return;
        }

        if (kind == REACH_STAGE_PRESSABLE_TILE)
        {
            out->action.kind = state->tiles[index].desktop ? REACH_FEATURE_ACTION_MINIMIZE_ALL_WINDOWS
                                                           : REACH_FEATURE_ACTION_ACTIVATE_WINDOW;
            out->action.index = index;
            out->action.window = state->tiles[index].window;
            state->selected_index = index;
            state->has_selection = 1;
        }
        return;
    }

    case REACH_POINTER_EVENT_MIDDLE:
        return;

    case REACH_POINTER_EVENT_LEAVE:
    case REACH_POINTER_EVENT_CANCEL:
    {
        int32_t was_tracking = reach_pressable_tracking(&stage->pressable);
        reach_pressable_result result = {};
        if (event->kind == REACH_POINTER_EVENT_LEAVE)
        {
            reach_pressable_update(&stage->pressable, REACH_PRESSABLE_TARGET_NONE, &result);
        }
        else
        {
            reach_pressable_cancel(&stage->pressable, nullptr, &result);
            stage->pressable_generation = 0;
        }
        reach_stage_apply_pressable_result(&result, out);
        out->handled = was_tracking;
        out->redraw |= state->has_hover;
        state->has_hover = 0;
        reach_animation_manager_animate_to(&stage->animations, REACH_STAGE_ANIMATION_CLOSE_HOVER,
                                           0.0f, reach_stage_close_hover_seconds(),
                                           REACH_EASING_EASE_OUT);
        return;
    }

    case REACH_POINTER_EVENT_WHEEL:
    default:
        return;
    }
}
