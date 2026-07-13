#include "reach/features/launcher.h"

#include "launcher_common.h"

#include "reach/features/common/scrollbar_render.h"

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

static size_t reach_launcher_visible_result_count(const reach_launcher_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    return model->result_count < REACH_SEARCH_VISIBLE_RESULTS ? model->result_count
                                                              : REACH_SEARCH_VISIBLE_RESULTS;
}

reach_result reach_launcher_build_render_commands(const reach_launcher_render_input *input,
                                                  reach_render_command_buffer *out_commands)
{
    if (input == nullptr || input->theme == nullptr || input->model == nullptr ||
        input->layout == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_theme *theme = input->theme;
    const reach_launcher_model *model = input->model;
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
    int32_t results_attached = model->result_count > 0 ? 1 : 0;

    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = layout->search_box.x - layout->bounds.x;
    command.rect.y = layout->search_box.y - layout->bounds.y;
    command.rect.width = layout->search_box.width;
    command.rect.height = layout->search_box.height;
    command.color = reach_launcher_opaque(theme->launcher_search_background);
    command.radius = launcher_radius;
    reach_render_command_buffer_push(out_commands, &command);

    if (results_attached)
    {
        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect.x = layout->search_box.x - layout->bounds.x;
        command.rect.y =
            layout->search_box.y - layout->bounds.y + layout->search_box.height - launcher_radius;
        command.rect.width = layout->search_box.width;
        command.rect.height = launcher_radius;
        command.color = reach_launcher_opaque(theme->launcher_search_background);
        command.radius = 0.0f;
        reach_render_command_buffer_push(out_commands, &command);
    }

    {
        reach_color transparent = {};
        command = {};
        command.type = REACH_RENDER_COMMAND_TEXTBOX;
        command.rect.x = layout->search_text_input.x - layout->bounds.x;
        command.rect.y = layout->search_text_input.y - layout->bounds.y;
        command.rect.width = layout->search_text_input.width;
        command.rect.height = layout->search_text_input.height;
        command.color = transparent;
        command.radius = 0.0f;
        command.stroke_width = 0.0f;
        command.text_size = reach_launcher_scale(input, 18.0f);
        command.text_weight = REACH_TEXT_WEIGHT_NORMAL;
        command.text_alignment = input->text_alignment_leading;
        command.text_color = reach_launcher_opaque(theme->launcher_search_text);
        command.placeholder_color = reach_launcher_rgb(255, 255, 255, 0.40f);
        command.selection_color = reach_launcher_rgb(255, 255, 255, 0.25f);
        command.caret_index = input->caret_index;
        command.caret_visible = input->caret_visible;
        command.selection_start = input->selection_start;
        command.selection_end = input->selection_end;
        reach_copy_utf16(command.text, 260, model->query);
        reach_copy_utf16(command.placeholder, 128, (const uint16_t *)L"Search for anything");
        reach_render_command_buffer_push(out_commands, &command);
    }

    if (model->result_count > 0)
    {
        size_t visible_count = reach_launcher_visible_result_count(model);
        float row_height = reach_launcher_scale(input, 56.0f);
        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect.x = layout->search_results.x - layout->bounds.x;
        command.rect.y = layout->search_results.y - layout->bounds.y;
        command.rect.width = layout->search_results.width;
        command.rect.height = layout->search_results.height;
        reach_color results_background = theme->dark_background;
        results_background.a = 0.96f;
        command.color = results_background;
        command.radius = launcher_radius;
        reach_render_command_buffer_push(out_commands, &command);

        float results_square_top_height = launcher_radius;
        if (results_square_top_height > layout->search_results.height)
        {
            results_square_top_height = layout->search_results.height;
        }
        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect.x = layout->search_results.x - layout->bounds.x;
        command.rect.y = layout->search_results.y - layout->bounds.y;
        command.rect.width = layout->search_results.width;
        command.rect.height = results_square_top_height;
        command.color = results_background;
        command.radius = 0.0f;
        reach_render_command_buffer_push(out_commands, &command);

        if (layout->search_result_scrollbar_track.height > 0.0f)
        {
            reach_scrollbar_build_render_commands(
                layout->search_result_scrollbar_track, layout->search_result_scrollbar_thumb,
                layout->bounds, reach_launcher_rgb(255, 255, 255, 0.16f),
                reach_launcher_rgb(255, 255, 255, 0.72f), out_commands);
        }

        size_t start = reach_launcher_model_result_scroll_offset(model);
        if (start > model->result_count)
        {
            start = model->result_count;
        }
        size_t end = start + visible_count;
        if (end > model->result_count)
        {
            end = model->result_count;
        }

        for (size_t index = start; index < end && index < REACH_SEARCH_MAX_RESULTS; ++index)
        {
            size_t visible_index = index - start;
            float row_x = layout->search_result_items.x - layout->bounds.x;
            float row_y = layout->search_result_items.y - layout->bounds.y +
                          row_height * (float)visible_index;
            float row_width = layout->search_result_items.width;
            int32_t selected = index == model->selected_result_index;

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
            uint64_t icon_id =
                input->result_icon_ids != nullptr ? input->result_icon_ids[index] : 0;

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
                command.icon_id = reach_launcher_fallback_icon(model->results[index].is_directory);
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
            reach_copy_utf16(command.text, 260, model->results[index].name);
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
            reach_copy_utf16(command.text, 260, model->results[index].path);
            reach_render_command_buffer_push(out_commands, &command);
        }
    }

    float outer_height = layout->search_box.height;
    if (results_attached)
    {
        outer_height =
            (layout->search_results.y - layout->bounds.y) + layout->search_results.height;
    }

    command = {};
    command.type = REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE;
    command.rect = {0.5f, 0.5f, layout->bounds.width - 1.0f, outer_height - 1.0f};
    command.color = theme->clipboard_border;
    command.radius = reach_launcher_scale(input, theme->clipboard_panel_radius);
    command.stroke_width = theme->border_thickness;
    reach_render_command_buffer_push(out_commands, &command);

    return REACH_OK;
}

reach_result reach_launcher_append_render_commands(reach_launcher *launcher,
                                                   const reach_launcher_render_context *ctx,
                                                   reach_render_command_buffer *out_commands)
{
    if (launcher == nullptr || ctx == nullptr || ctx->theme == nullptr || ctx->layout == nullptr ||
        out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_launcher_state *state = reach_launcher_state_mut(launcher);

    uint64_t result_icon_ids[REACH_SEARCH_MAX_RESULTS] = {};
    for (size_t index = 0;
         index < state->model.result_count && index < REACH_SEARCH_MAX_RESULTS; ++index)
    {
        const uint16_t *path = state->model.results[index].path;
        if (path[0] != 0)
        {
            result_icon_ids[index] =
                reach_icon_service_get(reach_launcher_icons(launcher), path, 32);
        }
    }

    reach_launcher_render_input input = {};
    input.theme = ctx->theme;
    input.model = &state->model;
    input.layout = ctx->layout;
    input.result_icon_ids = result_icon_ids;
    input.dpi_scale = ctx->dpi_scale;
    input.text_alignment_leading = REACH_TEXT_ALIGNMENT_LEADING;
    input.caret_index = state->launcher_text_edit.caret;
    input.caret_visible = state->launcher_caret_visible;
    reach_text_edit_selection_range(&state->launcher_text_edit, &input.selection_start,
                                    &input.selection_end);

    return reach_launcher_build_render_commands(&input, out_commands);
}
