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

static uint64_t reach_launcher_fallback_icon(int32_t is_directory)
{
    return is_directory ? REACH_VECTOR_ICON_FOLDER : REACH_VECTOR_ICON_FILE;
}

static void reach_launcher_copy_query_prefix(uint16_t *dst, size_t dst_count,
                                             const reach_ui_state *state)
{
    if (dst == nullptr || dst_count == 0 || state == nullptr)
    {
        return;
    }
    size_t count = state->launcher.caret_index < state->launcher.query_length
                       ? state->launcher.caret_index
                       : state->launcher.query_length;
    if (count + 1 > dst_count)
    {
        count = dst_count - 1;
    }
    for (size_t index = 0; index < count; ++index)
    {
        dst[index] = state->launcher.query[index];
    }
    dst[count] = 0;
}

reach_result reach_launcher_build_render_commands(const reach_launcher_render_input *input,
                                                  reach_render_command_buffer *out_commands)
{
    if (input == nullptr || input->theme == nullptr || input->state == nullptr ||
        input->layout == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_theme *theme = input->theme;
    const reach_ui_state *state = input->state;
    const reach_launcher_layout *layout = input->layout;
    reach_render_command_buffer_clear(out_commands);
    float launcher_radius = 10.0f;

    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = layout->search_box.x - layout->bounds.x;
    command.rect.y = layout->search_box.y - layout->bounds.y;
    command.rect.width = layout->search_box.width;
    command.rect.height = layout->search_box.height;
    command.color = theme->tray_popup_background;
    command.radius = launcher_radius;
    reach_render_command_buffer_push(out_commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE;
    command.rect.x = layout->search_box.x - layout->bounds.x + 0.5f;
    command.rect.y = layout->search_box.y - layout->bounds.y + 0.5f;
    command.rect.width = layout->search_box.width - 1.0f;
    command.rect.height = layout->search_box.height - 1.0f;
    command.color = theme->dock_border;
    command.color.a = 0.88f;
    command.radius = launcher_radius;
    command.stroke_width = 1.0f;
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
    reach_copy_utf16(command.text, 260,
                     state->launcher.query_length > 0 ? state->launcher.query
                                                      : (const uint16_t *)L"Search");
    reach_render_command_buffer_push(out_commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_TEXT_CARET;
    command.rect.x = layout->search_box.x - layout->bounds.x + 18.0f;
    command.rect.y = layout->search_box.y - layout->bounds.y + 14.0f;
    command.rect.width = layout->search_box.width - 36.0f;
    command.rect.height = layout->search_box.height - 28.0f;
    command.color = reach_launcher_rgb(255, 255, 255, 0.90f);
    command.text_size = 18.0f;
    command.text_alignment = input->text_alignment_leading;
    command.radius = 1.0f;
    reach_launcher_copy_query_prefix(command.text, 260, state);
    reach_render_command_buffer_push(out_commands, &command);

    if (state->launcher.result_count > 0)
    {
        float row_height = layout->search_results.height / (float)state->launcher.result_count;
        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect.x = layout->search_results.x - layout->bounds.x;
        command.rect.y = layout->search_results.y - layout->bounds.y;
        command.rect.width = layout->search_results.width;
        command.rect.height = layout->search_results.height;
        command.color = theme->tray_popup_background;
        command.color.a = 0.96f;
        command.radius = launcher_radius;
        reach_render_command_buffer_push(out_commands, &command);

        for (size_t index = 0;
             index < state->launcher.result_count && index < REACH_SEARCH_MAX_RESULTS; ++index)
        {
            float row_x = layout->search_results.x - layout->bounds.x;
            float row_y = layout->search_results.y - layout->bounds.y + row_height * (float)index;
            float row_width = layout->search_results.width;
            int32_t selected = index == state->launcher.selected_result_index;

            if (selected)
            {
                command = {};
                command.type = REACH_RENDER_COMMAND_RECT;
                command.rect.x = row_x + 6.0f;
                command.rect.y = row_y + 5.0f;
                command.rect.width = row_width - 12.0f;
                command.rect.height = row_height - 10.0f;
                command.color = reach_launcher_rgb(255, 255, 255, 0.14f);
                command.radius = 8.0f;
                reach_render_command_buffer_push(out_commands, &command);
            }

            float icon_size = 32.0f;
            float icon_x = row_x + 16.0f;
            float icon_y = row_y + 12.0f;
            uint64_t icon_id = state->launcher.result_icon_ids[index];

            if (icon_id != 0)
            {
                command = {};
                command.type = REACH_RENDER_COMMAND_ICON;
                command.rect.x = icon_x;
                command.rect.y = icon_y;
                command.rect.width = icon_size;
                command.rect.height = icon_size;
                command.icon_id = icon_id;
                command.color.a = 1.0f;
                command.radius = 0.0f;
                reach_render_command_buffer_push(out_commands, &command);
            }
            else
            {
                command = {};
                command.type = REACH_RENDER_COMMAND_RECT;
                command.rect.x = icon_x - 2.0f;
                command.rect.y = icon_y - 2.0f;
                command.rect.width = icon_size + 4.0f;
                command.rect.height = icon_size + 4.0f;
                command.color = reach_launcher_rgb(255, 255, 255, 0.10f);
                command.radius = 7.0f;
                reach_render_command_buffer_push(out_commands, &command);

                command = {};
                command.type = REACH_RENDER_COMMAND_VECTOR_ICON;
                command.rect.x = icon_x;
                command.rect.y = icon_y;
                command.rect.width = icon_size;
                command.rect.height = icon_size;
                command.color = reach_launcher_rgb(255, 255, 255, 0.78f);
                command.icon_id =
                    reach_launcher_fallback_icon(state->launcher.results[index].is_directory);
                reach_render_command_buffer_push(out_commands, &command);
            }

            command = {};
            command.type = REACH_RENDER_COMMAND_TEXT;
            command.rect.x = row_x + 62.0f;
            command.rect.y = row_y + 6.0f;
            command.rect.width = row_width - 78.0f;
            command.rect.height = 24.0f;
            command.color = reach_launcher_rgb(255, 255, 255, selected ? 0.96f : 0.86f);
            command.text_size = 15.0f;
            command.text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
            command.text_alignment = input->text_alignment_leading;
            command.text_ellipsis = 1;
            reach_copy_utf16(command.text, 260, state->launcher.results[index].name);
            reach_render_command_buffer_push(out_commands, &command);

            command = {};
            command.type = REACH_RENDER_COMMAND_TEXT;
            command.rect.x = row_x + 62.0f;
            command.rect.y = row_y + 28.0f;
            command.rect.width = row_width - 78.0f;
            command.rect.height = 20.0f;
            command.color = reach_launcher_rgb(255, 255, 255, selected ? 0.62f : 0.44f);
            command.text_size = 12.0f;
            command.text_alignment = input->text_alignment_leading;
            command.text_ellipsis = 1;
            reach_copy_utf16(command.text, 260, state->launcher.results[index].path);
            reach_render_command_buffer_push(out_commands, &command);
        }
    }

    return REACH_OK;
}
