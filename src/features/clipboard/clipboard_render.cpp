#include "reach/features/clipboard.h"

static float reach_clipboard_render_scale(const reach_clipboard_render_input *input, float value)
{
    return value * (input->dpi_scale > 0.0f ? input->dpi_scale : 1.0f);
}

static reach_rect_f32 reach_clipboard_local(reach_rect_f32 rect, reach_rect_f32 bounds)
{
    rect.x -= bounds.x;
    rect.y -= bounds.y;
    return rect;
}

static int32_t reach_clipboard_inside(reach_rect_f32 a, reach_rect_f32 b)
{
    return a.x >= b.x && a.x + a.width <= b.x + b.width && a.y >= b.y &&
           a.y + a.height <= b.y + b.height;
}

reach_result reach_clipboard_build_render_commands(const reach_clipboard_render_input *input,
                                                   reach_render_command_buffer *commands)
{
    if (input == nullptr || input->theme == nullptr || input->model == nullptr ||
        input->layout == nullptr || commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    const reach_theme *theme = input->theme;
    const reach_clipboard_model *model = input->model;
    const reach_clipboard_layout *layout = input->layout;
    reach_render_command_buffer_clear(commands);

    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = {0.0f, 0.0f, layout->bounds.width, layout->bounds.height};
    command.color = theme->clipboard_background;
    command.radius = reach_clipboard_render_scale(input, theme->clipboard_panel_radius);
    reach_render_command_buffer_push(commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE;
    command.rect = {0.5f, 0.5f, layout->bounds.width - 1.0f, layout->bounds.height - 1.0f};
    command.color = theme->clipboard_border;
    command.radius = reach_clipboard_render_scale(input, theme->clipboard_panel_radius);
    command.stroke_width = theme->border_thickness;
    reach_render_command_buffer_push(commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = reach_clipboard_local(layout->title, layout->bounds);
    command.color = theme->clipboard_primary_text;
    command.text_size = reach_clipboard_render_scale(input, theme->clipboard_title_text_size);
    command.text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
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

    for (size_t index = 0; index < model->count; ++index)
    {
        reach_rect_f32 item = layout->items[index];
        if (!reach_clipboard_inside(item, layout->viewport))
        {
            continue;
        }
        reach_rect_f32 local_item = reach_clipboard_local(item, layout->bounds);
        float hover = input->hover_values != nullptr ? input->hover_values[index] : 0.0f;

        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect = local_item;
        command.color = theme->clipboard_item_background;
        command.color.a +=
            (theme->clipboard_item_hover_background.a - command.color.a) * hover;
        command.radius = reach_clipboard_render_scale(input, theme->clipboard_item_radius);
        reach_render_command_buffer_push(commands, &command);

        command = {};
        command.type = REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE;
        command.rect = {local_item.x + 0.5f, local_item.y + 0.5f, local_item.width - 1.0f,
                        local_item.height - 1.0f};
        command.color = theme->clipboard_item_hover_border;
        command.color.a *= hover;
        command.radius = reach_clipboard_render_scale(input, theme->clipboard_item_radius);
        command.stroke_width = theme->border_thickness;
        reach_render_command_buffer_push(commands, &command);

        {
            float close_size = reach_clipboard_render_scale(input, 20.0f);
            float close_margin = reach_clipboard_render_scale(input, 6.0f);
            reach_rect_f32 close_rect = {local_item.x + local_item.width - close_size - close_margin,
                                         local_item.y + close_margin,
                                         close_size,
                                         close_size};
            float close_hover_alpha = 0.12f;
            command = {};
            command.type = REACH_RENDER_COMMAND_RECT;
            command.rect = close_rect;
            command.color.a = close_hover_alpha * hover;
            command.radius = reach_clipboard_render_scale(input, 4.0f);
            reach_render_command_buffer_push(commands, &command);

            float inset = close_size * 0.18f;
            command = {};
            command.type = REACH_RENDER_COMMAND_VECTOR_ICON;
            command.rect = {close_rect.x + inset, close_rect.y + inset,
                            close_rect.width - inset * 2.0f,
                            close_rect.height - inset * 2.0f};
            command.icon_id = REACH_VECTOR_ICON_CLOSE;
            float text_color_r = theme->clipboard_secondary_text.r;
            float text_color_g = theme->clipboard_secondary_text.g;
            float text_color_b = theme->clipboard_secondary_text.b;
            command.color.r = text_color_r;
            command.color.g = text_color_g;
            command.color.b = text_color_b;
            command.color.a = theme->clipboard_secondary_text.a * (0.5f + 0.5f * hover);
            reach_render_command_buffer_push(commands, &command);
        }

        const reach_clipboard_item *item_data = &model->items[index];
        float padding = reach_clipboard_render_scale(input, 12.0f);
        if (item_data->kind == REACH_CLIPBOARD_ITEM_IMAGE && item_data->thumbnail_id != 0)
        {
            float thumbnail = local_item.height - padding * 2.0f;
            command = {};
            command.type = REACH_RENDER_COMMAND_ICON;
            command.rect = {local_item.x + padding, local_item.y + padding, thumbnail, thumbnail};
            command.icon_id = item_data->thumbnail_id;
            command.icon_crop_to_fill = 1;
            command.color.a = 1.0f;
            command.radius = reach_clipboard_render_scale(input, 6.0f);
            reach_render_command_buffer_push(commands, &command);

            command = {};
            command.type = REACH_RENDER_COMMAND_TEXT;
            command.rect = {local_item.x + padding + thumbnail +
                                reach_clipboard_render_scale(input, 12.0f),
                            local_item.y + padding, local_item.width - thumbnail - padding * 3.0f,
                            local_item.height - padding * 2.0f};
            command.color = theme->clipboard_secondary_text;
            command.text_size =
                reach_clipboard_render_scale(input, theme->clipboard_body_text_size);
            command.text_alignment = input->text_alignment_leading;
            command.text_ellipsis = 1;
            reach_copy_utf16(command.text, 260, item_data->preview);
            reach_render_command_buffer_push(commands, &command);
        }
        else
        {
            command = {};
            command.type = REACH_RENDER_COMMAND_TEXT;
            command.rect = {local_item.x + padding, local_item.y + padding,
                            local_item.width - padding * 2.0f,
                            local_item.height - padding * 2.0f};
            command.color = theme->clipboard_primary_text;
            command.text_size =
                reach_clipboard_render_scale(input, theme->clipboard_body_text_size);
            command.text_alignment = input->text_alignment_leading;
            command.text_ellipsis = 1;
            reach_copy_utf16(command.text, 260, item_data->preview);
            reach_render_command_buffer_push(commands, &command);
        }
    }

    if (layout->scrollbar.track.height > 0.0f)
    {
        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect = reach_clipboard_local(layout->scrollbar.track, layout->bounds);
        command.color = theme->clipboard_scrollbar_track;
        command.radius = command.rect.width * 0.5f;
        reach_render_command_buffer_push(commands, &command);

        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect = reach_clipboard_local(layout->scrollbar.thumb, layout->bounds);
        command.color = theme->clipboard_scrollbar_thumb;
        command.radius = command.rect.width * 0.5f;
        reach_render_command_buffer_push(commands, &command);
    }
    return REACH_OK;
}
