#include "reach/features/tray.h"

#include <math.h>

static reach_color reach_tray_rgb(uint8_t r, uint8_t g, uint8_t b, float a)
{
    reach_color color = {};
    color.r = (float)r / 255.0f;
    color.g = (float)g / 255.0f;
    color.b = (float)b / 255.0f;
    color.a = a;
    return color;
}

reach_result reach_tray_build_render_commands(const reach_tray_render_input *input, reach_render_command_buffer *out_commands)
{
    if (input == nullptr || input->theme == nullptr || input->model == nullptr || out_commands == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_render_command_buffer_clear(out_commands);
    const reach_theme *theme = input->theme;
    reach_render_command command = {};
    float icon_box_radius = reach_theme_icon_box_corner_radius(theme, reach_theme_tray_slot_size(theme, input->dock_height));
    reach_popup_background_input popup = {};
    popup.theme = theme;
    popup.bounds = input->bounds;
    popup.notch_center_x = input->bounds.width * 0.5f;
    reach_result popup_result = reach_popup_push_background(&popup, out_commands);
    if (popup_result != REACH_OK) {
        return popup_result;
    }

    for (size_t index = 0; index < input->model->item_count; ++index) {
        reach_rect_f32 slot = input->model->item_slots[index];
        slot.x -= input->bounds.x;
        slot.y -= input->bounds.y;
        float icon_size = floorf(slot.height * 0.86f);
        if (icon_size < 16.0f && slot.height >= 16.0f) {
            icon_size = 16.0f;
        }
        float icon_x = slot.x + (slot.width - icon_size) * 0.5f;
        float icon_y = slot.y + (slot.height - icon_size) * 0.5f;

        if (input->model->items[index].icon_id != 0) {
            command = {};
            command.type = REACH_RENDER_COMMAND_ICON;
            command.rect.x = icon_x;
            command.rect.y = icon_y;
            command.rect.width = icon_size;
            command.rect.height = icon_size;
            command.icon_id = input->model->items[index].icon_id;
            command.color.a = 1.0f;
            command.radius = 0.0f;
            reach_render_command_buffer_push(out_commands, &command);
        } else {
            command = {};
            command.type = REACH_RENDER_COMMAND_TEXT;
            command.rect = slot;
            command.color = theme->fallback_icon_text;
            command.text_alignment = input->text_alignment_center;
            command.text[0] = input->model->items[index].title[0] != 0 ? input->model->items[index].title[0] : '?';
            command.text[1] = 0;
            reach_render_command_buffer_push(out_commands, &command);
        }

        if (input->click_feedback_index == index && input->click_feedback_opacity > 0.001f) {
            command = {};
            command.type = REACH_RENDER_COMMAND_RECT;
            command.rect = slot;
            command.color.r = 0.0f;
            command.color.g = 0.0f;
            command.color.b = 0.0f;
            command.color.a = input->click_feedback_opacity;
            command.radius = icon_box_radius;
            reach_render_command_buffer_push(out_commands, &command);
        }
    }

    return REACH_OK;
}
