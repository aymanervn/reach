#include "reach/features/launcher.h"

static int32_t reach_launcher_rect_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    if (rect.width <= 0.0f || rect.height <= 0.0f)
    {
        return 0;
    }
    return (float)x >= rect.x && (float)x <= rect.x + rect.width && (float)y >= rect.y &&
           (float)y <= rect.y + rect.height;
}

static size_t reach_launcher_visible_result_count(const reach_ui_state *state)
{
    if (state == nullptr)
    {
        return 0;
    }
    return state->launcher.result_count < REACH_SEARCH_VISIBLE_RESULTS
               ? state->launcher.result_count
               : REACH_SEARCH_VISIBLE_RESULTS;
}

reach_launcher_hit_result reach_launcher_hit_test(const reach_ui_state *state,
                                                  const reach_launcher_layout *layout, int32_t x,
                                                  int32_t y)
{
    reach_launcher_hit_result hit = {};
    hit.type = REACH_LAUNCHER_HIT_NONE;
    hit.index = REACH_MAX_PINNED_APPS;
    if (state == nullptr || layout == nullptr || !state->launcher.open)
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
        hit.index = reach_ui_state_launcher_result_scroll_offset(state);
        return hit;
    }

    if (reach_launcher_rect_contains(layout->search_result_scrollbar_track, x, y))
    {
        hit.type = REACH_LAUNCHER_HIT_SCROLLBAR_TRACK;
        hit.index = reach_ui_state_launcher_result_scroll_offset(state);
        return hit;
    }

    if (reach_launcher_rect_contains(layout->search_result_items, x, y))
    {
        hit.type = REACH_LAUNCHER_HIT_SEARCH_RESULT;
        float local_y = (float)y - layout->search_result_items.y;
        size_t visible_count = reach_launcher_visible_result_count(state);
        float row_height =
            visible_count > 0 ? layout->search_result_items.height / (float)visible_count : 0.0f;
        size_t visible_index =
            row_height > 0.0f && local_y > 0.0f ? (size_t)(local_y / row_height) : 0;
        size_t index = reach_ui_state_launcher_result_scroll_offset(state) + visible_index;
        hit.index = index < state->launcher.result_count ? index : REACH_SEARCH_MAX_RESULTS;
    }
    return hit;
}
