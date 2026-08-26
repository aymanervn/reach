#include "reach/features/launcher.h"

#include "launcher_common.h"

#include "reach/core/typography.h"
#include "reach/features/common/scrollbar_render.h"
#include "reach/features/common/section_reveal.h"

static uint64_t reach_launcher_fallback_icon(reach_search_result_kind kind)
{
    switch (kind)
    {
    case REACH_SEARCH_RESULT_APP:
        return REACH_VECTOR_ICON_EXECUTABLE;
    case REACH_SEARCH_RESULT_FOLDER:
        return REACH_VECTOR_ICON_FOLDER;
    case REACH_SEARCH_RESULT_PHOTO:
        return REACH_VECTOR_ICON_PHOTO;
    case REACH_SEARCH_RESULT_VIDEO:
        return REACH_VECTOR_ICON_VIDEO;
    case REACH_SEARCH_RESULT_MUSIC:
        return REACH_VECTOR_ICON_MUSIC;
    case REACH_SEARCH_RESULT_DOCUMENT:
        return REACH_VECTOR_ICON_DOCUMENT;
    case REACH_SEARCH_RESULT_FILE:
    default:
        return REACH_VECTOR_ICON_FILE;
    }
}

static int32_t reach_launcher_error_row_visible(const reach_launcher_model *model)
{
    return model != nullptr && model->search_error && model->result_count == 0 &&
           model->query_length > 0;
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
    float launcher_radius = layout->search_box.height * 0.5f;
    float row_selected_inset_x = reach_launcher_scale(input, 6.0f);
    float row_selected_inset_y = reach_launcher_scale(input, 5.0f);
    float row_selected_radius = reach_launcher_scale(input, theme->radius_small);
    float row_icon_size = reach_launcher_scale(input, 32.0f);
    float row_icon_x = reach_launcher_scale(input, 16.0f);
    float row_icon_y = reach_launcher_scale(input, 12.0f);
    float row_fallback_icon_padding = reach_launcher_scale(input, 2.0f);
    float row_fallback_icon_radius = reach_launcher_scale(input, theme->radius_small);
    float row_text_x = reach_launcher_scale(input, 62.0f);
    float row_text_right_padding = reach_launcher_scale(input, 16.0f);
    float row_title_y = reach_launcher_scale(input, 6.0f);
    float row_title_height = reach_launcher_scale(input, 24.0f);
    float row_title_size = reach_launcher_scale(input, REACH_TEXT_SIZE_LARGE);
    float row_path_y = reach_launcher_scale(input, 28.0f);
    float row_path_height = reach_launcher_scale(input, 20.0f);
    float row_path_size = reach_launcher_scale(input, REACH_TEXT_SIZE_SMALL);
    int32_t error_row_visible = reach_launcher_error_row_visible(model);
    int32_t results_attached = model->result_count > 0 || error_row_visible ? 1 : 0;
    float outer_height = layout->search_box.height;
    if (results_attached)
    {
        float expanded_height =
            (layout->search_results.y - layout->bounds.y) + layout->search_results.height;
        outer_height += (expanded_height - outer_height) * input->results_expansion;
    }
    float border_thickness = reach_theme_border_thickness(theme, input->dpi_scale);

    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = 0.0f;
    command.rect.y = 0.0f;
    command.rect.width = layout->bounds.width;
    command.rect.height = outer_height;
    command.radius = launcher_radius;
    reach_result result = reach_render_push_bordered_background(
        out_commands, &command, theme->launcher_search_background, theme->launcher_border,
        border_thickness, &theme->popup_shadow, input->dpi_scale);
    if (result != REACH_OK)
    {
        return result;
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
        command.text_size = reach_launcher_scale(input, REACH_TEXT_SIZE_LARGE);
        command.text_weight = REACH_TEXT_WEIGHT_NORMAL;
        command.text_alignment = input->text_alignment_leading;
        command.text_color = theme->launcher_search_text;
        command.placeholder_color = theme->launcher_placeholder_text;
        command.selection_color = theme->launcher_selection_highlight;
        command.caret_index = input->caret_index;
        command.caret_visible = input->caret_visible;
        command.selection_start = input->selection_start;
        command.selection_end = input->selection_end;
        reach_copy_utf16(command.text, 260, model->query);
        reach_copy_utf16(command.placeholder, 128, (const uint16_t *)L"Search for anything");
        reach_render_command_buffer_push(out_commands, &command);
    }

    if (model->query[0] == 0)
    {
        command = {};
        command.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        command.rect.x = layout->search_icon.x - layout->bounds.x;
        command.rect.y = layout->search_icon.y - layout->bounds.y;
        command.rect.width = layout->search_icon.width;
        command.rect.height = layout->search_icon.height;
        command.icon_id = REACH_VECTOR_ICON_SEARCH;
        command.color = theme->launcher_search_icon;
        reach_render_command_buffer_push(out_commands, &command);
    }

    if (results_attached && input->results_expansion > 0.001f)
    {
        reach_rect_f32 results_clip = {};
        results_clip.x = 0.0f;
        results_clip.y = layout->search_results.y - layout->bounds.y;
        results_clip.width = layout->bounds.width;
        results_clip.height = outer_height - results_clip.y;
        if (results_clip.height < 0.0f)
        {
            results_clip.height = 0.0f;
        }
        reach_render_command_buffer_set_scissor(out_commands, results_clip);
        size_t first_result_command = out_commands->count;
        size_t visible_count = reach_launcher_visible_result_count(model);
        float row_height = reach_launcher_scale(input, 56.0f);

        if (layout->search_result_scrollbar_track.height > 0.0f)
        {
            reach_scrollbar_build_render_commands(layout->search_result_scrollbar_track,
                                                  layout->search_result_scrollbar_thumb,
                                                  layout->bounds, theme->launcher_scrollbar_track,
                                                  theme->launcher_scrollbar_thumb, out_commands);
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
                command.color = theme->launcher_row_selected_background;
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
                command.color = theme->launcher_row_icon_background;
                command.radius = row_fallback_icon_radius;
                reach_render_command_buffer_push(out_commands, &command);

                command = {};
                command.type = REACH_RENDER_COMMAND_VECTOR_ICON;
                command.rect.x = icon_x;
                command.rect.y = icon_y;
                command.rect.width = icon_size;
                command.rect.height = icon_size;
                command.color = theme->launcher_row_icon_glyph;
                command.icon_id = reach_launcher_fallback_icon(model->results[index].visual_kind);
                reach_render_command_buffer_push(out_commands, &command);
            }

            command = {};
            command.type = REACH_RENDER_COMMAND_TEXT;
            command.rect.x = row_x + row_text_x;
            command.rect.y = row_y + row_title_y;
            command.rect.width = row_width - row_text_x - row_text_right_padding;
            command.rect.height = row_title_height;
            command.color =
                selected ? theme->launcher_row_title_selected : theme->launcher_row_title;
            command.text_size = row_title_size;
            command.text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
            command.text_alignment = input->text_alignment_leading;
            command.text_ellipsis = 1;
            reach_copy_utf16(command.text, 260, model->results[index].title);
            reach_render_command_buffer_push(out_commands, &command);

            command = {};
            command.type = REACH_RENDER_COMMAND_TEXT;
            command.rect.x = row_x + row_text_x;
            command.rect.y = row_y + row_path_y;
            command.rect.width = row_width - row_text_x - row_text_right_padding;
            command.rect.height = row_path_height;
            command.color = selected ? theme->launcher_row_path_selected : theme->launcher_row_path;
            command.text_size = row_path_size;
            command.text_alignment = input->text_alignment_leading;
            command.text_ellipsis = 1;
            reach_copy_utf16(command.text, 260, model->results[index].subtitle);
            reach_render_command_buffer_push(out_commands, &command);
        }

        if (error_row_visible)
        {
            float row_x = layout->search_result_items.x - layout->bounds.x;
            float row_y = layout->search_result_items.y - layout->bounds.y;
            float row_width = layout->search_result_items.width;

            command = {};
            command.type = REACH_RENDER_COMMAND_TEXT;
            command.rect.x = row_x;
            command.rect.y = row_y + row_title_y;
            command.rect.width = row_width;
            command.rect.height = row_title_height;
            command.color = theme->launcher_row_title;
            command.text_size = row_title_size;
            command.text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
            command.text_alignment = REACH_TEXT_ALIGNMENT_CENTER;
            command.text_ellipsis = 1;
            reach_copy_utf16(command.text, 260, (const uint16_t *)L"Search is unavailable");
            reach_render_command_buffer_push(out_commands, &command);

            command = {};
            command.type = REACH_RENDER_COMMAND_TEXT;
            command.rect.x = row_x;
            command.rect.y = row_y + row_path_y;
            command.rect.width = row_width;
            command.rect.height = row_path_height;
            command.color = theme->launcher_row_path;
            command.text_size = row_path_size;
            command.text_alignment = REACH_TEXT_ALIGNMENT_CENTER;
            command.text_ellipsis = 1;
            reach_copy_utf16(command.text, 260,
                             (const uint16_t *)L"Could not connect to retriever. Is it installed?");
            reach_render_command_buffer_push(out_commands, &command);
        }

        reach_section_reveal_apply(out_commands, first_result_command, input->results_expansion,
                                   reach_launcher_scale(input, 4.0f));
        reach_render_command_buffer_clear_scissor(out_commands);
    }

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

    const size_t icon_overscan_rows = 4;
    size_t icon_start = reach_launcher_model_result_scroll_offset(&state->model);
    icon_start = icon_start > icon_overscan_rows ? icon_start - icon_overscan_rows : 0;
    size_t icon_end = reach_launcher_model_result_scroll_offset(&state->model) +
                      REACH_SEARCH_VISIBLE_RESULTS + icon_overscan_rows;

    uint64_t result_icon_ids[REACH_SEARCH_MAX_RESULTS] = {};
    for (size_t index = icon_start; index < icon_end && index < state->model.result_count &&
                                    index < REACH_SEARCH_MAX_RESULTS;
         ++index)
    {
        const uint16_t *path = state->model.results[index].icon_path;
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
    input.results_expansion = reach_launcher_results_expansion(launcher);

    return reach_launcher_build_render_commands(&input, out_commands);
}
