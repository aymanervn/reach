#include "reach/features/quick_settings.h"
#include "reach/features/common/popup.h"

#include "quick_settings_common.h"
#include "quick_settings_metrics.h"

#include <math.h>
#include <new>

static reach_rect_f32 reach_quick_settings_rect(float x, float y, float width, float height)
{
    reach_rect_f32 rect = {};
    rect.x = x;
    rect.y = y;
    rect.width = reach_quick_settings_clamp_min0(width);
    rect.height = reach_quick_settings_clamp_min0(height);
    return rect;
}

static reach_rect_f32 reach_quick_settings_content_line(reach_rect_f32 content_bounds, float y,
                                                        float height,
                                                        const reach_quick_settings_metrics *metrics)
{
    const reach_quick_settings_metrics *values =
        metrics != nullptr ? metrics : &reach_quick_settings_metrics_values;
    return reach_quick_settings_rect(content_bounds.x + values->content_padding, y,
                                     content_bounds.width - values->content_padding * 2.0f, height);
}

static size_t reach_quick_settings_two_column_row_count(size_t item_count)
{
    return (item_count + 1) / 2;
}

static float reach_quick_settings_stacked_rows_height(size_t row_count, float row_height,
                                                      float row_gap)
{
    if (row_count == 0)
    {
        return 0.0f;
    }
    return (float)row_count * row_height + (float)(row_count - 1) * row_gap;
}

static float reach_quick_settings_expandable_panel_span(float panel_height, float panel_gap)
{
    return panel_height > 0.0f ? panel_gap + panel_height : 0.0f;
}

static size_t reach_quick_settings_visible_session_count(const reach_quick_settings_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    return model->sessions.count < REACH_AUDIO_VOLUME_MAX_SESSIONS
               ? model->sessions.count
               : REACH_AUDIO_VOLUME_MAX_SESSIONS;
}

static size_t
reach_quick_settings_visible_output_device_count(const reach_quick_settings_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    return model->output_devices.count < REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES
               ? model->output_devices.count
               : REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES;
}

static size_t
reach_quick_settings_visible_system_tile_count(const reach_quick_settings_model *model)
{
    (void)model;
    return 3;
}

void reach_quick_settings_model_init(reach_quick_settings_model *model)
{
    if (model == nullptr)
    {
        return;
    }

    model->main_volume_level = 0.0f;
    model->main_muted = 0;
    model->expanded = 0;
    model->output_devices_expanded = 0;
    model->sessions = {};
    model->output_devices = {};
    model->network = {};
    model->bluetooth = {};
    model->bluetooth_pending = 0;
    model->bluetooth_pending_enabled = 0;
    model->brightness = {};
}

void reach_quick_settings_model_set_main_volume(reach_quick_settings_model *model,
                                                float volume_level, int32_t muted)
{
    if (model == nullptr)
    {
        return;
    }

    model->main_volume_level = reach_quick_settings_clamp01(volume_level);
    model->main_muted = muted ? 1 : 0;
}

uint32_t reach_quick_settings_volume_icon_id(float volume_level, int32_t muted)
{
    return reach_volume_level_icon(volume_level, muted);
}

void reach_quick_settings_model_set_sessions(reach_quick_settings_model *model,
                                             const reach_audio_volume_session_list *sessions)
{
    if (model == nullptr)
    {
        return;
    }

    model->sessions = {};
    if (sessions == nullptr)
    {
        return;
    }

    model->sessions.count = sessions->count < REACH_AUDIO_VOLUME_MAX_SESSIONS
                                ? sessions->count
                                : REACH_AUDIO_VOLUME_MAX_SESSIONS;
    for (size_t index = 0; index < model->sessions.count; ++index)
    {
        model->sessions.sessions[index] = sessions->sessions[index];
        model->sessions.sessions[index].level =
            reach_quick_settings_clamp01(model->sessions.sessions[index].level);
        model->sessions.sessions[index].muted = model->sessions.sessions[index].muted ? 1 : 0;
    }
}

void reach_quick_settings_model_set_output_devices(reach_quick_settings_model *model,
                                                   const reach_audio_output_device_list *devices)
{
    if (model == nullptr)
    {
        return;
    }

    model->output_devices = {};
    if (devices == nullptr)
    {
        return;
    }

    model->output_devices.count = devices->count < REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES
                                      ? devices->count
                                      : REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES;
    for (size_t index = 0; index < model->output_devices.count; ++index)
    {
        model->output_devices.devices[index] = devices->devices[index];
        model->output_devices.devices[index].is_default =
            model->output_devices.devices[index].is_default ? 1 : 0;
    }
}

void reach_quick_settings_model_set_system_states(reach_quick_settings_model *model,
                                                  const reach_network_state *network,
                                                  const reach_bluetooth_state *bluetooth,
                                                  const reach_brightness_state *brightness)
{
    if (model == nullptr)
    {
        return;
    }

    model->network = network != nullptr ? *network : reach_network_state{};
    model->bluetooth = bluetooth != nullptr ? *bluetooth : reach_bluetooth_state{};
    model->brightness = brightness != nullptr ? *brightness : reach_brightness_state{};

    if (model->network.signal_strength < 0)
    {
        model->network.signal_strength = 0;
    }
    if (model->network.signal_strength > 100)
    {
        model->network.signal_strength = 100;
    }
    model->brightness.level = reach_quick_settings_clamp01(model->brightness.level);
    model->bluetooth.available = model->bluetooth.available ? 1 : 0;
    model->bluetooth.enabled = model->bluetooth.enabled ? 1 : 0;
    model->brightness.available = model->brightness.available ? 1 : 0;
}

static void reach_quick_settings_model_set_bluetooth_pending(reach_quick_settings_model *model,
                                                             int32_t pending,
                                                             int32_t pending_enabled)
{
    if (model == nullptr)
    {
        return;
    }
    model->bluetooth_pending = pending ? 1 : 0;
    model->bluetooth_pending_enabled = pending_enabled ? 1 : 0;
}

void reach_quick_settings_volume_pill_model_init(reach_quick_settings_volume_pill_model *model,
                                                 float volume_level, int32_t muted,
                                                 const uint16_t *label)
{
    if (model == nullptr)
    {
        return;
    }

    model->volume_level = reach_quick_settings_clamp01(volume_level);
    model->muted = muted ? 1 : 0;
    model->icon_id = reach_quick_settings_volume_icon_id(model->volume_level, model->muted);
    model->session_instance_id[0] = 0;
    reach_copy_utf16(model->label, REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY, label);
}

reach_quick_settings_volume_pill_layout
reach_quick_settings_volume_pill_layout_for_bounds_scaled(reach_rect_f32 bounds,
                                                          const reach_theme *theme, float dpi_scale)
{
    (void)theme;

    reach_quick_settings_volume_pill_layout layout = {};
    layout.bounds = bounds;

    reach_quick_settings_metrics metrics = reach_quick_settings_metrics_for_scale(dpi_scale);

    layout.header_icon.x = bounds.x;
    layout.header_icon.y = bounds.y - metrics.pill_header_gap - metrics.pill_header_height +
                           (metrics.pill_header_height - metrics.pill_header_icon_size) * 0.5f;
    layout.header_icon.width = metrics.pill_header_icon_size;
    layout.header_icon.height = metrics.pill_header_icon_size;

    layout.header_label = reach_quick_settings_rect(
        layout.header_icon.x + metrics.pill_header_icon_size + metrics.pill_header_label_gap,
        bounds.y - metrics.pill_header_gap - metrics.pill_header_height,
        bounds.width - metrics.pill_header_icon_size - metrics.pill_header_label_gap,
        metrics.pill_header_height);

    layout.slider_track = bounds;

    layout.slider_fill = layout.slider_track;

    return layout;
}

float reach_quick_settings_content_height_for_model(const reach_quick_settings_model *model)
{
    return reach_quick_settings_content_height_for_model_scaled(model, 1.0f);
}

static void reach_quick_settings_section_spans(const reach_quick_settings_model *model,
                                               float dpi_scale, float *out_output_span,
                                               float *out_app_span)
{
    reach_quick_settings_metrics metrics = reach_quick_settings_metrics_for_scale(dpi_scale);
    *out_output_span = reach_quick_settings_expandable_panel_span(
        (float)reach_quick_settings_visible_output_device_count(model) *
            metrics.output_device_row_height,
        metrics.output_panel_gap);
    *out_app_span = reach_quick_settings_expandable_panel_span(
        (float)reach_quick_settings_visible_session_count(model) * metrics.app_volume_row_height,
        metrics.app_panel_gap);
}

static void reach_quick_settings_target_spans(const reach_quick_settings_model *model,
                                              float dpi_scale, float *out_output_span,
                                              float *out_app_span)
{
    reach_quick_settings_section_spans(model, dpi_scale, out_output_span, out_app_span);
    if (model == nullptr || !model->output_devices_expanded)
        *out_output_span = 0.0f;
    if (model == nullptr || !model->expanded)
        *out_app_span = 0.0f;
}

static float
reach_quick_settings_content_height_for_spans_scaled(const reach_quick_settings_model *model,
                                                     float output_devices_span,
                                                     float app_volumes_span, float dpi_scale)
{
    reach_quick_settings_metrics metrics = reach_quick_settings_metrics_for_scale(dpi_scale);

    size_t visible_tiles = reach_quick_settings_visible_system_tile_count(model);
    size_t grid_rows = reach_quick_settings_two_column_row_count(visible_tiles);
    float grid_component_height =
        reach_quick_settings_stacked_rows_height(grid_rows, metrics.system_grid_tile_height,
                                                 metrics.system_grid_gap) +
        metrics.system_grid_bottom_gap;
    float volume_component_height =
        metrics.pill_header_height + metrics.section_header_gap + metrics.pill_height;
    float brightness_component_height = 0.0f;
    if (model != nullptr && model->brightness.available)
    {
        brightness_component_height = metrics.pill_header_height + metrics.section_header_gap +
                                      metrics.pill_height + metrics.system_grid_bottom_gap;
    }
    float output_component_height =
        metrics.output_button_gap + metrics.output_button_height + output_devices_span;
    float app_volume_component_height =
        metrics.expand_button_gap + metrics.expand_button_height + app_volumes_span;
    return metrics.content_padding * 2.0f + grid_component_height + brightness_component_height +
           volume_component_height + output_component_height + app_volume_component_height;
}

float reach_quick_settings_content_height_for_model_scaled(const reach_quick_settings_model *model,
                                                           float dpi_scale)
{
    float output_span = 0, app_span = 0;
    reach_quick_settings_target_spans(model, dpi_scale, &output_span, &app_span);
    return reach_quick_settings_content_height_for_spans_scaled(model, output_span, app_span,
                                                                dpi_scale);
}

float reach_quick_settings_volume_pill_level_for_x(
    const reach_quick_settings_volume_pill_layout *layout, float x)
{
    if (layout == nullptr || layout->slider_track.width <= 0.0f)
    {
        return 0.0f;
    }

    return reach_quick_settings_clamp01((x - layout->slider_track.x) / layout->slider_track.width);
}

static void reach_quick_settings_place_system_tile(reach_quick_settings_tile_layout *tile,
                                                   reach_rect_f32 grid_bounds, float tile_width,
                                                   size_t tile_index,
                                                   const reach_quick_settings_metrics *metrics)
{
    if (tile == nullptr)
    {
        return;
    }

    const reach_quick_settings_metrics *values =
        metrics != nullptr ? metrics : &reach_quick_settings_metrics_values;
    size_t tile_row = tile_index / 2;
    size_t tile_column = tile_index % 2;

    tile->bounds = reach_quick_settings_rect(
        grid_bounds.x + (float)tile_column * (tile_width + values->system_grid_gap),
        grid_bounds.y +
            (float)tile_row * (values->system_grid_tile_height + values->system_grid_gap),
        tile_width, values->system_grid_tile_height);

    tile->icon_background = reach_quick_settings_rect(
        tile->bounds.x + values->system_tile_icon_inset,
        tile->bounds.y + (tile->bounds.height - values->system_tile_icon_box) * 0.5f,
        values->system_tile_icon_box, values->system_tile_icon_box);

    tile->icon = reach_quick_settings_rect(
        tile->icon_background.x +
            (values->system_tile_icon_box - values->system_tile_icon_size) * 0.5f,
        tile->icon_background.y +
            (values->system_tile_icon_box - values->system_tile_icon_size) * 0.5f,
        values->system_tile_icon_size, values->system_tile_icon_size);

    float label_x =
        tile->icon_background.x + values->system_tile_icon_box + values->system_tile_icon_gap;
    tile->label = reach_quick_settings_rect(label_x, tile->bounds.y,
                                            tile->bounds.x + tile->bounds.width -
                                                values->content_padding - label_x,
                                            tile->bounds.height);
}

reach_quick_settings_layout
reach_quick_settings_layout_for_content_bounds(reach_rect_f32 content_bounds,
                                               const reach_theme *theme,
                                               const reach_quick_settings_model *model)
{
    return reach_quick_settings_layout_for_content_bounds_scaled(content_bounds, theme, model,
                                                                 1.0f);
}

reach_quick_settings_output_device_row_layout
reach_quick_settings_output_device_row_layout_at(reach_rect_f32 panel, float top, float dpi_scale)
{
    reach_quick_settings_metrics metrics = reach_quick_settings_metrics_for_scale(dpi_scale);
    reach_quick_settings_output_device_row_layout row = {};
    row.bounds.x = panel.x;
    row.bounds.y = top;
    row.bounds.width = panel.width;
    row.bounds.height = metrics.output_device_row_height;

    row.icon.width = metrics.output_icon_size;
    row.icon.height = metrics.output_icon_size;
    row.icon.x = row.bounds.x + metrics.output_row_horizontal_padding;
    row.icon.y = row.bounds.y + (row.bounds.height - metrics.output_icon_size) * 0.5f;

    row.checkmark.width = metrics.output_check_size;
    row.checkmark.height = metrics.output_check_size;
    row.checkmark.x = row.bounds.x + row.bounds.width - metrics.output_row_horizontal_padding -
                      metrics.output_check_size;
    row.checkmark.y = row.bounds.y + (row.bounds.height - metrics.output_check_size) * 0.5f;

    row.label.x = row.icon.x + metrics.output_icon_size + metrics.output_row_label_gap;
    row.label.y = row.bounds.y;
    row.label.width = row.checkmark.x - metrics.output_row_label_gap - row.label.x;
    row.label.height = row.bounds.height;
    if (row.label.width < 0.0f)
    {
        row.label.width = 0.0f;
    }

    row.separator.x = row.bounds.x + metrics.separator_inset;
    row.separator.y = row.bounds.y + row.bounds.height - metrics.separator_thickness;
    row.separator.width = row.bounds.width - metrics.separator_inset * 2.0f;
    row.separator.height = metrics.separator_thickness;
    if (row.separator.width < 0.0f)
    {
        row.separator.width = 0.0f;
    }
    return row;
}

reach_quick_settings_app_volume_row_layout
reach_quick_settings_app_volume_row_layout_at(reach_rect_f32 panel, float top, float level,
                                              float dpi_scale)
{
    reach_quick_settings_metrics metrics = reach_quick_settings_metrics_for_scale(dpi_scale);
    reach_quick_settings_app_volume_row_layout row = {};
    row.bounds.x = panel.x;
    row.bounds.y = top;
    row.bounds.width = panel.width;
    row.bounds.height = metrics.app_volume_row_height;

    row.app_icon.width = metrics.app_icon_size;
    row.app_icon.height = metrics.app_icon_size;
    row.app_icon.x = row.bounds.x + metrics.app_row_horizontal_padding;
    row.app_icon.y = row.bounds.y + (row.bounds.height - metrics.app_icon_size) * 0.5f;

    row.app_volume_percent.x = row.bounds.x + row.bounds.width -
                               metrics.app_row_horizontal_padding - metrics.app_row_percent_width;
    row.app_volume_percent.y = row.bounds.y;
    row.app_volume_percent.width = metrics.app_row_percent_width;
    row.app_volume_percent.height = row.bounds.height;

    row.slider_full_range_line.width = metrics.app_row_slider_width;
    row.slider_full_range_line.height = metrics.app_row_slider_line_height;
    row.slider_full_range_line.x =
        row.app_volume_percent.x - metrics.app_row_percent_gap - metrics.app_row_slider_width;
    row.slider_full_range_line.y =
        row.bounds.y + (row.bounds.height - metrics.app_row_slider_line_height) * 0.5f;
    if (row.slider_full_range_line.x <
        row.app_icon.x + metrics.app_icon_size + metrics.app_row_label_gap)
    {
        row.slider_full_range_line.x =
            row.app_icon.x + metrics.app_icon_size + metrics.app_row_label_gap;
        row.slider_full_range_line.width = row.bounds.x + row.bounds.width -
                                           metrics.app_row_horizontal_padding -
                                           row.slider_full_range_line.x;
        if (row.slider_full_range_line.width < 0.0f)
        {
            row.slider_full_range_line.width = 0.0f;
        }
    }

    row.app_label.x = row.app_icon.x + metrics.app_icon_size + metrics.app_row_label_gap;
    row.app_label.y = row.bounds.y;
    row.app_label.width =
        row.slider_full_range_line.x - metrics.app_row_slider_gap - row.app_label.x;
    row.app_label.height = row.bounds.height;
    if (row.app_label.width < 0.0f)
    {
        row.app_label.width = 0.0f;
    }

    float session_level = reach_quick_settings_clamp01(level);
    row.slider_level_line = row.slider_full_range_line;
    row.slider_level_line.width = row.slider_full_range_line.width * session_level;
    row.slider_thumb.width = metrics.app_row_thumb_size;
    row.slider_thumb.height = metrics.app_row_thumb_size;
    row.slider_thumb.x = row.slider_full_range_line.x +
                         row.slider_full_range_line.width * session_level -
                         metrics.app_row_thumb_size * 0.5f;
    row.slider_thumb.y = row.bounds.y + (row.bounds.height - metrics.app_row_thumb_size) * 0.5f;

    row.separator.x = row.bounds.x + metrics.separator_inset;
    row.separator.y = row.bounds.y + row.bounds.height - metrics.separator_thickness;
    row.separator.width = row.bounds.width - metrics.separator_inset * 2.0f;
    row.separator.height = metrics.separator_thickness;
    if (row.separator.width < 0.0f)
    {
        row.separator.width = 0.0f;
    }
    return row;
}

reach_quick_settings_layout reach_quick_settings_layout_for_sections_scaled(
    reach_rect_f32 content_bounds, const reach_theme *theme,
    const reach_quick_settings_model *model, float output_devices_span, float app_volumes_span,
    float dpi_scale)
{
    (void)theme;

    reach_quick_settings_layout layout = {};
    layout.content_bounds = content_bounds;

    reach_quick_settings_metrics metrics = reach_quick_settings_metrics_for_scale(dpi_scale);

    reach_rect_f32 grid_bounds = reach_quick_settings_content_line(
        content_bounds, content_bounds.y + metrics.content_padding, 0.0f, &metrics);

    float tile_width =
        reach_quick_settings_clamp_min0((grid_bounds.width - metrics.system_grid_gap) * 0.5f);

    size_t tile_index = 0;
    reach_quick_settings_place_system_tile(&layout.network_tile, grid_bounds, tile_width,
                                           tile_index++, &metrics);
    reach_quick_settings_place_system_tile(&layout.bluetooth_tile, grid_bounds, tile_width,
                                           tile_index++, &metrics);
    reach_quick_settings_place_system_tile(&layout.project_tile, grid_bounds, tile_width,
                                           tile_index++, &metrics);

    layout.system_tile_count = tile_index;
    size_t grid_rows = reach_quick_settings_two_column_row_count(tile_index);
    layout.system_grid_bounds = grid_bounds;
    layout.system_grid_bounds.height = reach_quick_settings_stacked_rows_height(
        grid_rows, metrics.system_grid_tile_height, metrics.system_grid_gap);

    float next_y = layout.system_grid_bounds.y + layout.system_grid_bounds.height +
                   metrics.system_grid_bottom_gap;

    if (model != nullptr && model->brightness.available)
    {
        reach_rect_f32 brightness_bounds = reach_quick_settings_content_line(
            content_bounds, next_y + metrics.pill_header_height + metrics.section_header_gap,
            metrics.pill_height, &metrics);
        layout.brightness_pill = reach_quick_settings_volume_pill_layout_for_bounds_scaled(
            brightness_bounds, theme, dpi_scale);
        layout.brightness_slider_track = layout.brightness_pill.slider_track;
        layout.brightness_slider_fill = layout.brightness_pill.slider_fill;
        next_y = brightness_bounds.y + brightness_bounds.height + metrics.system_grid_bottom_gap;
    }

    reach_rect_f32 pill_bounds = reach_quick_settings_content_line(
        content_bounds, next_y + metrics.pill_header_height + metrics.section_header_gap,
        metrics.pill_height, &metrics);

    layout.main_volume_pill =
        reach_quick_settings_volume_pill_layout_for_bounds_scaled(pill_bounds, theme, dpi_scale);
    layout.main_slider_track = layout.main_volume_pill.slider_track;
    layout.main_slider_fill = layout.main_volume_pill.slider_fill;

    next_y = pill_bounds.y + pill_bounds.height;

    layout.output_device_row_count = 0;
    size_t visible_output_devices = reach_quick_settings_visible_output_device_count(model);
    layout.output_device_button = reach_quick_settings_content_line(
        content_bounds, next_y + metrics.output_button_gap, metrics.output_button_height, &metrics);

    layout.output_device_button_icon.width = metrics.output_icon_size;
    layout.output_device_button_icon.height = metrics.output_icon_size;
    layout.output_device_button_icon.x =
        layout.output_device_button.x + metrics.output_row_horizontal_padding;
    layout.output_device_button_icon.y =
        layout.output_device_button.y +
        (layout.output_device_button.height - metrics.output_icon_size) * 0.5f;

    layout.output_device_button_chevron.width = metrics.chevron_icon_size;
    layout.output_device_button_chevron.height = metrics.chevron_icon_size;
    layout.output_device_button_chevron.x =
        layout.output_device_button.x + layout.output_device_button.width -
        2.0f * metrics.content_padding - metrics.chevron_icon_size;
    layout.output_device_button_chevron.y =
        layout.output_device_button.y +
        (layout.output_device_button.height - metrics.chevron_icon_size) * 0.5f;

    layout.output_device_button_label.x = layout.output_device_button_icon.x +
                                          metrics.output_icon_size + metrics.output_row_label_gap;
    layout.output_device_button_label.y = layout.output_device_button.y;
    layout.output_device_button_label.width = layout.output_device_button_chevron.x -
                                              layout.output_device_button_label.x -
                                              metrics.content_padding;
    layout.output_device_button_label.height = layout.output_device_button.height;
    if (layout.output_device_button_label.width < 0.0f)
    {
        layout.output_device_button_label.width = 0.0f;
    }

    layout.output_devices_panel.x = layout.output_device_button.x;
    layout.output_devices_panel.y = layout.output_device_button.y +
                                    layout.output_device_button.height + metrics.output_panel_gap;
    layout.output_devices_panel.width = layout.output_device_button.width;
    layout.output_devices_panel.height =
        (float)visible_output_devices * metrics.output_device_row_height;
    layout.output_devices_clip = layout.output_devices_panel;
    layout.output_devices_clip.height = output_devices_span - metrics.output_panel_gap;

    for (size_t index = 0; index < visible_output_devices; ++index)
    {
        layout.output_device_rows[index] = reach_quick_settings_output_device_row_layout_at(
            layout.output_devices_panel,
            layout.output_devices_panel.y + (float)index * metrics.output_device_row_height,
            dpi_scale);
        layout.output_device_row_count++;
    }

    next_y =
        layout.output_device_button.y + layout.output_device_button.height + output_devices_span;

    layout.app_volume_row_count = 0;
    size_t visible_sessions = reach_quick_settings_visible_session_count(model);
    layout.expand_button = reach_quick_settings_content_line(
        content_bounds, next_y + metrics.expand_button_gap, metrics.expand_button_height, &metrics);

    layout.app_volumes_panel.x = layout.expand_button.x;
    layout.app_volumes_panel.y =
        layout.expand_button.y + layout.expand_button.height + metrics.app_panel_gap;
    layout.app_volumes_panel.width = layout.expand_button.width;
    layout.app_volumes_panel.height = (float)visible_sessions * metrics.app_volume_row_height;
    layout.app_volumes_clip = layout.app_volumes_panel;
    layout.app_volumes_clip.height = app_volumes_span - metrics.app_panel_gap;

    for (size_t index = 0; index < visible_sessions; ++index)
    {
        layout.app_volume_rows[index] = reach_quick_settings_app_volume_row_layout_at(
            layout.app_volumes_panel,
            layout.app_volumes_panel.y + (float)index * metrics.app_volume_row_height,
            index < model->sessions.count ? model->sessions.sessions[index].level : 0.0f,
            dpi_scale);
        layout.app_volume_row_count++;
    }

    if (layout.expand_button.width < 0.0f)
    {
        layout.expand_button.width = 0.0f;
    }

    layout.expand_button_icon.width = metrics.chevron_icon_size;
    layout.expand_button_icon.height = metrics.chevron_icon_size;
    layout.expand_button_icon.x = layout.expand_button.x + layout.expand_button.width -
                                  2.0f * metrics.content_padding - metrics.chevron_icon_size;
    layout.expand_button_icon.y =
        layout.expand_button.y + (layout.expand_button.height - metrics.chevron_icon_size) * 0.5f;

    layout.expand_button_label.x = layout.expand_button.x + metrics.text_padding;
    layout.expand_button_label.y = layout.expand_button.y;
    layout.expand_button_label.width =
        layout.expand_button_icon.x - layout.expand_button_label.x - metrics.content_padding;
    layout.expand_button_label.height = layout.expand_button.height;

    if (layout.expand_button_label.width < 0.0f)
    {
        layout.expand_button_label.width = 0.0f;
    }

    return layout;
}

reach_quick_settings_layout reach_quick_settings_layout_for_content_bounds_scaled(
    reach_rect_f32 content_bounds, const reach_theme *theme,
    const reach_quick_settings_model *model, float dpi_scale)
{
    float output_span = 0, app_span = 0;
    reach_quick_settings_target_spans(model, dpi_scale, &output_span, &app_span);
    return reach_quick_settings_layout_for_sections_scaled(content_bounds, theme, model,
                                                           output_span, app_span, dpi_scale);
}

static const float REACH_QUICK_SETTINGS_POPUP_MARGIN = 8.0f;

#define REACH_QUICK_SETTINGS_MAX_RETIRED_RENDER_ICONS                                              \
    (2 * (REACH_AUDIO_VOLUME_MAX_SESSIONS + REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES))

struct reach_quick_settings
{
    reach_motion height;
    reach_motion layout_progress;
    reach_motion scroll_maximum;
    reach_quick_settings_layout_context layout_context = {};
    int32_t layout_initialized = 0;
    double pending_motion_seconds = 0;
    float content_target_height = 0;
    float output_content_span = 0;
    float app_content_span = 0;
    reach_feature_transition popup_transition;
    reach_pressable pressable;
    reach_motion press_feedback;
    reach_motion network_tile_state;
    reach_motion bluetooth_tile_state;
    reach_keyed_row output_device_rows[REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES];
    reach_quick_settings_output_device_row
        output_device_items[REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES];
    reach_keyed_row app_volume_rows[REACH_AUDIO_VOLUME_MAX_SESSIONS];
    reach_quick_settings_app_volume_row app_volume_items[REACH_AUDIO_VOLUME_MAX_SESSIONS];
    reach_quick_settings_action press_action;
    reach_quick_settings_state state;

    reach_system_status *status;

    int32_t relayout_animate_pending;
    int32_t section_cleanup_pending;
    reach_theme layout_theme;
    float layout_dpi = 1.0f;

    uint64_t retired_render_icons[REACH_QUICK_SETTINGS_MAX_RETIRED_RENDER_ICONS];
    size_t retired_render_icon_count;
};

static reach_pressable_feedback_style
reach_quick_settings_pressable_feedback(reach_quick_settings *quick_settings)
{
    reach_pressable_feedback_style feedback = {};
    feedback.motion = quick_settings != nullptr ? &quick_settings->press_feedback : nullptr;
    feedback.pressed_value = 1.0f;
    return feedback;
}

static void reach_quick_settings_reset_pressable(reach_quick_settings *quick_settings)
{
    if (quick_settings == nullptr)
    {
        return;
    }
    reach_pressable_feedback_style feedback =
        reach_quick_settings_pressable_feedback(quick_settings);
    reach_pressable_reset(&quick_settings->pressable, &feedback);
    quick_settings->press_action = {};
}

const reach_quick_settings_state *
reach_quick_settings_state_ptr(reach_quick_settings *quick_settings)
{
    return quick_settings != nullptr ? &quick_settings->state : nullptr;
}

reach_quick_settings_state *reach_quick_settings_state_mut(reach_quick_settings *quick_settings)
{
    return quick_settings != nullptr ? &quick_settings->state : nullptr;
}

static const reach_theme *
reach_quick_settings_motion_theme(const reach_quick_settings *quick_settings)
{
    return quick_settings->layout_initialized ? &quick_settings->layout_theme : nullptr;
}

static void reach_quick_settings_fade_to(const reach_quick_settings *quick_settings,
                                         reach_motion *motion, int32_t visible, double now_seconds)
{
    float target = visible ? 1.0f : 0.0f;
    if (motion->generation != 0 && motion->to == target)
        return;
    reach_motion_animate_spec_to_at(
        motion, target,
        reach_theme_motion_preset(reach_quick_settings_motion_theme(quick_settings),
                                  visible ? REACH_MOTION_PRESET_FADE_IN
                                          : REACH_MOTION_PRESET_FADE_OUT),
        now_seconds);
}

static void reach_quick_settings_retire_render_icon(reach_quick_settings *quick_settings,
                                                    uint64_t icon_id);

static const uint16_t (&reach_quick_settings_row_key(const reach_audio_output_device &item))
    [REACH_AUDIO_VOLUME_DEVICE_ID_CAPACITY] { return item.device_id; }

static const uint16_t (&reach_quick_settings_row_key(const reach_audio_volume_session &item))
    [REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY] { return item.session_instance_id; }

static reach_audio_output_device &reach_quick_settings_row_item(
    reach_quick_settings_output_device_row &row)
{
    return row.device;
}

static reach_audio_volume_session &
reach_quick_settings_row_item(reach_quick_settings_app_volume_row &row)
{
    return row.session;
}

static const reach_audio_output_device &
reach_quick_settings_row_item(const reach_quick_settings_output_device_row &row)
{
    return row.device;
}

static const reach_audio_volume_session &
reach_quick_settings_row_item(const reach_quick_settings_app_volume_row &row)
{
    return row.session;
}

template <size_t Capacity>
static bool reach_quick_settings_row_keys_equal(const uint16_t (&first)[Capacity],
                                                const uint16_t (&second)[Capacity])
{
    for (size_t index = 0; index < Capacity; ++index)
    {
        if (first[index] != second[index])
            return false;
        if (first[index] == 0)
            return true;
    }
    return true;
}

template <typename Item, typename Payload> struct reach_quick_settings_row_identity
{
    const Payload *payloads;
    const Item *items;
};

template <typename Item, typename Payload>
static int32_t reach_quick_settings_row_same(const void *context, size_t slot, size_t item_index)
{
    const auto *identity =
        static_cast<const reach_quick_settings_row_identity<Item, Payload> *>(context);
    return reach_quick_settings_row_keys_equal(
        reach_quick_settings_row_key(reach_quick_settings_row_item(identity->payloads[slot])),
        reach_quick_settings_row_key(identity->items[item_index]));
}

template <typename Payload>
static void reach_quick_settings_release_row(reach_quick_settings *quick_settings, Payload *payload,
                                             int32_t previous_lifecycle)
{
    if (previous_lifecycle == REACH_KEYED_ROW_RETIRING)
        reach_quick_settings_retire_render_icon(quick_settings,
                                                reach_quick_settings_row_item(*payload).icon_id);
    *payload = {};
}

template <typename Payload, size_t Capacity>
static int32_t reach_quick_settings_release_settled_rows(reach_quick_settings *quick_settings,
                                                         reach_keyed_row (&rows)[Capacity],
                                                         Payload (&payloads)[Capacity])
{
    reach_keyed_row_change changes[Capacity];
    size_t count = reach_keyed_rows_release_settled(rows, Capacity, changes, Capacity);
    for (size_t index = 0; index < count; ++index)
        reach_quick_settings_release_row(quick_settings, &payloads[changes[index].slot],
                                         changes[index].previous_lifecycle);
    return count > 0;
}

template <typename Payload, typename Item, size_t Capacity>
static void reach_quick_settings_sync_rows(reach_quick_settings *quick_settings,
                                           reach_keyed_row (&rows)[Capacity],
                                           Payload (&payloads)[Capacity], const Item *items,
                                           size_t count, float row_height, int32_t animate,
                                           double now_seconds)
{
    count = count < Capacity ? count : Capacity;
    float targets[Capacity] = {};
    for (size_t index = 0; index < count; ++index)
        targets[index] = (float)index * row_height;
    reach_quick_settings_row_identity<Item, Payload> identity = {payloads, items};
    reach_keyed_rows_sync_input input = {};
    input.count = count;
    input.targets = targets;
    input.same = reach_quick_settings_row_same<Item, Payload>;
    input.context = &identity;
    input.animate = animate;
    input.theme = reach_quick_settings_motion_theme(quick_settings);
    input.now_seconds = now_seconds;
    reach_keyed_row_change changes[REACH_KEYED_ROWS_MAX_CHANGES(Capacity)];
    size_t change_count = reach_keyed_rows_sync(rows, Capacity, &input, changes,
                                                REACH_KEYED_ROWS_MAX_CHANGES(Capacity));
    for (size_t index = 0; index < change_count; ++index)
    {
        const reach_keyed_row_change &change = changes[index];
        Payload &payload = payloads[change.slot];
        if (change.kind == REACH_KEYED_ROW_RELEASED)
            reach_quick_settings_release_row(quick_settings, &payload, change.previous_lifecycle);
        else if (change.kind == REACH_KEYED_ROW_REVIVED &&
                 reach_quick_settings_row_item(payload).icon_id != items[change.index].icon_id)
            reach_quick_settings_retire_render_icon(quick_settings,
                                                    reach_quick_settings_row_item(payload).icon_id);
    }
    for (size_t slot = 0; slot < Capacity; ++slot)
    {
        if (rows[slot].lifecycle != REACH_KEYED_ROW_LIVE)
            continue;
        size_t index = rows[slot].index;
        payloads[slot].separator = index + 1 < count;
        reach_quick_settings_row_item(payloads[slot]) = items[index];
    }
}

template <typename Payload, typename Item, size_t Capacity>
static void reach_quick_settings_export_rows(const reach_keyed_row (&rows)[Capacity],
                                             const Payload (&payloads)[Capacity], const Item *items,
                                             size_t count, reach_keyed_row *out_rows,
                                             Payload *out_payloads)
{
    for (size_t slot = 0; slot < Capacity; ++slot)
    {
        out_rows[slot] = rows[slot];
        out_payloads[slot] = payloads[slot];
        size_t index = rows[slot].index;
        if (rows[slot].lifecycle == REACH_KEYED_ROW_LIVE && index < count &&
            reach_quick_settings_row_keys_equal(
                reach_quick_settings_row_key(reach_quick_settings_row_item(payloads[slot])),
                reach_quick_settings_row_key(items[index])))
            reach_quick_settings_row_item(out_payloads[slot]) = items[index];
    }
}

void reach_quick_settings_row_motions(
    const reach_quick_settings *quick_settings,
    reach_keyed_row out_output_motions[REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES],
    reach_quick_settings_output_device_row
        out_output_devices[REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES],
    reach_keyed_row out_app_motions[REACH_AUDIO_VOLUME_MAX_SESSIONS],
    reach_quick_settings_app_volume_row out_app_volumes[REACH_AUDIO_VOLUME_MAX_SESSIONS])
{
    if (quick_settings == nullptr || out_output_motions == nullptr ||
        out_output_devices == nullptr || out_app_motions == nullptr || out_app_volumes == nullptr)
        return;
    const reach_quick_settings_model &model = quick_settings->state.model;
    reach_quick_settings_export_rows(quick_settings->output_device_rows,
                                     quick_settings->output_device_items,
                                     model.output_devices.devices, model.output_devices.count,
                                     out_output_motions, out_output_devices);
    reach_quick_settings_export_rows(quick_settings->app_volume_rows,
                                     quick_settings->app_volume_items, model.sessions.sessions,
                                     model.sessions.count, out_app_motions, out_app_volumes);
}

static int32_t reach_quick_settings_output_shown(const reach_quick_settings *quick_settings)
{
    const reach_quick_settings_state &state = quick_settings->state;
    return state.model.output_devices_expanded || reach_motion_active(&state.output_devices_span) ||
           reach_motion_active(&state.output_devices_opacity);
}

static int32_t reach_quick_settings_apps_shown(const reach_quick_settings *quick_settings)
{
    const reach_quick_settings_state &state = quick_settings->state;
    return state.model.expanded || reach_motion_active(&state.app_volumes_span) ||
           reach_motion_active(&state.app_volumes_opacity);
}

static int32_t reach_quick_settings_rows_animate(const reach_quick_settings *quick_settings)
{
    return quick_settings->state.open && quick_settings->layout_initialized;
}

static void reach_quick_settings_sync_output_rows(reach_quick_settings *quick_settings,
                                                  float dpi_scale, int32_t animate)
{
    const reach_quick_settings_model &model = quick_settings->state.model;
    reach_quick_settings_sync_rows(
        quick_settings, quick_settings->output_device_rows, quick_settings->output_device_items,
        model.output_devices.devices, model.output_devices.count,
        reach_quick_settings_metrics_for_scale(dpi_scale).output_device_row_height,
        animate && reach_quick_settings_output_shown(quick_settings), reach_motion_now_seconds());
    if (reach_keyed_rows_active(quick_settings->output_device_rows,
                                REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES))
        quick_settings->section_cleanup_pending = 1;
}

static void reach_quick_settings_sync_app_rows(reach_quick_settings *quick_settings,
                                               float dpi_scale, int32_t animate)
{
    const reach_quick_settings_model &model = quick_settings->state.model;
    reach_quick_settings_sync_rows(
        quick_settings, quick_settings->app_volume_rows, quick_settings->app_volume_items,
        model.sessions.sessions, model.sessions.count,
        reach_quick_settings_metrics_for_scale(dpi_scale).app_volume_row_height,
        animate && reach_quick_settings_apps_shown(quick_settings), reach_motion_now_seconds());
    if (reach_keyed_rows_active(quick_settings->app_volume_rows, REACH_AUDIO_VOLUME_MAX_SESSIONS))
        quick_settings->section_cleanup_pending = 1;
}

static int32_t reach_quick_settings_sections_active(const reach_quick_settings *quick_settings)
{
    const reach_quick_settings_state &state = quick_settings->state;
    return reach_motion_active(&state.output_devices_expansion) ||
           reach_motion_active(&state.app_volumes_expansion) ||
           reach_motion_active(&state.output_devices_span) ||
           reach_motion_active(&state.app_volumes_span) ||
           reach_motion_active(&state.output_devices_opacity) ||
           reach_motion_active(&state.app_volumes_opacity) ||
           reach_keyed_rows_active(quick_settings->output_device_rows,
                                   REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES) ||
           reach_keyed_rows_active(quick_settings->app_volume_rows,
                                   REACH_AUDIO_VOLUME_MAX_SESSIONS);
}

static void reach_quick_settings_snap_sections(reach_quick_settings *quick_settings,
                                               float dpi_scale)
{
    reach_quick_settings_state *state = &quick_settings->state;
    float output = state->model.output_devices_expanded ? 1.0f : 0.0f;
    float apps = state->model.expanded ? 1.0f : 0.0f;
    float output_span = 0, app_span = 0;
    reach_quick_settings_target_spans(&state->model, dpi_scale, &output_span, &app_span);
    reach_motion_set(&state->output_devices_expansion, output);
    reach_motion_set(&state->app_volumes_expansion, apps);
    reach_motion_set(&state->output_devices_span, output_span);
    reach_motion_set(&state->app_volumes_span, app_span);
    reach_motion_set(&state->output_devices_opacity, output);
    reach_motion_set(&state->app_volumes_opacity, apps);
    reach_quick_settings_sync_output_rows(quick_settings, dpi_scale, 0);
    reach_quick_settings_sync_app_rows(quick_settings, dpi_scale, 0);
}

static void reach_quick_settings_retarget_sections(reach_quick_settings *quick_settings,
                                                   double now_seconds)
{
    reach_quick_settings_state &state = quick_settings->state;
    reach_quick_settings_fade_to(quick_settings, &state.output_devices_opacity,
                                 state.model.output_devices_expanded, now_seconds);
    reach_quick_settings_fade_to(quick_settings, &state.app_volumes_opacity, state.model.expanded,
                                 now_seconds);
    quick_settings->section_cleanup_pending = 1;
    quick_settings->pending_motion_seconds = now_seconds;
    quick_settings->relayout_animate_pending = 1;
}

static void reach_quick_settings_sync_tile_states(reach_quick_settings *quick_settings,
                                                  int32_t animate)
{
    const reach_quick_settings_model &model = quick_settings->state.model;
    int32_t bluetooth =
        model.bluetooth_pending ? model.bluetooth_pending_enabled : model.bluetooth.enabled;
    reach_motion *motions[] = {&quick_settings->network_tile_state,
                               &quick_settings->bluetooth_tile_state};
    int32_t active[] = {model.network.connected ? 1 : 0, bluetooth ? 1 : 0};
    double now_seconds = reach_motion_now_seconds();
    for (size_t index = 0; index < 2; ++index)
    {
        if (animate)
            reach_quick_settings_fade_to(quick_settings, motions[index], active[index],
                                         now_seconds);
        else
            reach_motion_set(motions[index], (float)active[index]);
    }
}

static int32_t reach_quick_settings_tiles_animate(const reach_quick_settings *quick_settings)
{
    return quick_settings->state.open &&
           reach_feature_transition_visible(&quick_settings->popup_transition);
}

static void reach_quick_settings_sample_input_layout(reach_quick_settings *quick_settings,
                                                     double now_seconds)
{
    reach_quick_settings_state &state = quick_settings->state;
    reach_rect_f32 body = reach_rect_motion_value_at(&state.surface_motion, now_seconds);
    state.bounds = body;
    state.bounds.x += state.render_bounds.x;
    state.bounds.y += state.render_bounds.y;
    state.content_bounds = reach_quick_settings_content_bounds_for(
        body, &quick_settings->layout_theme, quick_settings->layout_dpi, state.drop_direction);
    state.scroll_offset = reach_motion_value_at(&state.scroll_motion, now_seconds);
    float output = reach_motion_value_at(&state.output_devices_span, now_seconds);
    float apps = reach_motion_value_at(&state.app_volumes_span, now_seconds);
    REACH_ASSERT(isfinite(output) && isfinite(apps));
    float height = reach_quick_settings_content_height_for_spans_scaled(&state.model, output, apps,
                                                                        quick_settings->layout_dpi);
    state.scroll_max = fmaxf(0.0f, height - state.content_bounds.height);
    state.scroll_offset = fminf(state.scroll_offset, state.scroll_max);
    reach_rect_f32 bounds = state.content_bounds;
    bounds.y -= state.scroll_offset;
    bounds.height = height;
    state.layout = reach_quick_settings_layout_for_sections_scaled(
        bounds, &quick_settings->layout_theme, &state.model, output, apps,
        quick_settings->layout_dpi);
    if (reach_motion_value_at(&state.output_devices_opacity, now_seconds) <= 0)
        state.layout.output_devices_clip.height = 0;
    if (reach_motion_value_at(&state.app_volumes_opacity, now_seconds) <= 0)
        state.layout.app_volumes_clip.height = 0;
}

uint64_t reach_quick_settings_press_feedback_target(const reach_quick_settings *quick_settings)
{
    if (quick_settings == nullptr)
    {
        return REACH_PRESSABLE_TARGET_NONE;
    }
    size_t feedback_index = reach_pressable_feedback_index(&quick_settings->pressable);
    return feedback_index == REACH_PRESSABLE_FEEDBACK_NONE ? REACH_PRESSABLE_TARGET_NONE
                                                           : static_cast<uint64_t>(feedback_index);
}

reach_motion reach_quick_settings_press_feedback_motion(const reach_quick_settings *quick_settings)
{
    return quick_settings != nullptr ? quick_settings->press_feedback : reach_motion{};
}

void reach_quick_settings_tile_state_motions(const reach_quick_settings *quick_settings,
                                             reach_motion *out_network, reach_motion *out_bluetooth)
{
    if (quick_settings == nullptr || out_network == nullptr || out_bluetooth == nullptr)
        return;
    *out_network = quick_settings->network_tile_state;
    *out_bluetooth = quick_settings->bluetooth_tile_state;
}

int32_t reach_quick_settings_is_open(const reach_quick_settings *quick_settings)
{
    return quick_settings != nullptr &&
           reach_quick_settings_state_ptr(const_cast<reach_quick_settings *>(quick_settings))->open;
}

int32_t reach_quick_settings_set_open(reach_quick_settings *quick_settings, int32_t open)
{
    if (quick_settings == nullptr)
    {
        return 0;
    }
    reach_quick_settings_state *state = reach_quick_settings_state_mut(quick_settings);
    int32_t next_open = open ? 1 : 0;
    if (state->open == next_open)
    {
        return 0;
    }
    state->open = next_open;
    (void)reach_feature_transition_set_open(&quick_settings->popup_transition, next_open);
    state->drag.active = 0;
    state->drag.type = REACH_QUICK_SETTINGS_HIT_NONE;
    state->drag.level_valid = 0;
    reach_quick_settings_reset_pressable(quick_settings);
    reach_quick_settings_set_bluetooth_pending(quick_settings, 0, 0);
    if (next_open)
    {
        state->scroll_offset = 0.0f;
        reach_motion_set(&state->scroll_motion, 0);
        reach_motion_set(&quick_settings->scroll_maximum, 0);
        reach_quick_settings_refresh_system(quick_settings, 0);
        reach_quick_settings_refresh_audio(quick_settings);
        reach_motion_set(&quick_settings->height, 0);
        quick_settings->layout_initialized = 0;
        reach_quick_settings_sync_tile_states(quick_settings, 0);
    }
    return 1;
}

void reach_quick_settings_force_close(reach_quick_settings *quick_settings)
{
    if (quick_settings == nullptr)
    {
        return;
    }
    reach_quick_settings_state *state = reach_quick_settings_state_mut(quick_settings);
    state->open = 0;
    reach_feature_transition_reset(&quick_settings->popup_transition);
    reach_quick_settings_snap_sections(quick_settings, quick_settings->layout_dpi);
    reach_quick_settings_sync_tile_states(quick_settings, 0);
    quick_settings->section_cleanup_pending = 0;
    reach_motion_set(&quick_settings->layout_progress, 1);
    reach_motion_set(&quick_settings->height, state->target_bounds.height);
    quick_settings->relayout_animate_pending = 0;
    quick_settings->pending_motion_seconds = 0;

    state->drag.active = 0;
    state->drag.type = REACH_QUICK_SETTINGS_HIT_NONE;
    reach_quick_settings_reset_pressable(quick_settings);
}

void reach_quick_settings_reset(reach_quick_settings *quick_settings)
{
    if (quick_settings == nullptr)
    {
        return;
    }
    reach_quick_settings_state *state = reach_quick_settings_state_mut(quick_settings);
    reach_quick_settings_model_init(&state->model);
    reach_motion_set(&quick_settings->height, 0);
    reach_motion_set(&quick_settings->layout_progress, 1);
    reach_motion_set(&quick_settings->scroll_maximum, 0);
    state->surface_motion = {};
    state->render_bounds = {};
    reach_motion_set(&state->scroll_motion, 0);
    quick_settings->layout_initialized = 0;
    quick_settings->pending_motion_seconds = 0;
    reach_quick_settings_snap_sections(quick_settings, quick_settings->layout_dpi);
    reach_quick_settings_sync_tile_states(quick_settings, 0);
    quick_settings->section_cleanup_pending = 0;
    state->open = 0;
    state->notch_anchor_x = 0.0f;
    state->bounds = {};
    state->target_bounds = {};
    state->content_bounds = {};
    state->scroll_offset = 0.0f;
    state->scroll_max = 0.0f;
    state->layout = {};
    state->drag = {};
    reach_feature_transition_reset(&quick_settings->popup_transition);
    reach_quick_settings_reset_pressable(quick_settings);

    quick_settings->retired_render_icon_count = 0;
}

void reach_quick_settings_apply_main_volume(reach_quick_settings *quick_settings, float level,
                                            int32_t muted)
{
    if (quick_settings != nullptr)
    {
        reach_quick_settings_model_set_main_volume(
            &reach_quick_settings_state_mut(quick_settings)->model, level, muted);
    }
}

void reach_quick_settings_apply_sessions(reach_quick_settings *quick_settings,
                                         const reach_audio_volume_session_list *sessions)
{
    if (quick_settings != nullptr)
    {
        reach_quick_settings_model_set_sessions(
            &reach_quick_settings_state_mut(quick_settings)->model, sessions);
        reach_quick_settings_sync_app_rows(quick_settings, quick_settings->layout_dpi,
                                           reach_quick_settings_rows_animate(quick_settings));
    }
}

void reach_quick_settings_apply_output_devices(reach_quick_settings *quick_settings,
                                               const reach_audio_output_device_list *devices)
{
    if (quick_settings != nullptr)
    {
        reach_quick_settings_model_set_output_devices(
            &reach_quick_settings_state_mut(quick_settings)->model, devices);
        reach_quick_settings_sync_output_rows(quick_settings, quick_settings->layout_dpi,
                                              reach_quick_settings_rows_animate(quick_settings));
    }
}

int32_t reach_quick_settings_drag_active(const reach_quick_settings *quick_settings)
{
    return quick_settings != nullptr &&
           reach_quick_settings_state_ptr(const_cast<reach_quick_settings *>(quick_settings))
               ->drag.active;
}

static void reach_quick_settings_capsule_reset(void *capsule)
{
    reach_quick_settings_reset(static_cast<reach_quick_settings *>(capsule));
}

static void reach_quick_settings_capsule_tick(void *capsule, double delta_seconds,
                                              reach_feature_tick_result *out)
{
    reach_quick_settings *quick_settings = static_cast<reach_quick_settings *>(capsule);
    int32_t popup_changed =
        quick_settings != nullptr
            ? reach_feature_transition_tick(&quick_settings->popup_transition, delta_seconds)
            : 0;
    if (quick_settings != nullptr)
    {
        reach_pressable_feedback_style feedback =
            reach_quick_settings_pressable_feedback(quick_settings);
        reach_pressable_settle_feedback(&quick_settings->pressable, &feedback);
    }
    reach_feature_tick_result changes = {};
    reach_quick_settings_process_changes(quick_settings, &changes);
    if (quick_settings != nullptr && reach_quick_settings_release_settled_rows(
                                         quick_settings, quick_settings->output_device_rows,
                                         quick_settings->output_device_items))
    {
        reach_quick_settings_sync_output_rows(quick_settings, quick_settings->layout_dpi,
                                              reach_quick_settings_rows_animate(quick_settings));
        changes.redraw = 1;
    }
    if (quick_settings != nullptr &&
        reach_quick_settings_release_settled_rows(quick_settings, quick_settings->app_volume_rows,
                                                  quick_settings->app_volume_items))
    {
        reach_quick_settings_sync_app_rows(quick_settings, quick_settings->layout_dpi,
                                           reach_quick_settings_rows_animate(quick_settings));
        changes.redraw = 1;
    }
    if (quick_settings != nullptr && quick_settings->section_cleanup_pending &&
        !reach_quick_settings_sections_active(quick_settings) &&
        !reach_motion_active(&quick_settings->layout_progress))
    {
        quick_settings->section_cleanup_pending = 0;
        changes.redraw = changes.relayout = 1;
        double now_seconds = reach_motion_now_seconds();
        reach_quick_settings_state &state = quick_settings->state;
        reach_motion_set(&state.output_devices_span,
                         reach_motion_value_at(&state.output_devices_span, now_seconds));
        reach_motion_set(&state.app_volumes_span,
                         reach_motion_value_at(&state.app_volumes_span, now_seconds));
        reach_quick_settings_sample_input_layout(quick_settings, now_seconds);
    }
    if (out != nullptr)
    {
        out->redraw = changes.redraw || popup_changed;
        out->relayout = changes.relayout;
        out->request_update = changes.request_update ||
                              (quick_settings != nullptr && reach_feature_transition_needs_update(
                                                                &quick_settings->popup_transition));
    }
}

static int32_t reach_quick_settings_capsule_is_open(const void *capsule)
{
    return reach_quick_settings_is_open(static_cast<const reach_quick_settings *>(capsule));
}

static int32_t reach_quick_settings_capsule_presentation_visible(const void *capsule)
{
    const reach_quick_settings *quick_settings = static_cast<const reach_quick_settings *>(capsule);
    return quick_settings != nullptr &&
           reach_feature_transition_visible(&quick_settings->popup_transition);
}

static int32_t reach_quick_settings_capsule_needs_frame(const void *capsule)
{
    const reach_quick_settings *quick_settings = static_cast<const reach_quick_settings *>(capsule);
    if (quick_settings == nullptr)
    {
        return 0;
    }
    return reach_feature_transition_needs_update(&quick_settings->popup_transition) ||
           (quick_settings->status != nullptr &&
            (reach_system_status_audio_pending(quick_settings->status) ||
             reach_system_status_system_pending(quick_settings->status)));
}

static int32_t reach_quick_settings_capsule_wants_pointer_move(const void *capsule)
{
    const reach_quick_settings *quick_settings = static_cast<const reach_quick_settings *>(capsule);
    return quick_settings != nullptr && (reach_quick_settings_drag_active(quick_settings) ||
                                         reach_pressable_tracking(&quick_settings->pressable));
}

static int32_t reach_quick_settings_capsule_pointer_sequence_active(const void *capsule)
{
    return reach_quick_settings_capsule_wants_pointer_move(capsule);
}

static void reach_quick_settings_capsule_surface_geometry(const void *capsule,
                                                          reach_feature_surface_geometry *out)
{
    if (capsule == nullptr || out == nullptr)
    {
        return;
    }
    const reach_quick_settings *quick_settings = static_cast<const reach_quick_settings *>(capsule);
    *out = {};
    out->visible_bounds = quick_settings->state.render_bounds;
    out->envelope_bounds = quick_settings->state.render_bounds;
    out->has_input_bounds = 1;
    out->input_bounds = quick_settings->state.surface_motion;
    out->notch_anchor_x = quick_settings->state.notch_anchor_x;
    out->notch_side = reach_popup_notch_side(quick_settings->state.drop_direction);
    reach_feature_transition_presentation(&quick_settings->popup_transition, out);
    if (quick_settings->section_cleanup_pending)
    {
        const reach_quick_settings_state &state = quick_settings->state;
        const reach_motion *motions[] = {
            &quick_settings->layout_progress, &state.output_devices_expansion,
            &state.app_volumes_expansion,     &state.output_devices_span,
            &state.app_volumes_span,          &state.output_devices_opacity,
            &state.app_volumes_opacity};
        for (const reach_motion *motion : motions)
            out->deadline_seconds = fmax(out->deadline_seconds, reach_motion_deadline(motion));
        out->deadline_seconds =
            fmax(out->deadline_seconds,
                 fmax(reach_keyed_rows_deadline(quick_settings->output_device_rows,
                                                REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES),
                      reach_keyed_rows_deadline(quick_settings->app_volume_rows,
                                                REACH_AUDIO_VOLUME_MAX_SESSIONS)));
    }
}

static reach_quick_settings_hit_result
reach_quick_settings_capsule_hit_test(reach_quick_settings *quick_settings, int32_t x, int32_t y)
{
    reach_quick_settings_hit_result hit = {};
    if (quick_settings == nullptr)
    {
        return hit;
    }
    const reach_quick_settings_state *state = &quick_settings->state;
    if ((float)x < state->content_bounds.x ||
        (float)x > state->content_bounds.x + state->content_bounds.width ||
        (float)y < state->content_bounds.y ||
        (float)y > state->content_bounds.y + state->content_bounds.height)
    {
        return hit;
    }
    return reach_quick_settings_hit_test(&state->layout, &state->model, (float)x, (float)y);
}

static int32_t reach_quick_settings_press_actions_match(const reach_quick_settings_action *pressed,
                                                        const reach_quick_settings_action *released)
{
    if (pressed == nullptr || released == nullptr || pressed->type != released->type)
    {
        return 0;
    }
    if (pressed->type == REACH_QUICK_SETTINGS_ACTION_SET_OUTPUT_DEVICE)
    {
        return pressed->output_device_index == released->output_device_index &&
               reach_utf16_equal(pressed->output_device_id, released->output_device_id);
    }
    return pressed->type != REACH_QUICK_SETTINGS_ACTION_NONE;
}

static void
reach_quick_settings_capsule_apply_pressable_result(const reach_pressable_result *pressable,
                                                    reach_capsule_pointer_result *out)
{
    if (pressable == nullptr || out == nullptr)
    {
        return;
    }
    out->redraw |= pressable->redraw;
    if (pressable->capture != 0)
    {
        out->capture = pressable->capture;
    }
    out->sync_pointer_subscriptions |= pressable->sync_pointer_subscriptions;
}

static void reach_quick_settings_toggle_bluetooth(reach_quick_settings *quick_settings,
                                                  reach_capsule_pointer_result *out)
{
    if (quick_settings->state.model.bluetooth_pending)
    {
        return;
    }

    out->redraw = 1;

    if (!reach_quick_settings_bluetooth_available(quick_settings))
    {
        reach_quick_settings_refresh_system(quick_settings, 0);
        return;
    }

    int32_t target_enabled = reach_quick_settings_bluetooth_enabled(quick_settings) ? 0 : 1;
    reach_quick_settings_set_bluetooth_pending(quick_settings, 1, target_enabled);

    switch (reach_system_status_set_bluetooth_enabled(quick_settings->status, target_enabled))
    {
    case REACH_SYSTEM_STATUS_BLUETOOTH_PENDING:
        break;
    case REACH_SYSTEM_STATUS_BLUETOOTH_REJECTED:
        reach_quick_settings_set_bluetooth_pending(quick_settings, 0, 0);
        reach_quick_settings_refresh_system(quick_settings, 0);
        break;
    case REACH_SYSTEM_STATUS_BLUETOOTH_APPLIED:
    case REACH_SYSTEM_STATUS_BLUETOOTH_UNSUPPORTED:
    default:
        reach_quick_settings_refresh_system(quick_settings, 0);
        reach_quick_settings_set_bluetooth_pending(quick_settings, 0, 0);
        break;
    }
}

static void reach_quick_settings_capsule_perform_action(reach_quick_settings *quick_settings,
                                                        const reach_quick_settings_action *action,
                                                        reach_capsule_pointer_result *out)
{
    if (quick_settings == nullptr || action == nullptr || out == nullptr)
    {
        return;
    }

    switch (action->type)
    {
    case REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME:
    {
        int32_t muted = quick_settings->state.model.main_muted;
        (void)reach_system_status_set_main_volume(quick_settings->status, action->volume_level,
                                                  &muted);
        reach_quick_settings_apply_main_volume(quick_settings, action->volume_level, muted);
        out->redraw = 1;
        break;
    }
    case REACH_QUICK_SETTINGS_ACTION_SET_SESSION_VOLUME:
    {
        const uint16_t *session_instance_id = reach_quick_settings_set_session_level(
            quick_settings, action->session_index, action->volume_level);
        if (session_instance_id != nullptr)
        {
            (void)reach_system_status_set_session_volume(quick_settings->status,
                                                         session_instance_id, action->volume_level);
        }
        out->redraw = 1;
        break;
    }
    case REACH_QUICK_SETTINGS_ACTION_SET_BRIGHTNESS:
        (void)reach_system_status_set_brightness(quick_settings->status, action->volume_level);
        out->redraw = 1;
        break;

    case REACH_QUICK_SETTINGS_ACTION_NETWORK_TILE:
        (void)reach_system_status_open_system_quick_settings(quick_settings->status);
        out->action.kind = REACH_FEATURE_ACTION_CLOSE_SELF;
        break;

    case REACH_QUICK_SETTINGS_ACTION_TOGGLE_BLUETOOTH:
        reach_quick_settings_toggle_bluetooth(quick_settings, out);
        break;

    case REACH_QUICK_SETTINGS_ACTION_OPEN_PROJECT:
        (void)reach_system_status_open_project_menu(quick_settings->status);
        out->action.kind = REACH_FEATURE_ACTION_CLOSE_SELF;
        break;

    case REACH_QUICK_SETTINGS_ACTION_TOGGLE_OUTPUT_DEVICES:
        (void)reach_quick_settings_toggle_output_devices(quick_settings);
        reach_quick_settings_refresh_system(quick_settings, 0);
        reach_quick_settings_refresh_audio(quick_settings);
        quick_settings->relayout_animate_pending = 1;
        out->redraw = 1;
        out->relayout = 1;
        break;

    case REACH_QUICK_SETTINGS_ACTION_SET_OUTPUT_DEVICE:
    {
        const uint16_t *output_device_id =
            reach_quick_settings_output_device_id(quick_settings, action->output_device_index);
        int32_t changed = output_device_id != nullptr &&
                          reach_system_status_set_default_output_device(
                              quick_settings->status, output_device_id) == REACH_OK;
        if (changed)
        {
            reach_quick_settings_collapse_output_devices(quick_settings);
        }
        reach_quick_settings_refresh_system(quick_settings, 0);
        reach_quick_settings_refresh_audio(quick_settings);
        if (changed)
        {
            quick_settings->relayout_animate_pending = 1;
            out->relayout = 1;
        }
        out->redraw = 1;
        break;
    }
    case REACH_QUICK_SETTINGS_ACTION_EXPAND:
        (void)reach_quick_settings_toggle_expanded(quick_settings);
        reach_quick_settings_refresh_system(quick_settings, 0);
        reach_quick_settings_refresh_audio(quick_settings);
        quick_settings->relayout_animate_pending = 1;
        out->redraw = 1;
        out->relayout = 1;
        break;

    case REACH_QUICK_SETTINGS_ACTION_NONE:
    default:
        break;
    }
}

static void reach_quick_settings_capsule_handle_pointer(void *capsule,
                                                        const reach_pointer_event *event,
                                                        reach_capsule_pointer_result *out)
{
    if (out != nullptr)
    {
        *out = {};
    }
    reach_quick_settings *quick_settings = static_cast<reach_quick_settings *>(capsule);
    if (quick_settings == nullptr || event == nullptr || out == nullptr ||
        event->coordinate_space != REACH_POINTER_COORDINATE_SURFACE_LOCAL)
    {
        return;
    }

    reach_quick_settings_sample_input_layout(quick_settings, reach_motion_now_seconds());

    if (event->kind == REACH_POINTER_EVENT_DOWN && event->button == REACH_POINTER_BUTTON_PRIMARY &&
        event->owner_trigger)
    {
        out->handled = 1;
        out->continue_source_sequence = 1;
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_DOWN &&
        event->surface_relation == REACH_POINTER_SURFACE_OUTSIDE)
    {
        out->handled = 1;
        out->action.kind = REACH_FEATURE_ACTION_CLOSE_SELF;
        out->cancel_source_sequence = event->button == REACH_POINTER_BUTTON_PRIMARY;
        out->continue_source_sequence = event->button != REACH_POINTER_BUTTON_PRIMARY;
        return;
    }

    if (event->kind == REACH_POINTER_EVENT_DOWN && event->button == REACH_POINTER_BUTTON_PRIMARY)
    {
        reach_quick_settings_action action =
            reach_quick_settings_begin_slider_gesture_if_hit(quick_settings, event->x, event->y);
        if (reach_quick_settings_drag_active(quick_settings))
        {
            out->handled = 1;
            reach_quick_settings_capsule_perform_action(quick_settings, &action, out);
            out->capture = 1;
            out->sync_pointer_subscriptions = 1;
            return;
        }

        reach_quick_settings_hit_result hit =
            reach_quick_settings_capsule_hit_test(quick_settings, event->x, event->y);
        uint64_t target = reach_quick_settings_pressable_target(hit);
        action = reach_quick_settings_action_for_hit(hit);
        reach_pressable_feedback_style feedback =
            reach_quick_settings_pressable_feedback(quick_settings);
        reach_pressable_result pressable = {};
        reach_pressable_press(&quick_settings->pressable, event->button, target,
                              target == REACH_PRESSABLE_TARGET_NONE ? REACH_PRESSABLE_FEEDBACK_NONE
                                                                    : static_cast<size_t>(target),
                              &feedback, &pressable);
        reach_quick_settings_capsule_apply_pressable_result(&pressable, out);
        if (pressable.capture == 1)
        {
            quick_settings->press_action = action;
        }
        out->handled = reach_pressable_tracking(&quick_settings->pressable);
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_UP && event->button == REACH_POINTER_BUTTON_PRIMARY &&
        reach_pressable_tracking(&quick_settings->pressable))
    {
        reach_quick_settings_hit_result hit =
            reach_quick_settings_capsule_hit_test(quick_settings, event->x, event->y);
        reach_quick_settings_action released_action = reach_quick_settings_action_for_hit(hit);
        reach_pressable_feedback_style feedback =
            reach_quick_settings_pressable_feedback(quick_settings);
        reach_pressable_result pressable = {};
        reach_pressable_release(&quick_settings->pressable, event->button,
                                reach_quick_settings_pressable_target(hit), &feedback, &pressable);
        reach_quick_settings_capsule_apply_pressable_result(&pressable, out);
        out->handled = 1;
        if (pressable.activated && reach_quick_settings_press_actions_match(
                                       &quick_settings->press_action, &released_action))
        {
            reach_quick_settings_capsule_perform_action(quick_settings, &released_action, out);
        }
        quick_settings->press_action = {};
        return;
    }
    if ((event->kind == REACH_POINTER_EVENT_MOVE || event->kind == REACH_POINTER_EVENT_LEAVE) &&
        reach_pressable_tracking(&quick_settings->pressable))
    {
        uint64_t target = REACH_PRESSABLE_TARGET_NONE;
        if (event->kind == REACH_POINTER_EVENT_MOVE)
        {
            target = reach_quick_settings_pressable_target(
                reach_quick_settings_capsule_hit_test(quick_settings, event->x, event->y));
        }
        reach_pressable_result pressable = {};
        reach_pressable_update(&quick_settings->pressable, target, &pressable);
        reach_quick_settings_capsule_apply_pressable_result(&pressable, out);
        out->handled = 1;
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_CANCEL &&
        reach_pressable_tracking(&quick_settings->pressable))
    {
        reach_pressable_feedback_style feedback =
            reach_quick_settings_pressable_feedback(quick_settings);
        reach_pressable_result pressable = {};
        reach_pressable_cancel(&quick_settings->pressable, &feedback, &pressable);
        reach_quick_settings_capsule_apply_pressable_result(&pressable, out);
        quick_settings->press_action = {};
        out->handled = 1;
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_MOVE && reach_quick_settings_drag_active(quick_settings))
    {
        reach_quick_settings_action action =
            reach_quick_settings_drag_move(quick_settings, event->x, event->y);
        if (action.type == REACH_QUICK_SETTINGS_ACTION_NONE)
        {
            out->handled = 1;
            return;
        }
        out->handled = 1;
        reach_quick_settings_capsule_perform_action(quick_settings, &action, out);
        return;
    }
    if (((event->kind == REACH_POINTER_EVENT_UP && event->button == REACH_POINTER_BUTTON_PRIMARY) ||
         event->kind == REACH_POINTER_EVENT_CANCEL) &&
        reach_quick_settings_drag_active(quick_settings))
    {
        reach_quick_settings_end_drag(quick_settings);
        out->handled = 1;
        out->capture = -1;
        out->sync_pointer_subscriptions = 1;
    }
    if (event->kind == REACH_POINTER_EVENT_WHEEL && event->wheel_delta != 0 &&
        quick_settings->state.scroll_max > 0.0f)
    {
        float scale = quick_settings->state.bounds.width > 0.0f
                          ? quick_settings->state.bounds.width / 280.0f
                          : 1.0f;
        quick_settings->state.scroll_offset +=
            event->wheel_delta > 0 ? -86.0f * scale : 86.0f * scale;
        if (quick_settings->state.scroll_offset < 0.0f)
        {
            quick_settings->state.scroll_offset = 0.0f;
        }
        if (quick_settings->state.scroll_offset > quick_settings->state.scroll_max)
        {
            quick_settings->state.scroll_offset = quick_settings->state.scroll_max;
        }
        out->handled = 1;
        quick_settings->state.scroll_motion = reach_motion_clamp(
            &quick_settings->scroll_maximum, 0, quick_settings->state.scroll_offset);
        out->redraw = 1;
        out->relayout = 1;
    }
}

const reach_feature_capsule_ops *reach_quick_settings_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {
        reach_quick_settings_capsule_reset,
        reach_quick_settings_capsule_tick,
        reach_quick_settings_capsule_is_open,
        nullptr,
        reach_quick_settings_capsule_needs_frame,
        reach_quick_settings_capsule_wants_pointer_move,
        reach_quick_settings_capsule_handle_pointer,
        reach_quick_settings_capsule_pointer_sequence_active,
        nullptr,
        reach_quick_settings_capsule_surface_geometry,
        reach_quick_settings_capsule_wants_pointer_move,
        nullptr,
        nullptr,
        reach_quick_settings_capsule_presentation_visible,
    };
    return &ops;
}

reach_result reach_quick_settings_create(reach_quick_settings **out_quick_settings)
{
    if (out_quick_settings == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_quick_settings *quick_settings = new (std::nothrow) reach_quick_settings();
    if (quick_settings == nullptr)
    {
        return REACH_ERROR;
    }
    reach_feature_transition_init(&quick_settings->popup_transition,
                                  REACH_FEATURE_PRESENTATION_POPUP,
                                  REACH_FEATURE_TRANSITION_FROM_ABOVE);
    reach_pressable_init(&quick_settings->pressable);
    reach_quick_settings_model_init(&quick_settings->state.model);
    *out_quick_settings = quick_settings;
    return REACH_OK;
}

void reach_quick_settings_destroy(reach_quick_settings *quick_settings)
{
    delete quick_settings;
}

void reach_quick_settings_attach_status(reach_quick_settings *quick_settings,
                                        reach_system_status *status)
{
    if (quick_settings != nullptr)
    {
        quick_settings->status = status;
    }
}

void reach_quick_settings_refresh_audio(reach_quick_settings *quick_settings)
{
    if (quick_settings != nullptr && quick_settings->status != nullptr)
    {
        reach_system_status_refresh_audio(quick_settings->status);
    }
}

void reach_quick_settings_refresh_system(reach_quick_settings *quick_settings,
                                         uint32_t change_flags)
{
    if (quick_settings != nullptr && quick_settings->status != nullptr)
    {
        reach_system_status_refresh_system(quick_settings->status, change_flags);
    }
}

static void reach_quick_settings_retire_render_icon(reach_quick_settings *quick_settings,
                                                    uint64_t icon_id)
{
    if (icon_id == 0)
    {
        return;
    }
    REACH_ASSERT(quick_settings->retired_render_icon_count <
                 REACH_QUICK_SETTINGS_MAX_RETIRED_RENDER_ICONS);
    if (quick_settings->retired_render_icon_count < REACH_QUICK_SETTINGS_MAX_RETIRED_RENDER_ICONS)
    {
        quick_settings->retired_render_icons[quick_settings->retired_render_icon_count++] = icon_id;
    }
}

template <typename Payload, size_t Capacity>
static bool reach_quick_settings_rows_hold_icon(const reach_keyed_row (&rows)[Capacity],
                                                const Payload (&payloads)[Capacity],
                                                uint64_t icon_id)
{
    for (size_t slot = 0; slot < Capacity; ++slot)
    {
        if (rows[slot].lifecycle == REACH_KEYED_ROW_RETIRING &&
            reach_quick_settings_row_item(payloads[slot]).icon_id == icon_id)
            return true;
    }
    return false;
}

static size_t reach_quick_settings_audio_render_icons(const reach_quick_settings_model *model,
                                                      uint64_t *out_ids)
{
    size_t count = 0;
    size_t session_count = reach_quick_settings_visible_session_count(model);
    for (size_t index = 0; index < session_count; ++index)
        out_ids[count++] = model->sessions.sessions[index].icon_id;
    size_t device_count = reach_quick_settings_visible_output_device_count(model);
    for (size_t index = 0; index < device_count; ++index)
        out_ids[count++] = model->output_devices.devices[index].icon_id;
    return count;
}

static void reach_quick_settings_retire_audio_render_icons(reach_quick_settings *quick_settings,
                                                           const uint64_t *icon_ids, size_t count)
{
    for (size_t index = 0; index < count; ++index)
    {
        if (!reach_quick_settings_rows_hold_icon(quick_settings->output_device_rows,
                                                 quick_settings->output_device_items,
                                                 icon_ids[index]) &&
            !reach_quick_settings_rows_hold_icon(quick_settings->app_volume_rows,
                                                 quick_settings->app_volume_items, icon_ids[index]))
            reach_quick_settings_retire_render_icon(quick_settings, icon_ids[index]);
    }
}

size_t reach_quick_settings_take_retired_render_icons(reach_quick_settings *quick_settings,
                                                      uint64_t *out_ids, size_t cap)
{
    if (quick_settings == nullptr || out_ids == nullptr)
    {
        return 0;
    }
    size_t count = quick_settings->retired_render_icon_count;
    if (count > cap)
    {
        count = cap;
    }
    for (size_t index = 0; index < count; ++index)
    {
        out_ids[index] = quick_settings->retired_render_icons[index];
    }
    for (size_t index = count; index < quick_settings->retired_render_icon_count; ++index)
        quick_settings->retired_render_icons[index - count] =
            quick_settings->retired_render_icons[index];
    quick_settings->retired_render_icon_count -= count;
    return count;
}

void reach_quick_settings_process_changes(reach_quick_settings *quick_settings,
                                          reach_feature_tick_result *out)
{
    if (out != nullptr)
    {
        *out = {};
    }
    if (quick_settings == nullptr || out == nullptr)
    {
        return;
    }
    reach_quick_settings_state *state = reach_quick_settings_state_mut(quick_settings);

    if (quick_settings->status == nullptr)
    {
        return;
    }

    reach_system_status_system_snapshot system_snapshot = {};
    if (reach_system_status_take_system(quick_settings->status, &system_snapshot))
    {
        reach_quick_settings_system_apply_result apply_result = {};
        reach_quick_settings_apply_system_states(
            quick_settings, &system_snapshot.network, &system_snapshot.bluetooth,
            &system_snapshot.brightness, system_snapshot.bluetooth_valid,
            system_snapshot.change_flags, &apply_result);
        if (apply_result.relayout)
        {
            out->relayout = 1;
        }
        out->redraw = 1;
        out->request_update = 1;
    }

    reach_system_status_audio_snapshot audio_snapshot = {};
    if (reach_system_status_take_audio(quick_settings->status, &audio_snapshot))
    {
        uint64_t previous_icons[REACH_AUDIO_VOLUME_MAX_SESSIONS +
                                REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES] = {};
        size_t previous_icon_count =
            reach_quick_settings_audio_render_icons(&state->model, previous_icons);

        if (audio_snapshot.state_valid)
        {
            reach_quick_settings_apply_main_volume(quick_settings, audio_snapshot.state.level,
                                                   audio_snapshot.state.muted);
        }
        reach_quick_settings_apply_sessions(
            quick_settings, audio_snapshot.sessions_valid ? &audio_snapshot.sessions : nullptr);
        reach_quick_settings_apply_output_devices(
            quick_settings,
            audio_snapshot.output_devices_valid ? &audio_snapshot.output_devices : nullptr);
        reach_quick_settings_retire_audio_render_icons(quick_settings, previous_icons,
                                                       previous_icon_count);

        if (state->open)
        {
            out->relayout = 1;
        }
        out->redraw = 1;
        out->request_update = 1;
    }
}

reach_rect_f32 reach_quick_settings_content_bounds_for(reach_rect_f32 surface_bounds,
                                                       const reach_theme *theme, float dpi_scale,
                                                       int32_t drop_direction)
{
    reach_rect_f32 bounds = reach_theme_border_content_rect(theme, dpi_scale, surface_bounds);

    float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    const float horizontal_padding = 8.0f * scale;
    const float top_padding = 8.0f * scale;
    const float bottom_padding = 12.0f * scale;
    const float notch_height = reach_popup_notch_height_scaled(scale);

    bounds.x += horizontal_padding;
    bounds.y += top_padding + (drop_direction == REACH_POPUP_DROP_DOWN ? notch_height : 0.0f);
    bounds.width -= horizontal_padding * 2.0f;
    bounds.height -= top_padding + bottom_padding + notch_height;

    if (bounds.width < 0.0f)
    {
        bounds.width = 0.0f;
    }
    if (bounds.height < 0.0f)
    {
        bounds.height = 0.0f;
    }

    return bounds;
}

static void reach_quick_settings_target_size(reach_quick_settings *quick_settings,
                                             const reach_theme *theme, float scale,
                                             float *out_width, float *out_height)
{
    float border_thickness = reach_theme_border_thickness(theme, scale);
    const float surface_vertical_padding = border_thickness * 2.0f + 8.0f * scale + 12.0f * scale +
                                           reach_popup_notch_height_scaled(scale);
    float content_height = reach_quick_settings_content_height_for_model_scaled(
        &reach_quick_settings_state_mut(quick_settings)->model, scale);

    if (out_width != nullptr)
    {
        *out_width = 280.0f * scale + border_thickness * 2.0f;
    }
    if (out_height != nullptr)
    {
        *out_height = content_height + surface_vertical_padding;
    }
}

static reach_popup_anchor
reach_quick_settings_popup_anchor(const reach_quick_settings_layout_context *ctx)
{
    reach_popup_anchor anchor = {};
    anchor.button = ctx->anchor_button;
    anchor.monitor = ctx->monitor;
    anchor.bar_edge_y = ctx->bar_edge_y;
    anchor.direction = ctx->drop_direction;
    anchor.bar_offset = reach_popup_hidden_notch_bar_offset_scaled(ctx->dpi_scale);
    anchor.has_bar_offset = 1;
    return anchor;
}

static reach_popup_placement
reach_quick_settings_placement(reach_quick_settings *quick_settings,
                               const reach_quick_settings_layout_context *ctx, float height)
{
    float width = 280.0f;
    float target_height = 140.0f;
    reach_quick_settings_target_size(quick_settings, ctx->theme, ctx->dpi_scale, &width,
                                     &target_height);

    reach_popup_anchor anchor = reach_quick_settings_popup_anchor(ctx);
    return reach_popup_place(&anchor, width, height > 0.0f ? height : target_height,
                             REACH_QUICK_SETTINGS_POPUP_MARGIN * ctx->dpi_scale);
}

struct reach_quick_settings_layout_channel
{
    float from;
    float to;
    float velocity;
    float minimum;
    float maximum;
    float weight;
};

static reach_motion
reach_quick_settings_channel_motion(const reach_motion *progress, const reach_motion *square,
                                    const reach_quick_settings_layout_channel *channel)
{
    float span = channel->to - channel->from;
    float slope =
        progress->spring_velocity != 0 ? channel->velocity / progress->spring_velocity : span;
    reach_motion linear = reach_motion_affine(progress, slope, 0);
    reach_motion result = {};
    if (!isfinite(slope) ||
        reach_motion_combine(&linear, 1, square, span - slope, 0, &result) != REACH_OK)
        return reach_motion_affine(progress, span, channel->from);
    return reach_motion_affine(&result, 1, channel->from);
}

static reach_motion reach_quick_settings_start_progress(reach_motion *progress, float rate,
                                                        reach_motion_spec spec,
                                                        double event_seconds)
{
    reach_motion_start_spec_at(progress, 0, 1, rate, spec, event_seconds);
    reach_motion square = {};
    if (reach_motion_multiply(progress, progress, &square) != REACH_OK)
        square = *progress;
    return square;
}

static void reach_quick_settings_score_progress(const reach_motion *progress,
                                                const reach_motion *square, reach_motion_spec spec,
                                                double event_seconds,
                                                const reach_quick_settings_layout_channel *channels,
                                                size_t count, double *out_violation,
                                                double *out_cost)
{
    double horizon =
        spec.easing == REACH_EASING_SPRING ? 2.0 * spec.response_seconds : spec.duration_seconds;
    double violation = 0, cost = 0;
    for (size_t index = 0; index < count; ++index)
    {
        const reach_quick_settings_layout_channel &channel = channels[index];
        reach_motion motion = reach_quick_settings_channel_motion(progress, square, &channel);
        float minimum = 0, maximum = 0;
        if (reach_motion_range(&motion, &minimum, &maximum) != REACH_OK)
        {
            violation = INFINITY;
            break;
        }
        violation += (fmax(0.0, (double)channel.minimum - minimum) +
                      fmax(0.0, (double)maximum - channel.maximum)) *
                     (channel.weight > 0 ? channel.weight : 1.0f);
        reach_motion reference = {};
        reach_motion_start_spec_at(&reference, channel.from, channel.to, channel.velocity, spec,
                                   event_seconds);
        for (int32_t sample = 1; sample <= 24; ++sample)
        {
            double at = event_seconds + horizon * sample / 24;
            double error = ((double)reach_motion_value_at(&motion, at) -
                            reach_motion_value_at(&reference, at)) *
                           channel.weight;
            cost += error * error;
        }
    }
    *out_violation = violation > 1e-3 ? violation : 0;
    *out_cost = cost;
}

static reach_motion reach_quick_settings_start_layout_progress(
    reach_motion *progress, reach_motion_spec spec, double event_seconds,
    const reach_quick_settings_layout_channel *channels, size_t count)
{
    double along = 0, length = 0;
    bool resting = true;
    for (size_t index = 0; index < count; ++index)
    {
        double displacement =
            ((double)channels[index].to - channels[index].from) * channels[index].weight;
        along += (double)channels[index].velocity * channels[index].weight * displacement;
        length += displacement * displacement;
        resting = resting && channels[index].velocity == 0;
    }
    double omega = spec.easing == REACH_EASING_SPRING && spec.response_seconds > 0
                       ? 2 * 3.14159265358979323846 / spec.response_seconds
                       : 0;
    float best_rate = 0;
    double best_violation = INFINITY, best_cost = INFINITY;
    for (int32_t step = -1; step <= 16; ++step)
    {
        float rate = (float)(step < 0 ? (length > 0 ? along / length : 0) : omega * step / 8);
        if (!isfinite(rate) || rate < 0 || (rate == 0 && !resting))
            continue;
        reach_motion square =
            reach_quick_settings_start_progress(progress, rate, spec, event_seconds);
        double violation = 0, cost = 0;
        reach_quick_settings_score_progress(progress, &square, spec, event_seconds, channels, count,
                                            &violation, &cost);
        if (violation < best_violation || (violation == best_violation && cost < best_cost))
        {
            best_rate = rate;
            best_violation = violation;
            best_cost = cost;
        }
    }
    return reach_quick_settings_start_progress(progress, best_rate, spec, event_seconds);
}

void reach_quick_settings_refresh_layout(reach_quick_settings *quick_settings,
                                         const reach_quick_settings_layout_context *ctx)
{
    if (quick_settings == nullptr || ctx == nullptr || ctx->theme == nullptr)
        return;
    reach_quick_settings_state &state = quick_settings->state;
    double now = reach_motion_now_seconds();
    reach_feature_transition_configure(&quick_settings->popup_transition, ctx->theme,
                                       ctx->dpi_scale, ctx->drop_direction);
    bool environment_changed =
        !quick_settings->layout_initialized || quick_settings->layout_dpi != ctx->dpi_scale ||
        quick_settings->layout_theme.border_thickness != ctx->theme->border_thickness ||
        !reach_rect_equal(quick_settings->layout_context.monitor, ctx->monitor) ||
        !reach_rect_equal(quick_settings->layout_context.anchor_button, ctx->anchor_button) ||
        quick_settings->layout_context.bar_edge_y != ctx->bar_edge_y ||
        quick_settings->layout_context.drop_direction != ctx->drop_direction;
    float current_height = quick_settings->height.generation != 0
                               ? reach_motion_value_at(&quick_settings->height, now)
                               : state.bounds.height;
    float current_scroll = reach_motion_value_at(&state.scroll_motion, now);
    reach_popup_placement target = reach_quick_settings_placement(quick_settings, ctx, 0);
    float target_content =
        reach_quick_settings_content_height_for_model_scaled(&state.model, ctx->dpi_scale);
    float output_span = 0, app_span = 0;
    reach_quick_settings_section_spans(&state.model, ctx->dpi_scale, &output_span, &app_span);
    bool target_changed = !reach_rect_equal(state.target_bounds, target.bounds) ||
                          fabsf(quick_settings->content_target_height - target_content) > 0.5f ||
                          fabsf(quick_settings->output_content_span - output_span) > 0.5f ||
                          fabsf(quick_settings->app_content_span - app_span) > 0.5f;
    bool animate =
        !environment_changed && (quick_settings->relayout_animate_pending || target_changed);
    float viewport_padding =
        target.bounds.height -
        reach_quick_settings_content_bounds_for({0, 0, target.bounds.width, target.bounds.height},
                                                ctx->theme, ctx->dpi_scale, ctx->drop_direction)
            .height;
    if (animate)
    {
        double event_time = quick_settings->pending_motion_seconds > 0
                                ? quick_settings->pending_motion_seconds
                                : now;
        current_height = reach_motion_value_at(&quick_settings->height, event_time);
        current_scroll = reach_motion_value_at(&state.scroll_motion, event_time);
        float output_from = reach_motion_value_at(&state.output_devices_expansion, event_time);
        float app_from = reach_motion_value_at(&state.app_volumes_expansion, event_time);
        float output_to = state.model.output_devices_expanded ? 1.0f : 0.0f;
        float app_to = state.model.expanded ? 1.0f : 0.0f;
        float output_velocity =
            reach_motion_velocity_at(&state.output_devices_expansion, event_time);
        float app_velocity = reach_motion_velocity_at(&state.app_volumes_expansion, event_time);
        float output_span_from = reach_motion_value_at(&state.output_devices_span, event_time);
        float app_span_from = reach_motion_value_at(&state.app_volumes_span, event_time);
        float output_span_velocity =
            reach_motion_velocity_at(&state.output_devices_span, event_time);
        float app_span_velocity = reach_motion_velocity_at(&state.app_volumes_span, event_time);
        float output_span_to = 0, app_span_to = 0;
        reach_quick_settings_target_spans(&state.model, ctx->dpi_scale, &output_span_to,
                                          &app_span_to);
        float height_velocity = reach_motion_velocity_at(&quick_settings->height, event_time);
        float height_limit =
            fmaxf(reach_quick_settings_placement(quick_settings, ctx, INFINITY).bounds.height,
                  target.bounds.height);
        const reach_quick_settings_layout_channel channels[] = {
            {output_span_from, output_span_to, output_span_velocity, fminf(0, output_span_from),
             fmaxf(output_span_from, output_span), 1},
            {app_span_from, app_span_to, app_span_velocity, fminf(0, app_span_from),
             fmaxf(app_span_from, app_span), 1},
            {current_height, target.bounds.height, height_velocity, 0, height_limit, 1},
            {output_from, output_to, output_velocity, 0, 1, 0},
            {app_from, app_to, app_velocity, 0, 1, 0}};
        reach_motion square = reach_quick_settings_start_layout_progress(
            &quick_settings->layout_progress,
            reach_theme_motion_preset(ctx->theme, REACH_MOTION_PRESET_SMOOTH), event_time, channels,
            5);
        const reach_motion *progress = &quick_settings->layout_progress;
        state.output_devices_span =
            reach_quick_settings_channel_motion(progress, &square, &channels[0]);
        state.app_volumes_span =
            reach_quick_settings_channel_motion(progress, &square, &channels[1]);
        quick_settings->height =
            reach_quick_settings_channel_motion(progress, &square, &channels[2]);
        state.output_devices_expansion =
            reach_quick_settings_channel_motion(progress, &square, &channels[3]);
        state.app_volumes_expansion =
            reach_quick_settings_channel_motion(progress, &square, &channels[4]);
        float content_from = reach_quick_settings_content_height_for_spans_scaled(
            &state.model, output_span_from, app_span_from, ctx->dpi_scale);
        const reach_quick_settings_layout_channel scroll = {
            content_from - current_height + viewport_padding,
            target_content - target.bounds.height + viewport_padding,
            output_span_velocity + app_span_velocity - height_velocity,
            -INFINITY,
            INFINITY,
            0};
        quick_settings->scroll_maximum =
            reach_quick_settings_channel_motion(progress, &square, &scroll);
        state.scroll_motion =
            reach_motion_clamp(&quick_settings->scroll_maximum, 0, fmaxf(0, current_scroll));
        quick_settings->section_cleanup_pending = 1;
    }
    else if (environment_changed || target_changed)
    {
        reach_motion_set(&quick_settings->height, target.bounds.height);
        reach_motion_set(&quick_settings->layout_progress, 1);
        reach_quick_settings_snap_sections(quick_settings, ctx->dpi_scale);
        quick_settings->section_cleanup_pending = 0;
        reach_motion_set(&quick_settings->scroll_maximum,
                         fmaxf(0, target_content - target.bounds.height + viewport_padding));
        state.scroll_motion =
            reach_motion_clamp(&quick_settings->scroll_maximum, 0, fmaxf(0, current_scroll));
    }
    quick_settings->relayout_animate_pending = 0;
    quick_settings->pending_motion_seconds = 0;
    quick_settings->content_target_height = target_content;
    quick_settings->output_content_span = output_span;
    quick_settings->app_content_span = app_span;
    state.target_bounds = target.bounds;
    float height_minimum = 0, height_maximum = 0;
    if (reach_motion_range(&quick_settings->height, &height_minimum, &height_maximum) != REACH_OK)
        height_maximum = fmaxf(quick_settings->height.from, quick_settings->height.to);
    float extent = environment_changed ? target.bounds.height
                                       : fmaxf(state.render_bounds.height, height_maximum);
    state.render_bounds = reach_quick_settings_placement(quick_settings, ctx, extent).bounds;
    reach_popup_anchor anchor = reach_quick_settings_popup_anchor(ctx);
    reach_rect_motion screen = {};
    if (reach_popup_height_motion(&anchor, target.bounds.width, &quick_settings->height,
                                  REACH_QUICK_SETTINGS_POPUP_MARGIN * ctx->dpi_scale,
                                  &screen) == REACH_OK)
    {
        screen.x = reach_motion_affine(&screen.x, 1, -state.render_bounds.x);
        screen.y = reach_motion_affine(&screen.y, 1, -state.render_bounds.y);
        state.surface_motion = screen;
    }
    state.notch_anchor_x = target.notch_anchor_x;
    reach_feature_transition_set_trigger(&quick_settings->popup_transition, state.render_bounds,
                                         target.notch_anchor_x);
    state.drop_direction = ctx->drop_direction;
    quick_settings->layout_theme = *ctx->theme;
    quick_settings->layout_dpi = ctx->dpi_scale;
    quick_settings->layout_context = *ctx;
    quick_settings->layout_initialized = 1;
    reach_quick_settings_sample_input_layout(quick_settings, now);
}

void reach_quick_settings_relayout(reach_quick_settings *quick_settings,
                                   const reach_quick_settings_layout_context *ctx,
                                   int32_t animate_height)
{
    if (quick_settings == nullptr || ctx == nullptr)
    {
        return;
    }

    quick_settings->relayout_animate_pending = animate_height ? 1 : 0;
    reach_quick_settings_refresh_layout(quick_settings, ctx);
}

int32_t reach_quick_settings_bluetooth_available(reach_quick_settings *quick_settings)
{
    return quick_settings != nullptr &&
           reach_quick_settings_state_mut(quick_settings)->model.bluetooth.available;
}

int32_t reach_quick_settings_bluetooth_enabled(reach_quick_settings *quick_settings)
{
    return quick_settings != nullptr &&
           reach_quick_settings_state_mut(quick_settings)->model.bluetooth.enabled;
}

void reach_quick_settings_set_bluetooth_pending(reach_quick_settings *quick_settings,
                                                int32_t pending, int32_t pending_enabled)
{
    if (quick_settings != nullptr)
    {
        reach_quick_settings_model_set_bluetooth_pending(
            &reach_quick_settings_state_mut(quick_settings)->model, pending, pending_enabled);
        reach_quick_settings_sync_tile_states(quick_settings,
                                              reach_quick_settings_tiles_animate(quick_settings));
    }
}

int32_t reach_quick_settings_toggle_expanded(reach_quick_settings *quick_settings)
{
    if (quick_settings == nullptr)
    {
        return 0;
    }
    reach_quick_settings_state *state = reach_quick_settings_state_mut(quick_settings);
    state->model.expanded = state->model.expanded ? 0 : 1;
    if (state->model.expanded)
        state->model.output_devices_expanded = 0;
    reach_quick_settings_retarget_sections(quick_settings, reach_motion_now_seconds());
    return state->model.expanded;
}

int32_t reach_quick_settings_toggle_output_devices(reach_quick_settings *quick_settings)
{
    if (quick_settings == nullptr)
    {
        return 0;
    }
    reach_quick_settings_state *state = reach_quick_settings_state_mut(quick_settings);
    state->model.output_devices_expanded = state->model.output_devices_expanded ? 0 : 1;
    if (state->model.output_devices_expanded)
        state->model.expanded = 0;
    reach_quick_settings_retarget_sections(quick_settings, reach_motion_now_seconds());
    return state->model.output_devices_expanded;
}

void reach_quick_settings_collapse_output_devices(reach_quick_settings *quick_settings)
{
    if (quick_settings != nullptr)
    {
        reach_quick_settings_state_mut(quick_settings)->model.output_devices_expanded = 0;
        reach_quick_settings_retarget_sections(quick_settings, reach_motion_now_seconds());
    }
}

const uint16_t *reach_quick_settings_set_session_level(reach_quick_settings *quick_settings,
                                                       size_t session_index, float level)
{
    if (quick_settings == nullptr)
    {
        return nullptr;
    }
    reach_quick_settings_state *state = reach_quick_settings_state_mut(quick_settings);
    if (session_index >= state->model.sessions.count)
    {
        return nullptr;
    }
    reach_audio_volume_session *session = &state->model.sessions.sessions[session_index];
    session->level = level;
    return session->session_instance_id;
}

const uint16_t *reach_quick_settings_output_device_id(const reach_quick_settings *quick_settings,
                                                      size_t output_device_index)
{
    if (quick_settings == nullptr ||
        output_device_index >= quick_settings->state.model.output_devices.count)
    {
        return nullptr;
    }
    return quick_settings->state.model.output_devices.devices[output_device_index].device_id;
}

void reach_quick_settings_apply_system_states(reach_quick_settings *quick_settings,
                                              const reach_network_state *network,
                                              const reach_bluetooth_state *bluetooth,
                                              const reach_brightness_state *brightness,
                                              int32_t bluetooth_valid, uint32_t change_flags,
                                              reach_quick_settings_system_apply_result *out)
{
    if (quick_settings == nullptr || out == nullptr)
    {
        return;
    }

    reach_quick_settings_state *state = reach_quick_settings_state_mut(quick_settings);

    reach_brightness_state previous_brightness = state->model.brightness;
    int32_t bluetooth_pending = state->model.bluetooth_pending;
    int32_t bluetooth_pending_enabled = state->model.bluetooth_pending_enabled;

    reach_quick_settings_model_set_system_states(&state->model, network, bluetooth, brightness);

    const int32_t bluetooth_request_settled =
        (change_flags & REACH_SYSTEM_CONTROLS_CHANGE_BLUETOOTH_REQUEST) != 0;
    if (bluetooth_pending &&
        (bluetooth_request_settled ||
         (bluetooth_valid && (!state->model.bluetooth.available ||
                              state->model.bluetooth.enabled == bluetooth_pending_enabled))))
    {
        reach_quick_settings_model_set_bluetooth_pending(&state->model, 0, 0);
    }

    reach_quick_settings_sync_tile_states(quick_settings,
                                          reach_quick_settings_tiles_animate(quick_settings));
    int32_t layout_changed = previous_brightness.available != state->model.brightness.available;

    out->relayout = layout_changed && state->open;
}
