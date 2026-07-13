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

static size_t reach_quick_settings_visible_session_count(const reach_quick_settings_model *model)
{
    if (model == nullptr || !model->expanded)
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
    if (model == nullptr || !model->output_devices_expanded)
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
    size_t count = 3;
    if (model != nullptr && model->power.has_battery)
    {
        ++count;
    }
    return count;
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
    model->power = {};
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
    float level = reach_quick_settings_clamp01(volume_level);
    if (muted || level <= 0.0f)
    {
        return REACH_VECTOR_ICON_VOLUME_ZERO;
    }
    if (level < 0.5f)
    {
        return REACH_VECTOR_ICON_VOLUME_LOW;
    }
    return REACH_VECTOR_ICON_VOLUME_HIGH;
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
                                                  const reach_power_state *power,
                                                  const reach_brightness_state *brightness)
{
    if (model == nullptr)
    {
        return;
    }

    model->network = network != nullptr ? *network : reach_network_state{};
    model->bluetooth = bluetooth != nullptr ? *bluetooth : reach_bluetooth_state{};
    model->power = power != nullptr ? *power : reach_power_state{};
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
    model->power.has_battery = model->power.has_battery ? 1 : 0;
    model->power.battery_saver_on = model->power.battery_saver_on ? 1 : 0;
    model->brightness.available = model->brightness.available ? 1 : 0;
}

static void reach_quick_settings_model_set_bluetooth_pending(
    reach_quick_settings_model *model, int32_t pending, int32_t pending_enabled)
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

float reach_quick_settings_content_height_for_model_scaled(const reach_quick_settings_model *model,
                                                           float dpi_scale)
{
    reach_quick_settings_metrics metrics = reach_quick_settings_metrics_for_scale(dpi_scale);

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
    float output_component_height = 0.0f;
    if (model != nullptr && model->output_devices_expanded)
    {
        output_component_height = metrics.output_button_gap + metrics.output_title_height +
                                  metrics.output_panel_gap +
                                  (float)visible_output_devices * metrics.output_device_row_height;
    }
    else
    {
        output_component_height = metrics.output_button_gap + metrics.output_button_height;
    }
    float app_volume_component_height = 0.0f;
    if (model != nullptr && model->expanded)
    {
        app_volume_component_height = metrics.app_title_gap + metrics.app_title_height +
                                      metrics.app_panel_gap +
                                      (float)(visible_sessions + 1) * metrics.app_volume_row_height;
    }
    else
    {
        app_volume_component_height = metrics.expand_button_gap + metrics.expand_button_height;
    }
    return metrics.content_padding * 2.0f + grid_component_height + brightness_component_height +
           volume_component_height + output_component_height + app_volume_component_height;
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

    tile->icon = reach_quick_settings_rect(
        tile->bounds.x + values->text_padding,
        tile->bounds.y + (tile->bounds.height - values->system_tile_icon_size) * 0.5f,
        values->system_tile_icon_size, values->system_tile_icon_size);

    tile->label = reach_quick_settings_rect(
        tile->icon.x + values->system_tile_icon_size + values->system_tile_icon_gap, tile->bounds.y,
        tile->bounds.x + tile->bounds.width - values->content_padding -
            (tile->icon.x + values->system_tile_icon_size + values->system_tile_icon_gap),
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

reach_quick_settings_layout reach_quick_settings_layout_for_content_bounds_scaled(
    reach_rect_f32 content_bounds, const reach_theme *theme,
    const reach_quick_settings_model *model, float dpi_scale)
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
    if (model != nullptr && model->power.has_battery)
    {
        reach_quick_settings_place_system_tile(&layout.battery_saver_tile, grid_bounds, tile_width,
                                               tile_index++, &metrics);
    }
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
    if (model != nullptr && model->output_devices_expanded)
    {
        layout.output_devices_title.x = pill_bounds.x;
        layout.output_devices_title.y = next_y + metrics.output_button_gap;
        layout.output_devices_title.width = pill_bounds.width;
        layout.output_devices_title.height = metrics.output_title_height;

        layout.output_devices_title_chevron.width = metrics.chevron_icon_size;
        layout.output_devices_title_chevron.height = metrics.chevron_icon_size;
        layout.output_devices_title_chevron.x = layout.output_devices_title.x +
                                                layout.output_devices_title.width -
                                                metrics.content_padding - metrics.chevron_icon_size;
        layout.output_devices_title_chevron.y =
            layout.output_devices_title.y +
            (layout.output_devices_title.height - metrics.chevron_icon_size) * 0.5f;

        layout.output_devices_panel.x = pill_bounds.x;
        layout.output_devices_panel.y = layout.output_devices_title.y +
                                        layout.output_devices_title.height +
                                        metrics.output_panel_gap;
        layout.output_devices_panel.width = pill_bounds.width;
        layout.output_devices_panel.height =
            (float)visible_output_devices * metrics.output_device_row_height;

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

        next_y = layout.output_devices_panel.y + layout.output_devices_panel.height;
    }
    else
    {
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
                                              metrics.output_icon_size +
                                              metrics.output_row_label_gap;
        layout.output_device_button_label.y = layout.output_device_button.y;
        layout.output_device_button_label.width = layout.output_device_button_chevron.x -
                                                  layout.output_device_button_label.x -
                                                  metrics.content_padding;
        layout.output_device_button_label.height = layout.output_device_button.height;
        if (layout.output_device_button_label.width < 0.0f)
        {
            layout.output_device_button_label.width = 0.0f;
        }

        next_y = layout.output_device_button.y + layout.output_device_button.height;
    }

    layout.app_volume_row_count = 0;
    size_t visible_sessions = reach_quick_settings_visible_session_count(model);
    if (model != nullptr && model->expanded)
    {
        layout.app_volumes_title.x = pill_bounds.x;
        layout.app_volumes_title.y = next_y + metrics.app_title_gap;
        layout.app_volumes_title.width = pill_bounds.width;
        layout.app_volumes_title.height = metrics.app_title_height;

        layout.app_volumes_panel.x = pill_bounds.x;
        layout.app_volumes_panel.y =
            layout.app_volumes_title.y + layout.app_volumes_title.height + metrics.app_panel_gap;
        layout.app_volumes_panel.width = pill_bounds.width;
        layout.app_volumes_panel.height =
            (float)(visible_sessions + 1) * metrics.app_volume_row_height;

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
                                            metrics.app_row_percent_gap -
                                            metrics.app_row_slider_width;
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

        next_y = layout.app_volumes_panel.y + layout.app_volumes_panel.height;

        layout.expand_button.x = layout.app_volumes_panel.x;
        layout.expand_button.y =
            layout.app_volumes_panel.y + (float)visible_sessions * metrics.app_volume_row_height;
        layout.expand_button.width = layout.app_volumes_panel.width;
        layout.expand_button.height = metrics.app_volume_row_height;
    }
    else
    {
        layout.expand_button =
            reach_quick_settings_content_line(content_bounds, next_y + metrics.expand_button_gap,
                                              metrics.expand_button_height, &metrics);
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

enum
{
    REACH_QUICK_SETTINGS_ANIMATION_HEIGHT = 0,
    REACH_QUICK_SETTINGS_ANIMATION_COUNT
};

static const double REACH_QUICK_SETTINGS_BLUETOOTH_PENDING_REFRESH_SECONDS = 0.35;
static const double REACH_QUICK_SETTINGS_BLUETOOTH_PENDING_TIMEOUT_SECONDS = 8.0;

#define REACH_QUICK_SETTINGS_MAX_RETIRED_RENDER_ICONS \
    (REACH_AUDIO_VOLUME_MAX_SESSIONS + REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES)

struct reach_quick_settings
{
    reach_animation_manager animations;
    reach_animation_track animation_tracks[REACH_QUICK_SETTINGS_ANIMATION_COUNT];
    reach_quick_settings_state state;

    reach_system_status *status;

    int32_t bluetooth_pending_active;
    double bluetooth_pending_elapsed_seconds;
    double bluetooth_pending_refresh_elapsed_seconds;

    uint64_t retired_render_icons[REACH_QUICK_SETTINGS_MAX_RETIRED_RENDER_ICONS];
    size_t retired_render_icon_count;
};

const reach_quick_settings_state *reach_quick_settings_state_ptr(reach_quick_settings *quick_settings)
{
    return quick_settings != nullptr ? &quick_settings->state : nullptr;
}

reach_quick_settings_state *reach_quick_settings_state_mut(reach_quick_settings *quick_settings)
{
    return quick_settings != nullptr ? &quick_settings->state : nullptr;
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
}

void reach_quick_settings_reset(reach_quick_settings *quick_settings)
{
    if (quick_settings == nullptr)
    {
        return;
    }
    reach_quick_settings_state *state = reach_quick_settings_state_mut(quick_settings);
    reach_quick_settings_model_init(&state->model);
    state->open = 0;
    state->notch_anchor_x = 0.0f;
    state->bounds = {};
    state->target_bounds = {};
    state->content_bounds = {};
    state->layout = {};
    state->drag = {};
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
    reach_quick_settings_tick(quick_settings, delta_seconds);
    if (out != nullptr && reach_quick_settings_height_animation_active(quick_settings))
    {
        out->redraw = 1;
    }
}

static int32_t reach_quick_settings_capsule_is_open(const void *capsule)
{
    return reach_quick_settings_is_open(static_cast<const reach_quick_settings *>(capsule));
}

static void reach_quick_settings_capsule_force_close(void *capsule)
{
    reach_quick_settings_force_close(static_cast<reach_quick_settings *>(capsule));
}

static int32_t reach_quick_settings_capsule_needs_frame(const void *capsule)
{
    const reach_quick_settings *quick_settings =
        static_cast<const reach_quick_settings *>(capsule);
    if (quick_settings == nullptr)
    {
        return 0;
    }
    return reach_quick_settings_height_animation_active(quick_settings) ||
           quick_settings->bluetooth_pending_active ||
           (quick_settings->status != nullptr &&
            (reach_system_status_audio_pending(quick_settings->status) ||
             reach_system_status_system_pending(quick_settings->status)));
}

static int32_t reach_quick_settings_capsule_wants_pointer_move(const void *capsule)
{
    return reach_quick_settings_drag_active(static_cast<const reach_quick_settings *>(capsule));
}

static void reach_quick_settings_capsule_apply_action(
    const reach_quick_settings_action *action, reach_capsule_pointer_result *out)
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
    case REACH_QUICK_SETTINGS_ACTION_TOGGLE_BATTERY_SAVER:
        out->action.kind = REACH_QUICK_SETTINGS_POINTER_ACTION_TOGGLE_BATTERY_SAVER;
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
    case REACH_QUICK_SETTINGS_ACTION_SET_SESSION_MUTED:
    case REACH_QUICK_SETTINGS_ACTION_NONE:
    default:
        break;
    }
}

static void reach_quick_settings_capsule_handle_pointer(
    void *capsule, const reach_pointer_event *event, reach_capsule_pointer_result *out)
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

    if (event->kind == REACH_POINTER_EVENT_DOWN)
    {
        reach_quick_settings_action action =
            reach_quick_settings_begin_drag_if_hit(quick_settings, event->x, event->y);
        if (action.type == REACH_QUICK_SETTINGS_ACTION_NONE)
        {
            return;
        }
        out->handled = 1;
        reach_quick_settings_capsule_apply_action(&action, out);
        if (reach_quick_settings_drag_active(quick_settings))
        {
            out->capture = 1;
            out->sync_pointer_subscriptions = 1;
        }
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_MOVE &&
        reach_quick_settings_drag_active(quick_settings))
    {
        reach_quick_settings_action action =
            reach_quick_settings_drag_move(quick_settings, event->x, event->y);
        out->handled = 1;
        reach_quick_settings_capsule_apply_action(&action, out);
        return;
    }
    if ((event->kind == REACH_POINTER_EVENT_UP ||
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
        reach_quick_settings_capsule_reset,       reach_quick_settings_capsule_tick,
        reach_quick_settings_capsule_is_open,     reach_quick_settings_capsule_force_close,
        nullptr  ,               reach_quick_settings_capsule_needs_frame,
        reach_quick_settings_capsule_wants_pointer_move,
        reach_quick_settings_capsule_handle_pointer,
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
    if (quick_settings->retired_render_icon_count <
        REACH_QUICK_SETTINGS_MAX_RETIRED_RENDER_ICONS)
    {
        quick_settings->retired_render_icons[quick_settings->retired_render_icon_count++] =
            icon_id;
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
                                          uint32_t change_flags, double delta_seconds,
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

    if (state->open)
    {
        if (change_flags != 0)
        {
            reach_quick_settings_refresh_system(quick_settings, change_flags);
        }

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
            &system_snapshot.power, &system_snapshot.brightness, system_snapshot.bluetooth_valid,
            &apply_result);
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
                                  0.16, REACH_EASING_EASE_IN_OUT);
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
                                                              float dpi_scale)
{
    reach_rect_f32 bounds = surface_bounds;

    float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    const float horizontal_padding = 8.0f * scale;
    const float top_padding = 8.0f * scale;
    const float bottom_padding = 12.0f * scale;

    bounds.x += horizontal_padding;
    bounds.y += top_padding;
    bounds.width -= horizontal_padding * 2.0f;
    bounds.height -= top_padding + bottom_padding + reach_popup_notch_height_scaled(scale);

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

static void reach_quick_settings_target_size(reach_quick_settings *quick_settings, float scale,
                                             float *out_width, float *out_height)
{
    const float surface_vertical_padding =
        8.0f * scale + 12.0f * scale + reach_popup_notch_height_scaled(scale);
    float content_height = reach_quick_settings_content_height_for_model_scaled(
        &reach_quick_settings_state_mut(quick_settings)->model, scale);

    if (out_width != nullptr)
    {
        *out_width = 280.0f * scale;
    }
    if (out_height != nullptr)
    {
        *out_height = content_height + surface_vertical_padding;
    }
}

static reach_rect_f32
reach_quick_settings_target_bounds(reach_quick_settings *quick_settings,
                                   const reach_quick_settings_layout_context *ctx)
{
    reach_rect_f32 bounds = {};

    float width = 280.0f;
    float height = 140.0f;
    reach_quick_settings_target_size(quick_settings, ctx->dpi_scale, &width, &height);

    const float gap = 8.0f * ctx->dpi_scale;

    bounds.width = width;
    bounds.height = height;
    bounds.x = ctx->anchor_x - width * 0.5f;
    bounds.y = ctx->dock_top - height - gap;
    return bounds;
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

    state->target_bounds = reach_quick_settings_target_bounds(quick_settings, ctx);
    if (!reach_quick_settings_height_animation_active(quick_settings))
    {
        state->bounds = state->target_bounds;
    }
    state->notch_anchor_x = ctx->anchor_x;

    reach_rect_f32 surface_bounds = {};
    surface_bounds.width = state->bounds.width;
    surface_bounds.height = state->bounds.height;
    state->content_bounds = reach_quick_settings_content_bounds_for(surface_bounds, ctx->dpi_scale);
    state->layout = reach_quick_settings_layout_for_content_bounds_scaled(
        state->content_bounds, ctx->theme, &state->model, ctx->dpi_scale);
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
    reach_rect_f32 new_target = reach_quick_settings_target_bounds(quick_settings, ctx);

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

int32_t
reach_quick_settings_update_open_animation(reach_quick_settings *quick_settings,
                                           const reach_quick_settings_layout_context *ctx)
{
    if (quick_settings == nullptr || ctx == nullptr)
    {
        return 0;
    }

    reach_quick_settings_state *state = reach_quick_settings_state_mut(quick_settings);

    if (!state->open)
    {
        return 0;
    }

    if (reach_quick_settings_height_animation_active(quick_settings) ||
        reach_quick_settings_height_changed(state->bounds.height, state->target_bounds.height))
    {
        const float gap = 8.0f * ctx->dpi_scale;
        reach_rect_f32 animated = state->target_bounds;
        animated.height = reach_quick_settings_height_animation_value(quick_settings);
        animated.y = floorf(ctx->dock_top - animated.height - gap + 0.5f);

        state->bounds = animated;

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
    }
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
    }
    return state->model.output_devices_expanded;
}

void reach_quick_settings_collapse_output_devices(reach_quick_settings *quick_settings)
{
    if (quick_settings != nullptr)
    {
        reach_quick_settings_state_mut(quick_settings)->model.output_devices_expanded = 0;
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

const uint16_t *reach_quick_settings_output_device_id(
    const reach_quick_settings *quick_settings, size_t output_device_index)
{
    if (quick_settings == nullptr ||
        output_device_index >= quick_settings->state.model.output_devices.count)
    {
        return nullptr;
    }
    return quick_settings->state.model.output_devices.devices[output_device_index].device_id;
}

void reach_quick_settings_apply_system_states(
    reach_quick_settings *quick_settings, const reach_network_state *network,
    const reach_bluetooth_state *bluetooth, const reach_power_state *power,
    const reach_brightness_state *brightness, int32_t bluetooth_valid,
    reach_quick_settings_system_apply_result *out)
{
    if (quick_settings == nullptr || out == nullptr)
    {
        return;
    }

    reach_quick_settings_state *state = reach_quick_settings_state_mut(quick_settings);

    reach_power_state previous_power = state->model.power;
    reach_brightness_state previous_brightness = state->model.brightness;
    int32_t bluetooth_pending = state->model.bluetooth_pending;
    int32_t bluetooth_pending_enabled = state->model.bluetooth_pending_enabled;

    reach_quick_settings_model_set_system_states(&state->model, network, bluetooth, power,
                                                 brightness);

    if (bluetooth_pending && bluetooth_valid &&
        (!state->model.bluetooth.available ||
         state->model.bluetooth.enabled == bluetooth_pending_enabled))
    {
        reach_quick_settings_model_set_bluetooth_pending(&state->model, 0, 0);
        out->bluetooth_pending_cleared = 1;
    }

    int32_t layout_changed =
        previous_power.has_battery != state->model.power.has_battery ||
        previous_brightness.available != state->model.brightness.available;

    out->relayout = layout_changed && state->open;
}
