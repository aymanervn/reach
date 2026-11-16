#include "reach/features/common/bluetooth_session.h"

#include "reach/support/util.h"

void reach_bluetooth_session_init(reach_bluetooth_session *session)
{
    if (session == nullptr)
    {
        return;
    }
    *session = {};
    session->selected = REACH_BLUETOOTH_SESSION_SELECTION_NONE;
}

void reach_bluetooth_session_apply_devices(reach_bluetooth_session *session,
                                           const reach_bluetooth_device_list *devices,
                                           const reach_bluetooth_pairing_request *pairing,
                                           int32_t scanning)
{
    if (session == nullptr)
    {
        return;
    }

    uint16_t selected_id[REACH_BLUETOOTH_DEVICE_ID_CAPACITY] = {};
    const reach_bluetooth_device *selected = reach_bluetooth_session_selected_device(session);
    if (selected != nullptr)
    {
        reach_copy_utf16(selected_id, REACH_BLUETOOTH_DEVICE_ID_CAPACITY, selected->id);
    }

    session->devices = devices != nullptr ? *devices : reach_bluetooth_device_list{};
    session->pairing = pairing != nullptr ? *pairing : reach_bluetooth_pairing_request{};
    session->scanning = scanning;
    session->loaded = 1;

    if (session->pairing.active)
    {
        size_t found =
            reach_bluetooth_device_list_find(&session->devices, session->pairing.device_id);
        if (found < session->devices.count)
        {
            session->selected = (int32_t)found;
        }
    }
    else if (selected_id[0] != 0)
    {
        size_t found = reach_bluetooth_device_list_find(&session->devices, selected_id);
        session->selected = found < session->devices.count ? (int32_t)found
                                                           : REACH_BLUETOOTH_SESSION_SELECTION_NONE;
    }
    else
    {
        session->selected = REACH_BLUETOOTH_SESSION_SELECTION_NONE;
    }
}

void reach_bluetooth_session_apply_snapshot(reach_bluetooth_session *session,
                                            const reach_bluetooth_snapshot *snapshot)
{
    if (session == nullptr || snapshot == nullptr)
    {
        return;
    }

    reach_bluetooth_session_apply_devices(session, &snapshot->devices, &snapshot->pairing,
                                          snapshot->scanning);

    if (snapshot->pair_result != REACH_BLUETOOTH_PAIR_RESULT_NONE)
    {
        reach_bluetooth_session_status status = REACH_BLUETOOTH_SESSION_STATUS_FAILED;
        if (snapshot->pair_result == REACH_BLUETOOTH_PAIR_RESULT_SUCCEEDED)
        {
            status = REACH_BLUETOOTH_SESSION_STATUS_PAIRED;
        }
        else if (snapshot->pair_result == REACH_BLUETOOTH_PAIR_RESULT_REJECTED)
        {
            status = REACH_BLUETOOTH_SESSION_STATUS_REJECTED;
        }
        reach_bluetooth_session_set_status(session, status, snapshot->pair_device_id);
    }
    else if (snapshot->pairing.active)
    {
        reach_bluetooth_session_set_status(session, REACH_BLUETOOTH_SESSION_STATUS_CONFIRM_PIN,
                                           snapshot->pairing.device_id);
    }
    else if (snapshot->completed_command == REACH_BLUETOOTH_SERVICE_COMMAND_SET_SCAN)
    {
        reach_bluetooth_session_set_status(session,
                                           snapshot->scanning && session->radio.available &&
                                                   session->radio.enabled
                                               ? REACH_BLUETOOTH_SESSION_STATUS_SCANNING
                                               : REACH_BLUETOOTH_SESSION_STATUS_IDLE,
                                           nullptr);
    }
}

void reach_bluetooth_session_set_radio(reach_bluetooth_session *session,
                                       reach_bluetooth_state state)
{
    if (session == nullptr)
    {
        return;
    }
    session->radio = state;
    if (!state.available || !state.enabled)
    {
        session->scanning = 0;
        if (session->status == REACH_BLUETOOTH_SESSION_STATUS_SCANNING)
        {
            reach_bluetooth_session_set_status(session, REACH_BLUETOOTH_SESSION_STATUS_IDLE,
                                               nullptr);
        }
    }
}

int32_t reach_bluetooth_session_toggle_radio(reach_bluetooth_session *session)
{
    if (session == nullptr || !session->radio.available)
    {
        return 0;
    }
    reach_bluetooth_state state = session->radio;
    state.enabled = state.enabled ? 0 : 1;
    reach_bluetooth_session_set_radio(session, state);
    return 1;
}

void reach_bluetooth_session_set_status(reach_bluetooth_session *session,
                                        reach_bluetooth_session_status status,
                                        const uint16_t *device_id)
{
    if (session == nullptr)
    {
        return;
    }
    session->status = status;
    session->status_device[0] = 0;
    if (device_id != nullptr)
    {
        reach_copy_utf16(session->status_device, REACH_BLUETOOTH_DEVICE_ID_CAPACITY, device_id);
    }
}

const uint16_t *reach_bluetooth_session_status_message(reach_bluetooth_session_status status)
{
    switch (status)
    {
    case REACH_BLUETOOTH_SESSION_STATUS_SCANNING:
        return (const uint16_t *)u"Looking for devices...";
    case REACH_BLUETOOTH_SESSION_STATUS_PAIRING:
        return (const uint16_t *)u"Pairing...";
    case REACH_BLUETOOTH_SESSION_STATUS_CONFIRM_PIN:
        return (const uint16_t *)u"Confirm the code on the device";
    case REACH_BLUETOOTH_SESSION_STATUS_PAIRED:
        return (const uint16_t *)u"Device paired";
    case REACH_BLUETOOTH_SESSION_STATUS_REJECTED:
        return (const uint16_t *)u"Pairing was cancelled";
    case REACH_BLUETOOTH_SESSION_STATUS_FAILED:
        return (const uint16_t *)u"Could not pair with this device";
    default:
        return (const uint16_t *)u"";
    }
}

void reach_bluetooth_session_select(reach_bluetooth_session *session, int32_t selection)
{
    if (session == nullptr)
    {
        return;
    }
    session->selected =
        session->selected == selection ? REACH_BLUETOOTH_SESSION_SELECTION_NONE : selection;
    if (session->status != REACH_BLUETOOTH_SESSION_STATUS_SCANNING &&
        session->status != REACH_BLUETOOTH_SESSION_STATUS_CONFIRM_PIN)
    {
        reach_bluetooth_session_set_status(session, REACH_BLUETOOTH_SESSION_STATUS_IDLE, nullptr);
    }
}

const reach_bluetooth_device *
reach_bluetooth_session_selected_device(const reach_bluetooth_session *session)
{
    if (session == nullptr || session->selected < 0 ||
        (size_t)session->selected >= session->devices.count)
    {
        return nullptr;
    }
    return &session->devices.devices[session->selected];
}

int32_t reach_bluetooth_session_device_is_pairing(const reach_bluetooth_session *session,
                                                  size_t index)
{
    if (session == nullptr || index >= session->devices.count || !session->pairing.active)
    {
        return 0;
    }
    return reach_bluetooth_device_id_equal(session->devices.devices[index].id,
                                           session->pairing.device_id);
}

const uint16_t *reach_bluetooth_session_device_detail(const reach_bluetooth_device *device)
{
    if (device == nullptr)
    {
        return (const uint16_t *)u"";
    }
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

reach_vector_icon_id reach_bluetooth_session_device_icon(reach_bluetooth_device_kind kind)
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
