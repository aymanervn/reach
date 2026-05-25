#include "reach/features/launcher.h"

static int32_t reach_launcher_rect_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return (float)x >= rect.x && (float)x <= rect.x + rect.width && (float)y >= rect.y && (float)y <= rect.y + rect.height;
}

reach_launcher_hit_result reach_launcher_hit_test(const reach_ui_state *state, const reach_launcher_layout *layout, int32_t x, int32_t y)
{
    reach_launcher_hit_result hit = {};
    hit.type = REACH_LAUNCHER_HIT_NONE;
    hit.index = REACH_MAX_PINNED_APPS;
    if (state == nullptr || layout == nullptr || !state->launcher.open) {
        return hit;
    }

    for (size_t index = 0; index < layout->pinned_app_slot_count; ++index) {
        if (reach_launcher_rect_contains(layout->pinned_app_slots[index], x, y)) {
            hit.type = REACH_LAUNCHER_HIT_PINNED_APP;
            hit.index = index;
            return hit;
        }
    }

    if (reach_launcher_rect_contains(layout->search_results, x, y)) {
        hit.type = REACH_LAUNCHER_HIT_SEARCH_RESULT;
        float local_y = (float)y - layout->search_results.y;
        float row_height = state->launcher.result_count > 0
            ? layout->search_results.height / (float)state->launcher.result_count
            : 0.0f;
        size_t index = row_height > 0.0f && local_y > 0.0f ? (size_t)(local_y / row_height) : 0;
        hit.index = index < state->launcher.result_count ? index : REACH_SEARCH_MAX_RESULTS;
    }
    return hit;
}
