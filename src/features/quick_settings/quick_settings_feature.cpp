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

static size_t reach_quick_settings_visible_output_device_count(
    const reach_quick_settings_model *model
)
{
    if (model == nullptr || !model->output_devices_expanded) {
        return 0;
    }
    return model->output_devices.count < REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES
        ? model->output_devices.count
        : REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES;
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
    model->output_devices_expanded = 0;
    model->sessions = {};
    model->output_devices = {};
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

void reach_quick_settings_model_set_output_devices(
    reach_quick_settings_model *model,
    const reach_audio_output_device_list *devices
)
{
    if (model == nullptr) {
        return;
    }

    model->output_devices = {};
    if (devices == nullptr) {
        return;
    }

    model->output_devices.count = devices->count < REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES
        ? devices->count
        : REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES;
    for (size_t index = 0; index < model->output_devices.count; ++index) {
        model->output_devices.devices[index] = devices->devices[index];
        model->output_devices.devices[index].is_default =
            model->output_devices.devices[index].is_default ? 1 : 0;
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
    const float output_button_gap = 10.0f;
    const float output_button_height = 46.0f;
    const float output_title_height = 18.0f;
    const float output_title_gap = 12.0f;
    const float output_panel_gap = 8.0f;
    const float output_device_row_height = 44.0f;
    const float title_height = 18.0f;
    const float title_gap = 12.0f;
    const float panel_gap = 8.0f;
    const float app_volume_row_height = 40.0f;
    const float expand_gap = 10.0f;
    const float expand_height = 34.0f;

    size_t visible_sessions = reach_quick_settings_visible_session_count(model);
    size_t visible_output_devices = reach_quick_settings_visible_output_device_count(model);
    float volume_component_height = header_height + header_gap + pill_height;
    float output_component_height = 0.0f;
    if (model != nullptr && model->output_devices_expanded) {
        output_component_height =
            output_button_gap +
            output_title_height +
            output_panel_gap +
            (float)visible_output_devices * output_device_row_height;
    } else {
        output_component_height = output_button_gap + output_button_height;
    }
    float app_volume_component_height = 0.0f;
    if (model != nullptr && model->expanded) {
        app_volume_component_height =
            title_gap +
            title_height +
            panel_gap +
            (float)(visible_sessions + 1) * app_volume_row_height;
    } else {
        app_volume_component_height = expand_gap + expand_height;
    }
    return padding * 2.0f +
        volume_component_height +
        output_component_height +
        app_volume_component_height;
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
    const float output_button_gap = 10.0f;
    const float output_button_height = 46.0f;
    const float output_title_height = 18.0f;
    const float output_title_gap = 12.0f;
    const float output_panel_gap = 8.0f;
    const float output_device_row_height = 44.0f;
    const float output_icon_size = 16.0f;
    const float output_check_size = 16.0f;
    const float output_row_horizontal_padding = 12.0f;
    const float output_row_label_gap = 8.0f;
    const float title_height = 18.0f;
    const float title_gap = 12.0f;
    const float panel_gap = 8.0f;
    const float app_volume_row_height = 40.0f;
    const float app_icon_size = 16.0f;
    const float row_horizontal_padding = 12.0f;
    const float row_label_gap = 8.0f;
    const float row_slider_width = 78.0f;
    const float row_slider_gap = 10.0f;
    const float row_slider_line_height = 2.0f;
    const float row_thumb_size = 8.0f;
    const float row_percent_width = 38.0f;
    const float row_percent_gap = 10.0f;
    const float separator_inset = 12.0f;
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

    float next_y = pill_bounds.y + pill_bounds.height;

    layout.output_device_row_count = 0;
    size_t visible_output_devices = reach_quick_settings_visible_output_device_count(model);
    if (model != nullptr && model->output_devices_expanded) {
        layout.output_devices_title.x = pill_bounds.x;
        layout.output_devices_title.y = next_y + output_button_gap;
        layout.output_devices_title.width = pill_bounds.width;
        layout.output_devices_title.height = output_title_height;

        layout.output_devices_title_chevron.width = icon_size;
        layout.output_devices_title_chevron.height = icon_size;
        layout.output_devices_title_chevron.x =
            layout.output_devices_title.x + layout.output_devices_title.width -
            padding - icon_size;
        layout.output_devices_title_chevron.y =
            layout.output_devices_title.y +
            (layout.output_devices_title.height - icon_size) * 0.5f;

        layout.output_devices_panel.x = pill_bounds.x;
        layout.output_devices_panel.y =
            layout.output_devices_title.y +
            layout.output_devices_title.height +
            output_panel_gap;
        layout.output_devices_panel.width = pill_bounds.width;
        layout.output_devices_panel.height =
            (float)visible_output_devices * output_device_row_height;

        for (size_t index = 0; index < visible_output_devices; ++index) {
            reach_quick_settings_output_device_row_layout *row =
                &layout.output_device_rows[index];
            row->bounds.x = layout.output_devices_panel.x;
            row->bounds.y =
                layout.output_devices_panel.y +
                (float)index * output_device_row_height;
            row->bounds.width = layout.output_devices_panel.width;
            row->bounds.height = output_device_row_height;

            row->icon.width = output_icon_size;
            row->icon.height = output_icon_size;
            row->icon.x = row->bounds.x + output_row_horizontal_padding;
            row->icon.y =
                row->bounds.y + (row->bounds.height - output_icon_size) * 0.5f;

            row->checkmark.width = output_check_size;
            row->checkmark.height = output_check_size;
            row->checkmark.x =
                row->bounds.x + row->bounds.width -
                output_row_horizontal_padding -
                output_check_size;
            row->checkmark.y =
                row->bounds.y + (row->bounds.height - output_check_size) * 0.5f;

            row->label.x = row->icon.x + output_icon_size + output_row_label_gap;
            row->label.y = row->bounds.y;
            row->label.width =
                row->checkmark.x - output_row_label_gap - row->label.x;
            row->label.height = row->bounds.height;
            if (row->label.width < 0.0f) {
                row->label.width = 0.0f;
            }

            row->separator.x = row->bounds.x + separator_inset;
            row->separator.y = row->bounds.y + row->bounds.height - 1.0f;
            row->separator.width = row->bounds.width - separator_inset * 2.0f;
            row->separator.height = 1.0f;
            if (row->separator.width < 0.0f) {
                row->separator.width = 0.0f;
            }

            layout.output_device_row_count++;
        }

        next_y = layout.output_devices_panel.y + layout.output_devices_panel.height;
    } else {
        layout.output_device_button.x = content_bounds.x + padding;
        layout.output_device_button.y = next_y + output_button_gap;
        layout.output_device_button.width = pill_bounds.width;
        layout.output_device_button.height = output_button_height;
        if (layout.output_device_button.width < 0.0f) {
            layout.output_device_button.width = 0.0f;
        }

        layout.output_device_button_icon.width = output_icon_size;
        layout.output_device_button_icon.height = output_icon_size;
        layout.output_device_button_icon.x =
            layout.output_device_button.x + output_row_horizontal_padding;
        layout.output_device_button_icon.y =
            layout.output_device_button.y +
            (layout.output_device_button.height - output_icon_size) * 0.5f;

        layout.output_device_button_chevron.width = icon_size;
        layout.output_device_button_chevron.height = icon_size;
        layout.output_device_button_chevron.x =
            layout.output_device_button.x + layout.output_device_button.width -
            padding - icon_size;
        layout.output_device_button_chevron.y =
            layout.output_device_button.y +
            (layout.output_device_button.height - icon_size) * 0.5f;

        layout.output_device_button_label.x =
            layout.output_device_button_icon.x + output_icon_size + output_row_label_gap;
        layout.output_device_button_label.y = layout.output_device_button.y;
        layout.output_device_button_label.width =
            layout.output_device_button_chevron.x -
            layout.output_device_button_label.x -
            padding;
        layout.output_device_button_label.height = layout.output_device_button.height;
        if (layout.output_device_button_label.width < 0.0f) {
            layout.output_device_button_label.width = 0.0f;
        }

        next_y = layout.output_device_button.y + layout.output_device_button.height;
    }

    layout.app_volume_row_count = 0;
    size_t visible_sessions = reach_quick_settings_visible_session_count(model);
    if (model != nullptr && model->expanded) {
        layout.app_volumes_title.x = pill_bounds.x;
        layout.app_volumes_title.y = next_y + title_gap;
        layout.app_volumes_title.width = pill_bounds.width;
        layout.app_volumes_title.height = title_height;

        layout.app_volumes_panel.x = pill_bounds.x;
        layout.app_volumes_panel.y =
            layout.app_volumes_title.y + layout.app_volumes_title.height + panel_gap;
        layout.app_volumes_panel.width = pill_bounds.width;
        layout.app_volumes_panel.height = (float)(visible_sessions + 1) * app_volume_row_height;

        for (size_t index = 0; index < visible_sessions; ++index) {
            reach_quick_settings_app_volume_row_layout *row =
                &layout.app_volume_rows[index];
            row->bounds.x = layout.app_volumes_panel.x;
            row->bounds.y = layout.app_volumes_panel.y + (float)index * app_volume_row_height;
            row->bounds.width = layout.app_volumes_panel.width;
            row->bounds.height = app_volume_row_height;

            row->app_icon.width = app_icon_size;
            row->app_icon.height = app_icon_size;
            row->app_icon.x = row->bounds.x + row_horizontal_padding;
            row->app_icon.y = row->bounds.y + (row->bounds.height - app_icon_size) * 0.5f;

            row->app_volume_percent.x =
                row->bounds.x + row->bounds.width - row_horizontal_padding - row_percent_width;
            row->app_volume_percent.y = row->bounds.y;
            row->app_volume_percent.width = row_percent_width;
            row->app_volume_percent.height = row->bounds.height;

            row->slider_full_range_line.width = row_slider_width;
            row->slider_full_range_line.height = row_slider_line_height;
            row->slider_full_range_line.x =
                row->app_volume_percent.x - row_percent_gap - row_slider_width;
            row->slider_full_range_line.y =
                row->bounds.y + (row->bounds.height - row_slider_line_height) * 0.5f;
            if (row->slider_full_range_line.x < row->app_icon.x + app_icon_size + row_label_gap) {
                row->slider_full_range_line.x = row->app_icon.x + app_icon_size + row_label_gap;
                row->slider_full_range_line.width =
                    row->bounds.x + row->bounds.width - row_horizontal_padding -
                    row->slider_full_range_line.x;
                if (row->slider_full_range_line.width < 0.0f) {
                    row->slider_full_range_line.width = 0.0f;
                }
            }

            row->app_label.x = row->app_icon.x + app_icon_size + row_label_gap;
            row->app_label.y = row->bounds.y;
            row->app_label.width =
                row->slider_full_range_line.x - row_slider_gap - row->app_label.x;
            row->app_label.height = row->bounds.height;
            if (row->app_label.width < 0.0f) {
                row->app_label.width = 0.0f;
            }

            row->slider_level_line = row->slider_full_range_line;
            float session_level = 0.0f;
            if (model != nullptr && index < model->sessions.count) {
                session_level = reach_quick_settings_clamp01(
                    model->sessions.sessions[index].level);
            }
            row->slider_level_line.width =
                row->slider_full_range_line.width * session_level;
            row->slider_thumb.width = row_thumb_size;
            row->slider_thumb.height = row_thumb_size;
            row->slider_thumb.x =
                row->slider_full_range_line.x +
                row->slider_full_range_line.width * session_level -
                row_thumb_size * 0.5f;
            row->slider_thumb.y =
                row->bounds.y + (row->bounds.height - row_thumb_size) * 0.5f;

            row->separator.x = row->bounds.x + separator_inset;
            row->separator.y = row->bounds.y + row->bounds.height - 1.0f;
            row->separator.width = row->bounds.width - separator_inset * 2.0f;
            row->separator.height = 1.0f;
            if (row->separator.width < 0.0f) {
                row->separator.width = 0.0f;
            }

            layout.app_volume_row_count++;
        }

        next_y = layout.app_volumes_panel.y + layout.app_volumes_panel.height;

        layout.expand_button.x = layout.app_volumes_panel.x;
        layout.expand_button.y =
            layout.app_volumes_panel.y + (float)visible_sessions * app_volume_row_height;
        layout.expand_button.width = layout.app_volumes_panel.width;
        layout.expand_button.height = app_volume_row_height;
    } else {
        layout.expand_button.x = content_bounds.x + padding;
        layout.expand_button.y = next_y + expand_gap;
        layout.expand_button.width = pill_bounds.width;
        layout.expand_button.height = expand_height;
    }

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
