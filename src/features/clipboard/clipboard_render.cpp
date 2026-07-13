#include "reach/features/clipboard.h"

#include "clipboard_common.h"
#include "reach/features/common/scrollbar_render.h"
#include "clipboard_metrics.h"
static float reach_clipboard_max_float(float a, float b)
{
    return a > b ? a : b;
}

static float reach_clipboard_min_float(float a, float b)
{
    return a < b ? a : b;
}

static float reach_clipboard_clamp01(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    if (value > 1.0f)
    {
        return 1.0f;
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

static constexpr size_t REACH_CLIPBOARD_RENDER_ITEM_LIMIT = 5;
static constexpr size_t REACH_CLIPBOARD_PREVIEW_LINE_LIMIT = 4;
static constexpr float REACH_CLIPBOARD_PREVIEW_LINE_HEIGHT_RATIO = 1.25f;
static constexpr float REACH_CLIPBOARD_PREVIEW_AVERAGE_GLYPH_WIDTH_RATIO = 0.58f;

struct reach_clipboard_preview_slice
{
    size_t start;
    size_t count;
    size_t next;
    int32_t has_more;
};

struct reach_clipboard_preview_line
{
    reach_clipboard_preview_slice slice;
    int32_t append_ellipsis;
};

static reach_rect_f32 reach_clipboard_local(reach_rect_f32 rect, reach_rect_f32 bounds)
{
    rect.x -= bounds.x;
    rect.y -= bounds.y;
    return rect;
}

static int32_t reach_clipboard_rect_has_area(reach_rect_f32 rect)
{
    return rect.width > 0.0f && rect.height > 0.0f;
}

static reach_rect_f32 reach_clipboard_intersect(reach_rect_f32 a, reach_rect_f32 b)
{
    const float left = reach_clipboard_max_float(a.x, b.x);
    const float top = reach_clipboard_max_float(a.y, b.y);
    const float right = reach_clipboard_min_float(a.x + a.width, b.x + b.width);
    const float bottom = reach_clipboard_min_float(a.y + a.height, b.y + b.height);

    if (right <= left || bottom <= top)
    {
        return {left, top, 0.0f, 0.0f};
    }

    return {left, top, right - left, bottom - top};
}

static int32_t reach_clipboard_clip_rect(reach_rect_f32 *rect, reach_rect_f32 clip)
{
    if (rect == nullptr)
    {
        return 0;
    }

    *rect = reach_clipboard_intersect(*rect, clip);
    return reach_clipboard_rect_has_area(*rect);
}

static size_t reach_clipboard_first_visible_item(const reach_clipboard_layout *layout,
                                                 size_t item_count)
{
    if (layout == nullptr)
    {
        return item_count;
    }

    for (size_t index = 0; index < item_count; ++index)
    {
        const reach_rect_f32 visible_item =
            reach_clipboard_intersect(layout->items[index], layout->viewport);
        if (reach_clipboard_rect_has_area(visible_item))
        {
            return index;
        }
    }

    return item_count;
}

static int32_t reach_clipboard_layout_has_scrollbar(const reach_clipboard_layout *layout)
{
    return layout != nullptr && layout->scrollbar.track.height > 0.0f &&
           layout->scrollbar.thumb.height > 0.0f &&
           layout->content_height > layout->viewport.height;
}

static int32_t reach_clipboard_preview_is_newline(const reach_clipboard_item *item, size_t index)
{
    return item->preview[index] == '\n' || item->preview[index] == '\r';
}

static int32_t reach_clipboard_preview_is_wrap_space(const reach_clipboard_item *item, size_t index)
{
    return item->preview[index] == ' ' || item->preview[index] == '\t';
}

static size_t reach_clipboard_preview_capacity(const reach_clipboard_item *item)
{
    return sizeof(item->preview) / sizeof(item->preview[0]);
}

static size_t reach_clipboard_preview_skip_newline(const reach_clipboard_item *item, size_t index)
{
    const size_t capacity = reach_clipboard_preview_capacity(item);
    if (index >= capacity)
    {
        return index;
    }

    if (item->preview[index] == '\r')
    {
        ++index;
        if (index < capacity && item->preview[index] == '\n')
        {
            ++index;
        }
        return index;
    }

    if (item->preview[index] == '\n')
    {
        ++index;
    }
    return index;
}

static size_t reach_clipboard_preview_skip_wrap_spaces(const reach_clipboard_item *item,
                                                       size_t index)
{
    const size_t capacity = reach_clipboard_preview_capacity(item);
    while (index < capacity && item->preview[index] != 0 &&
           reach_clipboard_preview_is_wrap_space(item, index))
    {
        ++index;
    }
    return index;
}

static int32_t reach_clipboard_preview_has_content_at(const reach_clipboard_item *item,
                                                      size_t index)
{
    const size_t capacity = reach_clipboard_preview_capacity(item);
    return index < capacity && item->preview[index] != 0;
}

static int32_t reach_clipboard_preview_is_blank_char(const reach_clipboard_item *item, size_t index)
{
    return item->preview[index] == ' ' || item->preview[index] == '\t' ||
           item->preview[index] == '\n' || item->preview[index] == '\r';
}

static int32_t reach_clipboard_preview_has_visible_text(const reach_clipboard_item *item)
{
    if (item == nullptr)
    {
        return 0;
    }

    const size_t capacity = reach_clipboard_preview_capacity(item);
    for (size_t index = 0; index < capacity && item->preview[index] != 0; ++index)
    {
        if (!reach_clipboard_preview_is_blank_char(item, index))
        {
            return 1;
        }
    }
    return 0;
}

static size_t reach_clipboard_preview_line_capacity(float text_width, float text_size)
{
    const float glyph_width = reach_clipboard_max_float(
        1.0f, text_size * REACH_CLIPBOARD_PREVIEW_AVERAGE_GLYPH_WIDTH_RATIO);
    size_t capacity = (size_t)(text_width / glyph_width);
    if (capacity < 1)
    {
        capacity = 1;
    }
    return capacity;
}

static reach_clipboard_preview_slice
reach_clipboard_next_preview_slice(const reach_clipboard_item *item, size_t start, size_t max_chars)
{
    reach_clipboard_preview_slice slice = {};
    slice.start = start;
    slice.next = start;

    if (item == nullptr || max_chars == 0)
    {
        return slice;
    }

    const size_t capacity = reach_clipboard_preview_capacity(item);
    if (start >= capacity || item->preview[start] == 0)
    {
        return slice;
    }

    size_t cursor = start;
    size_t last_wrap_space = capacity;
    const size_t hard_end = start + max_chars < capacity ? start + max_chars : capacity;

    while (cursor < hard_end && item->preview[cursor] != 0)
    {
        if (reach_clipboard_preview_is_newline(item, cursor))
        {
            slice.count = cursor - start;
            slice.next = reach_clipboard_preview_skip_newline(item, cursor);
            slice.has_more = reach_clipboard_preview_has_content_at(item, slice.next);
            return slice;
        }

        if (reach_clipboard_preview_is_wrap_space(item, cursor))
        {
            last_wrap_space = cursor;
        }

        ++cursor;
    }

    if (cursor < capacity && item->preview[cursor] != 0)
    {
        if (last_wrap_space != capacity && last_wrap_space > start)
        {
            slice.count = last_wrap_space - start;
            slice.next = reach_clipboard_preview_skip_wrap_spaces(item, last_wrap_space + 1);
        }
        else
        {
            slice.count = cursor - start;
            slice.next = cursor;
        }
        slice.has_more = reach_clipboard_preview_has_content_at(item, slice.next);
        return slice;
    }

    slice.count = cursor - start;
    slice.next = cursor;
    slice.has_more = 0;
    return slice;
}

static void reach_clipboard_copy_preview_slice(reach_render_command *command,
                                               const reach_clipboard_item *item,
                                               reach_clipboard_preview_slice slice,
                                               size_t max_visible_chars, int32_t append_ellipsis)
{
    if (command == nullptr || item == nullptr)
    {
        return;
    }

    const size_t capacity = sizeof(command->text) / sizeof(command->text[0]);
    if (capacity == 0)
    {
        return;
    }

    size_t output = 0;
    size_t ellipsis_count = append_ellipsis ? (size_t)3 : (size_t)0;
    if (ellipsis_count > capacity - 1)
    {
        ellipsis_count = capacity - 1;
    }

    size_t visible_capacity = max_visible_chars;
    if (visible_capacity > capacity - 1)
    {
        visible_capacity = capacity - 1;
    }
    if (ellipsis_count > visible_capacity)
    {
        ellipsis_count = visible_capacity;
    }

    size_t copy_count = slice.count;
    if (copy_count > visible_capacity - ellipsis_count)
    {
        copy_count = visible_capacity - ellipsis_count;
    }

    for (size_t index = 0; index < copy_count; ++index)
    {
        command->text[output++] = item->preview[slice.start + index];
    }

    for (size_t index = 0; index < ellipsis_count; ++index)
    {
        command->text[output++] = '.';
    }

    command->text[output] = 0;
}

static void
reach_clipboard_push_preview_text_lines(reach_render_command_buffer *commands,
                                        const reach_clipboard_item *item, reach_rect_f32 text_rect,
                                        reach_rect_f32 clip_rect,
                                        decltype(((reach_render_command *)nullptr)->color) color,
                                        float text_size, int32_t text_alignment)
{
    if (commands == nullptr || item == nullptr || text_rect.width <= 0.0f ||
        text_rect.height <= 0.0f || !reach_clipboard_preview_has_visible_text(item))
    {
        return;
    }

    const float line_height =
        reach_clipboard_max_float(1.0f, text_size * REACH_CLIPBOARD_PREVIEW_LINE_HEIGHT_RATIO);
    const size_t max_lines_by_height = (size_t)(text_rect.height / line_height);
    size_t allowed_line_count = max_lines_by_height < REACH_CLIPBOARD_PREVIEW_LINE_LIMIT
                                    ? max_lines_by_height
                                    : REACH_CLIPBOARD_PREVIEW_LINE_LIMIT;
    if (allowed_line_count < 1)
    {
        allowed_line_count = 1;
    }

    const float glyph_width = reach_clipboard_max_float(
        1.0f, text_size * REACH_CLIPBOARD_PREVIEW_AVERAGE_GLYPH_WIDTH_RATIO);
    const size_t line_capacity = reach_clipboard_preview_line_capacity(text_rect.width, text_size);
    reach_clipboard_preview_line lines[REACH_CLIPBOARD_PREVIEW_LINE_LIMIT] = {};
    size_t actual_line_count = 0;
    size_t cursor = 0;

    for (size_t line_index = 0; line_index < allowed_line_count; ++line_index)
    {
        reach_clipboard_preview_slice slice =
            reach_clipboard_next_preview_slice(item, cursor, line_capacity);
        if (slice.count == 0 && !slice.has_more &&
            !reach_clipboard_preview_has_content_at(item, cursor))
        {
            break;
        }

        const int32_t last_allowed_line = line_index + 1 == allowed_line_count;
        lines[actual_line_count].slice = slice;
        lines[actual_line_count].append_ellipsis = last_allowed_line && slice.has_more;
        ++actual_line_count;

        cursor = slice.next;
        if (!slice.has_more)
        {
            break;
        }
    }

    if (actual_line_count == 0)
    {
        return;
    }

    const float used_text_height = line_height * (float)actual_line_count;
    const float start_y =
        text_rect.y + reach_clipboard_max_float(0.0f, text_rect.height - used_text_height) * 0.5f;

    for (size_t line_index = 0; line_index < actual_line_count; ++line_index)
    {
        reach_render_command command = {};
        reach_clipboard_preview_line line = lines[line_index];
        command.type = REACH_RENDER_COMMAND_TEXT;
        command.color = color;
        command.text_size = text_size;
        command.text_alignment = text_alignment;
        command.text_ellipsis = line.append_ellipsis;
        reach_clipboard_copy_preview_slice(&command, item, line.slice, line_capacity,
                                           line.append_ellipsis);

        if (command.text[0] == 0)
        {
            continue;
        }

        size_t visible_char_count = 0;
        while (visible_char_count < sizeof(command.text) / sizeof(command.text[0]) &&
               command.text[visible_char_count] != 0)
        {
            ++visible_char_count;
        }

        const float estimated_line_width =
            reach_clipboard_min_float(text_rect.width, glyph_width * (float)visible_char_count);
        command.rect = {text_rect.x + (text_rect.width - estimated_line_width) * 0.5f,
                        start_y + line_height * (float)line_index, estimated_line_width,
                        line_height};

        if (reach_clipboard_clip_rect(&command.rect, clip_rect))
        {
            reach_render_command_buffer_push(commands, &command);
        }
    }
}

static float reach_clipboard_thumbnail_width_for_item(const reach_clipboard_item *item,
                                                      float thumbnail_height,
                                                      float max_thumbnail_width)
{
    if (thumbnail_height <= 0.0f || max_thumbnail_width <= 0.0f)
    {
        return 0.0f;
    }

    float aspect = 1.0f;
    if (item != nullptr && item->image_width > 0 && item->image_height > 0)
    {
        aspect = (float)item->image_width / (float)item->image_height;
    }

    float width = thumbnail_height * aspect;
    if (width > max_thumbnail_width)
    {
        width = max_thumbnail_width;
    }

    if (width < 1.0f)
    {
        width = 1.0f;
    }

    return width;
}
reach_result reach_clipboard_build_render_commands(const reach_clipboard_render_input *input,
                                                   reach_render_command_buffer *commands)
{
    if (input == nullptr || input->theme == nullptr || input->model == nullptr ||
        input->layout == nullptr || commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_clipboard_metrics metrics = reach_clipboard_metrics_for_scale(input->dpi_scale);
    const reach_theme *theme = input->theme;
    const reach_clipboard_model *model = input->model;
    const reach_clipboard_layout *layout = input->layout;
    reach_render_command_buffer_clear(commands);

    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = {0.0f, 0.0f, layout->bounds.width, layout->bounds.height};
    command.color = theme->clipboard_background;
    command.radius = reach_clipboard_scale_value(theme->clipboard_panel_radius, input->dpi_scale);
    reach_render_command_buffer_push(commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE;
    command.rect = {0.5f, 0.5f, reach_clipboard_max_float(0.0f, layout->bounds.width - 1.0f),
                    reach_clipboard_max_float(0.0f, layout->bounds.height - 1.0f)};
    command.color = theme->clipboard_border;
    command.radius = reach_clipboard_scale_value(theme->clipboard_panel_radius, input->dpi_scale);
    command.stroke_width = theme->border_thickness;
    reach_render_command_buffer_push(commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = reach_clipboard_local(layout->title, layout->bounds);
    command.color = theme->clipboard_primary_text;
    command.text_size = metrics.title_font_size;
    command.text_weight = metrics.title_font_weight;
    command.text_alignment = input->text_alignment_leading;
    command.text[0] = 'C';
    command.text[1] = 'l';
    command.text[2] = 'i';
    command.text[3] = 'p';
    command.text[4] = 'b';
    command.text[5] = 'o';
    command.text[6] = 'a';
    command.text[7] = 'r';
    command.text[8] = 'd';
    command.text[9] = 0;
    reach_render_command_buffer_push(commands, &command);

    const size_t item_count = reach_clipboard_count_clamped(model);
    if (item_count == 0 && layout->viewport.width > 0.0f && layout->viewport.height > 0.0f)
    {
        command = {};
        const float empty_text_height =
            reach_clipboard_max_float(1.0f, metrics.items_font_size * 1.5f);
        reach_rect_f32 empty_text_rect = layout->viewport;
        empty_text_rect.y +=
            reach_clipboard_max_float(0.0f, empty_text_rect.height - empty_text_height) * 0.5f;
        empty_text_rect.height = empty_text_height;

        command.type = REACH_RENDER_COMMAND_TEXT;
        command.rect = reach_clipboard_local(empty_text_rect, layout->bounds);
        command.color = theme->clipboard_secondary_text;
        command.color.a *= 0.75f;
        command.text_size = metrics.items_font_size;
        command.text_alignment = REACH_TEXT_ALIGNMENT_CENTER;
        command.text[0] = 'N';
        command.text[1] = 'o';
        command.text[2] = 't';
        command.text[3] = 'h';
        command.text[4] = 'i';
        command.text[5] = 'n';
        command.text[6] = 'g';
        command.text[7] = ' ';
        command.text[8] = 'i';
        command.text[9] = 'n';
        command.text[10] = ' ';
        command.text[11] = 't';
        command.text[12] = 'h';
        command.text[13] = 'e';
        command.text[14] = ' ';
        command.text[15] = 'c';
        command.text[16] = 'l';
        command.text[17] = 'i';
        command.text[18] = 'p';
        command.text[19] = 'b';
        command.text[20] = 'o';
        command.text[21] = 'a';
        command.text[22] = 'r';
        command.text[23] = 'd';
        command.text[24] = 0;
        reach_render_command_buffer_push(commands, &command);
    }
    if (layout->clear_button.width > 0.0f && layout->clear_button.height > 0.0f)
    {
        const int32_t clear_enabled = item_count > 0;
        const reach_rect_f32 clear_button =
            reach_clipboard_local(layout->clear_button, layout->bounds);

        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect = clear_button;
        command.color = theme->clipboard_secondary_text;
        command.color.a = metrics.clear_button_background_alpha * (clear_enabled ? 1.0f : 0.35f);
        command.radius = metrics.clear_button_radius;
        reach_render_command_buffer_push(commands, &command);

        command = {};
        command.type = REACH_RENDER_COMMAND_TEXT;
        command.rect = clear_button;
        command.color = theme->clipboard_secondary_text;
        command.color.a *= clear_enabled ? 0.90f : 0.35f;
        command.text_size = metrics.items_font_size;
        command.text_alignment = REACH_TEXT_ALIGNMENT_CENTER;
        command.text[0] = 'C';
        command.text[1] = 'l';
        command.text[2] = 'e';
        command.text[3] = 'a';
        command.text[4] = 'r';
        command.text[5] = 0;
        reach_render_command_buffer_push(commands, &command);
    }
    const size_t first_rendered_item = reach_clipboard_first_visible_item(layout, item_count);
    size_t last_rendered_item = first_rendered_item + REACH_CLIPBOARD_RENDER_ITEM_LIMIT;
    if (last_rendered_item > item_count)
    {
        last_rendered_item = item_count;
    }

    for (size_t index = first_rendered_item; index < last_rendered_item; ++index)
    {
        const reach_rect_f32 item = layout->items[index];
        const reach_rect_f32 visible_item = reach_clipboard_intersect(item, layout->viewport);
        if (!reach_clipboard_rect_has_area(visible_item))
        {
            continue;
        }

        const reach_rect_f32 local_item = reach_clipboard_local(item, layout->bounds);
        const reach_rect_f32 local_visible_item =
            reach_clipboard_local(visible_item, layout->bounds);
        const reach_clipboard_item *item_data = &model->items[index];
        const float hover = reach_clipboard_clamp01(
            input->hover_values != nullptr ? input->hover_values[index] : 0.0f);

        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect = local_visible_item;
        command.color = theme->clipboard_item_background;
        command.color.r += (theme->clipboard_item_hover_background.r - command.color.r) * hover;
        command.color.g += (theme->clipboard_item_hover_background.g - command.color.g) * hover;
        command.color.b += (theme->clipboard_item_hover_background.b - command.color.b) * hover;
        command.color.a += (theme->clipboard_item_hover_background.a - command.color.a) * hover;
        command.radius =
            reach_clipboard_scale_value(theme->clipboard_item_radius, input->dpi_scale);
        reach_render_command_buffer_push(commands, &command);

        command = {};
        command.type = REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE;
        command.rect = {local_visible_item.x + 0.5f, local_visible_item.y + 0.5f,
                        reach_clipboard_max_float(0.0f, local_visible_item.width - 1.0f),
                        reach_clipboard_max_float(0.0f, local_visible_item.height - 1.0f)};
        command.color = theme->clipboard_item_hover_border;
        command.color.a *= hover;
        command.radius =
            reach_clipboard_scale_value(theme->clipboard_item_radius, input->dpi_scale);
        command.stroke_width = theme->border_thickness;
        reach_render_command_buffer_push(commands, &command);

        const float close_size = metrics.close_button_size;
        const float close_margin = metrics.close_button_margin;
        const reach_rect_f32 close_rect = {local_item.x + local_item.width - close_size -
                                               close_margin,
                                           local_item.y + close_margin, close_size, close_size};

        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect = close_rect;
        command.color = theme->clipboard_secondary_text;
        command.color.a = metrics.close_button_hover_alpha * hover;
        command.radius = reach_clipboard_scale_value(4.0f, input->dpi_scale);
        if (reach_clipboard_clip_rect(&command.rect, local_visible_item))
        {
            reach_render_command_buffer_push(commands, &command);
        }

        const float inset = close_size * metrics.close_button_inset_ratio;
        command = {};
        command.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        command.rect = {close_rect.x + inset, close_rect.y + inset,
                        reach_clipboard_max_float(0.0f, close_rect.width - inset * 2.0f),
                        reach_clipboard_max_float(0.0f, close_rect.height - inset * 2.0f)};
        command.icon_id = REACH_VECTOR_ICON_CLOSE;
        command.color = theme->clipboard_secondary_text;
        command.color.a = theme->clipboard_secondary_text.a * (0.5f + 0.5f * hover);
        if (reach_clipboard_clip_rect(&command.rect, local_visible_item))
        {
            reach_render_command_buffer_push(commands, &command);
        }

        const float padding = metrics.thumbnail_padding;
        const float reserved_right = close_size + close_margin * 2.0f;
        const float text_right = local_item.x + local_item.width - reserved_right;
        if (item_data->kind == REACH_CLIPBOARD_ITEM_IMAGE && item_data->thumbnail_id != 0)
        {
            const float thumbnail_height =
                reach_clipboard_min_float(metrics.thumbnail_height, local_item.height);
            const float thumbnail_max_width = reach_clipboard_max_float(
                0.0f, local_item.width * metrics.thumbnail_max_width_ratio);
            const float thumbnail_width = reach_clipboard_thumbnail_width_for_item(
                item_data, thumbnail_height, thumbnail_max_width);

            const float thumbnail_x = local_item.x + padding;
            const float thumbnail_y =
                local_item.y +
                reach_clipboard_max_float(0.0f, local_item.height - thumbnail_height) * 0.5f;

            command = {};
            command.type = REACH_RENDER_COMMAND_ICON;
            command.rect = {thumbnail_x, thumbnail_y, thumbnail_width, thumbnail_height};
            command.icon_id = item_data->thumbnail_id;
            command.icon_crop_to_fill = 1;
            command.color.a = 1.0f;
            command.radius = metrics.item_radius;
            if (reach_clipboard_clip_rect(&command.rect, local_visible_item))
            {
                reach_render_command_buffer_push(commands, &command);
            }

            const float text_x = thumbnail_x + thumbnail_width + metrics.thumbnail_text_gap;
            reach_clipboard_push_preview_text_lines(
                commands, item_data,
                {text_x, local_item.y + padding,
                 reach_clipboard_max_float(0.0f, text_right - text_x),
                 reach_clipboard_max_float(0.0f, local_item.height - padding * 2.0f)},
                local_visible_item, theme->clipboard_secondary_text, metrics.items_font_size,
                input->text_alignment_leading);
        }
        else
        {
            const float text_x = local_item.x + padding;
            reach_clipboard_push_preview_text_lines(
                commands, item_data,
                {text_x, local_item.y + padding,
                 reach_clipboard_max_float(0.0f, text_right - text_x),
                 reach_clipboard_max_float(0.0f, local_item.height - padding * 2.0f)},
                local_visible_item, theme->clipboard_primary_text, metrics.items_font_size,
                input->text_alignment_leading);
        }
    }

    if (reach_clipboard_layout_has_scrollbar(layout))
    {
        reach_scrollbar_build_render_commands(layout->scrollbar.track, layout->scrollbar.thumb,
                                              layout->bounds, theme->clipboard_scrollbar_track,
                                              theme->clipboard_scrollbar_thumb, commands);
    }

    return REACH_OK;
}
reach_result reach_clipboard_append_render_commands(reach_clipboard_feature *clipboard,
                                                    const reach_theme *theme, float dpi_scale,
                                                    reach_render_command_buffer *out_commands)
{
    if (clipboard == nullptr || theme == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_clipboard_state *state = reach_clipboard_feature_state_mut(clipboard);

    float hover_values[REACH_CLIPBOARD_MAX_ITEMS] = {};
    reach_clipboard_feature_fill_hover_values(clipboard, hover_values, REACH_CLIPBOARD_MAX_ITEMS);

    reach_clipboard_render_input input = {};
    input.theme = theme;
    input.model = &state->model;
    input.layout = &state->layout;
    input.hover_values = hover_values;
    input.dpi_scale = dpi_scale;
    input.text_alignment_leading = REACH_TEXT_ALIGNMENT_LEADING;

    return reach_clipboard_build_render_commands(&input, out_commands);
}
