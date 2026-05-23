#include "reach/features/quick_settings.h"

static float reach_quick_settings_clamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static float reach_quick_settings_min_f32(float a, float b)
{
    return a < b ? a : b;
}

static void reach_quick_settings_copy_utf16(
    uint16_t *dst,
    size_t dst_count,
    const uint16_t *src)
{
    if (dst == nullptr || dst_count == 0) {
        return;
    }

    size_t index = 0;
    if (src != nullptr) {
        while (index + 1 < dst_count && src[index] != 0) {
            dst[index] = src[index];
            ++index;
        }
    }
    dst[index] = 0;
}

static void reach_quick_settings_push_rounded_rect(
    reach_render_command_buffer *commands,
    reach_rect_f32 rect,
    float radius,
    reach_color color
)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = rect;
    command.radius = radius;
    command.color = color;
    (void)reach_render_command_buffer_push(commands, &command);
}

static void reach_quick_settings_push_text(
    reach_render_command_buffer *commands,
    reach_rect_f32 rect,
    const uint16_t *text,
    float size,
    int32_t weight,
    int32_t alignment,
    reach_color color
)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = rect;
    command.text_size = size;
    command.text_weight = weight;
    command.text_alignment = alignment;
    command.color = color;
    reach_quick_settings_copy_utf16(command.text, 260, text);
    (void)reach_render_command_buffer_push(commands, &command);
}

reach_result reach_quick_settings_build_render_commands(
    const reach_quick_settings_render_input *input,
    reach_render_command_buffer *commands
)
{
    if (input == nullptr || commands == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_render_command_buffer_clear(commands);

    reach_quick_settings_layout layout = input->layout;
    float volume = reach_quick_settings_clamp01(input->model.main_volume_level);

    layout.main_slider_fill.width = layout.main_slider_track.width * volume;
    if (layout.main_slider_fill.width < 0.0f) {
        layout.main_slider_fill.width = 0.0f;
    }

    float track_radius = layout.main_slider_track.height * 0.5f;
    float fill_radius = reach_quick_settings_min_f32(
        layout.main_slider_fill.height * 0.5f,
        layout.main_slider_fill.width * 0.5f);

    reach_color slider_track_color = input->theme.quick_settings_slider_track_color;
    reach_color slider_fill_color = input->model.main_muted
        ? input->theme.quick_settings_slider_muted_fill_color
        : input->theme.quick_settings_slider_fill_color;
    reach_color expand_button_color = input->theme.quick_settings_expand_button_color;
    reach_color expand_text_color = input->theme.quick_settings_expand_text_color;
    reach_color expand_icon_color = input->theme.quick_settings_expand_icon_color;

    reach_quick_settings_push_rounded_rect(
        commands,
        layout.main_slider_track,
        track_radius,
        slider_track_color);

    if (layout.main_slider_fill.width > 0.0f) {
        reach_quick_settings_push_rounded_rect(
            commands,
            layout.main_slider_fill,
            fill_radius,
            slider_fill_color);
    }

    reach_quick_settings_push_rounded_rect(
        commands,
        layout.expand_button,
        layout.expand_button.height * 0.5f,
        expand_button_color);

    static const uint16_t expand_label[] = {
        'A','l','l',' ','v','o','l','u','m','e',' ','s','l','i','d','e','r','s',0
    };

    reach_quick_settings_push_text(
        commands,
        layout.expand_button_label,
        expand_label,
        13.0f,
        400,
        0,
        expand_text_color);

    reach_render_command icon = {};
    icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    icon.rect = layout.expand_button_icon;
    icon.icon_id = REACH_VECTOR_ICON_TRAY_ARROW;
    icon.color = expand_icon_color;
    (void)reach_render_command_buffer_push(commands, &icon);

    return REACH_OK;
}
