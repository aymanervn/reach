#include "reach/features/switcher.h"

#include "switcher_common.h"

#include "reach/core/typography.h"

static float reach_switcher_input_scale(const reach_switcher_render_input *input, float value)
{
    float scale = input != nullptr && input->dpi_scale > 0.0f ? input->dpi_scale : 1.0f;
    return value * scale;
}

static void reach_switcher_label_for_window(uint16_t *out_label, size_t out_count,
                                            const reach_window_snapshot *window)
{
    if (out_label == nullptr || out_count == 0)
    {
        return;
    }

    reach_window_tracking_app_display_name(window, out_label, out_count);
    if (out_label[0] == 0)
    {
        const uint16_t fallback[] = {'A', 'p', 'p', 0};
        (void)reach_copy_utf16(out_label, out_count, fallback);
    }
}

reach_result reach_switcher_append_render_commands(reach_switcher *switcher,
                                                   const reach_switcher_render_context *ctx,
                                                   reach_render_command_buffer *out_commands)
{
    if (switcher == nullptr || ctx == nullptr || ctx->theme == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_switcher_state *state = reach_switcher_state_mut(switcher);

    reach_switcher_model visible_model = {};
    visible_model.window_count = state->window_count;
    visible_model.selected_index = state->selected_index;
    visible_model.visible_start = state->visible_start;
    reach_switcher_update_visible_start(&visible_model);
    state->visible_start = visible_model.visible_start;

    reach_switcher_render_item items[REACH_MAX_OPEN_WINDOWS] = {};
    for (size_t index = 0; index < state->window_count && index < REACH_MAX_OPEN_WINDOWS; ++index)
    {
        const reach_window_snapshot *window = reach_window_tracking_window_by_id(
            reach_switcher_windows(switcher), state->windows[index]);
        if (window == nullptr)
        {
            continue;
        }
        const uint16_t *icon_path = window->icon_ref[0] != 0 ? window->icon_ref : window->path;
        if (icon_path[0] != 0)
        {
            items[index].icon_id = reach_icon_service_get(reach_switcher_icons(switcher), icon_path,
                                                          ctx->icon_size_px);
        }
        reach_switcher_label_for_window(items[index].label, 260, window);
    }

    reach_switcher_render_input input = {};
    input.theme = ctx->theme;
    input.bounds = ctx->bounds;
    input.model = &visible_model;
    input.items = items;
    input.item_count = state->window_count;
    input.dpi_scale = ctx->dpi_scale;
    input.text_alignment_center = REACH_TEXT_ALIGNMENT_CENTER;
    input.text_weight_demi_bold = REACH_TEXT_WEIGHT_DEMIBOLD;

    return reach_switcher_build_render_commands(&input, out_commands);
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
    float radius = reach_switcher_input_scale(input, input->theme->radius_large);
    float padding = reach_switcher_input_scale(input, 24.0f);
    float item_size = reach_switcher_input_scale(input, 112.0f);
    float icon_box_size = reach_switcher_input_scale(input, 88.0f);
    float gap = reach_switcher_input_scale(input, 14.0f);
    float selected_inset = reach_switcher_input_scale(input, 5.0f);
    float icon_top_offset = reach_switcher_input_scale(input, 4.0f);
    float label_top = reach_switcher_input_scale(input, 104.0f);
    float label_height = reach_switcher_input_scale(input, 20.0f);
    float label_text_size = reach_switcher_input_scale(input, REACH_TEXT_SIZE_MEDIUM);
    const reach_theme *theme = input->theme;
    float icon_box_radius = reach_theme_icon_box_corner_radius(theme, icon_box_size);
    size_t visible_count = reach_switcher_visible_count(input->model->window_count);

    float border_thickness = reach_theme_border_thickness(theme, input->dpi_scale);
    reach_rect_f32 content_bounds = reach_theme_border_content_rect(
        theme, input->dpi_scale, {0.0f, 0.0f, input->bounds.width, input->bounds.height});

    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_RECT;
    shape.rect.width = input->bounds.width;
    shape.rect.height = input->bounds.height;
    shape.radius = radius;
    reach_result result = reach_render_push_bordered_background(
        out_commands, &shape, theme->switcher_background, theme->bar_border, border_thickness,
        &theme->popup_shadow, input->dpi_scale);
    if (result != REACH_OK)
    {
        return result;
    }

    if (visible_count > 0)
    {
        float total_width = (float)visible_count * item_size + (float)(visible_count - 1) * gap;
        float x = content_bounds.x + (content_bounds.width - total_width) * 0.5f;
        if (x < content_bounds.x + padding)
        {
            x = content_bounds.x + padding;
        }
        float y = content_bounds.y + (content_bounds.height - item_size) * 0.5f;
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
            float box_y = item.y + icon_top_offset;
            uint64_t icon_id = input->items[index].icon_id;

            if (selected)
            {
                command = {};
                command.type = REACH_RENDER_COMMAND_RECT;
                command.rect.x = box_x - selected_inset;
                command.rect.y = box_y - selected_inset;
                command.rect.width = icon_box_size + selected_inset * 2.0f;
                command.rect.height = icon_box_size + selected_inset * 2.0f;
                command.color = theme->switcher_selection_background;
                command.radius = icon_box_radius + selected_inset;
                reach_render_command_buffer_push(out_commands, &command);
            }

            if (icon_id != 0)
            {
                command = {};
                command.type = REACH_RENDER_COMMAND_ICON;
                command.rect.x = box_x;
                command.rect.y = box_y;
                command.rect.width = icon_box_size;
                command.rect.height = icon_box_size;
                command.radius = 0.0f;
                command.color.a = 1.0f;
                command.icon_id = icon_id;
                reach_render_command_buffer_push(out_commands, &command);
            }

            if (selected)
            {
                command = {};
                command.type = REACH_RENDER_COMMAND_TEXT;
                command.rect.x = item.x;
                command.rect.y = item.y + label_top;
                command.rect.width = item.width;
                command.rect.height = label_height;
                command.color = theme->switcher_label_text;
                command.text_weight = input->text_weight_demi_bold;
                command.text_alignment = input->text_alignment_center;
                command.text_size = label_text_size;
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
