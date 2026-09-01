#include "settings_pages_internal.h"

#include "reach/support/util.h"

#define REACH_SETTINGS_BLUETOOTH_ROW_ANIMATION_SECONDS 0.18

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
    if (model == nullptr || index >= model->bluetooth_devices.count ||
        !model->bluetooth_pairing.active)
    {
        return 0;
    }
    return reach_bluetooth_device_id_equal(model->bluetooth_devices.devices[index].id,
                                           model->bluetooth_pairing.device_id);
}

void reach_settings_model_apply_bluetooth(reach_settings_model *model,
                                          const reach_bluetooth_device_list *devices,
                                          const reach_bluetooth_pairing_request *pairing,
                                          int32_t scanning)
{
    if (model == nullptr)
    {
        return;
    }

    uint16_t expanded_id[REACH_BLUETOOTH_DEVICE_ID_CAPACITY] = {};
    if (model->bluetooth_expanded_row >= 0 &&
        (size_t)model->bluetooth_expanded_row < model->bluetooth_devices.count)
    {
        reach_copy_utf16(expanded_id, REACH_BLUETOOTH_DEVICE_ID_CAPACITY,
                         model->bluetooth_devices.devices[model->bluetooth_expanded_row].id);
    }

    model->bluetooth_devices = devices != nullptr ? *devices : reach_bluetooth_device_list{};
    model->bluetooth_pairing = pairing != nullptr ? *pairing : reach_bluetooth_pairing_request{};
    model->bluetooth_scanning = scanning;
    model->bluetooth_loaded = 1;
    for (size_t index = 0; index < REACH_BLUETOOTH_MAX_DEVICES; ++index)
    {
        model->bluetooth_icons[index] = 0;
    }

    /* A pairing request always opens its own row, otherwise the open row follows its device. */
    if (model->bluetooth_pairing.active)
    {
        size_t found = reach_bluetooth_device_list_find(&model->bluetooth_devices,
                                                        model->bluetooth_pairing.device_id);
        if (found < model->bluetooth_devices.count)
        {
            model->bluetooth_expanded_row = (int32_t)found;
        }
    }
    else if (expanded_id[0] != 0)
    {
        size_t found = reach_bluetooth_device_list_find(&model->bluetooth_devices, expanded_id);
        model->bluetooth_expanded_row = found < model->bluetooth_devices.count
                                            ? (int32_t)found
                                            : REACH_SETTINGS_BLUETOOTH_ROW_NONE;
    }
    else
    {
        model->bluetooth_expanded_row = REACH_SETTINGS_BLUETOOTH_ROW_NONE;
    }

    for (size_t index = 0; index < REACH_BLUETOOTH_MAX_DEVICES; ++index)
    {
        float target =
            model->bluetooth_expanded_row >= 0 && (size_t)model->bluetooth_expanded_row == index
                ? 1.0f
                : 0.0f;
        if (reach_animation_manager_target(&model->bluetooth_row_animations, index) != target)
        {
            reach_animation_manager_animate_to(&model->bluetooth_row_animations, index, target,
                                               REACH_SETTINGS_BLUETOOTH_ROW_ANIMATION_SECONDS,
                                               REACH_EASING_EASE_IN_OUT);
        }
    }
}

void reach_settings_model_set_bluetooth_icon(reach_settings_model *model, size_t index,
                                             uint64_t icon_id)
{
    if (model != nullptr && index < REACH_BLUETOOTH_MAX_DEVICES)
    {
        model->bluetooth_icons[index] = icon_id;
    }
}

static void reach_settings_model_apply_bluetooth_radio(reach_settings_model *model,
                                                       reach_bluetooth_state state)
{
    model->bluetooth_radio = state;
    if (!state.available || !state.enabled)
    {
        model->bluetooth_scanning = 0;
        if (model->bluetooth_status == REACH_SETTINGS_BLUETOOTH_STATUS_SCANNING)
        {
            reach_settings_model_set_bluetooth_status(model, REACH_SETTINGS_BLUETOOTH_STATUS_IDLE,
                                                      nullptr);
        }
    }
}

void reach_settings_model_set_bluetooth_radio(reach_settings_model *model,
                                              reach_bluetooth_state state)
{
    if (model == nullptr)
    {
        return;
    }
    reach_settings_model_apply_bluetooth_radio(model, state);
    float target = state.available && state.enabled ? 1.0f : 0.0f;
    if (!model->bluetooth_loaded)
    {
        reach_animation_manager_set(&model->bluetooth_radio_animation, 0, target);
    }
    else if (reach_animation_manager_target(&model->bluetooth_radio_animation, 0) != target)
    {
        float current = reach_animation_manager_value(&model->bluetooth_radio_animation, 0);
        reach_animation_manager_start(&model->bluetooth_radio_animation, 0, current, target, 0.18,
                                      REACH_EASING_EASE_OUT);
    }
}

int32_t reach_settings_model_toggle_bluetooth_radio(reach_settings_model *model)
{
    if (model == nullptr || !model->bluetooth_radio.available)
    {
        return 0;
    }
    reach_bluetooth_state state = model->bluetooth_radio;
    state.enabled = state.enabled ? 0 : 1;
    reach_settings_model_apply_bluetooth_radio(model, state);
    float current = reach_animation_manager_value(&model->bluetooth_radio_animation, 0);
    reach_animation_manager_start(&model->bluetooth_radio_animation, 0, current,
                                  state.enabled ? 1.0f : 0.0f, 0.18, REACH_EASING_EASE_OUT);
    return 1;
}

void reach_settings_model_set_bluetooth_status(reach_settings_model *model, int32_t status,
                                               const uint16_t *device_id)
{
    if (model == nullptr)
    {
        return;
    }
    model->bluetooth_status = status;
    model->bluetooth_status_device[0] = 0;
    if (device_id != nullptr)
    {
        reach_copy_utf16(model->bluetooth_status_device, REACH_BLUETOOTH_DEVICE_ID_CAPACITY,
                         device_id);
    }
    if (status == REACH_SETTINGS_BLUETOOTH_STATUS_SCANNING)
    {
        reach_loader_model_reset(&model->bluetooth_loader);
    }
}

const uint16_t *reach_settings_bluetooth_status_message(int32_t status)
{
    switch (status)
    {
    case REACH_SETTINGS_BLUETOOTH_STATUS_SCANNING:
        return (const uint16_t *)u"Looking for devices...";
    case REACH_SETTINGS_BLUETOOTH_STATUS_PAIRING:
        return (const uint16_t *)u"Pairing...";
    case REACH_SETTINGS_BLUETOOTH_STATUS_CONFIRM_PIN:
        return (const uint16_t *)u"Confirm the code on the device";
    case REACH_SETTINGS_BLUETOOTH_STATUS_PAIRED:
        return (const uint16_t *)u"Device paired";
    case REACH_SETTINGS_BLUETOOTH_STATUS_REJECTED:
        return (const uint16_t *)u"Pairing was cancelled";
    case REACH_SETTINGS_BLUETOOTH_STATUS_FAILED:
        return (const uint16_t *)u"Could not pair with this device";
    default:
        return (const uint16_t *)u"";
    }
}

void reach_settings_model_bluetooth_expand_row(reach_settings_model *model, int32_t row)
{
    if (model == nullptr)
    {
        return;
    }
    if (model->bluetooth_expanded_row == row)
    {
        row = REACH_SETTINGS_BLUETOOTH_ROW_NONE;
    }
    model->bluetooth_expanded_row = row;
    if (model->bluetooth_status != REACH_SETTINGS_BLUETOOTH_STATUS_SCANNING &&
        model->bluetooth_status != REACH_SETTINGS_BLUETOOTH_STATUS_CONFIRM_PIN)
    {
        reach_settings_model_set_bluetooth_status(model, REACH_SETTINGS_BLUETOOTH_STATUS_IDLE,
                                                  nullptr);
    }

    for (size_t index = 0; index < REACH_BLUETOOTH_MAX_DEVICES; ++index)
    {
        float target = row >= 0 && (size_t)row == index ? 1.0f : 0.0f;
        if (reach_animation_manager_target(&model->bluetooth_row_animations, index) != target)
        {
            reach_animation_manager_animate_to(&model->bluetooth_row_animations, index, target,
                                               REACH_SETTINGS_BLUETOOTH_ROW_ANIMATION_SECONDS,
                                               REACH_EASING_EASE_IN_OUT);
        }
    }
}

void reach_settings_model_scroll_bluetooth(reach_settings_model *model, float delta)
{
    if (model != nullptr)
    {
        reach_scrollbar_scroll(&model->bluetooth_scrollbar, delta);
    }
}

int32_t reach_settings_model_bluetooth_scroll(reach_settings_model *model, double delta_seconds)
{
    return model != nullptr ? reach_scrollbar_update(&model->bluetooth_scrollbar, delta_seconds)
                            : 0;
}

int32_t reach_settings_model_bluetooth_loader(reach_settings_model *model, double delta_seconds)
{
    if (model == nullptr || model->bluetooth_status != REACH_SETTINGS_BLUETOOTH_STATUS_SCANNING)
    {
        return 0;
    }
    return reach_loader_update(&model->bluetooth_loader, delta_seconds);
}

int32_t reach_settings_model_tick_bluetooth_animations(reach_settings_model *model,
                                                       double delta_seconds)
{
    if (model == nullptr ||
        (!reach_animation_manager_any_active(&model->bluetooth_row_animations) &&
         !reach_animation_manager_any_active(&model->bluetooth_radio_animation)))
    {
        return 0;
    }
    reach_animation_manager_tick(&model->bluetooth_row_animations, delta_seconds);
    reach_animation_manager_tick(&model->bluetooth_radio_animation, delta_seconds);
    return 1;
}

int32_t reach_settings_model_bluetooth_animations_active(const reach_settings_model *model)
{
    return model != nullptr &&
           (reach_animation_manager_any_active(&model->bluetooth_row_animations) ||
            reach_animation_manager_any_active(&model->bluetooth_radio_animation));
}
