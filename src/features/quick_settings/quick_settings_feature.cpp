#include "reach/features/quick_settings.h"
#include "reach/features/popup.h"

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
    reach_quick_settings_copy_utf16(model->label, REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY, label);
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

static float reach_quick_settings_content_height_for_expansion_scaled(
    const reach_quick_settings_model *model, float output_devices_expansion,
    float app_volumes_expansion, float dpi_scale)
{
    reach_quick_settings_metrics metrics = reach_quick_settings_metrics_for_scale(dpi_scale);
    output_devices_expansion = reach_quick_settings_clamp01(output_devices_expansion);
    app_volumes_expansion = reach_quick_settings_clamp01(app_volumes_expansion);

    size_t visible_sessions = reach_quick_settings_visible_session_count(model);
    size_t visible_output_devices = reach_quick_settings_visible_output_device_count(model);
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
    float output_panel_height = (float)visible_output_devices * metrics.output_device_row_height;
    float output_panel_span = reach_quick_settings_expandable_panel_span(
        output_panel_height, metrics.output_panel_gap);
    float output_component_height =
        metrics.output_button_gap + metrics.output_button_height +
        output_devices_expansion * output_panel_span;
    float app_panel_height = (float)visible_sessions * metrics.app_volume_row_height;
    float app_panel_span =
        reach_quick_settings_expandable_panel_span(app_panel_height, metrics.app_panel_gap);
    float app_volume_component_height =
        metrics.expand_button_gap + metrics.expand_button_height +
        app_volumes_expansion * app_panel_span;
    return metrics.content_padding * 2.0f + grid_component_height + brightness_component_height +
           volume_component_height + output_component_height + app_volume_component_height;
}

float reach_quick_settings_content_height_for_model_scaled(const reach_quick_settings_model *model,
                                                           float dpi_scale)
{
    float output_devices_expansion =
        model != nullptr && model->output_devices_expanded ? 1.0f : 0.0f;
    float app_volumes_expansion = model != nullptr && model->expanded ? 1.0f : 0.0f;
    return reach_quick_settings_content_height_for_expansion_scaled(
        model, output_devices_expansion, app_volumes_expansion, dpi_scale);
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

static reach_quick_settings_layout reach_quick_settings_layout_for_expansion_scaled(
    reach_rect_f32 content_bounds, const reach_theme *theme,
    const reach_quick_settings_model *model, float output_devices_expansion,
    float app_volumes_expansion, float dpi_scale)
{
    (void)theme;

    output_devices_expansion = reach_quick_settings_clamp01(output_devices_expansion);
    app_volumes_expansion = reach_quick_settings_clamp01(app_volumes_expansion);

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
    layout.output_device_button =
        reach_quick_settings_content_line(content_bounds, next_y + metrics.output_button_gap,
                                          metrics.output_button_height, &metrics);

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
    layout.output_devices_clip.height *= output_devices_expansion;

    for (size_t index = 0; index < visible_output_devices; ++index)
    {
        reach_quick_settings_output_device_row_layout *row = &layout.output_device_rows[index];
        row->bounds.x = layout.output_devices_panel.x;
        row->bounds.y =
            layout.output_devices_panel.y + (float)index * metrics.output_device_row_height;
        row->bounds.width = layout.output_devices_panel.width;
        row->bounds.height = metrics.output_device_row_height;

        row->icon.width = metrics.output_icon_size;
        row->icon.height = metrics.output_icon_size;
        row->icon.x = row->bounds.x + metrics.output_row_horizontal_padding;
        row->icon.y = row->bounds.y + (row->bounds.height - metrics.output_icon_size) * 0.5f;

        row->checkmark.width = metrics.output_check_size;
        row->checkmark.height = metrics.output_check_size;
        row->checkmark.x = row->bounds.x + row->bounds.width -
                           metrics.output_row_horizontal_padding - metrics.output_check_size;
        row->checkmark.y =
            row->bounds.y + (row->bounds.height - metrics.output_check_size) * 0.5f;

        row->label.x = row->icon.x + metrics.output_icon_size + metrics.output_row_label_gap;
        row->label.y = row->bounds.y;
        row->label.width = row->checkmark.x - metrics.output_row_label_gap - row->label.x;
        row->label.height = row->bounds.height;
        if (row->label.width < 0.0f)
        {
            row->label.width = 0.0f;
        }

        row->separator.x = row->bounds.x + metrics.separator_inset;
        row->separator.y = row->bounds.y + row->bounds.height - metrics.separator_thickness;
        row->separator.width = row->bounds.width - metrics.separator_inset * 2.0f;
        row->separator.height = metrics.separator_thickness;
        if (row->separator.width < 0.0f)
        {
            row->separator.width = 0.0f;
        }

        layout.output_device_row_count++;
    }

    float output_panel_span = reach_quick_settings_expandable_panel_span(
        layout.output_devices_panel.height, metrics.output_panel_gap);
    next_y = layout.output_device_button.y + layout.output_device_button.height +
             output_devices_expansion * output_panel_span;

    layout.app_volume_row_count = 0;
    size_t visible_sessions = reach_quick_settings_visible_session_count(model);
    layout.expand_button =
        reach_quick_settings_content_line(content_bounds, next_y + metrics.expand_button_gap,
                                          metrics.expand_button_height, &metrics);

    layout.app_volumes_panel.x = layout.expand_button.x;
    layout.app_volumes_panel.y =
        layout.expand_button.y + layout.expand_button.height + metrics.app_panel_gap;
    layout.app_volumes_panel.width = layout.expand_button.width;
    layout.app_volumes_panel.height = (float)visible_sessions * metrics.app_volume_row_height;
    layout.app_volumes_clip = layout.app_volumes_panel;
    layout.app_volumes_clip.height *= app_volumes_expansion;

    for (size_t index = 0; index < visible_sessions; ++index)
    {
        reach_quick_settings_app_volume_row_layout *row = &layout.app_volume_rows[index];
        row->bounds.x = layout.app_volumes_panel.x;
        row->bounds.y =
            layout.app_volumes_panel.y + (float)index * metrics.app_volume_row_height;
        row->bounds.width = layout.app_volumes_panel.width;
        row->bounds.height = metrics.app_volume_row_height;

        row->app_icon.width = metrics.app_icon_size;
        row->app_icon.height = metrics.app_icon_size;
        row->app_icon.x = row->bounds.x + metrics.app_row_horizontal_padding;
        row->app_icon.y = row->bounds.y + (row->bounds.height - metrics.app_icon_size) * 0.5f;

        row->app_volume_percent.x = row->bounds.x + row->bounds.width -
                                    metrics.app_row_horizontal_padding -
                                    metrics.app_row_percent_width;
        row->app_volume_percent.y = row->bounds.y;
        row->app_volume_percent.width = metrics.app_row_percent_width;
        row->app_volume_percent.height = row->bounds.height;

        row->slider_full_range_line.width = metrics.app_row_slider_width;
        row->slider_full_range_line.height = metrics.app_row_slider_line_height;
        row->slider_full_range_line.x = row->app_volume_percent.x -
                                        metrics.app_row_percent_gap - metrics.app_row_slider_width;
        row->slider_full_range_line.y =
            row->bounds.y + (row->bounds.height - metrics.app_row_slider_line_height) * 0.5f;
        if (row->slider_full_range_line.x <
            row->app_icon.x + metrics.app_icon_size + metrics.app_row_label_gap)
        {
            row->slider_full_range_line.x =
                row->app_icon.x + metrics.app_icon_size + metrics.app_row_label_gap;
            row->slider_full_range_line.width = row->bounds.x + row->bounds.width -
                                                metrics.app_row_horizontal_padding -
                                                row->slider_full_range_line.x;
            if (row->slider_full_range_line.width < 0.0f)
            {
                row->slider_full_range_line.width = 0.0f;
            }
        }

        row->app_label.x = row->app_icon.x + metrics.app_icon_size + metrics.app_row_label_gap;
        row->app_label.y = row->bounds.y;
        row->app_label.width =
            row->slider_full_range_line.x - metrics.app_row_slider_gap - row->app_label.x;
        row->app_label.height = row->bounds.height;
        if (row->app_label.width < 0.0f)
        {
            row->app_label.width = 0.0f;
        }

        row->slider_level_line = row->slider_full_range_line;
        float session_level = 0.0f;
        if (model != nullptr && index < model->sessions.count)
        {
            session_level = reach_quick_settings_clamp01(model->sessions.sessions[index].level);
        }
        row->slider_level_line.width = row->slider_full_range_line.width * session_level;
        row->slider_thumb.width = metrics.app_row_thumb_size;
        row->slider_thumb.height = metrics.app_row_thumb_size;
        row->slider_thumb.x = row->slider_full_range_line.x +
                              row->slider_full_range_line.width * session_level -
                              metrics.app_row_thumb_size * 0.5f;
        row->slider_thumb.y =
            row->bounds.y + (row->bounds.height - metrics.app_row_thumb_size) * 0.5f;

        row->separator.x = row->bounds.x + metrics.separator_inset;
        row->separator.y = row->bounds.y + row->bounds.height - metrics.separator_thickness;
        row->separator.width = row->bounds.width - metrics.separator_inset * 2.0f;
        row->separator.height = metrics.separator_thickness;
        if (row->separator.width < 0.0f)
        {
            row->separator.width = 0.0f;
        }

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
    float output_devices_expansion =
        model != nullptr && model->output_devices_expanded ? 1.0f : 0.0f;
    float app_volumes_expansion = model != nullptr && model->expanded ? 1.0f : 0.0f;
    return reach_quick_settings_layout_for_expansion_scaled(
        content_bounds, theme, model, output_devices_expansion, app_volumes_expansion, dpi_scale);
}

enum
{
    REACH_QUICK_SETTINGS_ANIMATION_HEIGHT = 0,
    REACH_QUICK_SETTINGS_ANIMATION_OUTPUT_DEVICES_EXPANSION,
    REACH_QUICK_SETTINGS_ANIMATION_APP_VOLUMES_EXPANSION,
    REACH_QUICK_SETTINGS_ANIMATION_PRESS_FEEDBACK,
    REACH_QUICK_SETTINGS_ANIMATION_COUNT
};

static const float REACH_QUICK_SETTINGS_POPUP_MARGIN = 8.0f;
static const double REACH_QUICK_SETTINGS_EXPANSION_SECONDS = 0.16;
static const double REACH_QUICK_SETTINGS_BLUETOOTH_PENDING_REFRESH_SECONDS = 0.35;
static const double REACH_QUICK_SETTINGS_BLUETOOTH_PENDING_TIMEOUT_SECONDS = 8.0;

#define REACH_QUICK_SETTINGS_MAX_RETIRED_RENDER_ICONS                                              \
    (REACH_AUDIO_VOLUME_MAX_SESSIONS + REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES)

struct reach_quick_settings
{
    reach_animation_manager animations;
    reach_animation_track animation_tracks[REACH_QUICK_SETTINGS_ANIMATION_COUNT];
    reach_pressable pressable;
    reach_quick_settings_action press_action;
    reach_quick_settings_state state;

    reach_system_status *status;

    int32_t bluetooth_pending_active;
    double bluetooth_pending_elapsed_seconds;
    double bluetooth_pending_refresh_elapsed_seconds;

    uint64_t retired_render_icons[REACH_QUICK_SETTINGS_MAX_RETIRED_RENDER_ICONS];
    size_t retired_render_icon_count;
};

static reach_pressable_feedback_style
reach_quick_settings_pressable_feedback(reach_quick_settings *quick_settings)
{
    reach_pressable_feedback_style feedback = {};
    feedback.animations = quick_settings != nullptr ? &quick_settings->animations : nullptr;
    feedback.track = REACH_QUICK_SETTINGS_ANIMATION_PRESS_FEEDBACK;
    feedback.pressed_value = 1.0f;
    feedback.press_seconds = 0.055;
    feedback.release_seconds = 0.10;
    feedback.press_easing = REACH_EASING_EASE_IN_OUT;
    feedback.release_easing = REACH_EASING_EASE_OUT;
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

static float reach_quick_settings_expansion_value(const reach_quick_settings *quick_settings,
                                                  size_t track)
{
    return quick_settings != nullptr
               ? reach_quick_settings_clamp01(
                     reach_animation_manager_value(&quick_settings->animations, track))
               : 0.0f;
}

static int32_t
reach_quick_settings_expansion_animation_active(const reach_quick_settings *quick_settings)
{
    return quick_settings != nullptr &&
           (reach_animation_manager_active(
                &quick_settings->animations,
                REACH_QUICK_SETTINGS_ANIMATION_OUTPUT_DEVICES_EXPANSION) ||
            reach_animation_manager_active(
                &quick_settings->animations,
                REACH_QUICK_SETTINGS_ANIMATION_APP_VOLUMES_EXPANSION));
}

static void reach_quick_settings_animate_expansion(reach_quick_settings *quick_settings,
                                                   size_t track, int32_t expanded)
{
    if (quick_settings != nullptr)
    {
        reach_animation_manager_animate_to(&quick_settings->animations, track,
                                           expanded ? 1.0f : 0.0f,
                                           REACH_QUICK_SETTINGS_EXPANSION_SECONDS,
                                           REACH_EASING_EASE_OUT);
    }
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

float reach_quick_settings_press_feedback_value(reach_quick_settings *quick_settings)
{
    reach_pressable_feedback_style feedback =
        reach_quick_settings_pressable_feedback(quick_settings);
    return quick_settings != nullptr
               ? reach_pressable_feedback_value(&quick_settings->pressable, &feedback)
               : 0.0f;
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
    state->drag.active = 0;
    state->drag.type = REACH_QUICK_SETTINGS_HIT_NONE;
    state->drag.level_valid = 0;
    reach_quick_settings_reset_pressable(quick_settings);
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
    reach_animation_manager_reset(&quick_settings->animations,
                                  REACH_QUICK_SETTINGS_ANIMATION_HEIGHT);
    reach_animation_manager_set(&quick_settings->animations,
                                REACH_QUICK_SETTINGS_ANIMATION_OUTPUT_DEVICES_EXPANSION, 0.0f);
    reach_animation_manager_set(&quick_settings->animations,
                                REACH_QUICK_SETTINGS_ANIMATION_APP_VOLUMES_EXPANSION, 0.0f);
    state->open = 0;
    state->notch_anchor_x = 0.0f;
    state->bounds = {};
    state->target_bounds = {};
    state->content_bounds = {};
    state->output_devices_expansion = 0.0f;
    state->app_volumes_expansion = 0.0f;
    state->layout = {};
    state->drag = {};
    reach_quick_settings_reset_pressable(quick_settings);
    quick_settings->bluetooth_pending_active = 0;
    quick_settings->bluetooth_pending_elapsed_seconds = 0.0;
    quick_settings->bluetooth_pending_refresh_elapsed_seconds = 0.0;

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
    }
}

void reach_quick_settings_apply_output_devices(reach_quick_settings *quick_settings,
                                               const reach_audio_output_device_list *devices)
{
    if (quick_settings != nullptr)
    {
        reach_quick_settings_model_set_output_devices(
            &reach_quick_settings_state_mut(quick_settings)->model, devices);
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
    int32_t animations_were_active =
        quick_settings != nullptr &&
        reach_animation_manager_any_active(&quick_settings->animations);
    reach_quick_settings_tick(quick_settings, delta_seconds);
    if (quick_settings != nullptr)
    {
        reach_pressable_feedback_style feedback =
            reach_quick_settings_pressable_feedback(quick_settings);
        reach_pressable_settle_feedback(&quick_settings->pressable, &feedback);
    }
    if (out != nullptr && (animations_were_active ||
                           (quick_settings != nullptr &&
                            reach_animation_manager_any_active(&quick_settings->animations))))
    {
        out->redraw = 1;
    }
}

static int32_t reach_quick_settings_capsule_is_open(const void *capsule)
{
    return reach_quick_settings_is_open(static_cast<const reach_quick_settings *>(capsule));
}

static int32_t reach_quick_settings_capsule_needs_frame(const void *capsule)
{
    const reach_quick_settings *quick_settings = static_cast<const reach_quick_settings *>(capsule);
    if (quick_settings == nullptr)
    {
        return 0;
    }
    return reach_animation_manager_any_active(&quick_settings->animations) ||
           quick_settings->bluetooth_pending_active ||
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

static reach_quick_settings_hit_result
reach_quick_settings_capsule_hit_test(reach_quick_settings *quick_settings, int32_t x, int32_t y)
{
    reach_quick_settings_hit_result hit = {};
    if (quick_settings == nullptr)
    {
        return hit;
    }
    const reach_quick_settings_state *state = &quick_settings->state;
    return reach_quick_settings_hit_test(&state->layout, &state->model, (float)x - state->bounds.x,
                                         (float)y - state->bounds.y);
}

static int32_t reach_quick_settings_utf16_equal(const uint16_t *a, const uint16_t *b)
{
    if (a == nullptr || b == nullptr)
    {
        return a == b;
    }
    size_t index = 0;
    while (a[index] != 0 && b[index] != 0 && a[index] == b[index])
    {
        ++index;
    }
    return a[index] == b[index];
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
               reach_quick_settings_utf16_equal(pressed->output_device_id,
                                                released->output_device_id);
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

static void reach_quick_settings_capsule_apply_action(const reach_quick_settings_action *action,
                                                      reach_capsule_pointer_result *out)
{
    if (action == nullptr || out == nullptr)
    {
        return;
    }
    out->action.value = action->volume_level;
    out->action.index = action->session_index;
    switch (action->type)
    {
    case REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME:
        out->action.kind = REACH_QUICK_SETTINGS_POINTER_ACTION_SET_MAIN_VOLUME;
        break;
    case REACH_QUICK_SETTINGS_ACTION_SET_SESSION_VOLUME:
        out->action.kind = REACH_QUICK_SETTINGS_POINTER_ACTION_SET_SESSION_VOLUME;
        break;
    case REACH_QUICK_SETTINGS_ACTION_SET_BRIGHTNESS:
        out->action.kind = REACH_QUICK_SETTINGS_POINTER_ACTION_SET_BRIGHTNESS;
        break;
    case REACH_QUICK_SETTINGS_ACTION_NETWORK_TILE:
        out->action.kind = REACH_QUICK_SETTINGS_POINTER_ACTION_NETWORK_TILE;
        break;
    case REACH_QUICK_SETTINGS_ACTION_TOGGLE_BLUETOOTH:
        out->action.kind = REACH_QUICK_SETTINGS_POINTER_ACTION_TOGGLE_BLUETOOTH;
        break;
    case REACH_QUICK_SETTINGS_ACTION_OPEN_PROJECT:
        out->action.kind = REACH_QUICK_SETTINGS_POINTER_ACTION_OPEN_PROJECT;
        break;
    case REACH_QUICK_SETTINGS_ACTION_TOGGLE_OUTPUT_DEVICES:
        out->action.kind = REACH_QUICK_SETTINGS_POINTER_ACTION_TOGGLE_OUTPUT_DEVICES;
        break;
    case REACH_QUICK_SETTINGS_ACTION_SET_OUTPUT_DEVICE:
        out->action.kind = REACH_QUICK_SETTINGS_POINTER_ACTION_SET_OUTPUT_DEVICE;
        out->action.index = action->output_device_index;
        break;
    case REACH_QUICK_SETTINGS_ACTION_EXPAND:
        out->action.kind = REACH_QUICK_SETTINGS_POINTER_ACTION_EXPAND;
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
    if (quick_settings == nullptr || event == nullptr || out == nullptr)
    {
        return;
    }

    if (event->kind == REACH_POINTER_EVENT_DOWN && event->button == REACH_POINTER_BUTTON_PRIMARY)
    {
        reach_quick_settings_action action =
            reach_quick_settings_begin_slider_gesture_if_hit(quick_settings, event->x, event->y);
        if (reach_quick_settings_drag_active(quick_settings))
        {
            out->handled = 1;
            reach_quick_settings_capsule_apply_action(&action, out);
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
            reach_quick_settings_capsule_apply_action(&released_action, out);
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
        reach_quick_settings_capsule_apply_action(&action, out);
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
    reach_animation_manager_init(&quick_settings->animations, quick_settings->animation_tracks,
                                 REACH_QUICK_SETTINGS_ANIMATION_COUNT);
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

static void reach_quick_settings_retire_audio_render_icons(reach_quick_settings *quick_settings)
{
    const reach_quick_settings_model *model = &quick_settings->state.model;

    size_t session_count = model->sessions.count;
    if (session_count > REACH_AUDIO_VOLUME_MAX_SESSIONS)
    {
        session_count = REACH_AUDIO_VOLUME_MAX_SESSIONS;
    }
    for (size_t index = 0; index < session_count; ++index)
    {
        reach_quick_settings_retire_render_icon(quick_settings,
                                                model->sessions.sessions[index].icon_id);
    }

    size_t device_count = model->output_devices.count;
    if (device_count > REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES)
    {
        device_count = REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES;
    }
    for (size_t index = 0; index < device_count; ++index)
    {
        reach_quick_settings_retire_render_icon(quick_settings,
                                                model->output_devices.devices[index].icon_id);
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
    quick_settings->retired_render_icon_count = 0;
    return count;
}

void reach_quick_settings_process_changes(reach_quick_settings *quick_settings,
                                          double delta_seconds, reach_feature_tick_result *out)
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

    if (state->open)
    {
        if (state->model.bluetooth_pending && quick_settings->bluetooth_pending_active)
        {
            if (delta_seconds < 0.0)
            {
                delta_seconds = 0.0;
            }
            quick_settings->bluetooth_pending_elapsed_seconds += delta_seconds;
            quick_settings->bluetooth_pending_refresh_elapsed_seconds += delta_seconds;

            if (quick_settings->bluetooth_pending_elapsed_seconds >=
                REACH_QUICK_SETTINGS_BLUETOOTH_PENDING_TIMEOUT_SECONDS)
            {
                reach_quick_settings_set_bluetooth_pending(quick_settings, 0, 0);
                reach_quick_settings_refresh_system(quick_settings,
                                                    REACH_SYSTEM_CONTROLS_CHANGE_BLUETOOTH);
                out->redraw = 1;
            }
            else
            {
                if (quick_settings->bluetooth_pending_refresh_elapsed_seconds >=
                        REACH_QUICK_SETTINGS_BLUETOOTH_PENDING_REFRESH_SECONDS &&
                    (quick_settings->status == nullptr ||
                     !reach_system_status_system_pending(quick_settings->status)))
                {
                    quick_settings->bluetooth_pending_refresh_elapsed_seconds = 0.0;
                    reach_quick_settings_refresh_system(quick_settings,
                                                        REACH_SYSTEM_CONTROLS_CHANGE_BLUETOOTH);
                }
                out->request_update = 1;
            }
        }
    }

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
            &system_snapshot.brightness, system_snapshot.bluetooth_valid, &apply_result);
        if (apply_result.bluetooth_pending_cleared)
        {
            quick_settings->bluetooth_pending_active = 0;
            quick_settings->bluetooth_pending_elapsed_seconds = 0.0;
            quick_settings->bluetooth_pending_refresh_elapsed_seconds = 0.0;
        }
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
        reach_quick_settings_retire_audio_render_icons(quick_settings);

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

        if (state->open)
        {
            out->relayout = 1;
        }
        out->redraw = 1;
        out->request_update = 1;
    }
}

void reach_quick_settings_tick(reach_quick_settings *quick_settings, double delta_seconds)
{
    if (quick_settings == nullptr)
    {
        return;
    }
    reach_animation_manager_tick(&quick_settings->animations, delta_seconds);
}

void reach_quick_settings_start_height_animation(reach_quick_settings *quick_settings,
                                                 float from_height, float to_height)
{
    if (quick_settings == nullptr)
    {
        return;
    }
    reach_animation_manager_start(&quick_settings->animations,
                                  REACH_QUICK_SETTINGS_ANIMATION_HEIGHT, from_height, to_height,
                                  REACH_QUICK_SETTINGS_EXPANSION_SECONDS, REACH_EASING_EASE_OUT);
}

void reach_quick_settings_reset_height_animation(reach_quick_settings *quick_settings)
{
    if (quick_settings == nullptr)
    {
        return;
    }
    reach_animation_manager_reset(&quick_settings->animations,
                                  REACH_QUICK_SETTINGS_ANIMATION_HEIGHT);
}

int32_t reach_quick_settings_height_animation_active(const reach_quick_settings *quick_settings)
{
    return quick_settings != nullptr &&
           reach_animation_manager_active(&quick_settings->animations,
                                          REACH_QUICK_SETTINGS_ANIMATION_HEIGHT);
}

float reach_quick_settings_height_animation_value(const reach_quick_settings *quick_settings)
{
    if (quick_settings == nullptr)
    {
        return 0.0f;
    }
    return reach_animation_manager_value(&quick_settings->animations,
                                         REACH_QUICK_SETTINGS_ANIMATION_HEIGHT);
}

static reach_rect_f32 reach_quick_settings_content_bounds_for(reach_rect_f32 surface_bounds,
                                                              const reach_theme *theme,
                                                              float dpi_scale,
                                                              int32_t drop_direction)
{
    reach_rect_f32 bounds =
        reach_theme_border_content_rect(theme, dpi_scale, surface_bounds);

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
    const float surface_vertical_padding =
        border_thickness * 2.0f + 8.0f * scale + 12.0f * scale +
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

static int32_t reach_quick_settings_height_changed(float a, float b)
{
    float diff = a - b;
    if (diff < 0.0f)
    {
        diff = -diff;
    }
    return diff > 0.5f;
}

void reach_quick_settings_refresh_layout(reach_quick_settings *quick_settings,
                                         const reach_quick_settings_layout_context *ctx)
{
    if (quick_settings == nullptr || ctx == nullptr)
    {
        return;
    }

    reach_quick_settings_state *state = reach_quick_settings_state_mut(quick_settings);

    reach_popup_placement placement = reach_quick_settings_placement(quick_settings, ctx, 0.0f);
    state->target_bounds = placement.bounds;
    if (!reach_quick_settings_height_animation_active(quick_settings))
    {
        state->bounds = state->target_bounds;
    }
    state->notch_anchor_x = placement.notch_anchor_x;
    state->drop_direction = ctx->drop_direction;

    reach_rect_f32 surface_bounds = {};
    surface_bounds.width = state->bounds.width;
    surface_bounds.height = state->bounds.height;
    state->content_bounds = reach_quick_settings_content_bounds_for(
        surface_bounds, ctx->theme, ctx->dpi_scale, ctx->drop_direction);
    float output_devices_expansion = reach_quick_settings_expansion_value(
        quick_settings, REACH_QUICK_SETTINGS_ANIMATION_OUTPUT_DEVICES_EXPANSION);
    float app_volumes_expansion = reach_quick_settings_expansion_value(
        quick_settings, REACH_QUICK_SETTINGS_ANIMATION_APP_VOLUMES_EXPANSION);
    state->output_devices_expansion = output_devices_expansion;
    state->app_volumes_expansion = app_volumes_expansion;
    state->layout = reach_quick_settings_layout_for_expansion_scaled(
        state->content_bounds, ctx->theme, &state->model, output_devices_expansion,
        app_volumes_expansion, ctx->dpi_scale);
}

void reach_quick_settings_relayout(reach_quick_settings *quick_settings,
                                   const reach_quick_settings_layout_context *ctx,
                                   int32_t animate_height)
{
    if (quick_settings == nullptr || ctx == nullptr)
    {
        return;
    }

    reach_quick_settings_state *state = reach_quick_settings_state_mut(quick_settings);

    reach_rect_f32 old_target = state->target_bounds;
    reach_rect_f32 current_bounds = state->bounds;
    reach_rect_f32 new_target = reach_quick_settings_placement(quick_settings, ctx, 0.0f).bounds;

    state->target_bounds = new_target;

    if (animate_height && reach_quick_settings_height_changed(old_target.height, new_target.height))
    {
        reach_quick_settings_start_height_animation(quick_settings, current_bounds.height,
                                                    new_target.height);
    }
    else if (!reach_quick_settings_height_animation_active(quick_settings))
    {
        state->bounds = new_target;
    }

    reach_quick_settings_refresh_layout(quick_settings, ctx);
}

int32_t reach_quick_settings_update_open_animation(reach_quick_settings *quick_settings,
                                                   const reach_quick_settings_layout_context *ctx)
{
    if (quick_settings == nullptr || ctx == nullptr)
    {
        return 0;
    }

    reach_quick_settings_state *state = reach_quick_settings_state_mut(quick_settings);
    float output_devices_expansion = reach_quick_settings_expansion_value(
        quick_settings, REACH_QUICK_SETTINGS_ANIMATION_OUTPUT_DEVICES_EXPANSION);
    float app_volumes_expansion = reach_quick_settings_expansion_value(
        quick_settings, REACH_QUICK_SETTINGS_ANIMATION_APP_VOLUMES_EXPANSION);
    int32_t expansion_layout_changed =
        fabsf(state->output_devices_expansion - output_devices_expansion) > 0.001f ||
        fabsf(state->app_volumes_expansion - app_volumes_expansion) > 0.001f;

    if (!state->open)
    {
        return 0;
    }

    if (reach_quick_settings_height_animation_active(quick_settings) ||
        reach_quick_settings_expansion_animation_active(quick_settings) ||
        expansion_layout_changed ||
        reach_quick_settings_height_changed(state->bounds.height, state->target_bounds.height))
    {
        reach_popup_placement animated = reach_quick_settings_placement(
            quick_settings, ctx, reach_quick_settings_height_animation_value(quick_settings));

        state->bounds = animated.bounds;

        reach_quick_settings_refresh_layout(quick_settings, ctx);
        return 1;
    }

    return 0;
}

int32_t reach_quick_settings_bluetooth_pending(reach_quick_settings *quick_settings)
{
    return quick_settings != nullptr &&
           reach_quick_settings_state_mut(quick_settings)->model.bluetooth_pending;
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
        quick_settings->bluetooth_pending_active = pending ? 1 : 0;
        quick_settings->bluetooth_pending_elapsed_seconds = 0.0;
        quick_settings->bluetooth_pending_refresh_elapsed_seconds = 0.0;
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
    {
        state->model.output_devices_expanded = 0;
        reach_quick_settings_animate_expansion(
            quick_settings, REACH_QUICK_SETTINGS_ANIMATION_OUTPUT_DEVICES_EXPANSION, 0);
    }
    reach_quick_settings_animate_expansion(
        quick_settings, REACH_QUICK_SETTINGS_ANIMATION_APP_VOLUMES_EXPANSION,
        state->model.expanded);
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
    {
        state->model.expanded = 0;
        reach_quick_settings_animate_expansion(
            quick_settings, REACH_QUICK_SETTINGS_ANIMATION_APP_VOLUMES_EXPANSION, 0);
    }
    reach_quick_settings_animate_expansion(
        quick_settings, REACH_QUICK_SETTINGS_ANIMATION_OUTPUT_DEVICES_EXPANSION,
        state->model.output_devices_expanded);
    return state->model.output_devices_expanded;
}

void reach_quick_settings_collapse_output_devices(reach_quick_settings *quick_settings)
{
    if (quick_settings != nullptr)
    {
        reach_quick_settings_state_mut(quick_settings)->model.output_devices_expanded = 0;
        reach_quick_settings_animate_expansion(
            quick_settings, REACH_QUICK_SETTINGS_ANIMATION_OUTPUT_DEVICES_EXPANSION, 0);
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
                                              int32_t bluetooth_valid,
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

    if (bluetooth_pending && bluetooth_valid &&
        (!state->model.bluetooth.available ||
         state->model.bluetooth.enabled == bluetooth_pending_enabled))
    {
        reach_quick_settings_model_set_bluetooth_pending(&state->model, 0, 0);
        out->bluetooth_pending_cleared = 1;
    }

    int32_t layout_changed = previous_brightness.available != state->model.brightness.available;

    out->relayout = layout_changed && state->open;
}
