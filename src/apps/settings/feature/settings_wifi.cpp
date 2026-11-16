#include "settings_pages_internal.h"

#include "reach/support/util.h"

#include <string.h>

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
        return &model->wifi.key_edit;
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
    if (model == nullptr || index >= model->wifi.networks.count)
    {
        return 0;
    }
    if (model->wifi_view == REACH_SETTINGS_WIFI_VIEW_KNOWN)
    {
        return model->wifi.networks.networks[index].saved;
    }
    return model->wifi.networks.networks[index].in_range;
}

static void reach_settings_wifi_sync_row_motions(reach_settings_model *model)
{
    for (size_t index = 0; index <= REACH_WIFI_MAX_NETWORKS; ++index)
    {
        float target = reach_settings_wifi_track_target(model->wifi.selected, index);
        if (model->wifi_row_motions[index].to != target)
            reach_settings_animate(&model->wifi_row_motions[index], target,
                                   REACH_MOTION_PRESET_SMOOTH);
    }
}

int32_t reach_settings_model_toggle_wifi_radio(reach_settings_model *model)
{
    if (model == nullptr || !reach_wifi_session_toggle_radio(&model->wifi))
    {
        return 0;
    }
    reach_settings_animate(&model->wifi_radio_toggle,
                           model->wifi.radio == REACH_WIFI_RADIO_ON ? 1.0f : 0.0f,
                           REACH_MOTION_PRESET_BOUNCY);
    return 1;
}

void reach_settings_model_apply_wifi_snapshot(reach_settings_model *model,
                                              const reach_wifi_snapshot *snapshot)
{
    if (model == nullptr || snapshot == nullptr)
    {
        return;
    }

    int32_t was_loaded = model->wifi.loaded;
    int32_t connected = reach_wifi_session_apply_snapshot(&model->wifi, snapshot);

    float radio_target = model->wifi.radio == REACH_WIFI_RADIO_ON ? 1.0f : 0.0f;
    if (!was_loaded)
    {
        reach_motion_set(&model->wifi_radio_toggle, radio_target);
    }
    else if (model->wifi_radio_toggle.to != radio_target)
    {
        reach_settings_animate(&model->wifi_radio_toggle, radio_target, REACH_MOTION_PRESET_BOUNCY);
    }

    if (connected)
    {
        reach_settings_model_wifi_clear_secrets(model);
        reach_settings_model_wifi_expand_row(model, REACH_WIFI_SESSION_SELECTION_NONE);
    }
    if (model->wifi.selected == REACH_WIFI_SESSION_SELECTION_NONE)
    {
        reach_settings_model_wifi_blur(model);
    }
    reach_settings_wifi_sync_row_motions(model);
}

void reach_settings_model_set_wifi_view(reach_settings_model *model, reach_settings_wifi_view view)
{
    if (model == nullptr || model->wifi_view == view)
    {
        return;
    }
    model->wifi_view = view;
    reach_settings_model_wifi_expand_row(model, REACH_WIFI_SESSION_SELECTION_NONE);
    reach_settings_scroll_init(&model->wifi_scroll);
}

void reach_settings_model_set_wifi_status(reach_settings_model *model,
                                          reach_wifi_session_status status, const uint16_t *ssid)
{
    if (model == nullptr)
    {
        return;
    }
    reach_wifi_session_set_status(&model->wifi, status, ssid);
    if (status == REACH_WIFI_SESSION_STATUS_SCANNING)
    {
        reach_loader_model_reset(&model->wifi_loader);
    }
}

void reach_settings_model_wifi_expand_row(reach_settings_model *model, int32_t row)
{
    if (model == nullptr)
    {
        return;
    }
    reach_wifi_session_select(&model->wifi, row);
    reach_settings_model_wifi_blur(model);
    reach_settings_model_wifi_clear_secrets(model);
    reach_settings_wifi_sync_row_motions(model);
}

void reach_settings_model_wifi_focus_field(reach_settings_model *model, int32_t field)
{
    if (model == nullptr)
    {
        return;
    }
    model->wifi_focused_field = field;
    reach_blink_reset_at(&model->wifi_caret, 0.53, 0.53, reach_motion_now_seconds());
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
    reach_blink_stop(&model->wifi_caret);
}

int32_t reach_settings_model_wifi_toggle_show_key(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    model->wifi.show_key = model->wifi.show_key ? 0 : 1;
    return 1;
}

int32_t reach_settings_model_wifi_toggle_auto(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    model->wifi.connect_automatically = model->wifi.connect_automatically ? 0 : 1;
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
    reach_blink_reset_at(&model->wifi_caret, 0.53, 0.53, reach_motion_now_seconds());
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
    reach_blink_reset_at(&model->wifi_caret, 0.53, 0.53, reach_motion_now_seconds());
    return 1;
}

int32_t reach_settings_model_wifi_add_ready(const reach_settings_model *model)
{
    if (model == nullptr || model->wifi_add_name_edit.length == 0)
    {
        return 0;
    }
    return reach_wifi_key_length_valid(model->wifi_add_security, model->wifi_add_key_edit.length);
}

int32_t reach_settings_model_wifi_build_add(const reach_settings_model *model,
                                            reach_wifi_connect_request *out_request)
{
    if (model == nullptr || out_request == nullptr || !reach_settings_model_wifi_add_ready(model))
    {
        return 0;
    }
    *out_request = {};
    reach_copy_utf16(out_request->ssid, REACH_WIFI_SSID_CAPACITY, model->wifi_add_name_edit.text);
    reach_copy_utf16(out_request->key, REACH_WIFI_KEY_CAPACITY, model->wifi_add_key_edit.text);
    out_request->security = model->wifi_add_security;
    out_request->connect_automatically = model->wifi.connect_automatically;
    out_request->hidden = 1;
    return 1;
}

void reach_settings_model_wifi_clear_secrets(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return;
    }
    reach_wifi_session_clear_secrets(&model->wifi);
    memset(model->wifi_add_key_edit.text, 0, sizeof(model->wifi_add_key_edit.text));
    reach_text_edit_clear(&model->wifi_add_key_edit);
}

void reach_settings_model_scroll_wifi(reach_settings_model *model, float delta)
{
    if (model != nullptr)
    {
        (void)reach_scroll_view_scroll_by(&model->wifi_scroll, delta, reach_motion_now_seconds());
    }
}

int32_t reach_settings_model_sample_wifi_animations_at(reach_settings_model *model,
                                                       double now_seconds)
{
    if (model == nullptr)
    {
        return 0;
    }
    return reach_settings_advance_motions(model->wifi_row_motions, REACH_WIFI_MAX_NETWORKS + 1,
                                          now_seconds);
}

int32_t reach_settings_model_wifi_animations_active(const reach_settings_model *model)
{
    return model != nullptr &&
           reach_settings_motions_pending(model->wifi_row_motions, REACH_WIFI_MAX_NETWORKS + 1);
}
