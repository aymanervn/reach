#include "reach/features/dock.h"

static reach_color reach_dock_rgb(uint8_t r, uint8_t g, uint8_t b, float a)
{
    reach_color color = {};
    color.r = (float)r / 255.0f;
    color.g = (float)g / 255.0f;
    color.b = (float)b / 255.0f;
    color.a = a;
    return color;
}

static void reach_dock_push_item(
    const reach_dock_render_input *input,
    reach_render_command_buffer *commands,
    size_t index,
    float override_box_x,
    int32_t use_override,
    float icon_box_size,
    float icon_box_radius)
{
    const reach_theme *theme = input->theme;
    const reach_dock_layout *layout = input->layout;
    reach_render_command command = {};
    uint16_t fallback_initial = '?';
    reach_icon_handle icon = reach_dock_icon_for_item(input->icons, input->model, index, &fallback_initial);
    float box_x = use_override
        ? override_box_x
        : (index < input->item_box_x_count ? input->item_box_x[index] : 0.0f);
    float box_y = layout->app_slots[index].y - layout->bounds.y
        + (layout->app_slots[index].height - icon_box_size) * 0.5f;

    if (icon.id != 0) {
        int32_t wants_backplate = icon.wants_backplate;
        float actual_icon_size = wants_backplate ? icon_box_size * theme->icon_backplate_scale : icon_box_size;

        if (wants_backplate) {
            command = {};
            command.type = REACH_RENDER_COMMAND_RECT;
            command.rect.x = box_x;
            command.rect.y = box_y;
            command.rect.width = icon_box_size;
            command.rect.height = icon_box_size;
            command.color = theme->icon_backplate_background;
            command.radius = icon_box_radius;
            reach_render_command_buffer_push(commands, &command);
        }

        float icon_x = box_x + (icon_box_size - actual_icon_size) * 0.5f;
        float icon_y = box_y + (icon_box_size - actual_icon_size) * 0.5f;

        command = {};
        command.type = REACH_RENDER_COMMAND_ICON;
        command.rect.x = icon_x;
        command.rect.y = icon_y;
        command.rect.width = actual_icon_size;
        command.rect.height = actual_icon_size;
        command.icon_id = icon.id;
        command.color.a = 1.0f;
        command.radius = wants_backplate ? 0.0f : icon_box_radius;
        reach_render_command_buffer_push(commands, &command);

        if (wants_backplate) {
            command = {};
            command.type = REACH_RENDER_COMMAND_BACKPLATE_EDGE;
            command.rect.x = box_x;
            command.rect.y = box_y;
            command.rect.width = icon_box_size;
            command.rect.height = icon_box_size;
            command.color = theme->icon_backplate_edge;
            command.radius = icon_box_radius;
            command.stroke_width = 0.55f;
            reach_render_command_buffer_push(commands, &command);
        }
    } else {
        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect.x = box_x;
        command.rect.y = box_y;
        command.rect.width = icon_box_size;
        command.rect.height = icon_box_size;
        command.color = theme->icon_box_background;
        command.color.a *= 0.35f;
        command.radius = icon_box_radius;
        reach_render_command_buffer_push(commands, &command);

        command = {};
        command.type = REACH_RENDER_COMMAND_TEXT;
        command.rect.x = box_x;
        command.rect.y = box_y;
        command.rect.width = icon_box_size;
        command.rect.height = icon_box_size;
        command.color = theme->fallback_icon_text;
        command.text_alignment = input->text_alignment_center;
        command.text[0] = fallback_initial;
        command.text[1] = 0;
        reach_render_command_buffer_push(commands, &command);
    }

    if (input->model != nullptr && index < input->model->item_count && input->model->items[index].window != 0) {
        int32_t focused = input->model->items[index].window == input->focused_window;
        float indicator_size = 4.0f;
        float indicator_gap = 4.0f;
        float indicator_y = box_y + icon_box_size + indicator_gap;
        float max_indicator_y = layout->bounds.height - indicator_size - 2.0f;
        if (indicator_y > max_indicator_y) {
            indicator_y = max_indicator_y;
        }

        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect.x = box_x + (icon_box_size - indicator_size) * 0.5f;
        command.rect.y = indicator_y;
        command.rect.width = indicator_size;
        command.rect.height = indicator_size;
        command.color.r = 1.0f;
        command.color.g = 1.0f;
        command.color.b = 1.0f;
        command.color.a = focused ? 1.0f : 0.5f;
        command.radius = 2.0f;
        reach_render_command_buffer_push(commands, &command);
    }

    if (input->click_feedback_index == index && input->click_feedback_opacity > 0.001f) {
        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect.x = box_x;
        command.rect.y = box_y;
        command.rect.width = icon_box_size;
        command.rect.height = icon_box_size;
        command.color.r = 0.0f;
        command.color.g = 0.0f;
        command.color.b = 0.0f;
        command.color.a = input->click_feedback_opacity;
        command.radius = icon_box_radius;
        reach_render_command_buffer_push(commands, &command);
    }
}

reach_result reach_dock_build_render_commands(const reach_dock_render_input *input, reach_render_command_buffer *out_commands)
{
    if (input == nullptr || input->theme == nullptr || input->layout == nullptr || input->model == nullptr || out_commands == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_render_command_buffer_clear(out_commands);
    const reach_theme *theme = input->theme;
    const reach_dock_layout *layout = input->layout;
    reach_render_command command = {};

    float dock_radius = reach_theme_dock_corner_radius(theme, layout->bounds.height);
    float icon_box_size = reach_theme_icon_box_size(theme, layout->bounds.height);
    float icon_box_radius = reach_theme_icon_box_corner_radius(theme, icon_box_size);

    command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = 0.0f;
    command.rect.y = 0.0f;
    command.rect.width = layout->bounds.width;
    command.rect.height = layout->bounds.height;
    command.color = theme->dock_background;
    command.radius = dock_radius;
    reach_render_command_buffer_push(out_commands, &command);

    if (theme->dock_border_thickness > 0.0f && theme->dock_border.a > 0.0f) {
        command = {};
        command.type = REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE;
        command.rect.x = theme->dock_border_thickness * 0.5f;
        command.rect.y = theme->dock_border_thickness * 0.5f;
        command.rect.width = layout->bounds.width - theme->dock_border_thickness;
        command.rect.height = layout->bounds.height - theme->dock_border_thickness;
        command.color = theme->dock_border;
        command.radius = dock_radius;
        command.stroke_width = theme->dock_border_thickness;
        reach_render_command_buffer_push(out_commands, &command);
    }

    for (size_t index = 0; index < layout->app_slot_count; ++index) {
        if (index != input->dragged_render_index) {
            reach_dock_push_item(input, out_commands, index, 0.0f, 0, icon_box_size, icon_box_radius);
        }
    }
    if (input->dragged_render_index < layout->app_slot_count) {
        reach_dock_push_item(input, out_commands, input->dragged_render_index, input->dragged_box_x, 1, icon_box_size, icon_box_radius);
    }

    float tray_box_x = layout->tray_button.x - layout->bounds.x
        + (layout->tray_button.width - icon_box_size) * 0.5f;
    float tray_box_y = layout->tray_button.y - layout->bounds.y
        + (layout->tray_button.height - icon_box_size) * 0.5f;

    float quick_settings_box_x = layout->quick_settings_button.x - layout->bounds.x
        + (layout->quick_settings_button.width - icon_box_size) * 0.5f;
    float quick_settings_box_y = layout->quick_settings_button.y - layout->bounds.y
        + (layout->quick_settings_button.height - icon_box_size) * 0.5f;

    float tray_group_x = tray_box_x < quick_settings_box_x
        ? tray_box_x
        : quick_settings_box_x;
    float tray_group_y = tray_box_y < quick_settings_box_y
        ? tray_box_y
        : quick_settings_box_y;
    float tray_group_right = tray_box_x + icon_box_size;
    float quick_settings_group_right = quick_settings_box_x + icon_box_size;
    if (quick_settings_group_right > tray_group_right) {
        tray_group_right = quick_settings_group_right;
    }
    float tray_group_bottom = tray_box_y + icon_box_size;
    float quick_settings_group_bottom = quick_settings_box_y + icon_box_size;
    if (quick_settings_group_bottom > tray_group_bottom) {
        tray_group_bottom = quick_settings_group_bottom;
    }

    command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = tray_group_x;
    command.rect.y = tray_group_y;
    command.rect.width = tray_group_right - tray_group_x;
    command.rect.height = tray_group_bottom - tray_group_y;
    command.color = theme->tray_button_background;
    command.radius = icon_box_radius;
    reach_render_command_buffer_push(out_commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    command.rect.x = tray_box_x + icon_box_size * 0.22f;
    command.rect.y = tray_box_y + icon_box_size * 0.22f;
    command.rect.width = icon_box_size * 0.56f;
    command.rect.height = icon_box_size * 0.56f;
    command.color = theme->icon_backplate_background;
    command.icon_id = REACH_VECTOR_ICON_ARROW_UP;
    reach_render_command_buffer_push(out_commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    command.rect.x = quick_settings_box_x + icon_box_size * 0.31f;
    command.rect.y = quick_settings_box_y + icon_box_size * 0.31f;
    command.rect.width = icon_box_size * 0.38f;
    command.rect.height = icon_box_size * 0.38f;
    command.color = theme->icon_backplate_background;
    command.icon_id = REACH_VECTOR_ICON_SETTINGS;
    reach_render_command_buffer_push(out_commands, &command);

    if (input->click_feedback_index == input->tray_feedback_index &&
        input->click_feedback_opacity > 0.001f) {
        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect.x = tray_box_x;
        command.rect.y = tray_box_y;
        command.rect.width = icon_box_size;
        command.rect.height = icon_box_size;
        command.color.r = 0.0f;
        command.color.g = 0.0f;
        command.color.b = 0.0f;
        command.color.a = input->click_feedback_opacity;
        command.radius = icon_box_radius;
        reach_render_command_buffer_push(out_commands, &command);
    }

    if (input->click_feedback_index == input->quick_settings_feedback_index &&
        input->click_feedback_opacity > 0.001f) {
        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect.x = quick_settings_box_x;
        command.rect.y = quick_settings_box_y;
        command.rect.width = icon_box_size;
        command.rect.height = icon_box_size;
        command.color.r = 0.0f;
        command.color.g = 0.0f;
        command.color.b = 0.0f;
        command.color.a = input->click_feedback_opacity;
        command.radius = icon_box_radius;
        reach_render_command_buffer_push(out_commands, &command);
    }

    command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = layout->system_separator.x - layout->bounds.x;
    command.rect.y = layout->system_separator.y - layout->bounds.y;
    command.rect.width = layout->system_separator.width;
    command.rect.height = layout->system_separator.height;
    command.color = theme->dock_system_separator;
    command.radius = layout->system_separator.width * 0.5f;
    reach_render_command_buffer_push(out_commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect.x = layout->clock.x - layout->bounds.x;
    command.rect.y = layout->clock.y - layout->bounds.y + 2.0f;
    command.rect.width = layout->clock.width;
    command.rect.height = layout->clock.height * 0.48f;
    command.color = theme->dock_clock_time;
    command.text_size = 17.0f;
    command.text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
    command.text_ellipsis = 1;
    reach_copy_utf16(command.text, 260, input->time_text != nullptr ? input->time_text : (const uint16_t *)L"");
    reach_render_command_buffer_push(out_commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect.x = layout->clock.x - layout->bounds.x;
    command.rect.y = layout->clock.y - layout->bounds.y + layout->clock.height * 0.44f;
    command.rect.width = layout->clock.width;
    command.rect.height = layout->clock.height * 0.56f;
    command.color = theme->dock_clock_date;
    command.text_size = 12.0f;
    command.text_ellipsis = 1;
    reach_copy_utf16(command.text, 260, input->date_text != nullptr ? input->date_text : (const uint16_t *)L"");
    reach_render_command_buffer_push(out_commands, &command);

    float power_box_x = layout->power_button.x - layout->bounds.x
        + (layout->power_button.width - icon_box_size) * 0.5f;
    float power_box_y = layout->power_button.y - layout->bounds.y
        + (layout->power_button.height - icon_box_size) * 0.5f;

    command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = power_box_x;
    command.rect.y = power_box_y;
    command.rect.width = icon_box_size;
    command.rect.height = icon_box_size;
    command.color = theme->dock_power_button_background;
    command.radius = 100.0f;
    reach_render_command_buffer_push(out_commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    command.rect.x = power_box_x + icon_box_size * 0.25f;
    command.rect.y = power_box_y + icon_box_size * 0.25f;
    command.rect.width = icon_box_size * 0.50f;
    command.rect.height = icon_box_size * 0.50f;
    command.color = theme->dock_power_glyph;
    command.icon_id = REACH_VECTOR_ICON_POWER;
    reach_render_command_buffer_push(out_commands, &command);

    if (input->click_feedback_index == input->power_feedback_index &&
        input->click_feedback_opacity > 0.001f) {
        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect.x = power_box_x;
        command.rect.y = power_box_y;
        command.rect.width = icon_box_size;
        command.rect.height = icon_box_size;
        command.color.r = 0.0f;
        command.color.g = 0.0f;
        command.color.b = 0.0f;
        command.color.a = input->click_feedback_opacity;
        command.radius = 100.0f;
        reach_render_command_buffer_push(out_commands, &command);
    }

    return REACH_OK;
}
