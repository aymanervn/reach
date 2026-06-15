#include "reach/features/settings.h"

static float reach_settings_scale(const reach_settings_render_input *input, float value)
{
    float scale = input != nullptr && input->dpi_scale > 0.0f ? input->dpi_scale : 1.0f;
    return value * scale;
}

static void reach_settings_push_rect(reach_render_command_buffer *commands, reach_rect_f32 rect,
                                     float radius, reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = rect;
    command.radius = radius;
    command.color = color;
    (void)reach_render_command_buffer_push(commands, &command);
}

static void reach_settings_push_text(reach_render_command_buffer *commands, reach_rect_f32 rect,
                                     const uint16_t *text, float size, int32_t weight,
                                     int32_t alignment, reach_color color, int32_t ellipsis)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = rect;
    command.text_size = size;
    command.text_weight = weight;
    command.text_alignment = alignment;
    command.text_ellipsis = ellipsis;
    command.color = color;
    reach_copy_utf16(command.text, 260, text);
    (void)reach_render_command_buffer_push(commands, &command);
}

static void reach_settings_push_control_text(reach_render_command_buffer *commands,
                                             reach_rect_f32 rect, const uint16_t *text,
                                             reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = rect;
    command.text_size = rect.height * 0.78f;
    command.text_weight = REACH_TEXT_WEIGHT_DEMIBOLD;
    command.text_alignment = REACH_TEXT_ALIGNMENT_CENTER;
    command.color = color;
    reach_copy_utf16(command.text, 260, text);
    (void)reach_render_command_buffer_push(commands, &command);
}

reach_result reach_settings_build_render_commands(const reach_settings_render_input *input,
                                                  reach_render_command_buffer *commands)
{
    if (input == nullptr || input->theme == nullptr || input->model == nullptr ||
        input->layout == nullptr || commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_render_command_buffer_clear(commands);

    float scale = input->dpi_scale > 0.0f ? input->dpi_scale : 1.0f;
    reach_color background = input->theme->dark_background;
    background.a = 0.96f;
    reach_settings_push_rect(commands, input->layout->bounds, 18.0f * scale, background);

    reach_color nav_background = {0.08f, 0.11f, 0.14f, 0.64f};
    reach_settings_push_rect(commands, input->layout->nav, 0.0f, nav_background);

    reach_color topbar_background = {0.06f, 0.08f, 0.10f, 0.24f};
    reach_settings_push_rect(commands, input->layout->topbar, 0.0f, topbar_background);

    reach_color close_color = {0.92f, 0.28f, 0.28f, 1.0f};
    reach_color maximize_color = {0.94f, 0.72f, 0.20f, 1.0f};
    reach_settings_push_rect(commands, input->layout->close_button,
                             input->layout->close_button.width * 0.5f, close_color);
    reach_settings_push_rect(commands, input->layout->maximize_button,
                             input->layout->maximize_button.width * 0.5f, maximize_color);
    reach_settings_push_control_text(commands, input->layout->close_button, (const uint16_t *)L"x",
                                     input->theme->dark_text);
    reach_settings_push_control_text(commands, input->layout->maximize_button,
                                     (const uint16_t *)L"<>", input->theme->dark_text);

    size_t nav_count = 0;
    const reach_settings_nav_item *items = reach_settings_nav_items(&nav_count);
    for (size_t index = 0; index < input->layout->nav_item_count && index < nav_count; ++index)
    {
        const reach_settings_nav_item_layout *item_layout = &input->layout->nav_items[index];
        const reach_settings_nav_item *item = &items[index];
        int32_t selected = input->model->selected_page == item->page;
        if (selected)
        {
            reach_settings_push_rect(commands, item_layout->bounds, reach_settings_scale(input, 8.0f),
                                     input->theme->settings_selected_nav_background);
        }
        reach_settings_push_rect(commands, item_layout->icon_background,
                                 reach_settings_scale(input, 9.0f), item->accent_background);

        reach_render_command icon = {};
        icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        icon.rect = item_layout->icon;
        icon.icon_id = item->icon_id;
        icon.color = item->accent;
        (void)reach_render_command_buffer_push(commands, &icon);

        reach_settings_push_text(commands, item_layout->label, item->label,
                                 reach_settings_scale(input, 13.0f),
                                 REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
                                 input->theme->settings_text, 1);
    }

    reach_settings_push_text(commands, input->layout->content_title,
                             reach_settings_page_title(input->model->selected_page),
                             reach_settings_scale(input, 28.0f), REACH_TEXT_WEIGHT_DEMIBOLD,
                             input->text_alignment_leading, input->theme->settings_text, 1);
    reach_settings_push_text(commands, input->layout->content_placeholder,
                             reach_settings_page_placeholder(input->model->selected_page),
                             reach_settings_scale(input, 15.0f), REACH_TEXT_WEIGHT_NORMAL,
                             input->text_alignment_leading, input->theme->settings_secondary_text,
                             1);

    return REACH_OK;
}
