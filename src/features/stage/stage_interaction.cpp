#include "reach/features/stage.h"

#include "stage_common.h"

static int32_t reach_stage_rect_contains(reach_rect_f32 rect, reach_point_f32 point)
{
    return point.x >= rect.x && point.x < rect.x + rect.width && point.y >= rect.y &&
           point.y < rect.y + rect.height;
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
        if (reach_stage_rect_contains(stage->state.tiles[index].current_rect, point))
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
        size_t index = 0;
        int32_t hit = reach_stage_tile_at_point(stage, point, &index);
        int32_t changed = hit != state->has_hover || (hit && index != state->hover_index);
        state->has_hover = hit;
        state->hover_index = hit ? index : 0;
        out->handled = 1;
        out->redraw = changed;
        return;
    }

    case REACH_POINTER_EVENT_DOWN:
    {
        size_t index = 0;
        out->handled = reach_stage_tile_at_point(stage, point, &index);
        return;
    }

    case REACH_POINTER_EVENT_UP:
    {
        size_t index = 0;
        if (reach_stage_tile_at_point(stage, point, &index))
        {
            out->handled = 1;
            out->action.kind = state->tiles[index].desktop
                                   ? REACH_STAGE_ACTION_SHOW_DESKTOP
                                   : REACH_STAGE_ACTION_ACTIVATE_WINDOW;
            out->action.index = index;
            out->action.window = state->tiles[index].window;
            state->selected_index = index;
            state->has_selection = 1;
        }
        return;
    }

    case REACH_POINTER_EVENT_CONTEXT:
    case REACH_POINTER_EVENT_MIDDLE:
        return;

    case REACH_POINTER_EVENT_LEAVE:
    case REACH_POINTER_EVENT_CANCEL:
        out->redraw = state->has_hover;
        state->has_hover = 0;
        state->hover_index = 0;
        return;

    case REACH_POINTER_EVENT_WHEEL:
    default:
        return;
    }
}
