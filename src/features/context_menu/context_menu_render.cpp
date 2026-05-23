#include "reach/features/context_menu.h"

static reach_color reach_context_menu_rgb(uint8_t r, uint8_t g, uint8_t b, float a)
{
    reach_color color = {};
    color.r = (float)r / 255.0f;
    color.g = (float)g / 255.0f;
    color.b = (float)b / 255.0f;
    color.a = a;
    return color;
}

reach_result reach_context_menu_build_render_commands(const reach_context_menu_render_input *input, reach_render_command_buffer *out_commands)
{
    if (input == nullptr || input->theme == nullptr || input->item_slots == nullptr || input->item_commands == nullptr || out_commands == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_render_command_buffer_clear(out_commands);
    reach_render_command command = {};
    float width = input->bounds.width;
    float height = input->bounds.height;
    float radius = 14.0f;
    float notch_width = 18.0f;
    float notch_height = 8.0f;
    float notch_center = width * 0.30f;
    reach_color context_border = input->theme->dock_border;
    context_border.a = 1.0f;
    if (input->use_anchor_x) {
        notch_center = input->anchor_x - input->bounds.x;
        if (notch_center < radius + notch_width) {
            notch_center = radius + notch_width;
        }
        if (notch_center > width - radius - notch_width) {
            notch_center = width - radius - notch_width;
        }
    } else if (input->has_layout &&
        input->dock_layout != nullptr &&
        input->target_index < input->dock_layout->app_slot_count) {
        reach_rect_f32 slot = input->dock_layout->app_slots[input->target_index];
        notch_center = slot.x + slot.width * 0.5f - input->bounds.x;
        if (notch_center < radius + notch_width) {
            notch_center = radius + notch_width;
        }
        if (notch_center > width - radius - notch_width) {
            notch_center = width - radius - notch_width;
        }
    }

    command.type = REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT;
    command.rect.x = 0.5f;
    command.rect.y = 0.5f;
    command.rect.width = width - 1.0f;
    command.rect.height = height - 1.0f;
    command.color = reach_context_menu_rgb(32, 30, 28, 0.96f);
    command.radius = radius;
    command.notch_center_x = notch_center;
    command.notch_width = notch_width;
    command.notch_height = notch_height;
    reach_render_command_buffer_push(out_commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT;
    command.rect.x = 0.5f;
    command.rect.y = 0.5f;
    command.rect.width = width - 1.0f;
    command.rect.height = height - 1.0f;
    command.color = context_border;
    command.radius = radius;
    command.stroke_width = 1.0f;
    command.notch_center_x = notch_center;
    command.notch_width = notch_width;
    command.notch_height = notch_height;
    reach_render_command_buffer_push(out_commands, &command);

    for (size_t index = 0; index < input->item_count; ++index) {
        reach_rect_f32 item = input->item_slots[index];
        item.x -= input->bounds.x;
        item.y -= input->bounds.y;

        if (input->hovered_index == index) {
            command = {};
            command.type = REACH_RENDER_COMMAND_RECT;
            command.rect = item;
            command.color = reach_context_menu_rgb(255, 255, 255, 0.12f);
            command.radius = 8.0f;
            reach_render_command_buffer_push(out_commands, &command);
        }

        command = {};
        command.type = REACH_RENDER_COMMAND_TEXT;
        float text_left = input->item_icon_ids != nullptr && input->item_icon_ids[index] != 0 ? 40.0f : 14.0f;
        if (input->item_icon_ids != nullptr && input->item_icon_ids[index] != 0) {
            reach_render_command icon_command = {};
            icon_command.type = REACH_RENDER_COMMAND_VECTOR_ICON;
            icon_command.rect.x = item.x + 13.0f;
            icon_command.rect.y = item.y + (item.height - 16.0f) * 0.5f;
            icon_command.rect.width = 16.0f;
            icon_command.rect.height = 16.0f;
            icon_command.color = reach_context_menu_rgb(232, 229, 224, 0.96f);
            icon_command.icon_id = input->item_icon_ids[index];
            reach_render_command_buffer_push(out_commands, &icon_command);
        }
        command.rect.x = item.x + text_left;
        command.rect.y = item.y;
        command.rect.width = item.width - text_left - 14.0f;
        command.rect.height = item.height;
        command.color = reach_context_menu_rgb(232, 229, 224, 0.96f);
        command.text_size = 14.0f;
        command.text_alignment = input->text_alignment_leading;
        command.text_ellipsis = 1;
        reach_copy_utf16(command.text, 260, reach_context_menu_command_text(input->item_commands[index]));
        reach_render_command_buffer_push(out_commands, &command);
    }

    return REACH_OK;
}
