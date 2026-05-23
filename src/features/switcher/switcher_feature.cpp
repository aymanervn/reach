#include "reach/features/switcher.h"

static size_t reach_switcher_min_size(size_t a, size_t b)
{
    return a < b ? a : b;
}

size_t reach_switcher_visible_count(size_t window_count)
{
    return reach_switcher_min_size(window_count, REACH_SWITCHER_VISIBLE_MAX);
}

reach_rect_f32 reach_switcher_bounds_for_count(reach_rect_f32 monitor_bounds, size_t visible_count)
{
    float padding = 24.0f;
    float item_size = 112.0f;
    float gap = 14.0f;
    reach_rect_f32 bounds = {};
    size_t count = visible_count > 0 ? visible_count : 1;
    bounds.width = padding * 2.0f + (float)count * item_size + (float)(count - 1) * gap;
    float max_width = monitor_bounds.width - 48.0f;
    if (bounds.width > max_width) {
        bounds.width = max_width;
    }
    if (bounds.width < 280.0f) {
        bounds.width = monitor_bounds.width < 280.0f ? monitor_bounds.width : 280.0f;
    }
    bounds.height = 184.0f;
    bounds.x = monitor_bounds.x + (monitor_bounds.width - bounds.width) * 0.5f;
    bounds.y = monitor_bounds.y + (monitor_bounds.height - bounds.height) * 0.5f;
    return bounds;
}

void reach_switcher_update_visible_start(reach_switcher_model *model)
{
    if (model == nullptr || model->window_count == 0) {
        if (model != nullptr) {
            model->visible_start = 0;
        }
        return;
    }
    size_t visible_count = reach_switcher_visible_count(model->window_count);
    if (visible_count == 0 || visible_count >= model->window_count) {
        model->visible_start = 0;
        return;
    }
    if (model->selected_index < model->visible_start) {
        model->visible_start = model->selected_index;
    } else if (model->selected_index >= model->visible_start + visible_count) {
        model->visible_start = model->selected_index - visible_count + 1;
    }
    size_t max_start = model->window_count - visible_count;
    if (model->visible_start > max_start) {
        model->visible_start = max_start;
    }
}
