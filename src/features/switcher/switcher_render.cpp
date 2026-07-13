#include "reach/features/switcher.h"

#include "switcher_common.h"

static reach_color reach_switcher_rgb(uint8_t r, uint8_t g, uint8_t b, float a)
{
    reach_color color = {};
    color.r = (float)r / 255.0f;
    color.g = (float)g / 255.0f;
    color.b = (float)b / 255.0f;
    color.a = a;
    return color;
}

static float reach_switcher_input_scale(const reach_switcher_render_input *input, float value)
{
    float scale = input != nullptr && input->dpi_scale > 0.0f ? input->dpi_scale : 1.0f;
    return value * scale;
}

static void reach_switcher_label_from_path(uint16_t *out_label, size_t out_count,
                                           const uint16_t *path)
{
    if (out_label == nullptr || out_count == 0)
    {
        return;
    }

    out_label[0] = 0;
    const uint16_t fallback[] = {'A', 'p', 'p', 0};
    if (path == nullptr || path[0] == 0)
    {
        (void)reach_copy_utf16(out_label, out_count, fallback);
        return;
    }

    const uint16_t *name = path;
    for (const uint16_t *cursor = path; *cursor != 0; ++cursor)
    {
        if (*cursor == '\\' || *cursor == '/')
        {
            name = cursor + 1;
        }
    }

    size_t name_length = 0;
    while (name[name_length] != 0)
    {
        ++name_length;
    }

    size_t end = name_length;
    for (size_t index = name_length; index > 0; --index)
    {
        if (name[index - 1] == '.')
        {
            end = index - 1;
            break;
        }
    }
    if (end == 0)
    {
        end = name_length;
    }

    size_t write = 0;
    while (write + 1 < out_count && write < end)
    {
        out_label[write] = name[write];
        ++write;
    }
    out_label[write] = 0;
    if (out_label[0] == 0)
    {
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

    reach_switcher_render_item items[REACH_MAX_PINNED_APPS] = {};
    for (size_t index = 0; index < state->window_count && index < REACH_MAX_PINNED_APPS; ++index)
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
        reach_switcher_label_from_path(items[index].label, 260, window->path);
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
    float radius = reach_switcher_input_scale(input, 20.0f);
    float padding = reach_switcher_input_scale(input, 24.0f);
    float item_size = reach_switcher_input_scale(input, 112.0f);
    float icon_box_size = reach_switcher_input_scale(input, 88.0f);
    float gap = reach_switcher_input_scale(input, 14.0f);
    float selected_inset = reach_switcher_input_scale(input, 5.0f);
    float icon_top_offset = reach_switcher_input_scale(input, 4.0f);
    float label_top = reach_switcher_input_scale(input, 104.0f);
    float label_height = reach_switcher_input_scale(input, 20.0f);
    float label_text_size = reach_switcher_input_scale(input, 13.0f);
    const reach_theme *theme = input->theme;
    float icon_box_radius = reach_theme_icon_box_corner_radius(theme, icon_box_size);
    size_t visible_count = reach_switcher_visible_count(input->model->window_count);

    command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = 0.5f;
    command.rect.y = 0.5f;
    command.rect.width = input->bounds.width - 1.0f;
    command.rect.height = input->bounds.height - 1.0f;
    command.color = theme->dark_background;
    command.radius = radius;
    reach_render_command_buffer_push(out_commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE;
    command.rect.x = 0.5f;
    command.rect.y = 0.5f;
    command.rect.width = input->bounds.width - 1.0f;
    command.rect.height = input->bounds.height - 1.0f;
    command.color = theme->dark_border;
    command.radius = radius;
    command.stroke_width = reach_switcher_input_scale(input, 1.0f);
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
                command.color = reach_switcher_rgb(255, 255, 255, 0.34f);
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
                command.color = reach_switcher_rgb(242, 240, 236, 0.96f);
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
