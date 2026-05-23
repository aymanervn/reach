#include "reach/features/quick_settings.h"
#include "reach/features/popup.h"

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

static void reach_quick_settings_push_clipped_rounded_rect(
    reach_render_command_buffer *commands,
    reach_rect_f32 rect,
    float radius,
    reach_rect_f32 clip_rect,
    float clip_radius,
    reach_color color
)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_CLIPPED_ROUNDED_RECT;
    command.rect = rect;
    command.clip_rect = clip_rect;
    command.radius = radius;
    command.clip_radius = clip_radius;
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

reach_result reach_quick_settings_push_volume_pill_commands(
    const reach_quick_settings_volume_pill_model *model,
    const reach_quick_settings_volume_pill_layout *layout,
    const reach_theme *theme,
    reach_render_command_buffer *commands
)
{
    if (model == nullptr || layout == nullptr || theme == nullptr || commands == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    float volume = reach_quick_settings_clamp01(model->volume_level);
    reach_quick_settings_volume_pill_layout pill = *layout;
    pill.slider_fill.y = pill.slider_track.y;
    pill.slider_fill.width = pill.slider_track.width * volume;
    pill.slider_fill.height = pill.slider_track.height;
    if (pill.slider_fill.width < 0.0f) {
        pill.slider_fill.width = 0.0f;
    }

    reach_render_command icon = {};
    icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    icon.rect = pill.header_icon;
    icon.icon_id = model->icon_id;
    icon.color = theme->icon_backplate_background;
    (void)reach_render_command_buffer_push(commands, &icon);

    reach_quick_settings_push_text(
        commands,
        pill.header_label,
        model->label,
        13.0f,
        REACH_TEXT_WEIGHT_SEMIBOLD,
        0,
        theme->quick_settings_expand_text_color);

    float track_radius = reach_popup_radius();
    float fill_radius = reach_quick_settings_min_f32(
        reach_popup_radius(),
        pill.slider_fill.width * 0.5f);

    reach_color slider_fill_color = model->muted
        ? theme->quick_settings_slider_muted_fill_color
        : theme->quick_settings_slider_fill_color;

    reach_quick_settings_push_rounded_rect(
        commands,
        pill.slider_track,
        track_radius,
        theme->quick_settings_slider_track_color);

    if (pill.slider_fill.width > 0.0f) {
        reach_quick_settings_push_clipped_rounded_rect(
            commands,
            pill.slider_fill,
            fill_radius,
            pill.slider_track,
            track_radius,
            slider_fill_color);
    }

    return REACH_OK;
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

    static const uint16_t master_volume_label[] = {
        'M','a','s','t','e','r',' ','v','o','l','u','m','e',0
    };

    reach_quick_settings_volume_pill_model pill_model = {};
    reach_quick_settings_volume_pill_model_init(
        &pill_model,
        input->model.main_volume_level,
        input->model.main_muted,
        master_volume_label);

    reach_result result = reach_quick_settings_push_volume_pill_commands(
        &pill_model,
        &input->layout.main_volume_pill,
        &input->theme,
        commands);
    if (result != REACH_OK) {
        return result;
    }

    for (size_t index = 0;
        index < input->layout.session_pill_count &&
        index < input->model.sessions.count;
        ++index) {
        const reach_audio_volume_session *session =
            &input->model.sessions.sessions[index];
        reach_quick_settings_volume_pill_model session_model = {};
        reach_quick_settings_volume_pill_model_init(
            &session_model,
            session->level,
            session->muted,
            session->label);
        reach_quick_settings_copy_utf16(
            session_model.session_instance_id,
            REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY,
            session->session_instance_id);

        result = reach_quick_settings_push_volume_pill_commands(
            &session_model,
            &input->layout.session_volume_pills[index],
            &input->theme,
            commands);
        if (result != REACH_OK) {
            return result;
        }
    }

    reach_quick_settings_push_rounded_rect(
        commands,
        input->layout.expand_button,
        reach_popup_radius(),
        input->theme.quick_settings_expand_button_color);

    static const uint16_t expand_label[] = {
        'A','l','l',' ','v','o','l','u','m','e',' ','s','l','i','d','e','r','s',0
    };

    reach_quick_settings_push_text(
        commands,
        input->layout.expand_button_label,
        expand_label,
        13.0f,
        REACH_TEXT_WEIGHT_NORMAL,
        0,
        input->theme.quick_settings_expand_text_color);

    reach_render_command icon = {};
    icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    icon.rect = input->layout.expand_button_icon;
    icon.icon_id = REACH_VECTOR_ICON_ARROW_DOWN;
    icon.color = input->theme.icon_backplate_background;
    (void)reach_render_command_buffer_push(commands, &icon);

    return REACH_OK;
}
