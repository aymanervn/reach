#include "reach/features/clipboard.h"
#include "clipboard_metrics.h"

static float reach_clipboard_max_float(float a, float b)
{
    return a > b ? a : b;
}

static float reach_clipboard_min_float(float a, float b)
{
    return a < b ? a : b;
}

static float reach_clipboard_clamp_float(float value, float minimum, float maximum)
{
    if (maximum < minimum)
    {
        return minimum;
    }
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

static size_t reach_clipboard_count_clamped(const reach_clipboard_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    return model->count <= REACH_CLIPBOARD_MAX_ITEMS ? model->count : REACH_CLIPBOARD_MAX_ITEMS;
}

static int32_t reach_clipboard_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return rect.width > 0.0f && rect.height > 0.0f && (float)x >= rect.x &&
           (float)x < rect.x + rect.width && (float)y >= rect.y && (float)y < rect.y + rect.height;
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

    (void)launcher_bounds;

    const reach_clipboard_metrics metrics = reach_clipboard_metrics_for_scale(dpi_scale);
    const float screen_edge_margin = metrics.screen_edge_margin;
    const float available_width =
        reach_clipboard_max_float(0.0f, monitor_bounds.width - screen_edge_margin * 2.0f);
    const float available_height =
        reach_clipboard_max_float(0.0f, monitor_bounds.height - screen_edge_margin * 2.0f);

    const float width = reach_clipboard_min_float(metrics.panel_width, available_width);

    const float max_items_height = metrics.item_large_size * 4.0f + metrics.item_gap * 3.0f;
    const float chrome_height = 2.0f * metrics.padding + metrics.title_height + metrics.title_gap;
    const float default_height = chrome_height + metrics.item_default_size;
    const float max_height =
        reach_clipboard_min_float(chrome_height + max_items_height, available_height);

    const float available_for_items = reach_clipboard_max_float(0.0f, max_height - chrome_height);
    size_t max_visible_items = 0;
    if (available_for_items > 0.0f)
    {
        max_visible_items = (size_t)((available_for_items + metrics.item_gap) /
                                     (metrics.item_large_size + metrics.item_gap));
        if (max_visible_items > 4)
        {
            max_visible_items = 4;
        }
    }

    const size_t item_count = reach_clipboard_count_clamped(model);
    const size_t visible_count = item_count < max_visible_items ? item_count : max_visible_items;
    const float visible_items_height = visible_count > 0
                                           ? metrics.item_large_size * (float)visible_count +
                                                 metrics.item_gap * (float)(visible_count - 1)
                                           : 0.0f;

    float height = chrome_height + visible_items_height;
    if (height < default_height)
    {
        height = default_height;
    }
    height = reach_clipboard_min_float(height, available_height);

    const float monitor_left = monitor_bounds.x + screen_edge_margin;
    const float monitor_top = monitor_bounds.y + screen_edge_margin;
    const float monitor_right = monitor_bounds.x + monitor_bounds.width - screen_edge_margin;
    const float monitor_bottom = monitor_bounds.y + monitor_bounds.height - screen_edge_margin;

    layout.bounds.x =
        reach_clipboard_clamp_float(monitor_left, monitor_left, monitor_right - width);
    layout.bounds.y =
        reach_clipboard_clamp_float(monitor_bottom - height, monitor_top, monitor_bottom - height);
    layout.bounds.width = width;
    layout.bounds.height = height;

    const float content_width = reach_clipboard_max_float(0.0f, width - metrics.padding * 2.0f);
    layout.title = {layout.bounds.x + metrics.padding, layout.bounds.y + metrics.padding,
                    content_width, metrics.title_height};

    layout.viewport = {layout.bounds.x + metrics.padding,
                       layout.title.y + metrics.title_height + metrics.title_gap, content_width,
                       reach_clipboard_max_float(0.0f, height - chrome_height)};

    layout.item_large_size = metrics.item_large_size;
    layout.content_height = item_count > 0 ? metrics.item_large_size * (float)item_count +
                                                 metrics.item_gap * (float)(item_count - 1)
                                           : 0.0f;

    const int32_t needs_scrollbar =
        layout.content_height > layout.viewport.height && layout.viewport.height > 0.0f;

    reach_scrollbar_set_extents(&model->scrollbar, layout.content_height, layout.viewport.height);

    float item_width = layout.viewport.width;
    if (needs_scrollbar)
    {
        item_width -= metrics.scrollbar_gutter + metrics.item_scrollbar_gap;
    }
    item_width = reach_clipboard_max_float(0.0f, item_width);

    for (size_t index = 0; index < item_count; ++index)
    {
        layout.items[index] = {layout.viewport.x,
                               layout.viewport.y +
                                   (metrics.item_large_size + metrics.item_gap) * (float)index -
                                   model->scrollbar.offset,
                               item_width, metrics.item_large_size};
        layout.close_buttons[index] = {layout.items[index].x + layout.items[index].width -
                                           metrics.close_button_size - metrics.close_button_margin,
                                       layout.items[index].y + metrics.close_button_margin,
                                       metrics.close_button_size, metrics.close_button_size};
    }

    if (needs_scrollbar)
    {
        const reach_rect_f32 track = {
            layout.viewport.x + item_width + metrics.item_scrollbar_gap + metrics.track_x_offset,
            layout.viewport.y, metrics.track_width, layout.viewport.height};
        layout.scrollbar =
            reach_scrollbar_compute_layout(&model->scrollbar, track, layout.viewport.height,
                                           layout.content_height, metrics.thumb_size);
    }

    return layout;
}

reach_clipboard_hit_result reach_clipboard_hit_test(const reach_clipboard_model *model,
                                                    const reach_clipboard_layout *layout, int32_t x,
                                                    int32_t y)
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

    const size_t item_count = reach_clipboard_count_clamped(model);
    for (size_t index = 0; index < item_count; ++index)
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
