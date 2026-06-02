#include "reach/features/switcher.h"

static reach_color reach_switcher_rgb(uint8_t r, uint8_t g, uint8_t b, float a)
{
    reach_color color = {};
    color.r = (float)r / 255.0f;
    color.g = (float)g / 255.0f;
    color.b = (float)b / 255.0f;
    color.a = a;
    return color;
}

reach_result reach_switcher_build_render_commands(const reach_switcher_render_input *input,
                                                  reach_render_command_buffer *out_commands)
{
    if (input == nullptr || input->theme == nullptr || input->model == nullptr ||
        input->items == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_render_command_buffer_clear(out_commands);

    reach_render_command command = {};
    float radius = 20.0f;
    float padding = 24.0f;
    float item_size = 112.0f;
    float icon_box_size = 88.0f;
    float gap = 14.0f;
    const reach_theme *theme = input->theme;
    float icon_box_radius = reach_theme_icon_box_corner_radius(theme, icon_box_size);
    size_t visible_count = reach_switcher_visible_count(input->model->window_count);

    command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = 0.5f;
    command.rect.y = 0.5f;
    command.rect.width = input->bounds.width - 1.0f;
    command.rect.height = input->bounds.height - 1.0f;
    command.color = theme->switcher_background;
    command.radius = radius;
    reach_render_command_buffer_push(out_commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE;
    command.rect.x = 0.5f;
    command.rect.y = 0.5f;
    command.rect.width = input->bounds.width - 1.0f;
    command.rect.height = input->bounds.height - 1.0f;
    command.color = theme->dock_border;
    command.radius = radius;
    command.stroke_width = 1.0f;
    reach_render_command_buffer_push(out_commands, &command);

    if (visible_count > 0)
    {
        float total_width = (float)visible_count * item_size + (float)(visible_count - 1) * gap;
        float x = (input->bounds.width - total_width) * 0.5f;
        if (x < padding)
        {
            x = padding;
        }
        float y = (input->bounds.height - item_size) * 0.5f;
        for (size_t visible_index = 0; visible_index < visible_count; ++visible_index)
        {
            size_t index = input->model->visible_start + visible_index;
            if (index >= input->model->window_count || index >= input->item_count)
            {
                break;
            }
            reach_rect_f32 item = {x + (float)visible_index * (item_size + gap), y, item_size,
                                   item_size};
            int32_t selected = index == input->model->selected_index;
            float box_x = item.x + (item.width - icon_box_size) * 0.5f;
            float box_y = item.y + 4.0f;
            reach_icon_handle icon = input->items[index].icon;

            if (selected)
            {
                command = {};
                command.type = REACH_RENDER_COMMAND_RECT;
                command.rect.x = box_x - 5.0f;
                command.rect.y = box_y - 5.0f;
                command.rect.width = icon_box_size + 10.0f;
                command.rect.height = icon_box_size + 10.0f;
                command.color = reach_switcher_rgb(255, 255, 255, 0.34f);
                command.radius = icon_box_radius + 5.0f;
                reach_render_command_buffer_push(out_commands, &command);
            }

            if (icon.id != 0)
            {
                command = {};
                command.type = REACH_RENDER_COMMAND_ICON;
                command.rect.x = box_x;
                command.rect.y = box_y;
                command.rect.width = icon_box_size;
                command.rect.height = icon_box_size;
                command.radius = 0.0f;
                command.color.a = 1.0f;
                command.icon_id = icon.id;
                reach_render_command_buffer_push(out_commands, &command);
            }

            if (selected)
            {
                command = {};
                command.type = REACH_RENDER_COMMAND_TEXT;
                command.rect.x = item.x;
                command.rect.y = item.y + 104.0f;
                command.rect.width = item.width;
                command.rect.height = 20.0f;
                command.color = reach_switcher_rgb(242, 240, 236, 0.96f);
                command.text_weight = input->text_weight_demi_bold;
                command.text_alignment = input->text_alignment_center;
                command.text_size = 13.0f;
                command.text_ellipsis = 1;
                reach_copy_utf16(command.text, 260,
                                 input->items[index].label[0] != 0 ? input->items[index].label
                                                                   : (const uint16_t *)L"App");
                reach_render_command_buffer_push(out_commands, &command);
            }
        }
    }

    return REACH_OK;
}
