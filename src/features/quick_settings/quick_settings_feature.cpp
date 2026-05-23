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

static size_t reach_quick_settings_visible_session_count(
    const reach_quick_settings_model *model
)
{
    if (model == nullptr || !model->expanded) {
        return 0;
    }
    return model->sessions.count < REACH_AUDIO_VOLUME_MAX_SESSIONS
        ? model->sessions.count
        : REACH_AUDIO_VOLUME_MAX_SESSIONS;
}

void reach_quick_settings_model_init(
    reach_quick_settings_model *model
)
{
    if (model == nullptr) {
        return;
    }

    model->main_volume_level = 0.0f;
    model->main_muted = 0;
    model->expanded = 0;
    model->sessions = {};
}

void reach_quick_settings_model_set_main_volume(
    reach_quick_settings_model *model,
    float volume_level,
    int32_t muted
)
{
    if (model == nullptr) {
        return;
    }

    model->main_volume_level = reach_quick_settings_clamp01(volume_level);
    model->main_muted = muted ? 1 : 0;
}

uint32_t reach_quick_settings_volume_icon_id(
    float volume_level,
    int32_t muted
)
{
    float level = reach_quick_settings_clamp01(volume_level);
    if (muted || level <= 0.0f) {
        return REACH_VECTOR_ICON_VOLUME_ZERO;
    }
    if (level < 0.5f) {
        return REACH_VECTOR_ICON_VOLUME_LOW;
    }
    return REACH_VECTOR_ICON_VOLUME_HIGH;
}

void reach_quick_settings_model_set_sessions(
    reach_quick_settings_model *model,
    const reach_audio_volume_session_list *sessions
)
{
    if (model == nullptr) {
        return;
    }

    model->sessions = {};
    if (sessions == nullptr) {
        return;
    }

    model->sessions.count = sessions->count < REACH_AUDIO_VOLUME_MAX_SESSIONS
        ? sessions->count
        : REACH_AUDIO_VOLUME_MAX_SESSIONS;
    for (size_t index = 0; index < model->sessions.count; ++index) {
        model->sessions.sessions[index] = sessions->sessions[index];
        model->sessions.sessions[index].level =
            reach_quick_settings_clamp01(model->sessions.sessions[index].level);
        model->sessions.sessions[index].muted =
            model->sessions.sessions[index].muted ? 1 : 0;
    }
}

void reach_quick_settings_volume_pill_model_init(
    reach_quick_settings_volume_pill_model *model,
    float volume_level,
    int32_t muted,
    const uint16_t *label
)
{
    if (model == nullptr) {
        return;
    }

    model->volume_level = reach_quick_settings_clamp01(volume_level);
    model->muted = muted ? 1 : 0;
    model->icon_id = reach_quick_settings_volume_icon_id(
        model->volume_level,
        model->muted);
    model->session_instance_id[0] = 0;
    reach_quick_settings_copy_utf16(
        model->label,
        REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY,
        label);
}

reach_quick_settings_volume_pill_layout reach_quick_settings_volume_pill_layout_for_bounds(
    reach_rect_f32 bounds,
    const reach_theme *theme
)
{
    (void)theme;

    reach_quick_settings_volume_pill_layout layout = {};
    layout.bounds = bounds;

    const float icon_size = 16.0f;
    const float label_gap = 6.0f;
    const float header_height = 16.0f;
    const float header_gap = 12.0f;

    layout.header_icon.x = bounds.x;
    layout.header_icon.y = bounds.y - header_gap - header_height + (header_height - icon_size) * 0.5f;
    layout.header_icon.width = icon_size;
    layout.header_icon.height = icon_size;

    layout.header_label.x = layout.header_icon.x + icon_size + label_gap;
    layout.header_label.y = bounds.y - header_gap - header_height;
    layout.header_label.width =
        bounds.width - icon_size - label_gap;
    layout.header_label.height = header_height;
    if (layout.header_label.width < 0.0f) {
        layout.header_label.width = 0.0f;
    }

    layout.slider_track = bounds;

    layout.slider_fill = layout.slider_track;

    return layout;
}

float reach_quick_settings_content_height_for_model(
    const reach_quick_settings_model *model
)
{
    const float padding = 8.0f;
    const float header_height = 16.0f;
    const float header_gap = 12.0f;
    const float pill_height = 24.0f;
    const float volume_gap = 10.0f;
    const float expand_gap = 10.0f;
    const float expand_height = 34.0f;

    size_t visible_sessions = reach_quick_settings_visible_session_count(model);
    float volume_component_height = header_height + header_gap + pill_height;
    return padding * 2.0f +
        volume_component_height +
        (float)visible_sessions * (volume_gap + volume_component_height) +
        expand_gap +
        expand_height;
}

float reach_quick_settings_volume_pill_level_for_x(
    const reach_quick_settings_volume_pill_layout *layout,
    float x
)
{
    if (layout == nullptr || layout->slider_track.width <= 0.0f) {
        return 0.0f;
    }

    return reach_quick_settings_clamp01(
        (x - layout->slider_track.x) / layout->slider_track.width);
}

reach_quick_settings_layout reach_quick_settings_layout_for_content_bounds(
    reach_rect_f32 content_bounds,
    const reach_theme *theme,
    const reach_quick_settings_model *model
)
{
    (void)theme;

    reach_quick_settings_layout layout = {};
    layout.content_bounds = content_bounds;

    const float padding = 8.0f;
    const float text_padding = 12.0f;
    const float header_height = 16.0f;
    const float header_gap = 12.0f;
    const float pill_height = 24.0f;
    const float volume_gap = 10.0f;
    const float expand_gap = 10.0f;
    const float expand_height = 34.0f;
    const float icon_size = 18.0f;

    reach_rect_f32 pill_bounds = {};
    pill_bounds.x = content_bounds.x + padding;
    pill_bounds.y = content_bounds.y + padding + header_height + header_gap;
    pill_bounds.width = content_bounds.width - padding * 2.0f;
    pill_bounds.height = pill_height;
    if (pill_bounds.width < 0.0f) {
        pill_bounds.width = 0.0f;
    }

    layout.main_volume_pill =
        reach_quick_settings_volume_pill_layout_for_bounds(pill_bounds, theme);
    layout.main_slider_track = layout.main_volume_pill.slider_track;
    layout.main_slider_fill = layout.main_volume_pill.slider_fill;

    reach_rect_f32 last_pill_bounds = pill_bounds;
    layout.session_pill_count = 0;
    size_t visible_sessions = reach_quick_settings_visible_session_count(model);
    for (size_t index = 0; index < visible_sessions; ++index) {
        reach_rect_f32 session_bounds = pill_bounds;
        session_bounds.y =
            last_pill_bounds.y +
            last_pill_bounds.height +
            volume_gap +
            header_height +
            header_gap;
        layout.session_volume_pills[index] =
            reach_quick_settings_volume_pill_layout_for_bounds(
                session_bounds,
                theme);
        last_pill_bounds = session_bounds;
        layout.session_pill_count++;
    }

    layout.expand_button.x = content_bounds.x + padding;
    layout.expand_button.y = last_pill_bounds.y + last_pill_bounds.height + expand_gap;
    layout.expand_button.width = pill_bounds.width;
    layout.expand_button.height = expand_height;

    if (layout.expand_button.width < 0.0f) {
        layout.expand_button.width = 0.0f;
    }

    layout.expand_button_icon.width = icon_size;
    layout.expand_button_icon.height = icon_size;
    layout.expand_button_icon.x =
        layout.expand_button.x + layout.expand_button.width - padding - icon_size;
    layout.expand_button_icon.y =
        layout.expand_button.y + (layout.expand_button.height - icon_size) * 0.5f;

    layout.expand_button_label.x = layout.expand_button.x + text_padding;
    layout.expand_button_label.y = layout.expand_button.y;
    layout.expand_button_label.width =
        layout.expand_button_icon.x - layout.expand_button_label.x - padding;
    layout.expand_button_label.height = layout.expand_button.height;

    if (layout.expand_button_label.width < 0.0f) {
        layout.expand_button_label.width = 0.0f;
    }

    return layout;
}
