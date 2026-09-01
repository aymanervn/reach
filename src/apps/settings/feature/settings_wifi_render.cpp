#include "reach/core/render_commands.h"
#include "reach/core/typography.h"
#include "reach/features/common/level_presentation.h"
#include "reach/features/common/loader_render.h"
#include "reach/features/common/scrollbar_render.h"
#include "reach/features/common/section_reveal.h"
#include "reach/features/common/ui_controls.h"

#include "settings_pages_internal.h"
#include "settings_render_internal.h"

static const uint16_t *reach_settings_wifi_row_detail(const reach_settings_model *model,
                                                      const reach_wifi_network *network)
{
    if (network->connected)
    {
        return (const uint16_t *)u"Connected";
    }
    if (model->wifi_view == REACH_SETTINGS_WIFI_VIEW_KNOWN && !network->in_range)
    {
        return (const uint16_t *)u"Not in range";
    }
    if (network->security == REACH_WIFI_SECURITY_ENTERPRISE)
    {
        return (const uint16_t *)u"Enterprise - use Windows settings";
    }
    if (network->saved)
    {
        return (const uint16_t *)u"Saved";
    }
    return network->security == REACH_WIFI_SECURITY_OPEN ? (const uint16_t *)u"Open"
                                                         : (const uint16_t *)u"Secured";
}

static void reach_settings_wifi_render_key_field(const reach_settings_render_input *input,
                                                 reach_render_command_buffer *commands,
                                                 reach_rect_f32 field, const reach_text_edit *edit,
                                                 int32_t focused, const uint16_t *placeholder,
                                                 reach_color accent)
{
    const reach_settings_model *model = input->model;
    reach_ui_selection_item_style style = reach_settings_pill_style(input, accent);
    style.background = input->theme->settings_input_background;
    style.text_size = reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL);
    reach_ui_selection_item_backdrop_render(commands, field, &style, focused ? 1.0f : 0.0f);

    uint16_t masked[REACH_TEXT_EDIT_CAPACITY] = {};
    const uint16_t *text = edit->text;
    if (!model->wifi_show_key)
    {
        for (size_t index = 0; index < edit->length && index + 1 < REACH_TEXT_EDIT_CAPACITY;
             ++index)
        {
            masked[index] = 0x2022;
        }
        text = masked;
    }

    reach_ui_textbox_state state = {};
    state.text = text;
    state.placeholder = placeholder;
    state.text_alignment = input->text_alignment_leading;
    state.caret_index = edit->caret;
    state.caret_visible = focused && model->wifi_caret_visible;
    reach_text_edit_selection_range(edit, &state.selection_start, &state.selection_end);
    if (!focused)
    {
        state.selection_start = 0;
        state.selection_end = 0;
    }
    state.text_color = input->theme->settings_text;
    state.placeholder_color =
        reach_settings_color_with_alpha(input->theme->settings_secondary_text, 0.55f);
    state.selection_color = reach_settings_color_with_alpha(accent, 0.30f);
    float inset = reach_settings_scale(input, 13.0f);
    reach_rect_f32 text_rect = {field.x + inset, field.y, field.width - inset * 2.0f, field.height};
    reach_ui_textbox_render(commands, text_rect, &style, focused ? 1.0f : 0.0f, &state);
}

static void reach_settings_wifi_render_add_row(const reach_settings_render_input *input,
                                               reach_render_command_buffer *commands,
                                               reach_color accent)
{
    const reach_settings_model *model = input->model;
    const reach_settings_layout *layout = input->layout;
    const reach_rect_f32 row = layout->wifi_add_row;
    if (row.width <= 0.0f)
    {
        return;
    }

    reach_settings_push_rect(commands, row, reach_settings_scale(input, input->theme->radius_small),
                             input->theme->settings_card_background);

    float text_x = row.x + reach_settings_scale(input, 16.0f);
    reach_settings_push_text(commands,
                             {text_x, row.y + reach_settings_scale(input, 14.0f),
                              row.width - (text_x - row.x) - reach_settings_scale(input, 16.0f),
                              reach_settings_scale(input, 18.0f)},
                             (const uint16_t *)u"Add a hidden network",
                             reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM),
                             REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
                             input->theme->settings_text, 1);

    if (layout->wifi_add_name_field.width <= 0.0f)
    {
        return;
    }

    reach_ui_selection_item_style name_style = reach_settings_pill_style(input, accent);
    name_style.background = input->theme->settings_input_background;
    name_style.text_size = reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL);
    int32_t name_focused = model->wifi_focused_field == REACH_SETTINGS_WIFI_FIELD_ADD_NAME;
    reach_ui_selection_item_backdrop_render(commands, layout->wifi_add_name_field, &name_style,
                                            name_focused ? 1.0f : 0.0f);
    reach_ui_textbox_state name_state = {};
    name_state.text = model->wifi_add_name_edit.text;
    name_state.placeholder = (const uint16_t *)u"Network name";
    name_state.text_alignment = input->text_alignment_leading;
    name_state.caret_index = model->wifi_add_name_edit.caret;
    name_state.caret_visible = name_focused && model->wifi_caret_visible;
    reach_text_edit_selection_range(&model->wifi_add_name_edit, &name_state.selection_start,
                                    &name_state.selection_end);
    if (!name_focused)
    {
        name_state.selection_start = 0;
        name_state.selection_end = 0;
    }
    name_state.text_color = input->theme->settings_text;
    name_state.placeholder_color =
        reach_settings_color_with_alpha(input->theme->settings_secondary_text, 0.55f);
    name_state.selection_color = reach_settings_color_with_alpha(accent, 0.30f);
    float name_inset = reach_settings_scale(input, 13.0f);
    reach_rect_f32 name_rect = {
        layout->wifi_add_name_field.x + name_inset, layout->wifi_add_name_field.y,
        layout->wifi_add_name_field.width - name_inset * 2.0f, layout->wifi_add_name_field.height};
    reach_ui_textbox_render(commands, name_rect, &name_style, name_focused ? 1.0f : 0.0f,
                            &name_state);

    reach_ui_selection_item_style option_style = reach_settings_pill_style(input, accent);
    for (size_t option = 0; option < REACH_SETTINGS_WIFI_SECURITY_OPTION_COUNT; ++option)
    {
        float selected =
            model->wifi_add_security == reach_settings_wifi_security_option(option) ? 1.0f : 0.0f;
        reach_ui_selection_item_render(commands, layout->wifi_add_security_options[option],
                                       reach_settings_wifi_security_option_label(option),
                                       &option_style, selected);
    }

    if (layout->wifi_add_key_field.width > 0.0f)
    {
        reach_settings_wifi_render_key_field(
            input, commands, layout->wifi_add_key_field, &model->wifi_add_key_edit,
            model->wifi_focused_field == REACH_SETTINGS_WIFI_FIELD_ADD_KEY,
            (const uint16_t *)u"Password", accent);
        reach_ui_button_style show_style =
            reach_settings_button_style(input, input->theme->settings_input_background);
        show_style.text_size = reach_settings_scale(input, REACH_TEXT_SIZE_SMALL);
        reach_ui_button_render(
            commands, layout->wifi_add_show_button,
            model->wifi_show_key ? (const uint16_t *)u"Hide" : (const uint16_t *)u"Show",
            &show_style, 1,
            reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_WIFI_ADD_SHOW_KEY));
    }

    reach_ui_toggle_style toggle_style = {};
    toggle_style.track_off = input->theme->settings_toggle_track_off;
    toggle_style.track_on = reach_settings_color_with_alpha(accent, 0.85f);
    toggle_style.knob = input->theme->settings_toggle_knob;
    reach_ui_toggle_render(commands, layout->wifi_add_auto_toggle, &toggle_style,
                           model->wifi_connect_automatically ? 1.0f : 0.0f);
    reach_settings_push_text(commands,
                             {layout->wifi_add_auto_toggle.x + layout->wifi_add_auto_toggle.width +
                                  reach_settings_scale(input, 10.0f),
                              layout->wifi_add_auto_toggle.y, reach_settings_scale(input, 190.0f),
                              layout->wifi_add_auto_toggle.height},
                             (const uint16_t *)u"Connect automatically",
                             reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL),
                             REACH_TEXT_WEIGHT_NORMAL, input->text_alignment_leading,
                             input->theme->settings_secondary_text, 1);

    reach_ui_button_style submit_style =
        reach_settings_button_style(input, input->theme->settings_button_primary);
    reach_ui_button_render(
        commands, layout->wifi_add_submit_button, (const uint16_t *)u"Add", &submit_style,
        reach_settings_model_wifi_add_ready(model),
        reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_WIFI_ADD_SUBMIT));
}

static void reach_settings_wifi_render_row(const reach_settings_render_input *input,
                                           reach_render_command_buffer *commands, size_t row_index,
                                           reach_color accent)
{
    const reach_settings_model *model = input->model;
    const reach_settings_layout *layout = input->layout;
    size_t index = layout->wifi_row_indices[row_index];
    if (index >= model->wifi_networks.count)
    {
        return;
    }
    const reach_wifi_network *network = &model->wifi_networks.networks[index];
    const reach_rect_f32 row = layout->wifi_rows[row_index];

    reach_color background =
        network->connected
            ? reach_settings_color_with_alpha(accent, input->theme->accent_tint_alpha)
            : input->theme->settings_card_background;
    reach_settings_push_rect(commands, row, reach_settings_scale(input, input->theme->radius_small),
                             background);

    float icon_size = reach_settings_scale(input, 22.0f);
    float header_height = reach_settings_scale(input, 58.0f);
    reach_rect_f32 icon = {row.x + reach_settings_scale(input, 15.0f),
                           row.y + (header_height - icon_size) * 0.5f, icon_size, icon_size};
    reach_color icon_color =
        network->in_range
            ? input->theme->settings_secondary_text
            : reach_settings_color_with_alpha(input->theme->settings_secondary_text, 0.6f);
    reach_settings_push_icon(commands, icon, icon_color,
                             reach_wifi_signal_icon(network->signal_strength), 0.0f);

    float text_x = icon.x + icon_size + reach_settings_scale(input, 14.0f);
    float text_right = row.x + row.width - reach_settings_scale(input, 16.0f);
    float lock_size = reach_settings_scale(input, 12.0f);
    if (network->security != REACH_WIFI_SECURITY_OPEN)
    {
        reach_rect_f32 lock = {text_right - lock_size, row.y + reach_settings_scale(input, 21.0f),
                               lock_size, lock_size};
        reach_settings_push_icon(commands, lock, input->theme->settings_secondary_text,
                                 REACH_VECTOR_ICON_LOCK, 0.0f);
        text_right -= lock_size + reach_settings_scale(input, 10.0f);
    }

    reach_settings_push_text(commands,
                             {text_x, row.y + reach_settings_scale(input, 12.0f),
                              text_right - text_x, reach_settings_scale(input, 18.0f)},
                             network->ssid, reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM),
                             REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
                             input->theme->settings_text, 1);

    const uint16_t *detail = reach_settings_wifi_row_detail(model, network);
    reach_color detail_color = input->theme->settings_secondary_text;
    if (model->wifi_expanded_row >= 0 && (size_t)model->wifi_expanded_row == index &&
        model->wifi_status != REACH_SETTINGS_WIFI_STATUS_IDLE &&
        model->wifi_status != REACH_SETTINGS_WIFI_STATUS_SCANNING)
    {
        detail = reach_settings_wifi_status_message(model->wifi_status);
        detail_color = model->wifi_status == REACH_SETTINGS_WIFI_STATUS_CONNECTED
                           ? input->theme->settings_status_success
                       : model->wifi_status == REACH_SETTINGS_WIFI_STATUS_CONNECTING
                           ? input->theme->settings_secondary_text
                           : input->theme->settings_status_error;
    }
    reach_settings_push_text(commands,
                             {text_x, row.y + reach_settings_scale(input, 33.0f),
                              text_right - text_x, reach_settings_scale(input, 15.0f)},
                             detail, reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL),
                             REACH_TEXT_WEIGHT_NORMAL, input->text_alignment_leading, detail_color,
                             1);

    if (layout->wifi_key_field.width > 0.0f && reach_settings_wifi_row_needs_key_form(model, index))
    {
        reach_settings_wifi_render_key_field(
            input, commands, layout->wifi_key_field, &model->wifi_key_edit,
            model->wifi_focused_field == REACH_SETTINGS_WIFI_FIELD_KEY,
            (const uint16_t *)u"Password", accent);
        reach_ui_button_style show_style =
            reach_settings_button_style(input, input->theme->settings_input_background);
        show_style.text_size = reach_settings_scale(input, REACH_TEXT_SIZE_SMALL);
        reach_ui_button_render(
            commands, layout->wifi_show_button,
            model->wifi_show_key ? (const uint16_t *)u"Hide" : (const uint16_t *)u"Show",
            &show_style, 1,
            reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_WIFI_SHOW_KEY));

        reach_ui_toggle_style toggle_style = {};
        toggle_style.track_off = input->theme->settings_toggle_track_off;
        toggle_style.track_on = reach_settings_color_with_alpha(accent, 0.85f);
        toggle_style.knob = input->theme->settings_toggle_knob;
        reach_ui_toggle_render(commands, layout->wifi_auto_toggle, &toggle_style,
                               model->wifi_connect_automatically ? 1.0f : 0.0f);
        reach_settings_push_text(commands,
                                 {layout->wifi_auto_toggle.x + layout->wifi_auto_toggle.width +
                                      reach_settings_scale(input, 10.0f),
                                  layout->wifi_auto_toggle.y, reach_settings_scale(input, 190.0f),
                                  layout->wifi_auto_toggle.height},
                                 (const uint16_t *)u"Connect automatically",
                                 reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL),
                                 REACH_TEXT_WEIGHT_NORMAL, input->text_alignment_leading,
                                 input->theme->settings_secondary_text, 1);
    }

    if (layout->wifi_connect_button.width > 0.0f)
    {
        reach_ui_button_style connect_style =
            reach_settings_button_style(input, input->theme->settings_button_primary);
        reach_ui_button_render(
            commands, layout->wifi_connect_button, (const uint16_t *)u"Connect", &connect_style,
            reach_settings_model_wifi_connect_ready(model) &&
                !reach_settings_model_wifi_busy(model),
            reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_WIFI_CONNECT));
    }
    if (layout->wifi_disconnect_button.width > 0.0f)
    {
        reach_ui_button_style disconnect_style =
            reach_settings_button_style(input, input->theme->settings_card_background);
        reach_ui_button_render(
            commands, layout->wifi_disconnect_button, (const uint16_t *)u"Disconnect",
            &disconnect_style, !reach_settings_model_wifi_busy(model),
            reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_WIFI_DISCONNECT));
    }
    if (layout->wifi_forget_button.width > 0.0f)
    {
        reach_ui_button_style forget_style =
            reach_settings_button_style(input, input->theme->settings_button_danger);
        reach_ui_button_render(
            commands, layout->wifi_forget_button, (const uint16_t *)u"Forget", &forget_style, 1,
            reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_WIFI_FORGET));
    }
}

void reach_settings_render_wifi_page(const reach_settings_render_input *input,
                                     reach_render_command_buffer *commands)
{
    const reach_settings_model *model = input->model;
    const reach_settings_layout *layout = input->layout;
    reach_color accent = reach_theme_accent_color(input->theme, REACH_THEME_ACCENT_GREEN);

    if (layout->wifi_back_button.width > 0.0f)
    {
        reach_ui_button_style back_style =
            reach_settings_button_style(input, input->theme->settings_button_success);
        reach_ui_button_render(
            commands, layout->wifi_back_button, (const uint16_t *)u"Return", &back_style, 1,
            reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_WIFI_BACK));
    }

    if (layout->wifi_radio_card.width > 0.0f)
    {
        reach_settings_push_rect(commands, layout->wifi_radio_card,
                                 reach_settings_scale(input, input->theme->radius_small),
                                 input->theme->settings_card_background);
        reach_settings_push_rect(commands, layout->wifi_radio_icon,
                                 reach_settings_scale(input, input->theme->radius_small),
                                 input->theme->settings_icon_box_background);
        reach_settings_push_icon(
            commands, layout->wifi_radio_icon, input->theme->settings_secondary_text,
            model->wifi_radio == REACH_WIFI_RADIO_ON ? REACH_VECTOR_ICON_WIFI_HIGH
                                                     : REACH_VECTOR_ICON_NO_INTERNET,
            0.24f);
        reach_settings_push_text(commands, layout->wifi_radio_title, (const uint16_t *)u"Wi-Fi",
                                 reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM),
                                 REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
                                 input->theme->settings_text, 1);

        const uint16_t *subtitle = (const uint16_t *)u"Wi-Fi is off";
        if (model->wifi_radio == REACH_WIFI_RADIO_UNAVAILABLE)
        {
            subtitle = (const uint16_t *)u"No Wi-Fi adapter found";
        }
        else if (model->wifi_radio == REACH_WIFI_RADIO_ON)
        {
            subtitle = (const uint16_t *)u"Choose a network to connect to";
            for (size_t index = 0; index < model->wifi_networks.count; ++index)
            {
                if (model->wifi_networks.networks[index].connected)
                {
                    subtitle = model->wifi_networks.networks[index].ssid;
                    break;
                }
            }
        }
        reach_settings_push_text(commands, layout->wifi_radio_subtitle, subtitle,
                                 reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL),
                                 REACH_TEXT_WEIGHT_NORMAL, input->text_alignment_leading,
                                 input->theme->settings_secondary_text, 1);

        reach_ui_toggle_style toggle_style = {};
        toggle_style.track_off = input->theme->settings_toggle_track_off;
        toggle_style.track_on = reach_settings_color_with_alpha(accent, 0.85f);
        toggle_style.knob = input->theme->settings_toggle_knob;
        reach_ui_toggle_render(commands, layout->wifi_radio_toggle, &toggle_style,
                               reach_animation_manager_value(&model->wifi_radio_animation, 0));

        int32_t enabled = model->wifi_radio == REACH_WIFI_RADIO_ON;
        reach_ui_button_style scan_style =
            reach_settings_button_style(input, input->theme->settings_button_success);
        reach_ui_button_render(
            commands, layout->wifi_scan_button,
            model->wifi_status == REACH_SETTINGS_WIFI_STATUS_SCANNING
                ? (const uint16_t *)u"Scanning..."
                : (const uint16_t *)u"Scan",
            &scan_style, enabled && !reach_settings_model_wifi_busy(model),
            reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_WIFI_SCAN));
        reach_ui_button_style add_style = reach_settings_muted_button_style(input);
        reach_ui_button_render(
            commands, layout->wifi_add_button, (const uint16_t *)u"Add network", &add_style,
            enabled, reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_WIFI_ADD));
        reach_ui_button_style known_style = reach_settings_muted_button_style(input);
        reach_ui_button_render(
            commands, layout->wifi_known_button, (const uint16_t *)u"Known networks", &known_style,
            1, reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_WIFI_KNOWN));
    }

    if (layout->wifi_loader_bar.width > 0.0f)
    {
        reach_rect_f32 bar = reach_loader_bar_rect(&model->wifi_loader, layout->wifi_loader_bar);
        reach_rect_f32 origin = {0.0f, 0.0f, 0.0f, 0.0f};
        (void)reach_loader_build_render_commands(layout->wifi_loader_bar, bar, origin, accent,
                                                 commands);
    }

    if (model->wifi_radio != REACH_WIFI_RADIO_ON)
    {
        reach_settings_push_text(commands, layout->wifi_viewport,
                                 model->wifi_radio == REACH_WIFI_RADIO_UNAVAILABLE
                                     ? (const uint16_t *)u"This device has no Wi-Fi adapter"
                                     : (const uint16_t *)u"Turn Wi-Fi on to see networks",
                                 reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM),
                                 REACH_TEXT_WEIGHT_NORMAL, REACH_TEXT_ALIGNMENT_CENTER,
                                 input->theme->settings_secondary_text, 1);
        return;
    }

    if (layout->wifi_row_count == 0 && layout->wifi_add_row.width <= 0.0f)
    {
        reach_settings_push_text(
            commands, layout->wifi_viewport,
            model->wifi_view == REACH_SETTINGS_WIFI_VIEW_KNOWN
                ? (const uint16_t *)u"No saved networks"
                : (model->wifi_loaded ? (const uint16_t *)u"No networks found"
                                      : (const uint16_t *)u"Reading networks..."),
            reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM), REACH_TEXT_WEIGHT_NORMAL,
            REACH_TEXT_ALIGNMENT_CENTER, input->theme->settings_secondary_text, 1);
        return;
    }

    reach_render_command_buffer_set_scissor(commands, layout->wifi_viewport);
    if (layout->wifi_add_clip.height > 0.0f)
    {
        reach_render_command_buffer_set_scissor(commands, layout->wifi_add_clip);
        size_t first_command = commands->count;
        reach_settings_wifi_render_add_row(input, commands, accent);
        float progress =
            reach_animation_manager_value(&model->wifi_row_animations, REACH_WIFI_MAX_NETWORKS);
        reach_section_reveal_apply(commands, first_command, progress,
                                   reach_settings_scale(input, 4.0f));
        reach_render_command_buffer_set_scissor(commands, layout->wifi_viewport);
    }
    for (size_t row_index = 0; row_index < layout->wifi_row_count; ++row_index)
    {
        reach_settings_wifi_render_row(input, commands, row_index, accent);
    }
    reach_render_command_buffer_clear_scissor(commands);

    if (layout->wifi_scrollbar_thumb.height > 0.0f)
    {
        reach_rect_f32 origin = {0.0f, 0.0f, 0.0f, 0.0f};
        reach_scrollbar_build_render_commands(layout->wifi_scrollbar_track,
                                              layout->wifi_scrollbar_thumb, origin,
                                              input->theme->settings_scrollbar_track,
                                              input->theme->settings_scrollbar_thumb, commands);
    }
}
