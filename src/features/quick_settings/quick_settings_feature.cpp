#include "reach/features/common/ui_controls.h"
#include "reach/features/quick_settings.h"
#include "reach/features/common/popup.h"
#include "reach/features/common/bluetooth_session.h"
#include "reach/features/common/wifi_session.h"

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

void reach_quick_settings_model_init(reach_quick_settings_model *model)
{
    if (model == nullptr)
    {
        return;
    }

    model->page = REACH_QUICK_SETTINGS_PAGE_OVERVIEW;
    model->main_volume_level = 0.0f;
    model->main_muted = 0;
    model->input = {};
    model->input_available = 0;
    model->sessions = {};
    model->output_devices = {};
    model->network = {};
    model->bluetooth = {};
    model->bluetooth_pending = 0;
    model->bluetooth_pending_enabled = 0;
    model->brightness = {};
    model->wifi_radio = REACH_WIFI_RADIO_UNAVAILABLE;
    model->wifi_row_count = 0;
    model->wifi_connected_count = 0;
    model->wifi_status = REACH_WIFI_SESSION_STATUS_IDLE;
    model->wifi_status_ssid[0] = 0;
    model->wifi_key_row = -1;
    reach_text_edit_init(&model->wifi_key, REACH_WIFI_KEY_MAXIMUM_LENGTH);
    model->bluetooth_row_count = 0;
    model->bluetooth_paired_count = 0;
    model->bluetooth_status = REACH_BLUETOOTH_SESSION_STATUS_IDLE;
    model->bluetooth_status_device[0] = 0;
    model->bluetooth_action_row = -1;
    model->bluetooth_pin_active = 0;
    model->bluetooth_pin[0] = 0;
    model->bluetooth_scanning = 0;
    model->light_theme = 0;
    model->display_name[0] = 0;
    model->picture_image_id = 0;
    model->uptime_minutes = -1;
    model->power = {};
    model->power.battery_seconds_remaining = -1;
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

static void reach_quick_settings_section_spans(const reach_quick_settings_model *model,
                                               float dpi_scale, float *out_output_span,
                                               float *out_app_span)
{
    reach_quick_settings_metrics metrics = reach_quick_settings_metrics_for_scale(dpi_scale);
    size_t outputs = reach_quick_settings_visible_output_device_count(model);
    size_t sessions = reach_quick_settings_visible_session_count(model);
    *out_output_span = outputs > 0 ? metrics.section_label_height +
                                         (float)outputs * metrics.output_device_row_height
                                   : 0.0f;
    *out_app_span = sessions > 0 ? metrics.section_label_height +
                                       (float)sessions * metrics.app_volume_row_height
                                 : 0.0f;
}
static float reach_quick_settings_rail_full_width(const reach_quick_settings_metrics *metrics)
{
    return metrics->surface_padding + metrics->rail_width;
}

static float reach_quick_settings_rail_item_inset(const reach_quick_settings_metrics *metrics)
{
    return (reach_quick_settings_rail_full_width(metrics) - metrics->rail_item_size) * 0.5f;
}

static float reach_quick_settings_rail_height(const reach_quick_settings_metrics *metrics)
{
    return reach_quick_settings_rail_item_inset(metrics) * 2.0f - metrics->surface_padding * 2.0f +
           reach_quick_settings_stacked_rows_height(
               REACH_QUICK_SETTINGS_PAGE_COUNT, metrics->rail_item_size, metrics->rail_item_gap);
}

static float
reach_quick_settings_header_background_height(const reach_quick_settings_metrics *metrics)
{
    return metrics->avatar_inset * 2.0f + metrics->avatar_size + metrics->avatar_ring * 2.0f;
}

static float reach_quick_settings_header_height(const reach_quick_settings_metrics *metrics)
{
    return reach_quick_settings_header_background_height(metrics) - metrics->surface_padding;
}

static float reach_quick_settings_overview_height(const reach_quick_settings_model *model,
                                                  const reach_quick_settings_metrics *metrics)
{
    float brightness = model != nullptr && model->brightness.available
                           ? metrics->overview_row_gap + metrics->overview_pill_height
                           : 0.0f;
    return metrics->content_padding * 2.0f + metrics->overview_pill_height + brightness +
           metrics->toggle_grid_gap +
           reach_quick_settings_stacked_rows_height(REACH_QUICK_SETTINGS_TOGGLE_COUNT / 2,
                                                    metrics->toggle_height, metrics->toggle_gap);
}

static int32_t reach_quick_settings_wifi_message_shown(const reach_quick_settings_model *model)
{
    return model->wifi_radio != REACH_WIFI_RADIO_ON || model->wifi_row_count == 0;
}

static float reach_quick_settings_wifi_row_offset(const reach_quick_settings_model *model,
                                                  size_t index,
                                                  const reach_quick_settings_metrics *metrics)
{
    float key = model->wifi_key_row >= 0 && index > (size_t)model->wifi_key_row
                    ? metrics->wifi_key_area_height
                    : 0.0f;
    float connected = (float)model->wifi_connected_count * metrics->output_device_row_height;
    if (index < model->wifi_connected_count)
    {
        return (float)index * metrics->output_device_row_height + key;
    }
    return connected + metrics->section_label_height +
           (float)(index - model->wifi_connected_count) * metrics->output_device_row_height + key;
}

static float reach_quick_settings_wifi_height(const reach_quick_settings_model *model,
                                              const reach_quick_settings_metrics *metrics)
{
    float body = 0.0f;
    if (reach_quick_settings_wifi_message_shown(model))
    {
        body = metrics->section_label_height;
    }
    else
    {
        body = reach_quick_settings_wifi_row_offset(model, model->wifi_row_count, metrics);
        if (model->wifi_row_count == model->wifi_connected_count)
        {
            body -= metrics->section_label_height;
        }
    }
    return metrics->content_padding * 2.0f + metrics->wifi_header_height +
           metrics->overview_row_gap + body;
}

static int32_t reach_quick_settings_bluetooth_message_shown(const reach_quick_settings_model *model)
{
    return !model->bluetooth.available || !model->bluetooth.enabled ||
           model->bluetooth_row_count == 0;
}

static float reach_quick_settings_bluetooth_row_offset(const reach_quick_settings_model *model,
                                                       size_t index,
                                                       const reach_quick_settings_metrics *metrics)
{
    float label = metrics->section_label_height;
    float row = metrics->output_device_row_height;
    size_t paired = model->bluetooth_paired_count;
    float y = paired > 0 ? label : 0.0f;
    if (index < paired)
        y += (float)index * row;
    else
        y += (float)paired * row + label + (float)(index - paired) * row;
    if (model->bluetooth_action_row >= 0 && index > (size_t)model->bluetooth_action_row)
        y += metrics->wifi_key_area_height;
    return y;
}

static float reach_quick_settings_bluetooth_height(const reach_quick_settings_model *model,
                                                   const reach_quick_settings_metrics *metrics)
{
    float body = metrics->section_label_height;
    if (!reach_quick_settings_bluetooth_message_shown(model))
    {
        body =
            reach_quick_settings_bluetooth_row_offset(model, model->bluetooth_row_count, metrics);
        if (model->bluetooth_row_count == model->bluetooth_paired_count)
            body -= metrics->section_label_height;
    }
    return metrics->content_padding * 2.0f + metrics->wifi_header_height +
           metrics->overview_row_gap + body;
}

static float
reach_quick_settings_content_height_for_spans_scaled(const reach_quick_settings_model *model,
                                                     float output_devices_span,
                                                     float app_volumes_span, float dpi_scale)
{
    reach_quick_settings_metrics metrics = reach_quick_settings_metrics_for_scale(dpi_scale);
    float page_height = 0.0f;
    if (model == nullptr || model->page == REACH_QUICK_SETTINGS_PAGE_OVERVIEW)
    {
        page_height = reach_quick_settings_overview_height(model, &metrics);
    }
    else if (model->page == REACH_QUICK_SETTINGS_PAGE_WIFI)
    {
        page_height = reach_quick_settings_wifi_height(model, &metrics);
    }
    else if (model->page == REACH_QUICK_SETTINGS_PAGE_BLUETOOTH)
    {
        page_height = reach_quick_settings_bluetooth_height(model, &metrics);
    }
    else
    {
        float input_height = model->input_available
                                 ? metrics.section_label_height + metrics.overview_pill_height
                                 : 0.0f;
        page_height = metrics.content_padding * 2.0f + metrics.overview_pill_height +
                      metrics.overview_row_gap + output_devices_span + app_volumes_span +
                      input_height;
    }
    return fmaxf(reach_quick_settings_header_height(&metrics) + page_height,
                 reach_quick_settings_rail_height(&metrics));
}

float reach_quick_settings_content_height_for_model_scaled(const reach_quick_settings_model *model,
                                                           float dpi_scale)
{
    float output_span = 0, app_span = 0;
    reach_quick_settings_section_spans(model, dpi_scale, &output_span, &app_span);
    return reach_quick_settings_content_height_for_spans_scaled(model, output_span, app_span,
                                                                dpi_scale);
}
static reach_quick_settings_level_pill_layout
reach_quick_settings_level_pill_at(reach_rect_f32 pill, int32_t labelled,
                                   const reach_quick_settings_metrics *metrics);

reach_quick_settings_output_device_row_layout
reach_quick_settings_output_device_row_layout_at(reach_rect_f32 panel, float top, float dpi_scale)
{
    reach_quick_settings_metrics metrics = reach_quick_settings_metrics_for_scale(dpi_scale);
    reach_quick_settings_output_device_row_layout row = {};
    row.bounds = reach_quick_settings_rect(panel.x, top, panel.width,
                                           metrics.output_device_row_height - 2.0f * dpi_scale);
    row.icon = reach_quick_settings_rect(row.bounds.x + metrics.output_row_horizontal_padding,
                                         row.bounds.y +
                                             (row.bounds.height - metrics.output_icon_size) * 0.5f,
                                         metrics.output_icon_size, metrics.output_icon_size);
    row.checkmark = reach_quick_settings_rect(
        row.bounds.x + row.bounds.width - metrics.output_row_horizontal_padding -
            metrics.output_check_size,
        row.bounds.y + (row.bounds.height - metrics.output_check_size) * 0.5f,
        metrics.output_check_size, metrics.output_check_size);
    float text_x = row.icon.x + row.icon.width + metrics.output_row_label_gap;
    float text_width = row.checkmark.x - metrics.output_row_label_gap - text_x;
    float text_top = row.bounds.y + (row.bounds.height - metrics.output_row_title_height -
                                     metrics.output_row_detail_height) *
                                        0.5f;
    row.title =
        reach_quick_settings_rect(text_x, text_top, text_width, metrics.output_row_title_height);
    row.detail = reach_quick_settings_rect(text_x, text_top + metrics.output_row_title_height,
                                           text_width, metrics.output_row_detail_height);
    return row;
}
reach_quick_settings_level_pill_layout
reach_quick_settings_app_volume_row_layout_at(reach_rect_f32 panel, float top, float dpi_scale)
{
    reach_quick_settings_metrics metrics = reach_quick_settings_metrics_for_scale(dpi_scale);
    return reach_quick_settings_level_pill_at(
        reach_quick_settings_rect(panel.x, top, panel.width, metrics.overview_pill_height), 1,
        &metrics);
}
static void reach_quick_settings_place_header(reach_quick_settings_layout *layout,
                                              const reach_quick_settings_metrics *metrics)
{
    float header_x = layout->rail.x + layout->rail.width;
    layout->header = reach_quick_settings_rect(
        header_x, layout->interior.y, layout->interior.x + layout->interior.width - header_x,
        reach_quick_settings_header_background_height(metrics));
    float ring = metrics->avatar_size + metrics->avatar_ring * 2.0f;
    layout->avatar_ring =
        reach_quick_settings_rect(layout->header.x + metrics->avatar_inset,
                                  layout->header.y + metrics->avatar_inset, ring, ring);
    layout->avatar = reach_quick_settings_rect(layout->avatar_ring.x + metrics->avatar_ring,
                                               layout->avatar_ring.y + metrics->avatar_ring,
                                               metrics->avatar_size, metrics->avatar_size);
    float button = metrics->header_button_size;
    float header_right = layout->header.x + layout->header.width - metrics->avatar_inset;
    layout->settings_button = reach_quick_settings_rect(
        header_right - button, layout->avatar_ring.y + (ring - button) * 0.5f, button, button);
    float text_x = layout->avatar_ring.x + ring + metrics->identity_gap;
    float text_width =
        reach_quick_settings_clamp_min0(layout->settings_button.x - metrics->identity_gap - text_x);
    float text_top =
        layout->avatar_ring.y +
        (ring - metrics->identity_name_height - metrics->identity_detail_height) * 0.5f;
    layout->identity_name =
        reach_quick_settings_rect(text_x, text_top, text_width, metrics->identity_name_height);
    layout->identity_detail =
        reach_quick_settings_rect(text_x, text_top + metrics->identity_name_height, text_width,
                                  metrics->identity_detail_height);
}

static void reach_quick_settings_place_rail(reach_quick_settings_layout *layout,
                                            const reach_quick_settings_metrics *metrics)
{
    layout->rail = reach_quick_settings_rect(layout->interior.x, layout->interior.y,
                                             reach_quick_settings_rail_full_width(metrics),
                                             layout->interior.height);
    float inset = reach_quick_settings_rail_item_inset(metrics);
    for (size_t index = 0; index < REACH_QUICK_SETTINGS_PAGE_COUNT; ++index)
    {
        layout->rail_items[index] = reach_quick_settings_rect(
            layout->rail.x + inset,
            layout->rail.y + inset +
                (float)index * (metrics->rail_item_size + metrics->rail_item_gap),
            metrics->rail_item_size, metrics->rail_item_size);
    }
}

static reach_quick_settings_level_pill_layout
reach_quick_settings_level_pill_at(reach_rect_f32 pill, int32_t labelled,
                                   const reach_quick_settings_metrics *metrics)
{
    reach_quick_settings_level_pill_layout layout = {};
    layout.bounds = pill;
    layout.icon = reach_quick_settings_rect(
        pill.x + metrics->overview_pill_icon_inset,
        pill.y + (pill.height - metrics->overview_pill_icon_size) * 0.5f,
        metrics->overview_pill_icon_size, metrics->overview_pill_icon_size);
    layout.value =
        reach_quick_settings_rect(pill.x + pill.width - metrics->overview_pill_value_inset -
                                      metrics->overview_pill_value_width,
                                  pill.y, metrics->overview_pill_value_width, pill.height);
    if (labelled)
    {
        float label_x = layout.icon.x + layout.icon.width + metrics->pill_label_gap;
        layout.label = reach_quick_settings_rect(
            label_x, pill.y, layout.value.x - metrics->pill_label_gap - label_x, pill.height);
    }
    return layout;
}
static void reach_quick_settings_place_toggle(reach_quick_settings_toggle_layout *toggle,
                                              reach_rect_f32 bounds,
                                              const reach_quick_settings_metrics *metrics)
{
    toggle->bounds = bounds;
    toggle->icon =
        reach_quick_settings_rect(bounds.x + metrics->toggle_leading_padding,
                                  bounds.y + (bounds.height - metrics->toggle_icon_size) * 0.5f,
                                  metrics->toggle_icon_size, metrics->toggle_icon_size);
    float divider_x =
        roundf(toggle->icon.x + toggle->icon.width + metrics->toggle_divider_leading_gap);
    float divider_width = 1.0f;
    toggle->divider = reach_quick_settings_rect(
        divider_x, bounds.y + (bounds.height - metrics->toggle_divider_height) * 0.5f,
        divider_width, metrics->toggle_divider_height);
    float text_x = divider_x + divider_width + metrics->toggle_divider_trailing_gap;
    float text_width = bounds.x + bounds.width - metrics->toggle_trailing_padding - text_x;
    float text_top =
        bounds.y +
        (bounds.height - metrics->toggle_title_height - metrics->toggle_status_height) * 0.5f;
    toggle->title =
        reach_quick_settings_rect(text_x, text_top, text_width, metrics->toggle_title_height);
    toggle->status = reach_quick_settings_rect(text_x, text_top + metrics->toggle_title_height,
                                               text_width, metrics->toggle_status_height);
}

static void reach_quick_settings_place_overview(reach_quick_settings_layout *layout,
                                                reach_rect_f32 page_bounds,
                                                const reach_quick_settings_model *model,
                                                const reach_quick_settings_metrics *metrics)
{
    float pill = metrics->overview_pill_height;
    reach_rect_f32 row = reach_quick_settings_content_line(
        page_bounds, page_bounds.y + metrics->content_padding, pill, metrics);
    layout->sound_page_button =
        reach_quick_settings_rect(row.x + row.width - pill, row.y, pill, pill);
    layout->overview_volume = reach_quick_settings_level_pill_at(
        reach_quick_settings_rect(row.x, row.y, row.width - pill - metrics->toggle_gap, pill), 0,
        metrics);
    float next_y = row.y + pill;
    if (model != nullptr && model->brightness.available)
    {
        layout->overview_brightness = reach_quick_settings_level_pill_at(
            reach_quick_settings_content_line(page_bounds, next_y + metrics->overview_row_gap, pill,
                                              metrics),
            0, metrics);
        next_y = layout->overview_brightness.bounds.y + pill;
    }
    reach_rect_f32 grid = reach_quick_settings_content_line(
        page_bounds, next_y + metrics->toggle_grid_gap, metrics->toggle_height, metrics);
    float toggle_width = reach_quick_settings_clamp_min0((grid.width - metrics->toggle_gap) * 0.5f);
    for (size_t index = 0; index < REACH_QUICK_SETTINGS_TOGGLE_COUNT; ++index)
    {
        float column = (float)(index % 2);
        float line = (float)(index / 2);
        reach_quick_settings_place_toggle(
            &layout->toggles[index],
            reach_quick_settings_rect(grid.x + column * (toggle_width + metrics->toggle_gap),
                                      grid.y +
                                          line * (metrics->toggle_height + metrics->toggle_gap),
                                      toggle_width, metrics->toggle_height),
            metrics);
    }
}

static void reach_quick_settings_place_sound(reach_quick_settings_layout *layout,
                                             reach_rect_f32 page_bounds,
                                             const reach_quick_settings_model *model,
                                             float output_span, float app_span,
                                             const reach_quick_settings_metrics *metrics,
                                             float dpi_scale)
{
    float pill = metrics->overview_pill_height;
    layout->sound_volume = reach_quick_settings_level_pill_at(
        reach_quick_settings_content_line(page_bounds, page_bounds.y + metrics->content_padding,
                                          pill, metrics),
        0, metrics);
    float section = layout->sound_volume.bounds.y + pill + metrics->overview_row_gap;

    size_t outputs = reach_quick_settings_visible_output_device_count(model);
    layout->output_label = reach_quick_settings_content_line(
        page_bounds, section, metrics->section_label_height, metrics);
    layout->output_devices_panel = reach_quick_settings_content_line(
        page_bounds, section + metrics->section_label_height,
        (float)outputs * metrics->output_device_row_height, metrics);
    layout->output_devices_clip = layout->output_devices_panel;
    layout->output_devices_clip.height =
        reach_quick_settings_clamp_min0(output_span - metrics->section_label_height);
    layout->output_device_row_count = outputs;
    for (size_t index = 0; index < outputs; ++index)
    {
        layout->output_device_rows[index] = reach_quick_settings_output_device_row_layout_at(
            layout->output_devices_panel,
            layout->output_devices_panel.y + (float)index * metrics->output_device_row_height,
            dpi_scale);
    }

    section += output_span;
    size_t sessions = reach_quick_settings_visible_session_count(model);
    layout->app_label = reach_quick_settings_content_line(page_bounds, section,
                                                          metrics->section_label_height, metrics);
    layout->app_volumes_panel = reach_quick_settings_content_line(
        page_bounds, section + metrics->section_label_height,
        (float)sessions * metrics->app_volume_row_height, metrics);
    layout->app_volumes_clip = layout->app_volumes_panel;
    layout->app_volumes_clip.height =
        reach_quick_settings_clamp_min0(app_span - metrics->section_label_height);
    layout->app_volume_row_count = sessions;
    for (size_t index = 0; index < sessions; ++index)
    {
        layout->app_volume_rows[index] = reach_quick_settings_app_volume_row_layout_at(
            layout->app_volumes_panel,
            layout->app_volumes_panel.y + (float)index * metrics->app_volume_row_height, dpi_scale);
    }

    section += app_span;
    if (model != nullptr && model->input_available)
    {
        layout->input_label = reach_quick_settings_content_line(
            page_bounds, section, metrics->section_label_height, metrics);
        layout->input_volume = reach_quick_settings_level_pill_at(
            reach_quick_settings_content_line(page_bounds, section + metrics->section_label_height,
                                              pill, metrics),
            0, metrics);
    }
}

void reach_quick_settings_wifi_key_rects(reach_rect_f32 row,
                                         const reach_quick_settings_metrics *metrics,
                                         reach_rect_f32 *out_field, reach_rect_f32 *out_connect)
{
    float top = row.y + row.height + metrics->wifi_key_gap;
    *out_connect =
        reach_quick_settings_rect(row.x + row.width - metrics->wifi_connect_width, top,
                                  metrics->wifi_connect_width, metrics->overview_pill_height);
    *out_field = reach_quick_settings_rect(row.x, top, out_connect->x - metrics->toggle_gap - row.x,
                                           metrics->overview_pill_height);
}

reach_rect_f32
reach_quick_settings_wifi_place_forget(reach_quick_settings_output_device_row_layout *row,
                                       const reach_wifi_network *network,
                                       const reach_quick_settings_metrics *metrics)
{
    if (!network->saved && !network->connected)
        return {};
    float inset = metrics->wifi_forget_inset;
    float height = fmaxf(0.0f, row->bounds.height - inset * 2.0f);
    reach_rect_f32 button = reach_quick_settings_rect(
        row->bounds.x + row->bounds.width - inset - metrics->wifi_forget_width,
        row->bounds.y + inset, metrics->wifi_forget_width, height);
    row->checkmark.x = button.x - metrics->output_row_label_gap - row->checkmark.width;
    float text_width = fmaxf(0.0f, row->checkmark.x - metrics->output_row_label_gap - row->title.x);
    row->title.width = text_width;
    row->detail.width = text_width;
    return button;
}

void reach_quick_settings_bluetooth_action_rects(reach_rect_f32 row, int32_t pin,
                                                 const reach_quick_settings_metrics *metrics,
                                                 reach_rect_f32 *out_label,
                                                 reach_rect_f32 out_buttons[2])
{
    float top = row.y + row.height + metrics->wifi_key_gap;
    float width = pin ? metrics->wifi_connect_width * 0.75f : metrics->wifi_connect_width;
    float right = row.x + row.width;
    out_buttons[0] =
        reach_quick_settings_rect(right - width, top, width, metrics->overview_pill_height);
    out_buttons[1] = {};
    if (pin)
    {
        out_buttons[1] = out_buttons[0];
        out_buttons[0].x -= width + metrics->toggle_gap;
    }
    float label_x = row.x + metrics->overview_pill_icon_inset;
    *out_label =
        reach_quick_settings_rect(label_x, top, out_buttons[0].x - metrics->toggle_gap - label_x,
                                  metrics->overview_pill_height);
}

static void reach_quick_settings_place_header_switch(reach_rect_f32 page_bounds,
                                                     const reach_quick_settings_metrics *metrics,
                                                     reach_rect_f32 *out_title,
                                                     reach_rect_f32 *out_switch)
{
    reach_rect_f32 header =
        reach_quick_settings_content_line(page_bounds, page_bounds.y + metrics->content_padding,
                                          metrics->wifi_header_height, metrics);
    *out_switch = reach_quick_settings_rect(
        header.x + header.width - metrics->overview_pill_icon_inset - metrics->wifi_switch_width,
        header.y + (header.height - metrics->wifi_switch_height) * 0.5f, metrics->wifi_switch_width,
        metrics->wifi_switch_height);
    *out_title = reach_quick_settings_rect(
        header.x + metrics->overview_pill_icon_inset, header.y,
        out_switch->x - header.x - metrics->overview_pill_icon_inset * 2.0f, header.height);
}

static void reach_quick_settings_place_bluetooth(reach_quick_settings_layout *layout,
                                                 reach_rect_f32 page_bounds,
                                                 const reach_quick_settings_model *model,
                                                 const reach_quick_settings_metrics *metrics,
                                                 float dpi_scale)
{
    reach_quick_settings_place_header_switch(page_bounds, metrics, &layout->bluetooth_title,
                                             &layout->bluetooth_switch);
    float base =
        layout->bluetooth_title.y + layout->bluetooth_title.height + metrics->overview_row_gap;
    int32_t message = reach_quick_settings_bluetooth_message_shown(model);
    layout->bluetooth_message =
        message ? reach_quick_settings_content_line(page_bounds, base,
                                                    metrics->section_label_height, metrics)
                : reach_rect_f32{};
    layout->bluetooth_mine_label =
        !message && model->bluetooth_paired_count > 0
            ? reach_quick_settings_content_line(page_bounds, base, metrics->section_label_height,
                                                metrics)
            : reach_rect_f32{};
    size_t paired = model->bluetooth_paired_count;
    layout->bluetooth_nearby_label =
        !message && model->bluetooth_row_count > paired
            ? reach_quick_settings_content_line(
                  page_bounds,
                  base + reach_quick_settings_bluetooth_row_offset(model, paired, metrics) -
                      metrics->section_label_height,
                  metrics->section_label_height, metrics)
            : reach_rect_f32{};
    layout->bluetooth_row_count = message ? 0 : model->bluetooth_row_count;
    reach_rect_f32 panel = reach_quick_settings_content_line(page_bounds, base, 0.0f, metrics);
    for (size_t index = 0; index < layout->bluetooth_row_count; ++index)
    {
        layout->bluetooth_rows[index] = reach_quick_settings_output_device_row_layout_at(
            panel, base + reach_quick_settings_bluetooth_row_offset(model, index, metrics),
            dpi_scale);
    }
    layout->bluetooth_action_label = {};
    layout->bluetooth_action_buttons[0] = {};
    layout->bluetooth_action_buttons[1] = {};
    if (model->bluetooth_action_row >= 0 &&
        (size_t)model->bluetooth_action_row < layout->bluetooth_row_count)
    {
        reach_quick_settings_bluetooth_action_rects(
            layout->bluetooth_rows[model->bluetooth_action_row].bounds, model->bluetooth_pin_active,
            metrics, &layout->bluetooth_action_label, layout->bluetooth_action_buttons);
    }
}

static void reach_quick_settings_place_wifi(reach_quick_settings_layout *layout,
                                            reach_rect_f32 page_bounds,
                                            const reach_quick_settings_model *model,
                                            const reach_quick_settings_metrics *metrics,
                                            float dpi_scale)
{
    reach_rect_f32 header =
        reach_quick_settings_content_line(page_bounds, page_bounds.y + metrics->content_padding,
                                          metrics->wifi_header_height, metrics);
    layout->wifi_switch = reach_quick_settings_rect(
        header.x + header.width - metrics->overview_pill_icon_inset - metrics->wifi_switch_width,
        header.y + (header.height - metrics->wifi_switch_height) * 0.5f, metrics->wifi_switch_width,
        metrics->wifi_switch_height);
    layout->wifi_title = reach_quick_settings_rect(
        header.x + metrics->overview_pill_icon_inset, header.y,
        layout->wifi_switch.x - header.x - metrics->overview_pill_icon_inset * 2.0f, header.height);
    float base = header.y + header.height + metrics->overview_row_gap;
    layout->wifi_message = reach_quick_settings_wifi_message_shown(model)
                               ? reach_quick_settings_content_line(
                                     page_bounds, base, metrics->section_label_height, metrics)
                               : reach_rect_f32{};
    layout->wifi_nearby_label =
        model->wifi_row_count > model->wifi_connected_count &&
                !reach_quick_settings_wifi_message_shown(model)
            ? reach_quick_settings_content_line(page_bounds,
                                                base + (float)model->wifi_connected_count *
                                                           metrics->output_device_row_height,
                                                metrics->section_label_height, metrics)
            : reach_rect_f32{};
    layout->wifi_row_count =
        reach_quick_settings_wifi_message_shown(model) ? 0 : model->wifi_row_count;
    reach_rect_f32 panel = reach_quick_settings_content_line(page_bounds, base, 0.0f, metrics);
    for (size_t index = 0; index < layout->wifi_row_count; ++index)
    {
        layout->wifi_rows[index] = reach_quick_settings_output_device_row_layout_at(
            panel, base + reach_quick_settings_wifi_row_offset(model, index, metrics), dpi_scale);
        layout->wifi_forget_buttons[index] = reach_quick_settings_wifi_place_forget(
            &layout->wifi_rows[index], &model->wifi_rows[index], metrics);
    }
    layout->wifi_key_field = {};
    layout->wifi_key_connect = {};
    if (model->wifi_key_row >= 0 && (size_t)model->wifi_key_row < layout->wifi_row_count)
    {
        reach_quick_settings_wifi_key_rects(layout->wifi_rows[model->wifi_key_row].bounds, metrics,
                                            &layout->wifi_key_field, &layout->wifi_key_connect);
    }
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
    layout.interior = reach_quick_settings_rect(
        content_bounds.x - metrics.surface_padding, content_bounds.y - metrics.surface_padding,
        content_bounds.width + metrics.surface_padding * 2.0f,
        content_bounds.height + metrics.surface_padding * 2.0f);

    reach_quick_settings_place_rail(&layout, &metrics);
    reach_quick_settings_place_header(&layout, &metrics);
    reach_rect_f32 body_bounds = content_bounds;
    float header_height = reach_quick_settings_header_height(&metrics);
    body_bounds.y += header_height;
    body_bounds.height = reach_quick_settings_clamp_min0(body_bounds.height - header_height);
    reach_rect_f32 page_bounds = body_bounds;
    page_bounds.x += metrics.rail_width;
    page_bounds.width = reach_quick_settings_clamp_min0(page_bounds.width - metrics.rail_width);
    layout.page_bounds = page_bounds;
    reach_quick_settings_place_overview(&layout, page_bounds, model, &metrics);

    reach_quick_settings_place_sound(&layout, page_bounds, model, output_devices_span,
                                     app_volumes_span, &metrics, dpi_scale);
    if (model != nullptr)
    {
        reach_quick_settings_place_wifi(&layout, page_bounds, model, &metrics, dpi_scale);
        reach_quick_settings_place_bluetooth(&layout, page_bounds, model, &metrics, dpi_scale);
    }
    return layout;
}

reach_quick_settings_layout reach_quick_settings_layout_for_content_bounds_scaled(
    reach_rect_f32 content_bounds, const reach_theme *theme,
    const reach_quick_settings_model *model, float dpi_scale)
{
    float output_span = 0, app_span = 0;
    reach_quick_settings_section_spans(model, dpi_scale, &output_span, &app_span);
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
    reach_motion toggle_states[REACH_QUICK_SETTINGS_TOGGLE_COUNT];
    reach_motion page_opacity[REACH_QUICK_SETTINGS_PAGE_COUNT];
    reach_motion page_shift[REACH_QUICK_SETTINGS_PAGE_COUNT];
    reach_motion rail_marker;
    reach_motion wifi_switch;
    reach_keyed_row wifi_row_motions[REACH_QUICK_SETTINGS_WIFI_ROWS];
    reach_wifi_network wifi_row_items[REACH_QUICK_SETTINGS_WIFI_ROWS];
    uint16_t wifi_key_ssid[REACH_WIFI_SSID_CAPACITY];
    reach_blink wifi_caret;
    reach_bluetooth_service *bluetooth_service;
    reach_bluetooth_session bluetooth_session;
    uint16_t bluetooth_selected_id[REACH_BLUETOOTH_DEVICE_ID_CAPACITY];
    reach_motion bluetooth_switch;
    reach_keyed_row bluetooth_row_motions[REACH_QUICK_SETTINGS_BLUETOOTH_ROWS];
    reach_bluetooth_device bluetooth_row_items[REACH_QUICK_SETTINGS_BLUETOOTH_ROWS];
    reach_keyed_row output_device_rows[REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES];
    reach_quick_settings_output_device_row
        output_device_items[REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES];
    reach_keyed_row app_volume_rows[REACH_AUDIO_VOLUME_MAX_SESSIONS];
    reach_quick_settings_app_volume_row app_volume_items[REACH_AUDIO_VOLUME_MAX_SESSIONS];
    reach_quick_settings_action press_action;
    reach_quick_settings_state state;

    reach_system_status *status;
    reach_wifi_service *wifi;
    reach_wifi_session wifi_session;
    reach_profile_service *profile;
    const reach_power_session_port *power_session;
    double uptime_deadline;
    reach_quick_settings_routes routes;

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
        reach_quick_settings_metrics_for_scale(dpi_scale).output_device_row_height, animate,
        reach_motion_now_seconds());
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
        reach_quick_settings_metrics_for_scale(dpi_scale).app_volume_row_height, animate,
        reach_motion_now_seconds());
    if (reach_keyed_rows_active(quick_settings->app_volume_rows, REACH_AUDIO_VOLUME_MAX_SESSIONS))
        quick_settings->section_cleanup_pending = 1;
}

static int32_t reach_quick_settings_wifi_row_same(const void *context, size_t slot,
                                                  size_t item_index)
{
    const reach_quick_settings *quick_settings = static_cast<const reach_quick_settings *>(context);
    return reach_utf16_equal(quick_settings->wifi_row_items[slot].ssid,
                             quick_settings->state.model.wifi_rows[item_index].ssid);
}

static void reach_quick_settings_sync_wifi_rows(reach_quick_settings *quick_settings,
                                                int32_t animate)
{
    reach_quick_settings_model &model = quick_settings->state.model;
    reach_quick_settings_metrics metrics =
        reach_quick_settings_metrics_for_scale(quick_settings->layout_dpi);
    size_t count = reach_quick_settings_wifi_message_shown(&model) ? 0 : model.wifi_row_count;
    float targets[REACH_QUICK_SETTINGS_WIFI_ROWS] = {};
    for (size_t index = 0; index < count; ++index)
        targets[index] = reach_quick_settings_wifi_row_offset(&model, index, &metrics);
    reach_keyed_rows_sync_input input = {};
    input.count = count;
    input.targets = targets;
    input.same = reach_quick_settings_wifi_row_same;
    input.context = quick_settings;
    input.animate = animate;
    input.theme = reach_quick_settings_motion_theme(quick_settings);
    input.now_seconds = reach_motion_now_seconds();
    reach_keyed_row_change changes[REACH_KEYED_ROWS_MAX_CHANGES(REACH_QUICK_SETTINGS_WIFI_ROWS)];
    (void)reach_keyed_rows_sync(quick_settings->wifi_row_motions, REACH_QUICK_SETTINGS_WIFI_ROWS,
                                &input, changes,
                                REACH_KEYED_ROWS_MAX_CHANGES(REACH_QUICK_SETTINGS_WIFI_ROWS));
    for (size_t slot = 0; slot < REACH_QUICK_SETTINGS_WIFI_ROWS; ++slot)
    {
        const reach_keyed_row &row = quick_settings->wifi_row_motions[slot];
        if (row.lifecycle == REACH_KEYED_ROW_LIVE && row.index < count)
            quick_settings->wifi_row_items[slot] = model.wifi_rows[row.index];
    }
    if (reach_keyed_rows_active(quick_settings->wifi_row_motions, REACH_QUICK_SETTINGS_WIFI_ROWS))
        quick_settings->section_cleanup_pending = 1;
}

static int32_t reach_quick_settings_bluetooth_row_same(const void *context, size_t slot,
                                                       size_t item_index)
{
    const reach_quick_settings *quick_settings = static_cast<const reach_quick_settings *>(context);
    return reach_bluetooth_device_id_equal(
        quick_settings->bluetooth_row_items[slot].id,
        quick_settings->state.model.bluetooth_rows[item_index].id);
}

static void reach_quick_settings_sync_bluetooth_rows(reach_quick_settings *quick_settings,
                                                     int32_t animate)
{
    reach_quick_settings_model &model = quick_settings->state.model;
    reach_quick_settings_metrics metrics =
        reach_quick_settings_metrics_for_scale(quick_settings->layout_dpi);
    size_t count =
        reach_quick_settings_bluetooth_message_shown(&model) ? 0 : model.bluetooth_row_count;
    float targets[REACH_QUICK_SETTINGS_BLUETOOTH_ROWS] = {};
    for (size_t index = 0; index < count; ++index)
        targets[index] = reach_quick_settings_bluetooth_row_offset(&model, index, &metrics);
    reach_keyed_rows_sync_input input = {};
    input.count = count;
    input.targets = targets;
    input.same = reach_quick_settings_bluetooth_row_same;
    input.context = quick_settings;
    input.animate = animate;
    input.theme = reach_quick_settings_motion_theme(quick_settings);
    input.now_seconds = reach_motion_now_seconds();
    reach_keyed_row_change
        changes[REACH_KEYED_ROWS_MAX_CHANGES(REACH_QUICK_SETTINGS_BLUETOOTH_ROWS)];
    (void)reach_keyed_rows_sync(quick_settings->bluetooth_row_motions,
                                REACH_QUICK_SETTINGS_BLUETOOTH_ROWS, &input, changes,
                                REACH_KEYED_ROWS_MAX_CHANGES(REACH_QUICK_SETTINGS_BLUETOOTH_ROWS));
    for (size_t slot = 0; slot < REACH_QUICK_SETTINGS_BLUETOOTH_ROWS; ++slot)
    {
        const reach_keyed_row &row = quick_settings->bluetooth_row_motions[slot];
        if (row.lifecycle == REACH_KEYED_ROW_LIVE && row.index < count)
            quick_settings->bluetooth_row_items[slot] = model.bluetooth_rows[row.index];
    }
    if (reach_keyed_rows_active(quick_settings->bluetooth_row_motions,
                                REACH_QUICK_SETTINGS_BLUETOOTH_ROWS))
        quick_settings->section_cleanup_pending = 1;
}

static void reach_quick_settings_build_bluetooth_rows(reach_quick_settings *quick_settings)
{
    reach_quick_settings_model &model = quick_settings->state.model;
    const reach_bluetooth_session &session = quick_settings->bluetooth_session;
    model.bluetooth_row_count = 0;
    for (int32_t pass = 0; pass < 3; ++pass)
    {
        for (size_t index = 0; index < session.devices.count &&
                               model.bluetooth_row_count < REACH_QUICK_SETTINGS_BLUETOOTH_ROWS;
             ++index)
        {
            const reach_bluetooth_device &device = session.devices.devices[index];
            int32_t wanted = pass == 0   ? device.paired && device.connected
                             : pass == 1 ? device.paired && !device.connected
                                         : !device.paired && device.can_pair && device.name[0] != 0;
            if (wanted)
                model.bluetooth_rows[model.bluetooth_row_count++] = device;
        }
        if (pass == 1)
            model.bluetooth_paired_count = model.bluetooth_row_count;
    }
    model.bluetooth_status = session.status;
    reach_copy_utf16(model.bluetooth_status_device, REACH_BLUETOOTH_DEVICE_ID_CAPACITY,
                     session.status_device);
    model.bluetooth_scanning = session.scanning;
    model.bluetooth_pin_active = 0;
    model.bluetooth_pin[0] = 0;
    model.bluetooth_action_row = -1;
    for (size_t index = 0; index < model.bluetooth_row_count; ++index)
    {
        const reach_bluetooth_device &device = model.bluetooth_rows[index];
        if (session.pairing.active &&
            reach_bluetooth_device_id_equal(device.id, session.pairing.device_id))
        {
            model.bluetooth_action_row = (int32_t)index;
            model.bluetooth_pin_active = 1;
            reach_copy_utf16(model.bluetooth_pin, REACH_BLUETOOTH_PIN_CAPACITY,
                             session.pairing.pin);
            break;
        }
        if (device.paired && quick_settings->bluetooth_selected_id[0] != 0 &&
            reach_bluetooth_device_id_equal(device.id, quick_settings->bluetooth_selected_id))
            model.bluetooth_action_row = (int32_t)index;
    }
    if (model.bluetooth_action_row < 0)
        quick_settings->bluetooth_selected_id[0] = 0;
    reach_quick_settings_sync_bluetooth_rows(quick_settings,
                                             reach_quick_settings_rows_animate(quick_settings));
}

static void reach_quick_settings_set_bluetooth_scan(reach_quick_settings *quick_settings,
                                                    int32_t enabled)
{
    if (quick_settings->bluetooth_service == nullptr)
        return;
    reach_bluetooth_session &session = quick_settings->bluetooth_session;
    if (enabled && (!session.radio.available || !session.radio.enabled))
        return;
    if ((session.scanning ? 1 : 0) == (enabled ? 1 : 0) &&
        (enabled || session.status != REACH_BLUETOOTH_SESSION_STATUS_SCANNING))
        return;
    reach_bluetooth_service_set_scan_enabled(quick_settings->bluetooth_service, enabled);
    session.scanning = enabled ? 1 : 0;
    quick_settings->state.model.bluetooth_scanning = session.scanning;
}

static void reach_quick_settings_build_wifi_rows(reach_quick_settings *quick_settings)
{
    reach_quick_settings_model &model = quick_settings->state.model;
    const reach_wifi_session &session = quick_settings->wifi_session;
    model.wifi_row_count = 0;
    model.wifi_connected_count = 0;
    for (size_t index = 0; index < session.networks.count; ++index)
    {
        const reach_wifi_network &network = session.networks.networks[index];
        if (network.connected && network.in_range && model.wifi_connected_count == 0)
        {
            model.wifi_rows[model.wifi_row_count++] = network;
            model.wifi_connected_count = 1;
        }
    }
    int32_t taken[REACH_WIFI_MAX_NETWORKS] = {};
    while (model.wifi_row_count < REACH_QUICK_SETTINGS_WIFI_ROWS)
    {
        size_t best = session.networks.count;
        for (size_t index = 0; index < session.networks.count; ++index)
        {
            const reach_wifi_network &network = session.networks.networks[index];
            if (taken[index] || network.connected || !network.in_range || network.ssid[0] == 0)
                continue;
            if (best == session.networks.count ||
                network.signal_strength > session.networks.networks[best].signal_strength)
                best = index;
        }
        if (best == session.networks.count)
            break;
        taken[best] = 1;
        model.wifi_rows[model.wifi_row_count++] = session.networks.networks[best];
    }
    model.wifi_key_row = -1;
    for (size_t index = 0; quick_settings->wifi_key_ssid[0] != 0 && index < model.wifi_row_count;
         ++index)
    {
        if (reach_utf16_equal(model.wifi_rows[index].ssid, quick_settings->wifi_key_ssid))
            model.wifi_key_row = (int32_t)index;
    }
    if (model.wifi_key_row < 0)
        quick_settings->wifi_key_ssid[0] = 0;
    model.wifi_radio = session.radio;
    model.wifi_status = session.status;
    reach_copy_utf16(model.wifi_status_ssid, REACH_WIFI_SSID_CAPACITY, session.status_ssid);
    reach_quick_settings_sync_wifi_rows(quick_settings,
                                        reach_quick_settings_rows_animate(quick_settings));
}

static int32_t reach_quick_settings_sections_active(const reach_quick_settings *quick_settings)
{
    const reach_quick_settings_state &state = quick_settings->state;
    return reach_keyed_rows_active(quick_settings->bluetooth_row_motions,
                                   REACH_QUICK_SETTINGS_BLUETOOTH_ROWS) ||
           reach_keyed_rows_active(quick_settings->wifi_row_motions,
                                   REACH_QUICK_SETTINGS_WIFI_ROWS) ||
           reach_motion_active(&state.output_devices_span) ||
           reach_motion_active(&state.app_volumes_span) ||
           reach_keyed_rows_active(quick_settings->output_device_rows,
                                   REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES) ||
           reach_keyed_rows_active(quick_settings->app_volume_rows,
                                   REACH_AUDIO_VOLUME_MAX_SESSIONS);
}
static void reach_quick_settings_snap_sections(reach_quick_settings *quick_settings,
                                               float dpi_scale)
{
    reach_quick_settings_state *state = &quick_settings->state;
    float output_span = 0, app_span = 0;
    reach_quick_settings_section_spans(&state->model, dpi_scale, &output_span, &app_span);
    reach_motion_set(&state->output_devices_span, output_span);
    reach_motion_set(&state->app_volumes_span, app_span);
    reach_quick_settings_sync_output_rows(quick_settings, dpi_scale, 0);
    reach_quick_settings_sync_app_rows(quick_settings, dpi_scale, 0);
}
static int32_t reach_quick_settings_tiles_animate(const reach_quick_settings *quick_settings);
static int32_t reach_quick_settings_refresh_uptime(reach_quick_settings *quick_settings);

static void reach_quick_settings_sync_tile_states(reach_quick_settings *quick_settings,
                                                  int32_t animate)
{
    const reach_quick_settings_model &model = quick_settings->state.model;
    int32_t bluetooth =
        model.bluetooth_pending ? model.bluetooth_pending_enabled : model.bluetooth.enabled;
    int32_t active[REACH_QUICK_SETTINGS_TOGGLE_COUNT] = {};
    active[REACH_QUICK_SETTINGS_TOGGLE_WIFI] = model.wifi_radio == REACH_WIFI_RADIO_ON ? 1 : 0;
    active[REACH_QUICK_SETTINGS_TOGGLE_BLUETOOTH] = bluetooth ? 1 : 0;
    float bluetooth_switch = active[REACH_QUICK_SETTINGS_TOGGLE_BLUETOOTH] ? 1.0f : 0.0f;
    if (!animate)
        reach_motion_set(&quick_settings->bluetooth_switch, bluetooth_switch);
    else if (quick_settings->bluetooth_switch.generation == 0 ||
             quick_settings->bluetooth_switch.to != bluetooth_switch)
        reach_motion_animate_spec_to_at(
            &quick_settings->bluetooth_switch, bluetooth_switch,
            reach_theme_motion_preset(reach_quick_settings_motion_theme(quick_settings),
                                      REACH_MOTION_PRESET_BOUNCY),
            reach_motion_now_seconds());
    float wifi_switch = active[REACH_QUICK_SETTINGS_TOGGLE_WIFI] ? 1.0f : 0.0f;
    if (!animate)
        reach_motion_set(&quick_settings->wifi_switch, wifi_switch);
    else if (quick_settings->wifi_switch.generation == 0 ||
             quick_settings->wifi_switch.to != wifi_switch)
        reach_motion_animate_spec_to_at(
            &quick_settings->wifi_switch, wifi_switch,
            reach_theme_motion_preset(reach_quick_settings_motion_theme(quick_settings),
                                      REACH_MOTION_PRESET_BOUNCY),
            reach_motion_now_seconds());
    active[REACH_QUICK_SETTINGS_TOGGLE_DARK_MODE] = model.light_theme ? 0 : 1;
    double now_seconds = reach_motion_now_seconds();
    for (size_t index = 0; index < REACH_QUICK_SETTINGS_TOGGLE_COUNT; ++index)
    {
        if (animate)
            reach_quick_settings_fade_to(quick_settings, &quick_settings->toggle_states[index],
                                         active[index], now_seconds);
        else
            reach_motion_set(&quick_settings->toggle_states[index], (float)active[index]);
    }
}

static float reach_quick_settings_rail_marker_offset(reach_quick_settings_page page, float dpi)
{
    reach_quick_settings_metrics metrics = reach_quick_settings_metrics_for_scale(dpi);
    return (float)page * (metrics.rail_item_size + metrics.rail_item_gap);
}

static void reach_quick_settings_snap_pages(reach_quick_settings *quick_settings)
{
    reach_quick_settings_page page = quick_settings->state.model.page;
    for (size_t index = 0; index < REACH_QUICK_SETTINGS_PAGE_COUNT; ++index)
    {
        reach_motion_set(&quick_settings->page_opacity[index], index == (size_t)page ? 1.0f : 0.0f);
        reach_motion_set(&quick_settings->page_shift[index], 0.0f);
    }
    reach_motion_set(&quick_settings->rail_marker,
                     reach_quick_settings_rail_marker_offset(page, quick_settings->layout_dpi));
}

static void reach_quick_settings_scan_wifi(reach_quick_settings *quick_settings)
{
    if (quick_settings->wifi == nullptr ||
        quick_settings->wifi_session.radio != REACH_WIFI_RADIO_ON ||
        reach_wifi_session_busy(&quick_settings->wifi_session))
    {
        return;
    }
    reach_wifi_session_set_status(&quick_settings->wifi_session, REACH_WIFI_SESSION_STATUS_SCANNING,
                                  nullptr);
    quick_settings->state.model.wifi_status = REACH_WIFI_SESSION_STATUS_SCANNING;
    reach_wifi_service_scan(quick_settings->wifi);
}

int32_t reach_quick_settings_select_page(reach_quick_settings *quick_settings,
                                         reach_quick_settings_page page)
{
    if (quick_settings == nullptr || page < 0 || page >= REACH_QUICK_SETTINGS_PAGE_COUNT ||
        quick_settings->state.model.page == page)
    {
        return 0;
    }
    reach_quick_settings_page previous = quick_settings->state.model.page;
    quick_settings->state.model.page = page;
    if (!reach_quick_settings_tiles_animate(quick_settings))
    {
        reach_quick_settings_snap_pages(quick_settings);
        reach_quick_settings_set_bluetooth_scan(quick_settings,
                                                page == REACH_QUICK_SETTINGS_PAGE_BLUETOOTH);
        return 1;
    }
    const reach_theme *theme = reach_quick_settings_motion_theme(quick_settings);
    float shift = reach_quick_settings_metrics_for_scale(quick_settings->layout_dpi).page_shift;
    float direction = page > previous ? 1.0f : -1.0f;
    double now = reach_motion_now_seconds();
    reach_quick_settings_fade_to(quick_settings, &quick_settings->page_opacity[previous], 0, now);
    reach_quick_settings_fade_to(quick_settings, &quick_settings->page_opacity[page], 1, now);
    reach_motion_spec smooth = reach_theme_motion_preset(theme, REACH_MOTION_PRESET_SMOOTH);
    reach_motion_animate_spec_to_at(&quick_settings->page_shift[previous], -shift * direction,
                                    smooth, now);
    reach_motion_start_spec_at(&quick_settings->page_shift[page], shift * direction, 0.0f, 0.0f,
                               smooth, now);
    reach_motion_animate_spec_to_at(
        &quick_settings->rail_marker,
        reach_quick_settings_rail_marker_offset(page, quick_settings->layout_dpi),
        reach_theme_motion_preset(theme, REACH_MOTION_PRESET_BOUNCY), now);
    quick_settings->relayout_animate_pending = 1;
    if (page == REACH_QUICK_SETTINGS_PAGE_WIFI)
    {
        reach_quick_settings_scan_wifi(quick_settings);
    }
    reach_quick_settings_set_bluetooth_scan(quick_settings,
                                            page == REACH_QUICK_SETTINGS_PAGE_BLUETOOTH);
    return 1;
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

void reach_quick_settings_page_motions(
    const reach_quick_settings *quick_settings,
    reach_motion out_toggles[REACH_QUICK_SETTINGS_TOGGLE_COUNT],
    reach_motion out_page_opacity[REACH_QUICK_SETTINGS_PAGE_COUNT],
    reach_motion out_page_shift[REACH_QUICK_SETTINGS_PAGE_COUNT], reach_motion *out_rail_marker)
{
    if (quick_settings == nullptr)
        return;
    for (size_t index = 0; index < REACH_QUICK_SETTINGS_TOGGLE_COUNT; ++index)
        out_toggles[index] = quick_settings->toggle_states[index];
    for (size_t index = 0; index < REACH_QUICK_SETTINGS_PAGE_COUNT; ++index)
    {
        out_page_opacity[index] = quick_settings->page_opacity[index];
        out_page_shift[index] = quick_settings->page_shift[index];
    }
    *out_rail_marker = quick_settings->rail_marker;
}

static void reach_quick_settings_mirror_wifi_key(reach_quick_settings *quick_settings);
static void reach_quick_settings_collapse_wifi_key(reach_quick_settings *quick_settings);
static void reach_quick_settings_submit_wifi_key(reach_quick_settings *quick_settings);

void reach_quick_settings_wifi_motions(const reach_quick_settings *quick_settings,
                                       reach_keyed_row out_rows[REACH_QUICK_SETTINGS_WIFI_ROWS],
                                       reach_wifi_network out_items[REACH_QUICK_SETTINGS_WIFI_ROWS],
                                       reach_motion *out_switch, reach_blink *out_caret)
{
    if (quick_settings == nullptr)
        return;
    for (size_t slot = 0; slot < REACH_QUICK_SETTINGS_WIFI_ROWS; ++slot)
    {
        out_rows[slot] = quick_settings->wifi_row_motions[slot];
        out_items[slot] = quick_settings->wifi_row_items[slot];
    }
    *out_switch = quick_settings->wifi_switch;
    *out_caret = quick_settings->wifi_caret;
}

void reach_quick_settings_bluetooth_motions(
    const reach_quick_settings *quick_settings,
    reach_keyed_row out_rows[REACH_QUICK_SETTINGS_BLUETOOTH_ROWS],
    reach_bluetooth_device out_items[REACH_QUICK_SETTINGS_BLUETOOTH_ROWS], reach_motion *out_switch)
{
    if (quick_settings == nullptr)
        return;
    for (size_t slot = 0; slot < REACH_QUICK_SETTINGS_BLUETOOTH_ROWS; ++slot)
    {
        out_rows[slot] = quick_settings->bluetooth_row_motions[slot];
        out_items[slot] = quick_settings->bluetooth_row_items[slot];
    }
    *out_switch = quick_settings->bluetooth_switch;
}

const reach_ui_event_type *reach_quick_settings_routed_events(size_t *out_count)
{
    static const reach_ui_event_type events[] = {REACH_UI_EVENT_ESCAPE, REACH_UI_EVENT_ENTER,
                                                 REACH_UI_EVENT_TEXT_CHAR,
                                                 REACH_UI_EVENT_TEXT_EDIT};
    if (out_count != nullptr)
    {
        *out_count = sizeof(events) / sizeof(events[0]);
    }
    return events;
}

static void reach_quick_settings_capsule_handle_event(void *capsule, const reach_ui_event *event,
                                                      reach_capsule_event_result *out)
{
    reach_quick_settings *quick_settings = static_cast<reach_quick_settings *>(capsule);
    if (quick_settings == nullptr || event == nullptr || out == nullptr ||
        !quick_settings->state.open)
    {
        return;
    }
    reach_quick_settings_model &model = quick_settings->state.model;
    if (model.wifi_key_row < 0)
    {
        if (event->type == REACH_UI_EVENT_ESCAPE)
        {
            out->handled = 1;
            out->action.kind = REACH_FEATURE_ACTION_CLOSE_SELF;
        }
        return;
    }
    out->handled = 1;
    out->redraw = 1;
    out->request_update = 1;
    if (event->type == REACH_UI_EVENT_ESCAPE)
    {
        reach_quick_settings_collapse_wifi_key(quick_settings);
        out->relayout = 1;
        return;
    }
    if (event->type == REACH_UI_EVENT_ENTER)
    {
        reach_quick_settings_submit_wifi_key(quick_settings);
        return;
    }
    reach_text_edit_event changed = REACH_TEXT_EDIT_EVENT_NONE;
    if (reach_text_edit_apply_ui_event(&quick_settings->wifi_session.key_edit, event, &changed))
    {
        if (changed == REACH_TEXT_EDIT_EVENT_TEXT_CHANGED &&
            quick_settings->wifi_session.status == REACH_WIFI_SESSION_STATUS_INVALID_KEY)
        {
            reach_wifi_session_set_status(&quick_settings->wifi_session,
                                          REACH_WIFI_SESSION_STATUS_IDLE, nullptr);
            model.wifi_status = REACH_WIFI_SESSION_STATUS_IDLE;
        }
        reach_quick_settings_mirror_wifi_key(quick_settings);
    }
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
    quick_settings->wifi_key_ssid[0] = 0;
    state->model.wifi_key_row = -1;
    reach_wifi_session_clear_secrets(&quick_settings->wifi_session);
    state->model.wifi_key = quick_settings->wifi_session.key_edit;
    reach_blink_stop(&quick_settings->wifi_caret);
    quick_settings->bluetooth_selected_id[0] = 0;
    if (!next_open)
        reach_quick_settings_set_bluetooth_scan(quick_settings, 0);
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
        reach_wifi_service_refresh(quick_settings->wifi);
        reach_bluetooth_service_refresh(quick_settings->bluetooth_service);
        (void)reach_quick_settings_refresh_uptime(quick_settings);
        reach_motion_set(&quick_settings->height, 0);
        quick_settings->layout_initialized = 0;
        state->model.page = REACH_QUICK_SETTINGS_PAGE_OVERVIEW;
        reach_quick_settings_sync_tile_states(quick_settings, 0);
        reach_quick_settings_snap_pages(quick_settings);
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
    reach_quick_settings_snap_pages(quick_settings);
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
    reach_quick_settings_snap_pages(quick_settings);
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

void reach_quick_settings_apply_input(reach_quick_settings *quick_settings,
                                      const reach_audio_volume_state *input)
{
    if (quick_settings == nullptr)
    {
        return;
    }
    reach_quick_settings_model &model = quick_settings->state.model;
    model.input_available = input != nullptr ? 1 : 0;
    model.input = input != nullptr ? *input : reach_audio_volume_state{};
    model.input.level = reach_quick_settings_clamp01(model.input.level);
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
    if (quick_settings != nullptr && quick_settings->state.open &&
        quick_settings->uptime_deadline > 0 &&
        reach_motion_now_seconds() >= quick_settings->uptime_deadline &&
        reach_quick_settings_refresh_uptime(quick_settings))
    {
        changes.redraw = 1;
    }
    if (quick_settings != nullptr && reach_quick_settings_release_settled_rows(
                                         quick_settings, quick_settings->output_device_rows,
                                         quick_settings->output_device_items))
    {
        reach_quick_settings_sync_output_rows(quick_settings, quick_settings->layout_dpi,
                                              reach_quick_settings_rows_animate(quick_settings));
        changes.redraw = 1;
    }
    reach_keyed_row_change
        bluetooth_changes[REACH_KEYED_ROWS_MAX_CHANGES(REACH_QUICK_SETTINGS_BLUETOOTH_ROWS)];
    if (quick_settings != nullptr &&
        reach_keyed_rows_release_settled(
            quick_settings->bluetooth_row_motions, REACH_QUICK_SETTINGS_BLUETOOTH_ROWS,
            bluetooth_changes,
            REACH_KEYED_ROWS_MAX_CHANGES(REACH_QUICK_SETTINGS_BLUETOOTH_ROWS)) > 0)
    {
        changes.redraw = 1;
    }
    reach_keyed_row_change
        wifi_changes[REACH_KEYED_ROWS_MAX_CHANGES(REACH_QUICK_SETTINGS_WIFI_ROWS)];
    if (quick_settings != nullptr &&
        reach_keyed_rows_release_settled(
            quick_settings->wifi_row_motions, REACH_QUICK_SETTINGS_WIFI_ROWS, wifi_changes,
            REACH_KEYED_ROWS_MAX_CHANGES(REACH_QUICK_SETTINGS_WIFI_ROWS)) > 0)
    {
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
        reach_motion_set(&quick_settings->height,
                         reach_motion_value_at(&quick_settings->height, now_seconds));
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
    if (quick_settings->state.open && quick_settings->uptime_deadline > 0 &&
        (out->deadline_seconds <= 0 || quick_settings->uptime_deadline < out->deadline_seconds))
    {
        out->deadline_seconds = quick_settings->uptime_deadline;
    }
    if (quick_settings->section_cleanup_pending)
    {
        const reach_quick_settings_state &state = quick_settings->state;
        const reach_motion *motions[] = {&quick_settings->layout_progress,
                                         &state.output_devices_span, &state.app_volumes_span};
        for (const reach_motion *motion : motions)
            out->deadline_seconds = fmax(out->deadline_seconds, reach_motion_deadline(motion));
        out->deadline_seconds =
            fmax(out->deadline_seconds,
                 fmax(fmax(fmax(reach_keyed_rows_deadline(quick_settings->wifi_row_motions,
                                                          REACH_QUICK_SETTINGS_WIFI_ROWS),
                                reach_keyed_rows_deadline(quick_settings->bluetooth_row_motions,
                                                          REACH_QUICK_SETTINGS_BLUETOOTH_ROWS)),
                           reach_keyed_rows_deadline(quick_settings->output_device_rows,
                                                     REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES)),
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

static int32_t reach_quick_settings_select_bluetooth(reach_quick_settings *quick_settings,
                                                     size_t row)
{
    reach_quick_settings_model &model = quick_settings->state.model;
    reach_bluetooth_session &session = quick_settings->bluetooth_session;
    if (row >= model.bluetooth_row_count || session.pairing.active)
    {
        return 0;
    }
    const reach_bluetooth_device &device = model.bluetooth_rows[row];
    if (device.paired)
    {
        if (reach_bluetooth_device_id_equal(quick_settings->bluetooth_selected_id, device.id))
            quick_settings->bluetooth_selected_id[0] = 0;
        else
            reach_copy_utf16(quick_settings->bluetooth_selected_id,
                             REACH_BLUETOOTH_DEVICE_ID_CAPACITY, device.id);
    }
    else if (quick_settings->bluetooth_service != nullptr &&
             session.status != REACH_BLUETOOTH_SESSION_STATUS_PAIRING)
    {
        quick_settings->bluetooth_selected_id[0] = 0;
        reach_bluetooth_session_set_status(&session, REACH_BLUETOOTH_SESSION_STATUS_PAIRING,
                                           device.id);
        reach_bluetooth_service_pair(quick_settings->bluetooth_service, device.id);
    }
    reach_quick_settings_build_bluetooth_rows(quick_settings);
    quick_settings->relayout_animate_pending = 1;
    return 1;
}

static int32_t reach_quick_settings_bluetooth_action(reach_quick_settings *quick_settings,
                                                     size_t button)
{
    reach_quick_settings_model &model = quick_settings->state.model;
    reach_bluetooth_session &session = quick_settings->bluetooth_session;
    if (quick_settings->bluetooth_service == nullptr || model.bluetooth_action_row < 0)
    {
        return 0;
    }
    const reach_bluetooth_device &device = model.bluetooth_rows[model.bluetooth_action_row];
    if (model.bluetooth_pin_active)
    {
        int32_t accept = button == 0;
        reach_bluetooth_session_set_status(&session,
                                           accept ? REACH_BLUETOOTH_SESSION_STATUS_PAIRING
                                                  : REACH_BLUETOOTH_SESSION_STATUS_IDLE,
                                           device.id);
        reach_bluetooth_service_respond_pairing(quick_settings->bluetooth_service, accept);
        session.pairing.active = 0;
    }
    else if (button == 0)
    {
        reach_bluetooth_service_unpair(quick_settings->bluetooth_service, device.id);
        quick_settings->bluetooth_selected_id[0] = 0;
    }
    reach_quick_settings_build_bluetooth_rows(quick_settings);
    quick_settings->relayout_animate_pending = 1;
    return 1;
}

static void reach_quick_settings_mirror_wifi_key(reach_quick_settings *quick_settings)
{
    quick_settings->state.model.wifi_key = quick_settings->wifi_session.key_edit;
    reach_blink_reset_at(&quick_settings->wifi_caret, 0.53, 0.53, reach_motion_now_seconds());
}

static void reach_quick_settings_collapse_wifi_key(reach_quick_settings *quick_settings)
{
    quick_settings->wifi_key_ssid[0] = 0;
    quick_settings->state.model.wifi_key_row = -1;
    reach_wifi_session_clear_secrets(&quick_settings->wifi_session);
    quick_settings->state.model.wifi_key = quick_settings->wifi_session.key_edit;
    reach_blink_stop(&quick_settings->wifi_caret);
    reach_quick_settings_sync_wifi_rows(quick_settings,
                                        reach_quick_settings_rows_animate(quick_settings));
    quick_settings->relayout_animate_pending = 1;
}

static int32_t reach_quick_settings_expand_wifi_key(reach_quick_settings *quick_settings,
                                                    size_t row)
{
    reach_quick_settings_model &model = quick_settings->state.model;
    if (model.wifi_key_row == (int32_t)row)
    {
        reach_quick_settings_collapse_wifi_key(quick_settings);
        return 0;
    }
    reach_copy_utf16(quick_settings->wifi_key_ssid, REACH_WIFI_SSID_CAPACITY,
                     model.wifi_rows[row].ssid);
    model.wifi_key_row = (int32_t)row;
    reach_wifi_session_clear_secrets(&quick_settings->wifi_session);
    reach_quick_settings_mirror_wifi_key(quick_settings);
    reach_quick_settings_sync_wifi_rows(quick_settings,
                                        reach_quick_settings_rows_animate(quick_settings));
    quick_settings->relayout_animate_pending = 1;
    return 1;
}

static void reach_quick_settings_submit_wifi_key(reach_quick_settings *quick_settings)
{
    reach_quick_settings_model &model = quick_settings->state.model;
    reach_wifi_session &session = quick_settings->wifi_session;
    if (quick_settings->wifi == nullptr || model.wifi_key_row < 0 ||
        reach_wifi_session_busy(&session))
    {
        return;
    }
    size_t index =
        reach_wifi_network_list_find(&session.networks, model.wifi_rows[model.wifi_key_row].ssid);
    if (index >= session.networks.count)
    {
        return;
    }
    session.selected = (int32_t)index;
    reach_wifi_connect_request request = {};
    if (!reach_wifi_session_connect_ready(&session) ||
        !reach_wifi_session_build_connect(&session, index, &request))
    {
        return;
    }
    reach_wifi_session_set_status(&session, REACH_WIFI_SESSION_STATUS_CONNECTING, request.ssid);
    model.wifi_status = session.status;
    reach_copy_utf16(model.wifi_status_ssid, REACH_WIFI_SSID_CAPACITY, session.status_ssid);
    reach_wifi_service_connect(quick_settings->wifi, &request);
}

static void reach_quick_settings_connect_wifi(reach_quick_settings *quick_settings, size_t row)
{
    reach_quick_settings_model &model = quick_settings->state.model;
    reach_wifi_session &session = quick_settings->wifi_session;
    if (quick_settings->wifi == nullptr || row >= model.wifi_row_count ||
        reach_wifi_session_busy(&session))
    {
        return;
    }
    if (model.wifi_key_row >= 0 && (size_t)model.wifi_key_row != row)
    {
        reach_quick_settings_collapse_wifi_key(quick_settings);
    }
    size_t index = reach_wifi_network_list_find(&session.networks, model.wifi_rows[row].ssid);
    reach_wifi_connect_request request = {};
    if (index >= session.networks.count ||
        !reach_wifi_session_build_connect(&session, index, &request))
    {
        return;
    }
    reach_wifi_session_set_status(&session, REACH_WIFI_SESSION_STATUS_CONNECTING, request.ssid);
    model.wifi_status = session.status;
    reach_copy_utf16(model.wifi_status_ssid, REACH_WIFI_SSID_CAPACITY, session.status_ssid);
    reach_wifi_service_connect(quick_settings->wifi, &request);
}

static void reach_quick_settings_forget_wifi(reach_quick_settings *quick_settings, size_t row)
{
    const reach_quick_settings_model &model = quick_settings->state.model;
    if (quick_settings->wifi == nullptr || row >= model.wifi_row_count ||
        reach_wifi_session_busy(&quick_settings->wifi_session) ||
        (!model.wifi_rows[row].saved && !model.wifi_rows[row].connected))
    {
        return;
    }
    reach_wifi_service_forget(quick_settings->wifi, model.wifi_rows[row].ssid);
}

static void reach_quick_settings_toggle_mute(reach_quick_settings *quick_settings, size_t target)
{
    reach_quick_settings_model &model = quick_settings->state.model;
    if (target == REACH_QUICK_SETTINGS_MUTE_INPUT)
    {
        model.input.muted = model.input.muted ? 0 : 1;
        (void)reach_system_status_set_input_muted(quick_settings->status, model.input.muted);
        return;
    }
    if (target >= REACH_QUICK_SETTINGS_MUTE_FIRST_SESSION)
    {
        size_t index = target - REACH_QUICK_SETTINGS_MUTE_FIRST_SESSION;
        if (index < model.sessions.count)
        {
            reach_audio_volume_session &session = model.sessions.sessions[index];
            session.muted = session.muted ? 0 : 1;
            (void)reach_system_status_set_session_muted(quick_settings->status,
                                                        session.session_instance_id, session.muted);
        }
        return;
    }
    model.main_muted = model.main_muted ? 0 : 1;
    (void)reach_system_status_set_main_muted(quick_settings->status, model.main_muted);
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

    case REACH_QUICK_SETTINGS_ACTION_SELECT_PAGE:
        if (reach_quick_settings_select_page(quick_settings,
                                             (reach_quick_settings_page)action->item_index))
        {
            out->redraw = 1;
            out->relayout = 1;
        }
        break;

    case REACH_QUICK_SETTINGS_ACTION_TOGGLE_WIFI:
        if (quick_settings->wifi != nullptr &&
            reach_wifi_session_toggle_radio(&quick_settings->wifi_session))
        {
            quick_settings->state.model.wifi_radio = quick_settings->wifi_session.radio;
            reach_wifi_service_set_radio_enabled(
                quick_settings->wifi, quick_settings->wifi_session.radio == REACH_WIFI_RADIO_ON);
            reach_quick_settings_sync_tile_states(quick_settings, 1);
            out->redraw = 1;
        }
        break;

    case REACH_QUICK_SETTINGS_ACTION_CONNECT_WIFI:
    {
        const reach_quick_settings_model &model = quick_settings->state.model;
        if (action->item_index < model.wifi_row_count &&
            reach_wifi_security_needs_key(model.wifi_rows[action->item_index].security) &&
            !model.wifi_rows[action->item_index].saved)
        {
            if (reach_quick_settings_expand_wifi_key(quick_settings, action->item_index))
                out->action.kind = REACH_FEATURE_ACTION_TAKE_KEYBOARD_FOCUS;
            out->relayout = 1;
        }
        else
        {
            reach_quick_settings_connect_wifi(quick_settings, action->item_index);
        }
        out->redraw = 1;
        break;
    }

    case REACH_QUICK_SETTINGS_ACTION_SELECT_BLUETOOTH:
        out->relayout |= reach_quick_settings_select_bluetooth(quick_settings, action->item_index);
        out->redraw = 1;
        break;

    case REACH_QUICK_SETTINGS_ACTION_BLUETOOTH_ACTION:
        out->relayout |= reach_quick_settings_bluetooth_action(quick_settings, action->item_index);
        out->redraw = 1;
        break;

    case REACH_QUICK_SETTINGS_ACTION_FOCUS_WIFI_KEY:
        reach_quick_settings_mirror_wifi_key(quick_settings);
        out->action.kind = REACH_FEATURE_ACTION_TAKE_KEYBOARD_FOCUS;
        out->redraw = 1;
        break;

    case REACH_QUICK_SETTINGS_ACTION_SUBMIT_WIFI_KEY:
        reach_quick_settings_submit_wifi_key(quick_settings);
        out->redraw = 1;
        break;

    case REACH_QUICK_SETTINGS_ACTION_FORGET_WIFI:
        reach_quick_settings_forget_wifi(quick_settings, action->item_index);
        out->redraw = 1;
        break;

    case REACH_QUICK_SETTINGS_ACTION_TOGGLE_DARK_MODE:
        if (quick_settings->routes.set_light_theme != nullptr)
        {
            int32_t light_theme = quick_settings->state.model.light_theme ? 0 : 1;
            quick_settings->state.model.light_theme = light_theme;
            reach_quick_settings_sync_tile_states(quick_settings, 1);
            quick_settings->routes.set_light_theme(quick_settings->routes.user, light_theme);
            out->redraw = 1;
        }
        break;

    case REACH_QUICK_SETTINGS_ACTION_TOGGLE_BLUETOOTH:
        reach_quick_settings_toggle_bluetooth(quick_settings, out);
        break;

    case REACH_QUICK_SETTINGS_ACTION_OPEN_PROJECT:
        (void)reach_system_status_open_project_menu(quick_settings->status);
        out->action.kind = REACH_FEATURE_ACTION_CLOSE_SELF;
        break;

    case REACH_QUICK_SETTINGS_ACTION_OPEN_SETTINGS:
        out->action.kind = REACH_FEATURE_ACTION_OPEN_SETTINGS_APP;
        out->action.flags |= REACH_FEATURE_ACTION_FLAG_CLOSE_SELF_FIRST;
        break;

    case REACH_QUICK_SETTINGS_ACTION_SET_OUTPUT_DEVICE:
    {
        const uint16_t *output_device_id =
            reach_quick_settings_output_device_id(quick_settings, action->output_device_index);
        int32_t changed = output_device_id != nullptr &&
                          reach_system_status_set_default_output_device(
                              quick_settings->status, output_device_id) == REACH_OK;
        (void)changed;
        reach_quick_settings_refresh_audio(quick_settings);
        out->redraw = 1;
        break;
    }
    case REACH_QUICK_SETTINGS_ACTION_SET_INPUT_VOLUME:
        (void)reach_system_status_set_input_volume(quick_settings->status, action->volume_level);
        if (quick_settings->state.model.input.muted)
        {
            (void)reach_system_status_set_input_muted(quick_settings->status, 0);
            quick_settings->state.model.input.muted = 0;
        }
        quick_settings->state.model.input.level = action->volume_level;
        out->redraw = 1;
        break;

    case REACH_QUICK_SETTINGS_ACTION_TOGGLE_MUTE:
        reach_quick_settings_toggle_mute(quick_settings, action->item_index);
        out->redraw = 1;
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
        float scale = quick_settings->layout_dpi > 0.0f ? quick_settings->layout_dpi : 1.0f;
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

static void reach_quick_settings_capsule_handle_event(void *capsule, const reach_ui_event *event,
                                                      reach_capsule_event_result *out);

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
        reach_quick_settings_capsule_handle_event,
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
                                  REACH_FEATURE_TRANSITION_FROM_ABOVE);
    reach_pressable_init(&quick_settings->pressable);
    reach_quick_settings_model_init(&quick_settings->state.model);
    reach_wifi_session_init(&quick_settings->wifi_session);
    reach_bluetooth_session_init(&quick_settings->bluetooth_session);
    reach_quick_settings_snap_pages(quick_settings);
    *out_quick_settings = quick_settings;
    return REACH_OK;
}

void reach_quick_settings_destroy(reach_quick_settings *quick_settings)
{
    delete quick_settings;
}

void reach_quick_settings_attach_services(reach_quick_settings *quick_settings,
                                          reach_system_status *status, reach_wifi_service *wifi,
                                          reach_bluetooth_service *bluetooth,
                                          reach_profile_service *profile,
                                          const reach_power_session_port *power_session)
{
    if (quick_settings != nullptr)
    {
        quick_settings->status = status;
        quick_settings->wifi = wifi;
        quick_settings->bluetooth_service = bluetooth;
        quick_settings->profile = profile;
        quick_settings->power_session = power_session;
    }
}

static int32_t reach_quick_settings_refresh_uptime(reach_quick_settings *quick_settings)
{
    const reach_power_session_port *session = quick_settings->power_session;
    if (session == nullptr || session->ops.now_milliseconds == nullptr)
    {
        quick_settings->uptime_deadline = 0;
        return 0;
    }
    uint64_t uptime = session->ops.now_milliseconds(session->session);
    int64_t minutes = (int64_t)(uptime / 60000u);
    quick_settings->uptime_deadline =
        reach_motion_now_seconds() + (double)(60000u - uptime % 60000u) / 1000.0;
    if (quick_settings->state.model.uptime_minutes == minutes)
    {
        return 0;
    }
    quick_settings->state.model.uptime_minutes = minutes;
    return 1;
}

void reach_quick_settings_set_routes(reach_quick_settings *quick_settings,
                                     const reach_quick_settings_routes *routes)
{
    if (quick_settings != nullptr)
    {
        quick_settings->routes = routes != nullptr ? *routes : reach_quick_settings_routes{};
    }
}

int32_t reach_quick_settings_apply_config(reach_quick_settings *quick_settings, int32_t light_theme)
{
    if (quick_settings == nullptr)
    {
        return 0;
    }
    int32_t changed = 0;
    int32_t next = light_theme ? 1 : 0;
    if (quick_settings->state.model.light_theme != next)
    {
        quick_settings->state.model.light_theme = next;
        reach_quick_settings_sync_tile_states(quick_settings,
                                              reach_quick_settings_tiles_animate(quick_settings));
        changed = 1;
    }
    return changed;
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

int32_t reach_quick_settings_apply_bluetooth_snapshot(reach_quick_settings *quick_settings,
                                                      const reach_bluetooth_snapshot *snapshot)
{
    if (quick_settings == nullptr || snapshot == nullptr)
    {
        return 0;
    }
    reach_quick_settings_model &model = quick_settings->state.model;
    size_t previous_rows = model.bluetooth_row_count;
    int32_t previous_action = model.bluetooth_action_row;
    int32_t previous_pin = model.bluetooth_pin_active;
    reach_bluetooth_session_apply_snapshot(&quick_settings->bluetooth_session, snapshot);
    reach_quick_settings_build_bluetooth_rows(quick_settings);
    if (quick_settings->state.open && model.page == REACH_QUICK_SETTINGS_PAGE_BLUETOOTH &&
        (previous_rows != model.bluetooth_row_count ||
         previous_action != model.bluetooth_action_row ||
         previous_pin != model.bluetooth_pin_active))
    {
        quick_settings->relayout_animate_pending = 1;
        return 1;
    }
    return 0;
}

int32_t reach_quick_settings_apply_wifi_snapshot(reach_quick_settings *quick_settings,
                                                 const reach_wifi_snapshot *snapshot)
{
    if (quick_settings == nullptr || snapshot == nullptr)
    {
        return 0;
    }
    reach_quick_settings_state *state = &quick_settings->state;
    reach_wifi_radio_state previous_radio = state->model.wifi_radio;
    size_t previous_rows = state->model.wifi_row_count;
    int32_t previous_key_row = state->model.wifi_key_row;
    if (reach_wifi_session_apply_snapshot(&quick_settings->wifi_session, snapshot))
    {
        quick_settings->wifi_key_ssid[0] = 0;
        reach_blink_stop(&quick_settings->wifi_caret);
    }
    else if (quick_settings->wifi_session.status == REACH_WIFI_SESSION_STATUS_INVALID_KEY)
    {
        reach_text_edit_select_all(&quick_settings->wifi_session.key_edit);
    }
    state->model.wifi_key = quick_settings->wifi_session.key_edit;
    reach_quick_settings_build_wifi_rows(quick_settings);
    if (previous_key_row != state->model.wifi_key_row)
    {
        reach_quick_settings_sync_wifi_rows(quick_settings,
                                            reach_quick_settings_rows_animate(quick_settings));
        previous_rows = (size_t)-1;
    }
    if (state->model.wifi_radio != previous_radio)
    {
        reach_quick_settings_sync_tile_states(quick_settings,
                                              reach_quick_settings_tiles_animate(quick_settings));
        if (state->model.wifi_radio == REACH_WIFI_RADIO_ON &&
            state->model.page == REACH_QUICK_SETTINGS_PAGE_WIFI)
            reach_quick_settings_scan_wifi(quick_settings);
    }
    if (state->open && state->model.page == REACH_QUICK_SETTINGS_PAGE_WIFI &&
        (previous_rows != state->model.wifi_row_count || previous_radio != state->model.wifi_radio))
    {
        quick_settings->relayout_animate_pending = 1;
        return 1;
    }
    return 0;
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

    reach_profile_snapshot profile = {};
    if (reach_profile_service_take(quick_settings->profile, &profile))
    {
        reach_copy_utf16(state->model.display_name, REACH_USER_ACCOUNT_NAME_CAPACITY,
                         profile.display_name);
        state->model.picture_image_id = profile.picture_image_id;
        out->redraw = 1;
        out->request_update = 1;
    }

    reach_bluetooth_snapshot bluetooth_snapshot = {};
    if (quick_settings->bluetooth_service != nullptr &&
        reach_bluetooth_service_take(quick_settings->bluetooth_service, &bluetooth_snapshot))
    {
        out->relayout |=
            reach_quick_settings_apply_bluetooth_snapshot(quick_settings, &bluetooth_snapshot);
        out->redraw = 1;
        out->request_update = 1;
    }

    reach_wifi_snapshot wifi_snapshot = {};
    if (quick_settings->wifi != nullptr &&
        reach_wifi_service_take(quick_settings->wifi, &wifi_snapshot))
    {
        out->relayout |= reach_quick_settings_apply_wifi_snapshot(quick_settings, &wifi_snapshot);
        out->redraw = 1;
        out->request_update = 1;
    }

    if (quick_settings->status == nullptr)
    {
        return;
    }

    reach_system_status_system_snapshot system_snapshot = {};
    if (reach_system_status_take_system(quick_settings->status, &system_snapshot))
    {
        if (system_snapshot.power_valid)
        {
            state->model.power = system_snapshot.power;
        }
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
        reach_quick_settings_apply_input(
            quick_settings, audio_snapshot.input_valid ? &audio_snapshot.input : nullptr);
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
    const float padding = reach_quick_settings_metrics_for_scale(scale).surface_padding;
    const float notch_height = reach_popup_notch_height_scaled(scale);

    bounds.x += padding;
    bounds.y += padding + (drop_direction == REACH_POPUP_DROP_DOWN ? notch_height : 0.0f);
    bounds.width -= padding * 2.0f;
    bounds.height -= padding * 2.0f + notch_height;

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
        border_thickness * 2.0f +
        reach_quick_settings_metrics_for_scale(scale).surface_padding * 2.0f +
        reach_popup_notch_height_scaled(scale);
    float content_height = reach_quick_settings_content_height_for_model_scaled(
        &reach_quick_settings_state_mut(quick_settings)->model, scale);

    if (out_width != nullptr)
    {
        *out_width =
            reach_quick_settings_metrics_for_scale(scale).panel_width + border_thickness * 2.0f;
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
    anchor.alignment = REACH_POPUP_ALIGN_MONITOR_RIGHT;
    anchor.edge_gap = reach_popup_bar_gap_scaled(ctx->dpi_scale);
    return anchor;
}

static reach_popup_placement
reach_quick_settings_placement(reach_quick_settings *quick_settings,
                               const reach_quick_settings_layout_context *ctx, float height)
{
    float width = 0.0f;
    float target_height = 0.0f;
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
        float output_span_from = reach_motion_value_at(&state.output_devices_span, event_time);
        float app_span_from = reach_motion_value_at(&state.app_volumes_span, event_time);
        float output_span_velocity =
            reach_motion_velocity_at(&state.output_devices_span, event_time);
        float app_span_velocity = reach_motion_velocity_at(&state.app_volumes_span, event_time);
        float output_span_to = 0, app_span_to = 0;
        reach_quick_settings_section_spans(&state.model, ctx->dpi_scale, &output_span_to,
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
            {current_height, target.bounds.height, height_velocity, 0, height_limit, 1}};
        reach_motion square = reach_quick_settings_start_layout_progress(
            &quick_settings->layout_progress,
            reach_theme_motion_preset(ctx->theme, REACH_MOTION_PRESET_SMOOTH), event_time, channels,
            3);
        const reach_motion *progress = &quick_settings->layout_progress;
        state.output_devices_span =
            reach_quick_settings_channel_motion(progress, &square, &channels[0]);
        state.app_volumes_span =
            reach_quick_settings_channel_motion(progress, &square, &channels[1]);
        quick_settings->height =
            reach_quick_settings_channel_motion(progress, &square, &channels[2]);
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
                   : quick_settings->section_cleanup_pending
                       ? fmaxf(state.render_bounds.height, height_maximum)
                       : height_maximum;
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
    reach_bluetooth_state previous_radio = quick_settings->bluetooth_session.radio;
    reach_bluetooth_session_set_radio(&quick_settings->bluetooth_session, state->model.bluetooth);
    reach_quick_settings_build_bluetooth_rows(quick_settings);
    int32_t radio_changed = previous_radio.available != state->model.bluetooth.available ||
                            previous_radio.enabled != state->model.bluetooth.enabled;
    if (radio_changed && state->model.page == REACH_QUICK_SETTINGS_PAGE_BLUETOOTH)
        reach_quick_settings_set_bluetooth_scan(quick_settings, state->open);
    int32_t layout_changed =
        previous_brightness.available != state->model.brightness.available ||
        (radio_changed && state->model.page == REACH_QUICK_SETTINGS_PAGE_BLUETOOTH);

    out->relayout = layout_changed && state->open;
}
