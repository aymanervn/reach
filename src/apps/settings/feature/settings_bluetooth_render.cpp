#include "reach/core/render_commands.h"
#include "reach/core/typography.h"
#include "reach/features/common/loader_render.h"
#include "reach/features/common/scrollbar_render.h"
#include "reach/features/common/ui_controls.h"

#include "settings_pages_internal.h"
#include "settings_render_internal.h"

static reach_vector_icon_id reach_settings_bluetooth_fallback_icon(reach_bluetooth_device_kind kind)
{
    switch (kind)
    {
    case REACH_BLUETOOTH_DEVICE_AUDIO:
        return REACH_VECTOR_ICON_MUSIC_NOTE;
    case REACH_BLUETOOTH_DEVICE_PHONE:
    case REACH_BLUETOOTH_DEVICE_COMPUTER:
        return REACH_VECTOR_ICON_PROJECT;
    default:
        return REACH_VECTOR_ICON_BLUETOOTH_ON;
    }
}

static const uint16_t *reach_settings_bluetooth_row_detail(const reach_bluetooth_device *device)
{
    if (device->connected)
    {
        return (const uint16_t *)u"Connected";
    }
    if (device->paired)
    {
        return (const uint16_t *)u"Paired";
    }
    return reach_bluetooth_device_kind_label(device->kind);
}

static void reach_settings_bluetooth_render_row(const reach_settings_render_input *input,
                                                reach_render_command_buffer *commands,
                                                size_t row_index, reach_color accent)
{
    const reach_settings_model *model = input->model;
    const reach_settings_layout *layout = input->layout;
    size_t index = layout->bluetooth_row_indices[row_index];
    if (index >= model->bluetooth_devices.count)
    {
        return;
    }
    const reach_bluetooth_device *device = &model->bluetooth_devices.devices[index];
    const reach_rect_f32 row = layout->bluetooth_rows[row_index];

    reach_color background =
        device->connected ? reach_settings_color_with_alpha(accent, input->theme->accent_tint_alpha)
                          : input->theme->settings_card_background;
    reach_settings_push_rect(commands, row, reach_settings_scale(input, input->theme->radius_small),
                             background);

    float header_height = reach_settings_scale(input, 58.0f);
    float icon_box_size = reach_settings_scale(input, 34.0f);
    reach_rect_f32 icon_box = {row.x + reach_settings_scale(input, 14.0f),
                               row.y + (header_height - icon_box_size) * 0.5f, icon_box_size,
                               icon_box_size};
    reach_settings_push_rect(commands, icon_box,
                             reach_settings_scale(input, input->theme->radius_small),
                             input->theme->settings_icon_box_background);
    if (model->bluetooth_icons[index] != 0)
    {
        float inset = reach_settings_scale(input, 6.0f);
        reach_rect_f32 image = {icon_box.x + inset, icon_box.y + inset,
                                icon_box.width - inset * 2.0f, icon_box.height - inset * 2.0f};
        reach_settings_push_app_icon(commands, image, model->bluetooth_icons[index], 1.0f);
    }
    else
    {
        reach_settings_push_icon(commands, icon_box, input->theme->settings_secondary_text,
                                 reach_settings_bluetooth_fallback_icon(device->kind), 0.26f);
    }

    float text_x = icon_box.x + icon_box_size + reach_settings_scale(input, 14.0f);
    float text_right = row.x + row.width - reach_settings_scale(input, 16.0f);
    reach_settings_push_text(commands,
                             {text_x, row.y + reach_settings_scale(input, 12.0f),
                              text_right - text_x, reach_settings_scale(input, 18.0f)},
                             device->name, reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM),
                             REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
                             input->theme->settings_text, 1);

    const uint16_t *detail = reach_settings_bluetooth_row_detail(device);
    reach_color detail_color = input->theme->settings_secondary_text;
    if (model->bluetooth_expanded_row >= 0 && (size_t)model->bluetooth_expanded_row == index &&
        model->bluetooth_status != REACH_SETTINGS_BLUETOOTH_STATUS_IDLE &&
        model->bluetooth_status != REACH_SETTINGS_BLUETOOTH_STATUS_SCANNING)
    {
        detail = reach_settings_bluetooth_status_message(model->bluetooth_status);
        detail_color = model->bluetooth_status == REACH_SETTINGS_BLUETOOTH_STATUS_PAIRED
                           ? input->theme->settings_status_success
                       : model->bluetooth_status == REACH_SETTINGS_BLUETOOTH_STATUS_FAILED ||
                               model->bluetooth_status == REACH_SETTINGS_BLUETOOTH_STATUS_REJECTED
                           ? input->theme->settings_status_error
                           : input->theme->settings_secondary_text;
    }
    reach_settings_push_text(commands,
                             {text_x, row.y + reach_settings_scale(input, 33.0f),
                              text_right - text_x, reach_settings_scale(input, 15.0f)},
                             detail, reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL),
                             REACH_TEXT_WEIGHT_NORMAL, input->text_alignment_leading, detail_color,
                             1);

    if (layout->bluetooth_pin_accept_button.width > 0.0f)
    {
        reach_settings_push_text(
            commands,
            {row.x + reach_settings_scale(input, 14.0f),
             layout->bluetooth_pin_accept_button.y - reach_settings_scale(input, 24.0f),
             row.width - reach_settings_scale(input, 28.0f), reach_settings_scale(input, 18.0f)},
            model->bluetooth_pairing.pin, reach_settings_scale(input, REACH_TEXT_SIZE_LARGE),
            REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading, input->theme->settings_text,
            1);

        reach_ui_button_style accept_style =
            reach_settings_button_style(input, input->theme->settings_button_primary);
        reach_ui_button_render(commands, layout->bluetooth_pin_accept_button,
                               (const uint16_t *)u"Yes", &accept_style, 1,
                               reach_settings_model_button_press_value(
                                   model, REACH_SETTINGS_HIT_BLUETOOTH_PIN_ACCEPT));
        reach_ui_button_style reject_style =
            reach_settings_button_style(input, input->theme->settings_card_background);
        reach_ui_button_render(commands, layout->bluetooth_pin_reject_button,
                               (const uint16_t *)u"No", &reject_style, 1,
                               reach_settings_model_button_press_value(
                                   model, REACH_SETTINGS_HIT_BLUETOOTH_PIN_REJECT));
        return;
    }

    if (layout->bluetooth_action_button.width > 0.0f)
    {
        reach_ui_button_style action_style = reach_settings_button_style(
            input, device->paired ? input->theme->settings_button_danger
                                  : input->theme->settings_button_primary);
        reach_ui_button_render(
            commands, layout->bluetooth_action_button,
            device->paired ? (const uint16_t *)u"Remove" : (const uint16_t *)u"Pair", &action_style,
            model->bluetooth_status != REACH_SETTINGS_BLUETOOTH_STATUS_PAIRING,
            reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_BLUETOOTH_ACTION));
    }
}

void reach_settings_render_bluetooth_page(const reach_settings_render_input *input,
                                          reach_render_command_buffer *commands)
{
    const reach_settings_model *model = input->model;
    const reach_settings_layout *layout = input->layout;
    reach_color accent = reach_theme_accent_color(input->theme, REACH_THEME_ACCENT_BLUE);
    int32_t enabled = model->bluetooth_radio.available && model->bluetooth_radio.enabled;

    reach_settings_push_rect(commands, layout->bluetooth_radio_card,
                             reach_settings_scale(input, input->theme->radius_small),
                             input->theme->settings_card_background);
    reach_settings_push_rect(commands, layout->bluetooth_radio_icon,
                             reach_settings_scale(input, input->theme->radius_small),
                             input->theme->settings_icon_box_background);
    reach_settings_push_icon(
        commands, layout->bluetooth_radio_icon, input->theme->settings_secondary_text,
        enabled ? REACH_VECTOR_ICON_BLUETOOTH_ON : REACH_VECTOR_ICON_BLUETOOTH_OFF, 0.24f);
    reach_settings_push_text(
        commands, layout->bluetooth_radio_title, (const uint16_t *)u"Bluetooth",
        reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM), REACH_TEXT_WEIGHT_SEMIBOLD,
        input->text_alignment_leading, input->theme->settings_text, 1);

    const uint16_t *subtitle = (const uint16_t *)u"Bluetooth is off";
    if (!model->bluetooth_radio.available)
    {
        subtitle = (const uint16_t *)u"No Bluetooth radio found";
    }
    else if (enabled)
    {
        subtitle = reach_bluetooth_paired_count(&model->bluetooth_devices) > 0
                       ? (const uint16_t *)u"Paired devices are ready to use"
                       : (const uint16_t *)u"Scan to find nearby devices";
    }
    reach_settings_push_text(commands, layout->bluetooth_radio_subtitle, subtitle,
                             reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL),
                             REACH_TEXT_WEIGHT_NORMAL, input->text_alignment_leading,
                             input->theme->settings_secondary_text, 1);

    reach_ui_toggle_style toggle_style = {};
    toggle_style.track_off = input->theme->settings_toggle_track_off;
    toggle_style.track_on = reach_settings_color_with_alpha(accent, 0.85f);
    toggle_style.knob = input->theme->settings_toggle_knob;
    reach_ui_toggle_render(commands, layout->bluetooth_radio_toggle, &toggle_style,
                           reach_animation_manager_value(&model->bluetooth_radio_animation, 0));

    reach_ui_button_style scan_style =
        reach_settings_button_style(input, input->theme->settings_button_primary);
    reach_ui_button_render(
        commands, layout->bluetooth_scan_button,
        model->bluetooth_scanning ? (const uint16_t *)u"Stop scanning" : (const uint16_t *)u"Scan",
        &scan_style, enabled,
        reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_BLUETOOTH_SCAN));

    if (layout->bluetooth_loader_bar.width > 0.0f)
    {
        reach_rect_f32 bar =
            reach_loader_bar_rect(&model->bluetooth_loader, layout->bluetooth_loader_bar);
        reach_rect_f32 origin = {0.0f, 0.0f, 0.0f, 0.0f};
        (void)reach_loader_build_render_commands(layout->bluetooth_loader_bar, bar, origin, accent,
                                                 commands);
    }

    if (!enabled)
    {
        reach_settings_push_text(commands, layout->bluetooth_viewport,
                                 model->bluetooth_radio.available
                                     ? (const uint16_t *)u"Turn Bluetooth on to see devices"
                                     : (const uint16_t *)u"This device has no Bluetooth radio",
                                 reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM),
                                 REACH_TEXT_WEIGHT_NORMAL, REACH_TEXT_ALIGNMENT_CENTER,
                                 input->theme->settings_secondary_text, 1);
        return;
    }

    if (layout->bluetooth_row_count == 0)
    {
        reach_settings_push_text(commands, layout->bluetooth_viewport,
                                 model->bluetooth_loaded ? (const uint16_t *)u"No devices found"
                                                         : (const uint16_t *)u"Reading devices...",
                                 reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM),
                                 REACH_TEXT_WEIGHT_NORMAL, REACH_TEXT_ALIGNMENT_CENTER,
                                 input->theme->settings_secondary_text, 1);
        return;
    }

    static const uint16_t *section_titles[] = {(const uint16_t *)u"Paired devices",
                                               (const uint16_t *)u"Nearby devices"};
    reach_render_command_buffer_set_scissor(commands, layout->bluetooth_viewport);
    for (size_t index = 0; index < layout->bluetooth_section_count; ++index)
    {
        reach_settings_push_text(commands, layout->bluetooth_section_titles[index],
                                 section_titles[layout->bluetooth_section_ids[index]],
                                 reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL),
                                 REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
                                 input->theme->settings_secondary_text, 1);
    }
    for (size_t row_index = 0; row_index < layout->bluetooth_row_count; ++row_index)
    {
        reach_settings_bluetooth_render_row(input, commands, row_index, accent);
    }
    reach_render_command_buffer_clear_scissor(commands);

    if (layout->bluetooth_scrollbar_thumb.height > 0.0f)
    {
        reach_rect_f32 origin = {0.0f, 0.0f, 0.0f, 0.0f};
        reach_scrollbar_build_render_commands(layout->bluetooth_scrollbar_track,
                                              layout->bluetooth_scrollbar_thumb, origin,
                                              input->theme->settings_scrollbar_track,
                                              input->theme->settings_scrollbar_thumb, commands);
    }
}
