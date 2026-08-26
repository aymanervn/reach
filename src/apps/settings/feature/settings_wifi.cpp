#include "settings_pages_internal.h"

#include "reach/support/util.h"

#include <string.h>

#define REACH_SETTINGS_WIFI_ROW_ANIMATION_SECONDS 0.18
#define REACH_SETTINGS_WIFI_CARET_PERIOD 1.06

const uint16_t *reach_settings_wifi_page_title(void)
{
    return (const uint16_t *)u"Wi-Fi";
}

const uint16_t *reach_settings_wifi_page_placeholder(void)
{
    return (const uint16_t *)u"";
}

static float reach_settings_wifi_track_target(int32_t expanded_row, size_t track)
{
    if (track == REACH_WIFI_MAX_NETWORKS)
    {
        return expanded_row == REACH_SETTINGS_WIFI_ROW_ADD ? 1.0f : 0.0f;
    }
    return expanded_row >= 0 && (size_t)expanded_row == track ? 1.0f : 0.0f;
}

static reach_text_edit *reach_settings_wifi_focused_edit(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return nullptr;
    }
    switch (model->wifi_focused_field)
    {
    case REACH_SETTINGS_WIFI_FIELD_KEY:
        return &model->wifi_key_edit;
    case REACH_SETTINGS_WIFI_FIELD_ADD_NAME:
        return &model->wifi_add_name_edit;
    case REACH_SETTINGS_WIFI_FIELD_ADD_KEY:
        return &model->wifi_add_key_edit;
    default:
        return nullptr;
    }
}

int32_t reach_settings_model_wifi_row_visible(const reach_settings_model *model, size_t index)
{
    if (model == nullptr || index >= model->wifi_networks.count)
    {
        return 0;
    }
    if (model->wifi_view == REACH_SETTINGS_WIFI_VIEW_KNOWN)
    {
        return model->wifi_networks.networks[index].saved;
    }
    return model->wifi_networks.networks[index].in_range;
}

void reach_settings_model_set_wifi_radio(reach_settings_model *model, reach_wifi_radio_state radio)
{
    if (model == nullptr)
    {
        return;
    }
    model->wifi_radio = radio;
    float target = radio == REACH_WIFI_RADIO_ON ? 1.0f : 0.0f;
    if (!model->wifi_loaded)
    {
        reach_animation_manager_set(&model->wifi_radio_animation, 0, target);
    }
    else if (reach_animation_manager_target(&model->wifi_radio_animation, 0) != target)
    {
        float current = reach_animation_manager_value(&model->wifi_radio_animation, 0);
        reach_animation_manager_start(&model->wifi_radio_animation, 0, current, target, 0.18,
                                      REACH_EASING_EASE_OUT);
    }
}

int32_t reach_settings_model_toggle_wifi_radio(reach_settings_model *model)
{
    if (model == nullptr || model->wifi_radio == REACH_WIFI_RADIO_UNAVAILABLE)
    {
        return 0;
    }
    model->wifi_radio = model->wifi_radio == REACH_WIFI_RADIO_ON ? REACH_WIFI_RADIO_OFF
                                                                   : REACH_WIFI_RADIO_ON;
    float current = reach_animation_manager_value(&model->wifi_radio_animation, 0);
    reach_animation_manager_start(&model->wifi_radio_animation, 0, current,
                                  model->wifi_radio == REACH_WIFI_RADIO_ON ? 1.0f : 0.0f, 0.18,
                                  REACH_EASING_EASE_OUT);
    return 1;
}

void reach_settings_model_apply_wifi(reach_settings_model *model, reach_wifi_radio_state radio,
                                     const reach_wifi_network_list *networks)
{
    if (model == nullptr)
    {
        return;
    }

    uint16_t expanded_ssid[REACH_WIFI_SSID_CAPACITY] = {};
    if (model->wifi_expanded_row >= 0 &&
        (size_t)model->wifi_expanded_row < model->wifi_networks.count)
    {
        reach_copy_utf16(expanded_ssid, REACH_WIFI_SSID_CAPACITY,
                         model->wifi_networks.networks[model->wifi_expanded_row].ssid);
    }

    reach_settings_model_set_wifi_radio(model, radio);
    model->wifi_networks = networks != nullptr ? *networks : reach_wifi_network_list{};
    model->wifi_loaded = 1;

    /* The list is re-sorted on every publish, so the open row follows its network, not its slot. */
    if (expanded_ssid[0] != 0)
    {
        size_t found = reach_wifi_network_list_find(&model->wifi_networks, expanded_ssid);
        model->wifi_expanded_row =
            found < model->wifi_networks.count ? (int32_t)found : REACH_SETTINGS_WIFI_ROW_NONE;
    }
    else if (model->wifi_expanded_row >= 0)
    {
        model->wifi_expanded_row = REACH_SETTINGS_WIFI_ROW_NONE;
    }

    if (model->wifi_expanded_row == REACH_SETTINGS_WIFI_ROW_NONE)
    {
        reach_settings_model_wifi_blur(model);
    }

    for (size_t index = 0; index <= REACH_WIFI_MAX_NETWORKS; ++index)
    {
        float target = reach_settings_wifi_track_target(model->wifi_expanded_row, index);
        if (reach_animation_manager_target(&model->wifi_row_animations, index) != target)
        {
            reach_animation_manager_animate_to(&model->wifi_row_animations, index, target,
                                               REACH_SETTINGS_WIFI_ROW_ANIMATION_SECONDS,
                                               REACH_EASING_EASE_IN_OUT);
        }
    }
}

void reach_settings_model_set_wifi_view(reach_settings_model *model,
                                        reach_settings_wifi_view view)
{
    if (model == nullptr || model->wifi_view == view)
    {
        return;
    }
    model->wifi_view = view;
    reach_settings_model_wifi_expand_row(model, REACH_SETTINGS_WIFI_ROW_NONE);
    reach_scrollbar_model_init(&model->wifi_scrollbar, REACH_SCROLLBAR_DRAG_FREE, 0.0f);
    reach_animation_manager_start(&model->wifi_view_animation, 0, 0.0f, 1.0f, 0.16,
                                  REACH_EASING_EASE_OUT);
}

void reach_settings_model_set_wifi_status(reach_settings_model *model, int32_t status,
                                          const uint16_t *ssid)
{
    if (model == nullptr)
    {
        return;
    }
    model->wifi_status = status;
    model->wifi_status_ssid[0] = 0;
    if (ssid != nullptr)
    {
        reach_copy_utf16(model->wifi_status_ssid, REACH_WIFI_SSID_CAPACITY, ssid);
    }
    if (status == REACH_SETTINGS_WIFI_STATUS_SCANNING)
    {
        reach_loader_model_reset(&model->wifi_loader);
    }
}

const uint16_t *reach_settings_wifi_status_message(int32_t status)
{
    switch (status)
    {
    case REACH_SETTINGS_WIFI_STATUS_SCANNING:
        return (const uint16_t *)u"Searching for networks...";
    case REACH_SETTINGS_WIFI_STATUS_CONNECTING:
        return (const uint16_t *)u"Connecting...";
    case REACH_SETTINGS_WIFI_STATUS_CONNECTED:
        return (const uint16_t *)u"Connected";
    case REACH_SETTINGS_WIFI_STATUS_INVALID_KEY:
        return (const uint16_t *)u"That password did not work";
    case REACH_SETTINGS_WIFI_STATUS_NOT_FOUND:
        return (const uint16_t *)u"That network is out of range";
    case REACH_SETTINGS_WIFI_STATUS_FAILED:
        return (const uint16_t *)u"Could not connect to this network";
    case REACH_SETTINGS_WIFI_STATUS_SCAN_FAILED:
        return (const uint16_t *)u"Could not search for networks";
    case REACH_SETTINGS_WIFI_STATUS_FORGET_FAILED:
        return (const uint16_t *)u"Could not forget this network";
    default:
        return (const uint16_t *)u"";
    }
}

int32_t reach_settings_model_wifi_busy(const reach_settings_model *model)
{
    return model != nullptr && (model->wifi_status == REACH_SETTINGS_WIFI_STATUS_SCANNING ||
                                model->wifi_status == REACH_SETTINGS_WIFI_STATUS_CONNECTING);
}

void reach_settings_model_wifi_expand_row(reach_settings_model *model, int32_t row)
{
    if (model == nullptr)
    {
        return;
    }
    if (model->wifi_expanded_row == row)
    {
        row = REACH_SETTINGS_WIFI_ROW_NONE;
    }
    model->wifi_expanded_row = row;
    reach_settings_model_wifi_blur(model);
    reach_settings_model_wifi_clear_secrets(model);
    model->wifi_show_key = 0;
    model->wifi_connect_automatically = 1;
    if (model->wifi_status != REACH_SETTINGS_WIFI_STATUS_SCANNING)
    {
        reach_settings_model_set_wifi_status(model, REACH_SETTINGS_WIFI_STATUS_IDLE, nullptr);
    }

    for (size_t index = 0; index <= REACH_WIFI_MAX_NETWORKS; ++index)
    {
        float target = reach_settings_wifi_track_target(row, index);
        if (reach_animation_manager_target(&model->wifi_row_animations, index) != target)
        {
            reach_animation_manager_animate_to(&model->wifi_row_animations, index, target,
                                               REACH_SETTINGS_WIFI_ROW_ANIMATION_SECONDS,
                                               REACH_EASING_EASE_IN_OUT);
        }
    }
}

void reach_settings_model_wifi_focus_field(reach_settings_model *model, int32_t field)
{
    if (model == nullptr)
    {
        return;
    }
    model->wifi_focused_field = field;
    model->wifi_caret_visible = 1;
    model->wifi_caret_phase = 0.0;
    reach_text_edit *edit = reach_settings_wifi_focused_edit(model);
    if (edit != nullptr)
    {
        reach_text_edit_select_all(edit);
    }
}

void reach_settings_model_wifi_blur(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return;
    }
    model->wifi_focused_field = REACH_SETTINGS_WIFI_FIELD_NONE;
    model->wifi_caret_visible = 0;
    model->wifi_caret_phase = 0.0;
}

int32_t reach_settings_model_wifi_toggle_show_key(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    model->wifi_show_key = model->wifi_show_key ? 0 : 1;
    return 1;
}

int32_t reach_settings_model_wifi_toggle_auto(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    model->wifi_connect_automatically = model->wifi_connect_automatically ? 0 : 1;
    return 1;
}

reach_wifi_security reach_settings_wifi_security_option(size_t option)
{
    switch (option)
    {
    case 0:
        return REACH_WIFI_SECURITY_OPEN;
    case 1:
        return REACH_WIFI_SECURITY_WPA2_PERSONAL;
    default:
        return REACH_WIFI_SECURITY_WPA3_PERSONAL;
    }
}

const uint16_t *reach_settings_wifi_security_option_label(size_t option)
{
    return reach_wifi_security_label(reach_settings_wifi_security_option(option));
}

void reach_settings_model_wifi_select_security(reach_settings_model *model, size_t option)
{
    if (model == nullptr || option >= REACH_SETTINGS_WIFI_SECURITY_OPTION_COUNT)
    {
        return;
    }
    model->wifi_add_security = reach_settings_wifi_security_option(option);
    if (model->wifi_add_security == REACH_WIFI_SECURITY_OPEN)
    {
        reach_text_edit_clear(&model->wifi_add_key_edit);
        if (model->wifi_focused_field == REACH_SETTINGS_WIFI_FIELD_ADD_KEY)
        {
            reach_settings_model_wifi_blur(model);
        }
    }
}

int32_t reach_settings_model_wifi_insert_char(reach_settings_model *model, uint16_t ch)
{
    reach_text_edit *edit = reach_settings_wifi_focused_edit(model);
    if (edit == nullptr || ch < 0x20)
    {
        return 0;
    }
    if (reach_text_edit_insert_char(edit, ch) == REACH_TEXT_EDIT_EVENT_NONE)
    {
        return 0;
    }
    model->wifi_caret_visible = 1;
    model->wifi_caret_phase = 0.0;
    return 1;
}

int32_t reach_settings_model_wifi_handle_edit_key(reach_settings_model *model,
                                                  reach_text_edit_key key,
                                                  reach_text_edit_modifiers modifiers)
{
    reach_text_edit *edit = reach_settings_wifi_focused_edit(model);
    if (edit == nullptr)
    {
        return 0;
    }
    if (reach_text_edit_handle_key(edit, key, modifiers) == REACH_TEXT_EDIT_EVENT_NONE)
    {
        return 0;
    }
    model->wifi_caret_visible = 1;
    model->wifi_caret_phase = 0.0;
    return 1;
}

int32_t reach_settings_model_wifi_connect_ready(const reach_settings_model *model)
{
    if (model == nullptr || model->wifi_expanded_row < 0 ||
        (size_t)model->wifi_expanded_row >= model->wifi_networks.count)
    {
        return 0;
    }
    const reach_wifi_network *network = &model->wifi_networks.networks[model->wifi_expanded_row];
    if (!reach_wifi_security_is_supported(network->security))
    {
        return 0;
    }
    if (!reach_wifi_security_needs_key(network->security) || network->saved)
    {
        return 1;
    }
    return reach_wifi_key_length_valid(network->security, model->wifi_key_edit.length);
}

int32_t reach_settings_model_wifi_add_ready(const reach_settings_model *model)
{
    if (model == nullptr || model->wifi_add_name_edit.length == 0)
    {
        return 0;
    }
    return reach_wifi_key_length_valid(model->wifi_add_security, model->wifi_add_key_edit.length);
}

int32_t reach_settings_model_wifi_build_connect(const reach_settings_model *model, size_t index,
                                                reach_wifi_connect_request *out_request)
{
    if (model == nullptr || out_request == nullptr || index >= model->wifi_networks.count)
    {
        return 0;
    }
    const reach_wifi_network *network = &model->wifi_networks.networks[index];
    *out_request = {};
    reach_copy_utf16(out_request->ssid, REACH_WIFI_SSID_CAPACITY, network->ssid);
    out_request->security = network->security;
    out_request->connect_automatically = model->wifi_connect_automatically;
    out_request->hidden = network->hidden;
    if (model->wifi_key_edit.length > 0)
    {
        reach_copy_utf16(out_request->key, REACH_WIFI_KEY_CAPACITY, model->wifi_key_edit.text);
    }
    return 1;
}

int32_t reach_settings_model_wifi_build_add(const reach_settings_model *model,
                                            reach_wifi_connect_request *out_request)
{
    if (model == nullptr || out_request == nullptr ||
        !reach_settings_model_wifi_add_ready(model))
    {
        return 0;
    }
    *out_request = {};
    reach_copy_utf16(out_request->ssid, REACH_WIFI_SSID_CAPACITY, model->wifi_add_name_edit.text);
    reach_copy_utf16(out_request->key, REACH_WIFI_KEY_CAPACITY, model->wifi_add_key_edit.text);
    out_request->security = model->wifi_add_security;
    out_request->connect_automatically = model->wifi_connect_automatically;
    out_request->hidden = 1;
    return 1;
}

void reach_settings_model_wifi_clear_secrets(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return;
    }
    memset(model->wifi_key_edit.text, 0, sizeof(model->wifi_key_edit.text));
    reach_text_edit_clear(&model->wifi_key_edit);
    memset(model->wifi_add_key_edit.text, 0, sizeof(model->wifi_add_key_edit.text));
    reach_text_edit_clear(&model->wifi_add_key_edit);
}

void reach_settings_model_scroll_wifi(reach_settings_model *model, float delta)
{
    if (model != nullptr)
    {
        reach_scrollbar_scroll(&model->wifi_scrollbar, delta);
    }
}

int32_t reach_settings_model_wifi_scroll(reach_settings_model *model, double delta_seconds)
{
    return model != nullptr ? reach_scrollbar_update(&model->wifi_scrollbar, delta_seconds) : 0;
}

int32_t reach_settings_model_wifi_loader(reach_settings_model *model, double delta_seconds)
{
    if (model == nullptr || model->wifi_status != REACH_SETTINGS_WIFI_STATUS_SCANNING)
    {
        return 0;
    }
    return reach_loader_update(&model->wifi_loader, delta_seconds);
}

int32_t reach_settings_model_tick_wifi_animations(reach_settings_model *model,
                                                  double delta_seconds)
{
    if (model == nullptr)
    {
        return 0;
    }
    int32_t changed = 0;
    if (reach_animation_manager_any_active(&model->wifi_row_animations))
    {
        reach_animation_manager_tick(&model->wifi_row_animations, delta_seconds);
        changed = 1;
    }
    if (reach_animation_manager_any_active(&model->wifi_view_animation))
    {
        reach_animation_manager_tick(&model->wifi_view_animation, delta_seconds);
        changed = 1;
    }
    if (reach_animation_manager_any_active(&model->wifi_radio_animation))
    {
        reach_animation_manager_tick(&model->wifi_radio_animation, delta_seconds);
        changed = 1;
    }
    return changed;
}

int32_t reach_settings_model_wifi_animations_active(const reach_settings_model *model)
{
    return model != nullptr &&
           (reach_animation_manager_any_active(&model->wifi_row_animations) ||
            reach_animation_manager_any_active(&model->wifi_view_animation) ||
            reach_animation_manager_any_active(&model->wifi_radio_animation));
}

int32_t reach_settings_model_tick_wifi_caret(reach_settings_model *model, double delta_seconds)
{
    if (model == nullptr || model->wifi_focused_field == REACH_SETTINGS_WIFI_FIELD_NONE)
    {
        return 0;
    }
    model->wifi_caret_phase += delta_seconds;
    if (model->wifi_caret_phase < REACH_SETTINGS_WIFI_CARET_PERIOD * 0.5)
    {
        return 0;
    }
    model->wifi_caret_phase = 0.0;
    model->wifi_caret_visible = model->wifi_caret_visible ? 0 : 1;
    return 1;
}
