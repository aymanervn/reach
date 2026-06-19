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

static reach_color reach_launcher_opaque(reach_color color)
{
    color.a = 1.0f;
    return color;
}

static uint64_t reach_launcher_fallback_icon(int32_t is_directory)
{
    return is_directory ? REACH_VECTOR_ICON_FOLDER : REACH_VECTOR_ICON_FILE;
}

static float reach_launcher_scale(const reach_launcher_render_input *input, float value)
{
    float scale = input != nullptr && input->dpi_scale > 0.0f ? input->dpi_scale : 1.0f;
    return value * scale;
}

static size_t reach_launcher_visible_result_count(const reach_ui_state *state)
{
    if (state == nullptr)
    {
        return 0;
    }
    return state->launcher.result_count < REACH_SEARCH_VISIBLE_RESULTS
               ? state->launcher.result_count
               : REACH_SEARCH_VISIBLE_RESULTS;
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
    float launcher_radius = reach_launcher_scale(input, 10.0f);
    float row_selected_inset_x = reach_launcher_scale(input, 6.0f);
    float row_selected_inset_y = reach_launcher_scale(input, 5.0f);
    float row_selected_radius = reach_launcher_scale(input, 8.0f);
    float row_icon_size = reach_launcher_scale(input, 32.0f);
    float row_icon_x = reach_launcher_scale(input, 16.0f);
    float row_icon_y = reach_launcher_scale(input, 12.0f);
    float row_fallback_icon_padding = reach_launcher_scale(input, 2.0f);
    float row_fallback_icon_radius = reach_launcher_scale(input, 7.0f);
    float row_text_x = reach_launcher_scale(input, 62.0f);
    float row_text_right_padding = reach_launcher_scale(input, 16.0f);
    float row_title_y = reach_launcher_scale(input, 6.0f);
    float row_title_height = reach_launcher_scale(input, 24.0f);
    float row_title_size = reach_launcher_scale(input, 15.0f);
    float row_path_y = reach_launcher_scale(input, 28.0f);
    float row_path_height = reach_launcher_scale(input, 20.0f);
    float row_path_size = reach_launcher_scale(input, 12.0f);

    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = layout->search_box.x - layout->bounds.x;
    command.rect.y = layout->search_box.y - layout->bounds.y;
    command.rect.width = layout->search_box.width;
    command.rect.height = layout->search_box.height;
    command.color = reach_launcher_opaque(theme->launcher_search_background);
    command.radius = launcher_radius;
    reach_render_command_buffer_push(out_commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE;
    command.rect.x = layout->search_box.x - layout->bounds.x + 0.5f;
    command.rect.y = layout->search_box.y - layout->bounds.y + 0.5f;
    command.rect.width = layout->search_box.width - 1.0f;
    command.rect.height = layout->search_box.height - 1.0f;
    command.color = theme->launcher_search_border;
    command.radius = launcher_radius;
    command.stroke_width = theme->border_thickness;
    reach_render_command_buffer_push(out_commands, &command);

    if (state->launcher.result_count > 0)
    {
        size_t visible_count = reach_launcher_visible_result_count(state);
        float row_height =
            visible_count > 0 ? layout->search_results.height / (float)visible_count : 0.0f;
        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect.x = layout->search_results.x - layout->bounds.x;
        command.rect.y = layout->search_results.y - layout->bounds.y;
        command.rect.width = layout->search_results.width;
        command.rect.height = layout->search_results.height;
        command.color = theme->dark_background;
        command.color.a = 0.96f;
        command.radius = launcher_radius;
        reach_render_command_buffer_push(out_commands, &command);

        if (layout->search_result_scrollbar_track.height > 0.0f)
        {
            command = {};
            command.type = REACH_RENDER_COMMAND_RECT;
            command.rect.x = layout->search_result_scrollbar_track.x - layout->bounds.x;
            command.rect.y = layout->search_result_scrollbar_track.y - layout->bounds.y;
            command.rect.width = layout->search_result_scrollbar_track.width;
            command.rect.height = layout->search_result_scrollbar_track.height;
            command.color = reach_launcher_rgb(255, 255, 255, 0.16f);
            command.radius = command.rect.width * 0.5f;
            reach_render_command_buffer_push(out_commands, &command);

            command = {};
            command.type = REACH_RENDER_COMMAND_RECT;
            command.rect.x = layout->search_result_scrollbar_thumb.x - layout->bounds.x;
            command.rect.y = layout->search_result_scrollbar_thumb.y - layout->bounds.y;
            command.rect.width = layout->search_result_scrollbar_thumb.width;
            command.rect.height = layout->search_result_scrollbar_thumb.height;
            command.color = reach_launcher_rgb(255, 255, 255, 0.72f);
            command.radius = command.rect.width * 0.5f;
            reach_render_command_buffer_push(out_commands, &command);
        }

        size_t start = state->launcher.result_scroll_offset;
        if (start > state->launcher.result_count)
        {
            start = state->launcher.result_count;
        }
        size_t end = start + visible_count;
        if (end > state->launcher.result_count)
        {
            end = state->launcher.result_count;
        }

        for (size_t index = start; index < end && index < REACH_SEARCH_MAX_RESULTS; ++index)
        {
            size_t visible_index = index - start;
            float row_x = layout->search_result_items.x - layout->bounds.x;
            float row_y = layout->search_result_items.y - layout->bounds.y +
                          row_height * (float)visible_index;
            float row_width = layout->search_result_items.width;
            int32_t selected = index == state->launcher.selected_result_index;

            if (selected)
            {
                command = {};
                command.type = REACH_RENDER_COMMAND_RECT;
                command.rect.x = row_x + row_selected_inset_x;
                command.rect.y = row_y + row_selected_inset_y;
                command.rect.width = row_width - row_selected_inset_x * 2.0f;
                command.rect.height = row_height - row_selected_inset_y * 2.0f;
                command.color = reach_launcher_rgb(255, 255, 255, 0.14f);
                command.radius = row_selected_radius;
                reach_render_command_buffer_push(out_commands, &command);
            }

            float icon_size = row_icon_size;
            float icon_x = row_x + row_icon_x;
            float icon_y = row_y + row_icon_y;
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
                command.rect.x = icon_x - row_fallback_icon_padding;
                command.rect.y = icon_y - row_fallback_icon_padding;
                command.rect.width = icon_size + row_fallback_icon_padding * 2.0f;
                command.rect.height = icon_size + row_fallback_icon_padding * 2.0f;
                command.color = reach_launcher_rgb(255, 255, 255, 0.10f);
                command.radius = row_fallback_icon_radius;
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
            command.rect.x = row_x + row_text_x;
            command.rect.y = row_y + row_title_y;
            command.rect.width = row_width - row_text_x - row_text_right_padding;
            command.rect.height = row_title_height;
            command.color = reach_launcher_rgb(255, 255, 255, selected ? 0.96f : 0.86f);
            command.text_size = row_title_size;
            command.text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
            command.text_alignment = input->text_alignment_leading;
            command.text_ellipsis = 1;
            reach_copy_utf16(command.text, 260, state->launcher.results[index].name);
            reach_render_command_buffer_push(out_commands, &command);

            command = {};
            command.type = REACH_RENDER_COMMAND_TEXT;
            command.rect.x = row_x + row_text_x;
            command.rect.y = row_y + row_path_y;
            command.rect.width = row_width - row_text_x - row_text_right_padding;
            command.rect.height = row_path_height;
            command.color = reach_launcher_rgb(255, 255, 255, selected ? 0.62f : 0.44f);
            command.text_size = row_path_size;
            command.text_alignment = input->text_alignment_leading;
            command.text_ellipsis = 1;
            reach_copy_utf16(command.text, 260, state->launcher.results[index].path);
            reach_render_command_buffer_push(out_commands, &command);
        }
    }

    return REACH_OK;
}
