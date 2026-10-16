#include "reach/features/clipboard.h"

#include "clipboard_common.h"
#include "reach/features/common/scrollbar_render.h"
#include "clipboard_metrics.h"
#include "reach/features/common/ui_controls.h"

#include <math.h>
#include <limits>

static float reach_clipboard_max_float(float a, float b)
{
    return a > b ? a : b;
}

static float reach_clipboard_min_float(float a, float b)
{
    return a < b ? a : b;
}

static constexpr size_t REACH_CLIPBOARD_RENDER_ITEM_LIMIT = 5;
static constexpr size_t REACH_CLIPBOARD_PREVIEW_LINE_LIMIT = 4;
static constexpr float REACH_CLIPBOARD_PREVIEW_LINE_HEIGHT_RATIO = 1.25f;
static constexpr float REACH_CLIPBOARD_PREVIEW_GLYPH_ADVANCE_RATIO = 0.60f;

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
    const float glyph_width =
        reach_clipboard_max_float(1.0f, text_size * REACH_CLIPBOARD_PREVIEW_GLYPH_ADVANCE_RATIO);
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

static size_t reach_clipboard_preview_commands(
    const reach_clipboard_item *item, reach_rect_f32 text_rect, reach_rect_f32 clip_rect,
    decltype(((reach_render_command *)nullptr)->color) color, float text_size,
    int32_t text_alignment, reach_render_command out[REACH_CLIPBOARD_PREVIEW_LINE_LIMIT])
{
    if (item == nullptr || text_rect.width <= 0.0f || text_rect.height <= 0.0f ||
        !reach_clipboard_preview_has_visible_text(item))
    {
        return 0;
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
        return 0;
    }

    const float used_text_height = line_height * (float)actual_line_count;
    const float start_y =
        text_rect.y + reach_clipboard_max_float(0.0f, text_rect.height - used_text_height) * 0.5f;

    size_t written = 0;
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

        command.rect = {text_rect.x, start_y + line_height * (float)line_index, text_rect.width,
                        line_height};

        if (reach_clipboard_clip_rect(&command.rect, clip_rect))
        {
            out[written++] = command;
        }
    }
    return written;
}

static reach_result reach_clipboard_push_width_preview(
    reach_render_command_buffer *commands, uint64_t first_id, const reach_clipboard_item *item,
    reach_rect_f32 text_rect, reach_rect_f32 clip_rect, reach_color color, float text_size,
    int32_t text_alignment, const reach_motion &width, const reach_motion &x, float width_span)
{
    if (text_rect.width <= 0 || text_rect.height <= 0 ||
        !reach_clipboard_preview_has_visible_text(item))
        return REACH_OK;
    auto available = reach_motion_clamp(&width, 0, text_rect.width);
    float minimum = fmaxf(0, text_rect.width - width_span), maximum = text_rect.width;
    size_t first = reach_clipboard_preview_line_capacity(minimum, text_size);
    size_t last = reach_clipboard_preview_line_capacity(maximum, text_size);
    if (last - first >= 4)
        return REACH_ERROR;
    reach_render_group position = {};
    position.id = first_id;
    position.bounds = text_rect;
    position.x = reach_motion_affine(&x, 1, -text_rect.x);
    reach_result result = reach_render_command_buffer_begin_group(commands, &position);
    reach_render_command_buffer_end_group(commands);
    if (result != REACH_OK)
        return result;
    float glyph = fmaxf(1, text_size * REACH_CLIPBOARD_PREVIEW_GLYPH_ADVANCE_RATIO);
    auto threshold_for = [&](size_t capacity)
    {
        float threshold = (float)capacity * glyph;
        while (reach_clipboard_preview_line_capacity(threshold, text_size) < capacity)
            threshold = nextafterf(threshold, std::numeric_limits<float>::infinity());
        while (reach_clipboard_preview_line_capacity(
                   nextafterf(threshold, -std::numeric_limits<float>::infinity()), text_size) >=
               capacity)
            threshold = nextafterf(threshold, -std::numeric_limits<float>::infinity());
        return threshold;
    };
    for (size_t capacity = first; capacity <= last; ++capacity)
    {
        reach_rect_f32 sample = text_rect;
        sample.width = fminf(maximum, fmaxf(minimum, ((float)capacity + .5f) * glyph));
        reach_render_command lines[REACH_CLIPBOARD_PREVIEW_LINE_LIMIT] = {};
        size_t count = reach_clipboard_preview_commands(item, sample, clip_rect, color, text_size,
                                                        text_alignment, lines);
        if (count == 0)
            continue;
        float thresholds[2] = {}, values[3] = {};
        size_t boundaries = 0;
        if (capacity > 1)
        {
            thresholds[boundaries++] = threshold_for(capacity);
            values[1] = 1;
        }
        else
            values[0] = 1;
        thresholds[boundaries++] = threshold_for(capacity + 1);
        reach_render_group variant = {};
        variant.id = first_id + 1 + (capacity - first) * 16;
        variant.parent_id = position.id;
        variant.bounds = text_rect;
        result = reach_motion_lookup(&available, thresholds, values, boundaries, &variant.opacity);
        if (result == REACH_OK)
            result = reach_render_command_buffer_begin_group(commands, &variant);
        reach_render_command_buffer_end_group(commands);
        if (result != REACH_OK)
            return result;
        for (size_t index = 0; index < count; ++index)
        {
            auto &line = lines[index];
            line.rect.width = text_rect.width;
            reach_render_group viewport = {};
            viewport.id = variant.id + index + 1;
            viewport.parent_id = variant.id;
            viewport.bounds = viewport.clip = line.rect;
            viewport.has_clip = 1;
            viewport.clip_insets[REACH_RENDER_CLIP_RIGHT] =
                reach_motion_affine(&available, -1, line.rect.width);
            line.text_fit_to_clip = REACH_RENDER_TEXT_FIT_VERTICAL_OVERFLOW;
            result = reach_render_push_width_text(commands, &line, &viewport);
            if (result != REACH_OK)
                return result;
        }
    }
    return REACH_OK;
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
    reach_clipboard_layout source_layout = *input->layout;
    const reach_clipboard_layout *layout = &source_layout;
    const float border_thickness = reach_theme_border_thickness(theme, input->dpi_scale);
    const float width_span = metrics.scrollbar_gutter + metrics.item_scrollbar_gap;
    bool retained_width = input->item_width != nullptr && input->item_width->duration_seconds > 0;
    reach_motion item_width = {};
    reach_motion_set(&item_width, layout->item_width);
    if (input->item_width != nullptr && input->item_width->generation != 0)
        item_width = *input->item_width;
    item_width = reach_motion_clamp(&item_width, 0, fmaxf(0, layout->viewport.width));
    if (retained_width)
    {
        float minimum = 0, maximum = 0;
        if (reach_motion_range(&item_width, &minimum, &maximum) != REACH_OK)
            return REACH_INVALID_ARGUMENT;
        if (minimum < metrics.close_button_size + metrics.close_button_margin * 2 +
                          border_thickness * 2 ||
            minimum < fmaxf(0, layout->viewport.width - width_span) - .01f ||
            maximum > layout->viewport.width + .01f)
        {
            reach_clipboard_layout_set_item_width(
                &source_layout, model->count, reach_motion_value(&item_width), border_thickness);
            auto sampled = *input;
            sampled.layout = &source_layout;
            sampled.item_width = nullptr;
            reach_result result = reach_clipboard_build_render_commands(&sampled, commands);
            if (result == REACH_OK)
                commands->requires_content_sampling = reach_motion_active(&item_width);
            return result;
        }
        reach_clipboard_layout_set_item_width(&source_layout, model->count, layout->viewport.width,
                                              border_thickness);
    }
    else if (item_width.generation != 0)
        reach_clipboard_layout_set_item_width(&source_layout, model->count,
                                              reach_motion_value(&item_width), border_thickness);
    reach_render_command_buffer_clear(commands);

    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = {0.0f, 0.0f, layout->bounds.width, layout->bounds.height};
    command.radius = reach_clipboard_scale_value(theme->radius_large, input->dpi_scale);
    reach_result result = reach_render_push_bordered_background(
        commands, &command, theme->clipboard_background, theme->clipboard_border, border_thickness,
        &theme->popup_shadow, input->dpi_scale);
    if (result != REACH_OK)
    {
        return result;
    }

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
        const reach_rect_f32 local_item_content =
            reach_theme_border_content_rect(theme, input->dpi_scale, local_item);
        const reach_rect_f32 local_visible_item =
            reach_clipboard_local(visible_item, layout->bounds);
        const reach_clipboard_item *item_data = &model->items[index];
        reach_motion hover = {};
        reach_motion_set(&hover, 0);
        if (input->hover_motions != nullptr)
            hover = reach_motion_clamp(&input->hover_motions[index], 0, 1);
        uint64_t feedback_id = input->hover_group_ids != nullptr
                                   ? input->hover_group_ids[index]
                                   : UINT64_C(0x7000000000000000) + index * 128;
        if (retained_width)
        {
            reach_rect_motion visible = {};
            reach_motion_set(&visible.x, local_visible_item.x);
            reach_motion_set(&visible.y, local_visible_item.y);
            visible.width = item_width;
            reach_motion_set(&visible.height, local_visible_item.height);
            result = reach_ui_bordered_motion_color_feedback_render(
                commands, feedback_id + 16, &visible,
                reach_clipboard_scale_value(theme->radius_small, input->dpi_scale),
                border_thickness, theme->clipboard_item_background,
                theme->clipboard_item_hover_background, theme->clipboard_item_hover_border, &hover);
        }
        else
            result = reach_ui_bordered_color_feedback_render(
                commands, feedback_id, local_visible_item,
                reach_clipboard_scale_value(theme->radius_small, input->dpi_scale),
                border_thickness, theme->clipboard_item_background,
                theme->clipboard_item_hover_background, theme->clipboard_item_hover_border, &hover);
        if (result != REACH_OK)
            return result;

        const float close_size = metrics.close_button_size;
        const float close_margin = metrics.close_button_margin;
        const reach_rect_f32 close_rect =
            reach_clipboard_local(layout->close_buttons[index], layout->bounds);

        reach_render_group close_placement = {};
        close_placement.parent_id = retained_width ? feedback_id + 8 : 0;
        if (retained_width)
        {
            reach_render_group position = {};
            position.id = close_placement.parent_id;
            position.bounds = close_rect;
            position.x = reach_motion_affine(&item_width, 1, -layout->item_width);
            result = reach_render_command_buffer_begin_group(commands, &position);
            reach_render_command_buffer_end_group(commands);
            if (result != REACH_OK)
                return result;
        }
        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect = close_rect;
        command.color = theme->clipboard_secondary_text;
        command.color.a = 1;
        reach_motion close_opacity =
            reach_motion_affine(&hover, metrics.close_button_hover_alpha, 0);
        command.radius = reach_clipboard_scale_value(theme->radius_small, input->dpi_scale);
        if (reach_clipboard_clip_rect(&command.rect, local_visible_item))
        {
            reach_rect_f32 source = {};
            if (!reach_render_retained_command_bounds(&command, &source))
                return REACH_ERROR;
            close_placement.bounds = source;
            result = reach_ui_feedback_begin(commands, feedback_id + 4, source, &close_opacity,
                                             retained_width ? &close_placement : nullptr);
            if (result != REACH_OK)
                return result;
            result = reach_render_command_buffer_push(commands, &command);
            reach_render_command_buffer_end_group(commands);
            if (result != REACH_OK)
                return result;
        }

        const float inset = close_size * metrics.close_button_inset_ratio;
        command = {};
        command.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        command.rect = {close_rect.x + inset, close_rect.y + inset,
                        reach_clipboard_max_float(0.0f, close_rect.width - inset * 2.0f),
                        reach_clipboard_max_float(0.0f, close_rect.height - inset * 2.0f)};
        command.icon_id = REACH_VECTOR_ICON_CLOSE;
        command.color = theme->clipboard_secondary_text;
        command.color.a = 1;
        reach_motion glyph_opacity =
            reach_motion_affine(&hover, theme->clipboard_secondary_text.a * 0.5f,
                                theme->clipboard_secondary_text.a * 0.5f);
        if (reach_clipboard_clip_rect(&command.rect, local_visible_item))
        {
            reach_rect_f32 source = {};
            if (!reach_render_retained_command_bounds(&command, &source))
                return REACH_ERROR;
            close_placement.bounds = source;
            result = reach_ui_feedback_begin(commands, feedback_id + 5, source, &glyph_opacity,
                                             retained_width ? &close_placement : nullptr);
            if (result != REACH_OK)
                return result;
            result = reach_render_command_buffer_push(commands, &command);
            reach_render_command_buffer_end_group(commands);
            if (result != REACH_OK)
                return result;
        }

        const float padding = metrics.thumbnail_padding;
        const float reserved_right = close_size + close_margin * 2.0f;
        const float text_right = local_item_content.x + local_item_content.width - reserved_right;
        auto push_text = [&](float x, reach_color color, const reach_motion *thumbnail)
        {
            reach_rect_f32 text_rect = {
                x, local_item_content.y + padding, reach_clipboard_max_float(0, text_right - x),
                reach_clipboard_max_float(0, local_item_content.height - padding * 2)};
            if (!retained_width)
            {
                reach_render_command lines[REACH_CLIPBOARD_PREVIEW_LINE_LIMIT] = {};
                size_t count = reach_clipboard_preview_commands(
                    item_data, text_rect, local_visible_item, color, metrics.items_font_size,
                    input->text_alignment_leading, lines);
                for (size_t index = 0; index < count; ++index)
                {
                    auto &line = lines[index];
                    line.text_fit_to_clip = REACH_RENDER_TEXT_FIT_VERTICAL_OVERFLOW;
                    reach_render_group viewport = {};
                    viewport.id = feedback_id + 32 + index;
                    viewport.bounds = viewport.clip = line.rect;
                    viewport.has_clip = 1;
                    auto result = reach_render_push_width_text(commands, &line, &viewport);
                    if (result != REACH_OK)
                        return result;
                }
                return REACH_OK;
            }
            auto text_width = reach_motion_affine(&item_width, 1,
                                                  -border_thickness * 2 - reserved_right - padding);
            reach_motion text_x = {};
            reach_motion_set(&text_x, local_item_content.x + padding);
            if (thumbnail != nullptr)
            {
                if (reach_motion_combine(&text_width, 1, thumbnail, -1, -metrics.thumbnail_text_gap,
                                         &text_width) != REACH_OK)
                    return REACH_INVALID_ARGUMENT;
                text_x = reach_motion_affine(
                    thumbnail, 1, local_item_content.x + padding + metrics.thumbnail_text_gap);
            }
            return reach_clipboard_push_width_preview(
                commands, feedback_id + 32, item_data, text_rect, local_visible_item, color,
                metrics.items_font_size, input->text_alignment_leading, text_width, text_x,
                width_span);
        };
        if (item_data->kind == REACH_CLIPBOARD_ITEM_IMAGE && item_data->thumbnail_id != 0)
        {
            const float thumbnail_height =
                reach_clipboard_min_float(metrics.thumbnail_height, local_item_content.height);
            const float thumbnail_max_width = reach_clipboard_max_float(
                0.0f, local_item_content.width * metrics.thumbnail_max_width_ratio);
            const float thumbnail_width = reach_clipboard_thumbnail_width_for_item(
                item_data, thumbnail_height, thumbnail_max_width);

            const float thumbnail_x = local_item_content.x + padding;
            const float thumbnail_y =
                local_item_content.y +
                reach_clipboard_max_float(0.0f, local_item_content.height - thumbnail_height) *
                    0.5f;

            command = {};
            command.type = REACH_RENDER_COMMAND_ICON;
            command.rect = {thumbnail_x, thumbnail_y, thumbnail_width, thumbnail_height};
            command.icon_id = item_data->thumbnail_id;
            command.icon_crop_to_fill = 1;
            command.color.a = 1.0f;
            command.radius = reach_clipboard_scale_value(theme->radius_small, input->dpi_scale);
            reach_motion thumbnail_motion =
                reach_motion_affine(&item_width, metrics.thumbnail_max_width_ratio,
                                    -border_thickness * 2 * metrics.thumbnail_max_width_ratio);
            float aspect = item_data->image_width > 0 && item_data->image_height > 0
                               ? (float)item_data->image_width / item_data->image_height
                               : 1;
            float cap = fmaxf(1, thumbnail_height * aspect);
            thumbnail_motion = reach_motion_clamp(&thumbnail_motion, 1, cap);
            if (reach_clipboard_clip_rect(&command.rect, local_visible_item))
            {
                reach_rect_motion image_bounds = {};
                reach_motion_set(&image_bounds.x, command.rect.x);
                reach_motion_set(&image_bounds.y, command.rect.y);
                image_bounds.width = thumbnail_motion;
                if (!retained_width)
                    reach_motion_set(&image_bounds.width, command.rect.width);
                reach_motion_set(&image_bounds.height, command.rect.height);
                reach_render_group source = {};
                source.id = feedback_id + 10;
                source.bounds = {0, 0, ceilf(retained_width ? thumbnail_width : command.rect.width),
                                 ceilf(command.rect.height)};
                float radius = command.radius;
                command.radius = 0;
                result = reach_render_push_image_motion(commands, &source, &image_bounds, radius,
                                                        &command);
                if (result != REACH_OK)
                    return result;
            }

            const float text_x = thumbnail_x + thumbnail_width + metrics.thumbnail_text_gap;
            result = push_text(text_x, theme->clipboard_secondary_text, &thumbnail_motion);
        }
        else
        {
            const float text_x = local_item_content.x + padding;
            result = push_text(text_x, theme->clipboard_primary_text, nullptr);
        }
        if (result != REACH_OK)
            return result;
    }

    if (reach_clipboard_layout_has_scrollbar(layout))
    {
        if (retained_width)
        {
            reach_render_group position = {};
            position.id = UINT64_C(0x7100000000000000);
            position.bounds = reach_clipboard_local(layout->scrollbar.track, layout->bounds);
            position.x = reach_motion_affine(&item_width, 1, -layout->item_width);
            result = reach_render_command_buffer_begin_group(commands, &position);
            if (result != REACH_OK)
                return result;
        }
        reach_scrollbar_build_render_commands(layout->scrollbar.track, layout->scrollbar.thumb,
                                              layout->bounds, theme->clipboard_scrollbar_track,
                                              theme->clipboard_scrollbar_thumb, commands);
        if (retained_width)
            reach_render_command_buffer_end_group(commands);
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

    reach_clipboard_render_input input = {};
    input.theme = theme;
    input.model = &state->model;
    input.layout = &state->layout;
    auto item_width = reach_clipboard_feature_item_width(clipboard);
    input.item_width = &item_width;
    input.hover_motions = reach_clipboard_feature_hover_motions(clipboard);
    input.hover_group_ids = reach_clipboard_feature_hover_group_ids(clipboard);
    input.dpi_scale = dpi_scale;
    input.text_alignment_leading = REACH_TEXT_ALIGNMENT_LEADING;

    return reach_clipboard_build_render_commands(&input, out_commands);
}
