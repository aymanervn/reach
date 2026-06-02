#include "reach/features/quick_settings.h"

#include "quick_settings_common.h"
#include "quick_settings_metrics.h"

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
                                                        float height)
{
    return reach_quick_settings_rect(
        content_bounds.x + reach_quick_settings_metrics_values.content_padding, y,
        content_bounds.width - reach_quick_settings_metrics_values.content_padding * 2.0f, height);
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

void reach_quick_settings_model_set_bluetooth_pending(reach_quick_settings_model *model,
                                                      int32_t pending, int32_t pending_enabled)
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
reach_quick_settings_volume_pill_layout_for_bounds(reach_rect_f32 bounds, const reach_theme *theme)
{
    (void)theme;

    reach_quick_settings_volume_pill_layout layout = {};
    layout.bounds = bounds;

    const reach_quick_settings_metrics &metrics = reach_quick_settings_metrics_values;

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
    const reach_quick_settings_metrics &metrics = reach_quick_settings_metrics_values;

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
                                                   size_t tile_index)
{
    if (tile == nullptr)
    {
        return;
    }

    const reach_quick_settings_metrics &metrics = reach_quick_settings_metrics_values;
    size_t tile_row = tile_index / 2;
    size_t tile_column = tile_index % 2;

    tile->bounds = reach_quick_settings_rect(
        grid_bounds.x + (float)tile_column * (tile_width + metrics.system_grid_gap),
        grid_bounds.y +
            (float)tile_row * (metrics.system_grid_tile_height + metrics.system_grid_gap),
        tile_width, metrics.system_grid_tile_height);

    tile->icon = reach_quick_settings_rect(
        tile->bounds.x + metrics.text_padding,
        tile->bounds.y + (tile->bounds.height - metrics.system_tile_icon_size) * 0.5f,
        metrics.system_tile_icon_size, metrics.system_tile_icon_size);

    tile->label = reach_quick_settings_rect(
        tile->icon.x + metrics.system_tile_icon_size + metrics.system_tile_icon_gap, tile->bounds.y,
        tile->bounds.x + tile->bounds.width - metrics.content_padding -
            (tile->icon.x + metrics.system_tile_icon_size + metrics.system_tile_icon_gap),
        tile->bounds.height);
}

reach_quick_settings_layout
reach_quick_settings_layout_for_content_bounds(reach_rect_f32 content_bounds,
                                               const reach_theme *theme,
                                               const reach_quick_settings_model *model)
{
    (void)theme;

    reach_quick_settings_layout layout = {};
    layout.content_bounds = content_bounds;

    const reach_quick_settings_metrics &metrics = reach_quick_settings_metrics_values;

    reach_rect_f32 grid_bounds = reach_quick_settings_content_line(
        content_bounds, content_bounds.y + metrics.content_padding, 0.0f);

    float tile_width =
        reach_quick_settings_clamp_min0((grid_bounds.width - metrics.system_grid_gap) * 0.5f);

    size_t tile_index = 0;
    reach_quick_settings_place_system_tile(&layout.network_tile, grid_bounds, tile_width,
                                           tile_index++);
    reach_quick_settings_place_system_tile(&layout.bluetooth_tile, grid_bounds, tile_width,
                                           tile_index++);
    if (model != nullptr && model->power.has_battery)
    {
        reach_quick_settings_place_system_tile(&layout.battery_saver_tile, grid_bounds, tile_width,
                                               tile_index++);
    }
    reach_quick_settings_place_system_tile(&layout.project_tile, grid_bounds, tile_width,
                                           tile_index++);

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
            metrics.pill_height);
        layout.brightness_pill =
            reach_quick_settings_volume_pill_layout_for_bounds(brightness_bounds, theme);
        layout.brightness_slider_track = layout.brightness_pill.slider_track;
        layout.brightness_slider_fill = layout.brightness_pill.slider_fill;
        next_y = brightness_bounds.y + brightness_bounds.height + metrics.system_grid_bottom_gap;
    }

    reach_rect_f32 pill_bounds = reach_quick_settings_content_line(
        content_bounds, next_y + metrics.pill_header_height + metrics.section_header_gap,
        metrics.pill_height);

    layout.main_volume_pill =
        reach_quick_settings_volume_pill_layout_for_bounds(pill_bounds, theme);
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
        layout.output_device_button = reach_quick_settings_content_line(
            content_bounds, next_y + metrics.output_button_gap, metrics.output_button_height);

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
        layout.expand_button = reach_quick_settings_content_line(
            content_bounds, next_y + metrics.expand_button_gap, metrics.expand_button_height);
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
