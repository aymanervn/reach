#include "reach/features/clipboard.h"

#include "clipboard_common.h"
#include "clipboard_metrics.h"
#include "reach/features/common/ui_controls.h"

#include <math.h>

static float reach_clipboard_max_float(float a, float b)
{
    return a > b ? a : b;
}

static float reach_clipboard_min_float(float a, float b)
{
    return a < b ? a : b;
}

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

reach_clipboard_frame reach_clipboard_panel_frame(const reach_clipboard_layout *layout,
                                                  const reach_motion *height,
                                                  int32_t content_follows)
{
    reach_clipboard_frame frame = {};
    if (layout == nullptr)
        return frame;
    float target = fmaxf(0.0f, layout->bounds.height);
    reach_motion panel_height = {};
    reach_motion_set(&panel_height, target);
    if (height != nullptr && height->generation != 0)
        panel_height = reach_motion_clamp(height, 0.0f, 16384.0f);
    float minimum = target, maximum = target;
    if (reach_motion_range(&panel_height, &minimum, &maximum) != REACH_OK)
    {
        reach_motion_set(&panel_height, target);
        maximum = target;
    }
    frame.envelope_height = ceilf(fmaxf(fmaxf(target, maximum), layout->max_height));
    frame.origin = {layout->bounds.x, layout->bounds.y + target - frame.envelope_height,
                    layout->bounds.width, frame.envelope_height};
    reach_motion_set(&frame.panel.x, 0.0f);
    reach_motion_set(&frame.panel.width, layout->bounds.width);
    frame.panel.height = panel_height;
    frame.panel.y = reach_motion_affine(&panel_height, -1.0f, frame.envelope_height);
    reach_motion_set(&frame.content_y, 0.0f);
    if (content_follows && height != nullptr && height->generation != 0)
        frame.content_y = reach_motion_affine(&panel_height, -1.0f, target);
    return frame;
}

static reach_result reach_clipboard_push_row(const reach_clipboard_render_input *input,
                                             const reach_clipboard_metrics &metrics,
                                             reach_rect_f32 origin, size_t slot, uint64_t parent_id,
                                             reach_render_command_buffer *commands)
{
    const reach_theme *theme = input->theme;
    const reach_clipboard_layout *layout = input->layout;
    const reach_keyed_row &row = input->rows[slot];
    const reach_clipboard_item *item_data = &input->row_items[slot];
    const float border_thickness = reach_theme_border_thickness(theme, input->dpi_scale);
    const float item_radius = reach_clipboard_scale_value(theme->radius_small, input->dpi_scale);
    reach_rect_f32 viewport = reach_clipboard_local(layout->viewport, origin);
    reach_rect_f32 card = {viewport.x, viewport.y + row.y.to, layout->item_width,
                           layout->item_large_size};
    reach_rect_f32 text_card = card;
    text_card.width = layout->item_text_width;
    if (card.width <= 0.0f || card.height <= 0.0f || text_card.width <= 0.0f)
        return REACH_OK;
    reach_motion width = {};
    reach_motion_set(&width, card.width);
    if (input->item_width.generation != 0)
        width = reach_motion_clamp(&input->item_width, fminf(card.width, text_card.width),
                                   viewport.width);
    uint64_t id = UINT64_C(0x7200000000000100) + slot * 32;

    reach_render_group container = {};
    container.id = id;
    container.parent_id = parent_id;
    container.bounds = card;
    container.bounds.width = fmaxf(card.width, viewport.width);
    container.opacity = row.opacity;
    container.y = reach_motion_affine(&row.y, 1.0f, -row.y.to);
    reach_result result = reach_render_command_buffer_begin_group(commands, &container);
    reach_render_command_buffer_end_group(commands);
    if (result != REACH_OK)
        return result;
    reach_motion hover = {};
    reach_motion_set(&hover, 0.0f);
    if (input->row_hover != nullptr && input->row_hover[slot].generation != 0)
        hover = reach_motion_clamp(&input->row_hover[slot], 0.0f, 1.0f);
    reach_rect_motion card_bounds = {};
    reach_motion_set(&card_bounds.x, card.x);
    reach_motion_set(&card_bounds.y, card.y);
    card_bounds.width = width;
    reach_motion_set(&card_bounds.height, card.height);
    commands->current_group_id = id;
    result = reach_ui_bordered_motion_color_feedback_render(
        commands, id + 1, &card_bounds, item_radius, border_thickness,
        theme->clipboard_item_background, theme->accent_fill, theme->accent_fill, &hover);
    commands->current_group_id = 0;
    if (result != REACH_OK)
        return result;

    const float item_border = fminf(border_thickness, fminf(card.width, card.height) * 0.5f);
    const reach_rect_f32 close_rect = {card.x + card.width - item_border -
                                           metrics.close_button_size - metrics.close_button_margin,
                                       card.y + item_border + metrics.close_button_margin,
                                       metrics.close_button_size, metrics.close_button_size};
    reach_render_group close_position = {};
    close_position.id = id + 12;
    close_position.parent_id = id;
    close_position.bounds = close_rect;
    close_position.x = reach_motion_affine(&width, 1.0f, -card.width);
    result = reach_render_command_buffer_begin_group(commands, &close_position);
    reach_render_command_buffer_end_group(commands);
    if (result != REACH_OK)
        return result;

    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = close_rect;
    command.color = theme->clipboard_secondary_text;
    command.color.a = 1;
    command.radius = item_radius;
    reach_motion close_opacity = reach_motion_affine(&hover, metrics.close_button_hover_alpha, 0);
    reach_render_group placement = {};
    placement.parent_id = close_position.id;
    if (!reach_render_retained_command_bounds(&command, &placement.bounds))
        return REACH_ERROR;
    result =
        reach_ui_feedback_begin(commands, id + 13, placement.bounds, &close_opacity, &placement);
    if (result == REACH_OK)
        result = reach_render_command_buffer_push(commands, &command);
    reach_render_command_buffer_end_group(commands);
    if (result != REACH_OK)
        return result;

    const float inset = metrics.close_button_size * metrics.close_button_inset_ratio;
    command = {};
    command.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    command.rect = {close_rect.x + inset, close_rect.y + inset,
                    fmaxf(0.0f, close_rect.width - inset * 2.0f),
                    fmaxf(0.0f, close_rect.height - inset * 2.0f)};
    command.icon_id = REACH_VECTOR_ICON_CLOSE;
    command.color = theme->clipboard_secondary_text;
    command.color.a = 1;
    reach_motion glyph_opacity = reach_motion_affine(
        &hover, theme->clipboard_secondary_text.a * 0.5f, theme->clipboard_secondary_text.a * 0.5f);
    if (!reach_render_retained_command_bounds(&command, &placement.bounds))
        return REACH_ERROR;
    result =
        reach_ui_feedback_begin(commands, id + 14, placement.bounds, &glyph_opacity, &placement);
    if (result == REACH_OK)
        result = reach_render_command_buffer_push(commands, &command);
    reach_render_command_buffer_end_group(commands);
    if (result != REACH_OK)
        return result;

    const reach_rect_f32 content =
        reach_theme_border_content_rect(theme, input->dpi_scale, text_card);
    const float padding = metrics.thumbnail_padding;
    const float text_right =
        content.x + content.width - metrics.close_button_size - metrics.close_button_margin * 2.0f;
    float text_x = content.x + padding;
    reach_color text_color = theme->clipboard_text;
    if (item_data->kind == REACH_CLIPBOARD_ITEM_IMAGE && item_data->thumbnail_id != 0)
    {
        const float thumbnail_height = fminf(metrics.thumbnail_height, content.height);
        const float thumbnail_width = reach_clipboard_thumbnail_width_for_item(
            item_data, thumbnail_height,
            fmaxf(0.0f, content.width * metrics.thumbnail_max_width_ratio));
        reach_rect_f32 image = {content.x + padding,
                                content.y + fmaxf(0.0f, content.height - thumbnail_height) * 0.5f,
                                thumbnail_width, thumbnail_height};
        if (thumbnail_width > 0.0f && thumbnail_height > 0.0f)
        {
            command = {};
            command.type = REACH_RENDER_COMMAND_ICON;
            command.rect = image;
            command.icon_id = item_data->thumbnail_id;
            command.icon_crop_to_fill = 1;
            command.color.a = 1.0f;
            reach_rect_motion image_bounds = {};
            reach_motion_set(&image_bounds.x, image.x);
            reach_motion_set(&image_bounds.y, image.y);
            reach_motion_set(&image_bounds.width, image.width);
            reach_motion_set(&image_bounds.height, image.height);
            reach_render_group source = {};
            source.id = id + 16;
            source.parent_id = id;
            source.bounds = {0, 0, ceilf(image.width), ceilf(image.height)};
            result = reach_render_push_image_motion(commands, &source, &image_bounds, item_radius,
                                                    &command);
            if (result != REACH_OK)
                return result;
        }
        text_x = image.x + image.width + metrics.thumbnail_text_gap;
        text_color = theme->clipboard_secondary_text;
    }

    reach_rect_f32 text_rect = {text_x, content.y + padding, fmaxf(0.0f, text_right - text_x),
                                fmaxf(0.0f, content.height - padding * 2.0f)};
    reach_render_command lines[REACH_CLIPBOARD_PREVIEW_LINE_LIMIT] = {};
    size_t count = reach_clipboard_preview_commands(item_data, text_rect, text_card, text_color,
                                                    metrics.items_font_size,
                                                    input->text_alignment_leading, lines);
    if (count == 0)
        return REACH_OK;
    reach_render_group text = {};
    text.id = id + 20;
    text.parent_id = id;
    text.bounds = text_card;
    result = reach_render_command_buffer_begin_group(commands, &text);
    for (size_t index = 0; result == REACH_OK && index < count; ++index)
        result = reach_render_command_buffer_push(commands, &lines[index]);
    reach_render_command_buffer_end_group(commands);
    return result;
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
    const float border_thickness = reach_theme_border_thickness(theme, input->dpi_scale);
    const float radius = reach_clipboard_scale_value(theme->radius_large, input->dpi_scale);
    reach_render_command_buffer_clear(commands);

    reach_clipboard_frame frame =
        reach_clipboard_panel_frame(layout, &input->height, input->content_follows);
    const reach_rect_f32 origin = {layout->bounds.x,
                                   layout->bounds.y + layout->bounds.height - frame.envelope_height,
                                   layout->bounds.width, frame.envelope_height};
    const float width = layout->bounds.width;
    const float height = frame.envelope_height;
    if (!(width > 0.0f) || !(height > 0.0f))
        return REACH_OK;

    reach_render_group anchor = {};
    anchor.id = UINT64_C(0x7200000000000009);
    anchor.bounds = {0.0f, 0.0f, width, height};
    anchor.y = frame.panel.y;
    reach_result result = reach_render_command_buffer_begin_group(commands, &anchor);
    reach_render_command_buffer_end_group(commands);
    if (result != REACH_OK)
        return result;
    reach_rect_motion background = frame.panel;
    reach_motion_set(&background.y, 0.0f);
    result = reach_ui_motion_background_render(
        commands, &background, radius, border_thickness, theme->clipboard_background,
        theme->clipboard_border, &theme->popup_shadow, input->dpi_scale,
        UINT64_C(0x7200000000000000), anchor.id, theme->glass);
    if (result != REACH_OK)
        return result;
    reach_render_group interior = {};
    interior.id = UINT64_C(0x7200000000000010);
    interior.bounds = {0.0f, 0.0f, width, height};
    interior.clip = interior.bounds;
    interior.has_clip = 1;
    interior.clip_radius = reach_nested_radius(radius, border_thickness);
    interior.clip_uniform_radius = interior.clip_radius > 0.0f ? 1 : 0;
    reach_motion_set(&interior.clip_insets[REACH_RENDER_CLIP_LEFT], border_thickness);
    reach_motion_set(&interior.clip_insets[REACH_RENDER_CLIP_RIGHT], border_thickness);
    reach_motion_set(&interior.clip_insets[REACH_RENDER_CLIP_BOTTOM], border_thickness);
    interior.clip_insets[REACH_RENDER_CLIP_TOP] =
        reach_motion_affine(&frame.panel.height, -1.0f, height + border_thickness);
    result = reach_render_command_buffer_begin_group(commands, &interior);
    reach_render_command_buffer_end_group(commands);
    if (result != REACH_OK)
        return result;

    reach_render_group content = {};
    content.id = UINT64_C(0x7200000000000012);
    content.parent_id = interior.id;
    content.bounds = interior.bounds;
    content.y = frame.content_y;
    result = reach_render_command_buffer_begin_group(commands, &content);
    reach_render_command_buffer_end_group(commands);
    if (result != REACH_OK)
        return result;

    reach_render_group chrome = {};
    chrome.id = UINT64_C(0x7200000000000011);
    chrome.parent_id = content.id;
    chrome.bounds = interior.bounds;
    result = reach_render_command_buffer_begin_group(commands, &chrome);
    if (result != REACH_OK)
        return result;

    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = reach_clipboard_local(layout->title, origin);
    command.color = theme->clipboard_text;
    command.text_size = metrics.title_font_size;
    command.text_weight = metrics.title_font_weight;
    command.text_alignment = input->text_alignment_leading;
    reach_copy_utf16(command.text, 260, (const uint16_t *)L"Clipboard");
    result = reach_render_command_buffer_push(commands, &command);

    const size_t item_count = reach_clipboard_count_clamped(model);
    if (result == REACH_OK && item_count == 0 && layout->viewport.width > 0.0f &&
        layout->viewport.height > 0.0f)
    {
        const float empty_text_height = fmaxf(1.0f, metrics.items_font_size * 1.5f);
        reach_rect_f32 empty_text_rect = layout->viewport;
        empty_text_rect.y += fmaxf(0.0f, empty_text_rect.height - empty_text_height) * 0.5f;
        empty_text_rect.height = empty_text_height;
        command = {};
        command.type = REACH_RENDER_COMMAND_TEXT;
        command.rect = reach_clipboard_local(empty_text_rect, origin);
        command.color = theme->clipboard_empty_text;
        command.text_size = metrics.items_font_size;
        command.text_alignment = REACH_TEXT_ALIGNMENT_CENTER;
        reach_copy_utf16(command.text, 260, (const uint16_t *)L"Nothing in the clipboard");
        result = reach_render_command_buffer_push(commands, &command);
    }
    if (result == REACH_OK && layout->clear_button.width > 0.0f &&
        layout->clear_button.height > 0.0f)
    {
        const int32_t clear_enabled = item_count > 0;
        const reach_rect_f32 clear_button = reach_clipboard_local(layout->clear_button, origin);
        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect = clear_button;
        command.color = clear_enabled ? theme->clipboard_clear_button_background
                                      : theme->clipboard_clear_button_disabled_background;
        command.radius = metrics.clear_button_radius;
        result = reach_render_command_buffer_push(commands, &command);

        command = {};
        command.type = REACH_RENDER_COMMAND_TEXT;
        command.rect = clear_button;
        command.color =
            clear_enabled ? theme->clipboard_secondary_text : theme->clipboard_disabled_text;
        command.text_size = metrics.items_font_size;
        command.text_alignment = REACH_TEXT_ALIGNMENT_CENTER;
        reach_copy_utf16(command.text, 260, (const uint16_t *)L"Clear");
        if (result == REACH_OK)
            result = reach_render_command_buffer_push(commands, &command);
    }
    reach_render_command_buffer_end_group(commands);
    if (result != REACH_OK)
        return result;

    if (layout->viewport.width <= 0.0f || layout->viewport.height <= 0.0f)
        return REACH_OK;

    reach_scroll_view_group viewport = {};
    viewport.id = UINT64_C(0x7200000000000020);
    viewport.parent_id = content.id;
    viewport.bounds = reach_clipboard_local(layout->viewport, origin);
    viewport.bounds.height += frame.envelope_height - layout->bounds.height;
    result = reach_scroll_view_push_viewport(&model->scroll, &viewport, commands);
    if (result != REACH_OK)
        return result;

    reach_motion opaque = {};
    reach_motion_set(&opaque, 1.0f);
    reach_scroll_view_group scrollbar = {};
    scrollbar.id = UINT64_C(0x7200000000000028);
    scrollbar.parent_id = viewport.id;
    scrollbar.opacity = &opaque;
    result = reach_scroll_view_push_scrollbar(
        &model->scroll, origin,
        {theme->clipboard_scrollbar_track, theme->clipboard_scrollbar_thumb}, &scrollbar,
        reach_motion_now_seconds(), commands);
    if (result != REACH_OK)
        return result;

    for (size_t slot = 0; input->rows != nullptr && input->row_items != nullptr &&
                          slot < REACH_CLIPBOARD_ROW_CAPACITY;
         ++slot)
    {
        if (input->rows[slot].lifecycle == REACH_KEYED_ROW_EMPTY)
            continue;
        result = reach_clipboard_push_row(input, metrics, origin, slot, viewport.id + 1, commands);
        if (result != REACH_OK)
            return result;
    }
    return REACH_OK;
}
