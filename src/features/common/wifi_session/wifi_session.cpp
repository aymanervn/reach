#include "reach/features/common/wifi_session.h"

#include "reach/support/util.h"

#include <string.h>

void reach_wifi_session_init(reach_wifi_session *session)
{
    if (session == nullptr)
    {
        return;
    }
    *session = {};
    session->selected = REACH_WIFI_SESSION_SELECTION_NONE;
    session->connect_automatically = 1;
    reach_text_edit_init(&session->key_edit, REACH_WIFI_KEY_MAXIMUM_LENGTH);
}

void reach_wifi_session_apply_networks(reach_wifi_session *session, reach_wifi_radio_state radio,
                                       const reach_wifi_network_list *networks)
{
    if (session == nullptr)
    {
        return;
    }

    uint16_t selected_ssid[REACH_WIFI_SSID_CAPACITY] = {};
    if (session->selected >= 0 && (size_t)session->selected < session->networks.count)
    {
        reach_copy_utf16(selected_ssid, REACH_WIFI_SSID_CAPACITY,
                         session->networks.networks[session->selected].ssid);
    }

    session->radio = radio;
    session->networks = networks != nullptr ? *networks : reach_wifi_network_list{};
    session->loaded = 1;

    if (selected_ssid[0] != 0)
    {
        size_t found = reach_wifi_network_list_find(&session->networks, selected_ssid);
        session->selected =
            found < session->networks.count ? (int32_t)found : REACH_WIFI_SESSION_SELECTION_NONE;
    }
    else if (session->selected >= 0)
    {
        session->selected = REACH_WIFI_SESSION_SELECTION_NONE;
    }
}

static reach_wifi_session_status reach_wifi_session_connect_status(reach_wifi_connect_result result)
{
    switch (result)
    {
    case REACH_WIFI_CONNECT_RESULT_SUCCEEDED:
        return REACH_WIFI_SESSION_STATUS_CONNECTED;
    case REACH_WIFI_CONNECT_RESULT_INVALID_KEY:
        return REACH_WIFI_SESSION_STATUS_INVALID_KEY;
    case REACH_WIFI_CONNECT_RESULT_NOT_FOUND:
        return REACH_WIFI_SESSION_STATUS_NOT_FOUND;
    default:
        return REACH_WIFI_SESSION_STATUS_FAILED;
    }
}

int32_t reach_wifi_session_apply_snapshot(reach_wifi_session *session,
                                          const reach_wifi_snapshot *snapshot)
{
    if (session == nullptr || snapshot == nullptr)
    {
        return 0;
    }

    reach_wifi_session_apply_networks(session, snapshot->radio, &snapshot->networks);

    switch (snapshot->completed_command)
    {
    case REACH_WIFI_SERVICE_COMMAND_SCAN:
        reach_wifi_session_set_status(session,
                                      snapshot->scan_result == REACH_WIFI_SCAN_RESULT_SUCCEEDED
                                          ? REACH_WIFI_SESSION_STATUS_IDLE
                                          : REACH_WIFI_SESSION_STATUS_SCAN_FAILED,
                                      nullptr);
        break;
    case REACH_WIFI_SERVICE_COMMAND_CONNECT:
    {
        reach_wifi_session_status status =
            reach_wifi_session_connect_status(snapshot->connect_result);
        reach_wifi_session_set_status(session, status, snapshot->connect_ssid);
        if (status == REACH_WIFI_SESSION_STATUS_CONNECTED)
        {
            reach_wifi_session_clear_secrets(session);
            return 1;
        }
        break;
    }
    case REACH_WIFI_SERVICE_COMMAND_FORGET:
        reach_wifi_session_set_status(session,
                                      snapshot->command_succeeded
                                          ? REACH_WIFI_SESSION_STATUS_IDLE
                                          : REACH_WIFI_SESSION_STATUS_FORGET_FAILED,
                                      nullptr);
        break;
    case REACH_WIFI_SERVICE_COMMAND_DISCONNECT:
    case REACH_WIFI_SERVICE_COMMAND_SET_RADIO:
        reach_wifi_session_set_status(session, REACH_WIFI_SESSION_STATUS_IDLE, nullptr);
        break;
    default:
        break;
    }
    return 0;
}

void reach_wifi_session_set_status(reach_wifi_session *session, reach_wifi_session_status status,
                                   const uint16_t *ssid)
{
    if (session == nullptr)
    {
        return;
    }
    session->status = status;
    session->status_ssid[0] = 0;
    if (ssid != nullptr)
    {
        reach_copy_utf16(session->status_ssid, REACH_WIFI_SSID_CAPACITY, ssid);
    }
}

const uint16_t *reach_wifi_session_status_message(reach_wifi_session_status status)
{
    switch (status)
    {
    case REACH_WIFI_SESSION_STATUS_SCANNING:
        return (const uint16_t *)u"Searching for networks...";
    case REACH_WIFI_SESSION_STATUS_CONNECTING:
        return (const uint16_t *)u"Connecting...";
    case REACH_WIFI_SESSION_STATUS_CONNECTED:
        return (const uint16_t *)u"Connected";
    case REACH_WIFI_SESSION_STATUS_INVALID_KEY:
        return (const uint16_t *)u"That password did not work";
    case REACH_WIFI_SESSION_STATUS_NOT_FOUND:
        return (const uint16_t *)u"That network is out of range";
    case REACH_WIFI_SESSION_STATUS_FAILED:
        return (const uint16_t *)u"Could not connect to this network";
    case REACH_WIFI_SESSION_STATUS_SCAN_FAILED:
        return (const uint16_t *)u"Could not search for networks";
    case REACH_WIFI_SESSION_STATUS_FORGET_FAILED:
        return (const uint16_t *)u"Could not forget this network";
    default:
        return (const uint16_t *)u"";
    }
}

int32_t reach_wifi_session_busy(const reach_wifi_session *session)
{
    return session != nullptr && (session->status == REACH_WIFI_SESSION_STATUS_SCANNING ||
                                  session->status == REACH_WIFI_SESSION_STATUS_CONNECTING);
}

int32_t reach_wifi_session_toggle_radio(reach_wifi_session *session)
{
    if (session == nullptr || session->radio == REACH_WIFI_RADIO_UNAVAILABLE)
    {
        return 0;
    }
    session->radio =
        session->radio == REACH_WIFI_RADIO_ON ? REACH_WIFI_RADIO_OFF : REACH_WIFI_RADIO_ON;
    return 1;
}

void reach_wifi_session_select(reach_wifi_session *session, int32_t selection)
{
    if (session == nullptr)
    {
        return;
    }
    session->selected =
        session->selected == selection ? REACH_WIFI_SESSION_SELECTION_NONE : selection;
    reach_wifi_session_clear_secrets(session);
    session->show_key = 0;
    session->connect_automatically = 1;
    if (session->status != REACH_WIFI_SESSION_STATUS_SCANNING)
    {
        reach_wifi_session_set_status(session, REACH_WIFI_SESSION_STATUS_IDLE, nullptr);
    }
}

const reach_wifi_network *reach_wifi_session_selected_network(const reach_wifi_session *session)
{
    if (session == nullptr || session->selected < 0 ||
        (size_t)session->selected >= session->networks.count)
    {
        return nullptr;
    }
    return &session->networks.networks[session->selected];
}

int32_t reach_wifi_session_connect_ready(const reach_wifi_session *session)
{
    const reach_wifi_network *network = reach_wifi_session_selected_network(session);
    if (network == nullptr || !reach_wifi_security_is_supported(network->security))
    {
        return 0;
    }
    if (!reach_wifi_security_needs_key(network->security) || network->saved)
    {
        return 1;
    }
    return reach_wifi_key_length_valid(network->security, session->key_edit.length);
}

int32_t reach_wifi_session_build_connect(const reach_wifi_session *session, size_t index,
                                         reach_wifi_connect_request *out_request)
{
    if (session == nullptr || out_request == nullptr || index >= session->networks.count)
    {
        return 0;
    }
    const reach_wifi_network *network = &session->networks.networks[index];
    *out_request = {};
    reach_copy_utf16(out_request->ssid, REACH_WIFI_SSID_CAPACITY, network->ssid);
    out_request->security = network->security;
    out_request->connect_automatically = session->connect_automatically;
    out_request->hidden = network->hidden;
    if (session->key_edit.length > 0)
    {
        reach_copy_utf16(out_request->key, REACH_WIFI_KEY_CAPACITY, session->key_edit.text);
    }
    return 1;
}

void reach_wifi_session_clear_secrets(reach_wifi_session *session)
{
    if (session == nullptr)
    {
        return;
    }
    memset(session->key_edit.text, 0, sizeof(session->key_edit.text));
    reach_text_edit_clear(&session->key_edit);
}

const uint16_t *reach_wifi_session_network_detail(const reach_wifi_network *network,
                                                  int32_t known_view)
{
    if (network == nullptr)
    {
        return (const uint16_t *)u"";
    }
    if (network->connected)
    {
        return (const uint16_t *)u"Connected";
    }
    if (known_view && !network->in_range)
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

const uint16_t *reach_wifi_session_key_display(const reach_wifi_session *session,
                                               const reach_text_edit *edit,
                                               uint16_t out_masked[REACH_TEXT_EDIT_CAPACITY])
{
    if (edit == nullptr)
    {
        return (const uint16_t *)u"";
    }
    if (session != nullptr && session->show_key)
    {
        return edit->text;
    }
    size_t index = 0;
    for (; index < edit->length && index + 1 < REACH_TEXT_EDIT_CAPACITY; ++index)
    {
        out_masked[index] = 0x2022;
    }
    out_masked[index] = 0;
    return out_masked;
}
