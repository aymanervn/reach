#include "settings_pages_internal.h"

const uint16_t *reach_settings_bluetooth_page_title(void)
{
    return (const uint16_t *)u"Bluetooth";
}

const uint16_t *reach_settings_bluetooth_page_placeholder(void)
{
    return (const uint16_t *)u"";
}

int32_t reach_settings_bluetooth_row_is_pairing(const reach_settings_model *model, size_t index)
{
    return model != nullptr && reach_bluetooth_session_device_is_pairing(&model->bluetooth, index);
}

static void reach_settings_bluetooth_sync_row_motions(reach_settings_model *model)
{
    for (size_t index = 0; index < REACH_BLUETOOTH_MAX_DEVICES; ++index)
    {
        float target = model->bluetooth.selected >= 0 && (size_t)model->bluetooth.selected == index
                           ? 1.0f
                           : 0.0f;
        if (model->bluetooth_row_motions[index].to != target)
            reach_settings_animate(&model->bluetooth_row_motions[index], target,
                                   REACH_MOTION_PRESET_SMOOTH);
    }
}

void reach_settings_model_apply_bluetooth_snapshot(reach_settings_model *model,
                                                   const reach_bluetooth_snapshot *snapshot)
{
    if (model == nullptr || snapshot == nullptr)
    {
        return;
    }
    int32_t starts_scanning =
        snapshot->pair_result == REACH_BLUETOOTH_PAIR_RESULT_NONE && !snapshot->pairing.active &&
        snapshot->completed_command == REACH_BLUETOOTH_SERVICE_COMMAND_SET_SCAN;
    reach_bluetooth_session_apply_snapshot(&model->bluetooth, snapshot);
    for (size_t index = 0; index < REACH_BLUETOOTH_MAX_DEVICES; ++index)
    {
        model->bluetooth_icons[index] = 0;
    }
    if (starts_scanning && model->bluetooth.status == REACH_BLUETOOTH_SESSION_STATUS_SCANNING)
    {
        reach_loader_model_reset(&model->bluetooth_loader);
    }
    reach_settings_bluetooth_sync_row_motions(model);
}

void reach_settings_model_set_bluetooth_icon(reach_settings_model *model, size_t index,
                                             uint64_t icon_id)
{
    if (model != nullptr && index < REACH_BLUETOOTH_MAX_DEVICES)
    {
        model->bluetooth_icons[index] = icon_id;
    }
}

void reach_settings_model_set_bluetooth_radio(reach_settings_model *model,
                                              reach_bluetooth_state state)
{
    if (model == nullptr)
    {
        return;
    }
    reach_bluetooth_session_set_radio(&model->bluetooth, state);
    float target = state.available && state.enabled ? 1.0f : 0.0f;
    if (!model->bluetooth.loaded)
    {
        reach_motion_set(&model->bluetooth_radio_toggle, target);
    }
    else if (model->bluetooth_radio_toggle.to != target)
    {
        reach_settings_animate(&model->bluetooth_radio_toggle, target, REACH_MOTION_PRESET_BOUNCY);
    }
}

int32_t reach_settings_model_toggle_bluetooth_radio(reach_settings_model *model)
{
    if (model == nullptr || !reach_bluetooth_session_toggle_radio(&model->bluetooth))
    {
        return 0;
    }
    reach_settings_animate(&model->bluetooth_radio_toggle,
                           model->bluetooth.radio.enabled ? 1.0f : 0.0f,
                           REACH_MOTION_PRESET_BOUNCY);
    return 1;
}

void reach_settings_model_set_bluetooth_status(reach_settings_model *model,
                                               reach_bluetooth_session_status status,
                                               const uint16_t *device_id)
{
    if (model == nullptr)
    {
        return;
    }
    reach_bluetooth_session_set_status(&model->bluetooth, status, device_id);
    if (status == REACH_BLUETOOTH_SESSION_STATUS_SCANNING)
    {
        reach_loader_model_reset(&model->bluetooth_loader);
    }
}

void reach_settings_model_bluetooth_expand_row(reach_settings_model *model, int32_t row)
{
    if (model == nullptr)
    {
        return;
    }
    reach_bluetooth_session_select(&model->bluetooth, row);
    reach_settings_bluetooth_sync_row_motions(model);
}

void reach_settings_model_scroll_bluetooth(reach_settings_model *model, float delta)
{
    if (model != nullptr)
    {
        (void)reach_scroll_view_scroll_by(&model->bluetooth_scroll, delta,
                                          reach_motion_now_seconds());
    }
}

int32_t reach_settings_model_sample_bluetooth_animations_at(reach_settings_model *model,
                                                            double now_seconds)
{
    return model != nullptr &&
           reach_settings_advance_motions(model->bluetooth_row_motions, REACH_BLUETOOTH_MAX_DEVICES,
                                          now_seconds);
}

int32_t reach_settings_model_bluetooth_animations_active(const reach_settings_model *model)
{
    return model != nullptr && reach_settings_motions_pending(model->bluetooth_row_motions,
                                                              REACH_BLUETOOTH_MAX_DEVICES);
}
