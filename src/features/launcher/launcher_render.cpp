#include "reach/features/launcher.h"

static reach_color reach_launcher_rgb(uint8_t r, uint8_t g, uint8_t b, float a)
{
    reach_color color = {};
    color.r = (float)r / 255.0f;
    color.g = (float)g / 255.0f;
    color.b = (float)b / 255.0f;
    color.a = a;
    return color;
}

reach_result reach_launcher_build_render_commands(const reach_launcher_render_input *input, reach_render_command_buffer *out_commands)
{
    if (input == nullptr || input->theme == nullptr || input->state == nullptr || input->layout == nullptr || out_commands == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_theme *theme = input->theme;
    const reach_ui_state *state = input->state;
    const reach_launcher_layout *layout = input->layout;
    reach_render_command_buffer_clear(out_commands);

    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = layout->search_box.x - layout->bounds.x;
    command.rect.y = layout->search_box.y - layout->bounds.y;
    command.rect.width = layout->search_box.width;
    command.rect.height = layout->search_box.height;
    command.color = theme->tray_popup_background;
    command.radius = 10.0f;
    reach_render_command_buffer_push(out_commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect.x = layout->search_box.x - layout->bounds.x + 18.0f;
    command.rect.y = layout->search_box.y - layout->bounds.y;
    command.rect.width = layout->search_box.width - 36.0f;
    command.rect.height = layout->search_box.height;
    command.color.r = 1.0f;
    command.color.g = 1.0f;
    command.color.b = 1.0f;
    command.color.a = state->launcher.query_length > 0 ? 0.95f : 0.58f;
    command.text_size = 18.0f;
    command.text_alignment = input->text_alignment_leading;
    command.text_ellipsis = 1;
    reach_copy_utf16(command.text, 260, state->launcher.query_length > 0 ? state->launcher.query : (const uint16_t *)L"Search");
    reach_render_command_buffer_push(out_commands, &command);

    float cursor_left = command.rect.x + 1.0f;
    if (state->launcher.query_length > 0) {
        cursor_left += (float)state->launcher.query_length * 9.5f;
    }
    float cursor_max = layout->search_box.x - layout->bounds.x + layout->search_box.width - 20.0f;
    if (cursor_left > cursor_max) {
        cursor_left = cursor_max;
    }

    command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = cursor_left;
    command.rect.y = layout->search_box.y - layout->bounds.y + 14.0f;
    command.rect.width = 2.0f;
    command.rect.height = layout->search_box.height - 28.0f;
    command.color = reach_launcher_rgb(255, 255, 255, 0.90f);
    command.radius = 1.0f;
    reach_render_command_buffer_push(out_commands, &command);

    return REACH_OK;
}
