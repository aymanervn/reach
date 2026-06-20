#include "reach/features/clipboard.h"

static float reach_clipboard_scale(float value, float scale)
{
    return value * (scale > 0.0f ? scale : 1.0f);
}

static int32_t reach_clipboard_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return rect.width > 0.0f && rect.height > 0.0f && (float)x >= rect.x &&
           (float)x <= rect.x + rect.width && (float)y >= rect.y &&
           (float)y <= rect.y + rect.height;
}

reach_clipboard_layout reach_clipboard_compute_layout(reach_clipboard_model *model,
                                                      reach_rect_f32 monitor_bounds,
                                                      reach_rect_f32 launcher_bounds,
                                                      float dpi_scale)
{
    reach_clipboard_layout layout = {};
    if (model == nullptr)
    {
        return layout;
    }
    float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    float width = reach_clipboard_scale(390.0f, scale);
    float height = reach_clipboard_scale(500.0f, scale);
    float margin = reach_clipboard_scale(12.0f, scale);
    if (height > monitor_bounds.height - margin * 2.0f)
    {
        height = monitor_bounds.height - margin * 2.0f;
    }
    float right_x = launcher_bounds.x + launcher_bounds.width + margin;
    float left_x = launcher_bounds.x - width - margin;
    layout.bounds.x =
        right_x + width <= monitor_bounds.x + monitor_bounds.width - margin ? right_x : left_x;
    if (layout.bounds.x < monitor_bounds.x + margin)
    {
        layout.bounds.x = monitor_bounds.x + margin;
    }
    layout.bounds.y = launcher_bounds.y + launcher_bounds.height * 0.5f - height * 0.5f;
    if (layout.bounds.y < monitor_bounds.y + margin)
    {
        layout.bounds.y = monitor_bounds.y + margin;
    }
    if (layout.bounds.y + height > monitor_bounds.y + monitor_bounds.height - margin)
    {
        layout.bounds.y = monitor_bounds.y + monitor_bounds.height - margin - height;
    }
    layout.bounds.width = width;
    layout.bounds.height = height;

    float padding = reach_clipboard_scale(16.0f, scale);
    float title_height = reach_clipboard_scale(34.0f, scale);
    float scrollbar_gutter = reach_clipboard_scale(15.0f, scale);
    layout.title = {layout.bounds.x + padding, layout.bounds.y + padding,
                    width - padding * 2.0f, title_height};
    layout.viewport = {layout.bounds.x + padding, layout.title.y + title_height,
                       width - padding * 2.0f - scrollbar_gutter,
                       height - padding * 2.0f - title_height};
    layout.row_height = reach_clipboard_scale(88.0f, scale);
    float row_gap = reach_clipboard_scale(8.0f, scale);
    layout.content_height =
        model->count > 0 ? layout.row_height * (float)model->count +
                               row_gap * (float)(model->count - 1)
                         : 0.0f;
    reach_scrollbar_set_extents(&model->scrollbar, layout.content_height,
                                layout.viewport.height);
    float close_button_size = reach_clipboard_scale(20.0f, scale);
    float close_button_margin = reach_clipboard_scale(6.0f, scale);
    for (size_t index = 0; index < model->count; ++index)
    {
        layout.items[index] = {layout.viewport.x,
                               layout.viewport.y +
                                   (layout.row_height + row_gap) * (float)index -
                                   model->scrollbar.offset,
                               layout.viewport.width, layout.row_height};
        layout.close_buttons[index] = {
            layout.items[index].x + layout.items[index].width - close_button_size - close_button_margin,
            layout.items[index].y + close_button_margin,
            close_button_size,
            close_button_size};
    }
    reach_rect_f32 track = {
        layout.viewport.x + layout.viewport.width + reach_clipboard_scale(7.0f, scale),
        layout.viewport.y, reach_clipboard_scale(4.0f, scale), layout.viewport.height};
    layout.scrollbar = reach_scrollbar_compute_layout(
        &model->scrollbar, track, layout.viewport.height, layout.content_height,
        reach_clipboard_scale(24.0f, scale));
    return layout;
}

reach_clipboard_hit_result reach_clipboard_hit_test(const reach_clipboard_model *model,
                                                    const reach_clipboard_layout *layout,
                                                    int32_t x, int32_t y)
{
    reach_clipboard_hit_result result = {};
    result.index = REACH_CLIPBOARD_MAX_ITEMS;
    if (model == nullptr || layout == nullptr || !model->open)
    {
        return result;
    }
    if (reach_clipboard_contains(layout->scrollbar.thumb, x, y))
    {
        result.type = REACH_CLIPBOARD_HIT_SCROLLBAR_THUMB;
        return result;
    }
    if (reach_clipboard_contains(layout->scrollbar.track, x, y))
    {
        result.type = REACH_CLIPBOARD_HIT_SCROLLBAR_TRACK;
        return result;
    }
    if (!reach_clipboard_contains(layout->viewport, x, y))
    {
        return result;
    }
    for (size_t index = 0; index < model->count; ++index)
    {
        if (reach_clipboard_contains(layout->close_buttons[index], x, y))
        {
            result.type = REACH_CLIPBOARD_HIT_ITEM_CLOSE;
            result.index = index;
            return result;
        }
        if (reach_clipboard_contains(layout->items[index], x, y))
        {
            result.type = REACH_CLIPBOARD_HIT_ITEM;
            result.index = index;
            return result;
        }
    }
    return result;
}
