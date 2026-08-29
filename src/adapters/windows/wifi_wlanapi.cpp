#include "windows_adapters_internal.h"

#include "reach/ports/wifi.h"
#include "wifi_profile.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <windows.h>
#include <wlanapi.h>
#include <winrt/base.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <mutex>
#include <new>

#define REACH_WIFI_CONNECT_TIMEOUT_MS 20000

/* Authentication algorithms newer than the minimum supported SDK are compared numerically. */
#define REACH_DOT11_AUTH_ALGO_WPA3_VALUE 8u
#define REACH_DOT11_AUTH_ALGO_WPA3_SAE_VALUE 9u
#define REACH_DOT11_AUTH_ALGO_OWE_VALUE 10u
#define REACH_DOT11_AUTH_ALGO_WPA3_ENT_VALUE 11u

struct reach_wifi_adapter
{
    HANDLE wlan;
    GUID interface_guid;
    int32_t has_interface;
    reach_wifi_change_callback callback;
    void *callback_user;
    DWORD main_thread_id;
    std::mutex mutex;
    HANDLE connect_event;
    reach_wifi_connect_result connect_result;
    int32_t connect_pending;
};

static void reach_wifi_notify(reach_wifi_adapter *adapter, uint32_t flags)
{
    if (adapter == nullptr || adapter->callback == nullptr || flags == 0)
    {
        return;
    }
    adapter->callback(adapter->callback_user, flags);
    if (adapter->main_thread_id != 0)
    {
        PostThreadMessageW(adapter->main_thread_id, WM_NULL, 0, 0);
    }
}

static reach_wifi_connect_result reach_wifi_result_from_reason(WLAN_REASON_CODE reason)
{
    if (reason == WLAN_REASON_CODE_SUCCESS)
    {
        return REACH_WIFI_CONNECT_RESULT_SUCCEEDED;
    }
    if (reason >= WLAN_REASON_CODE_MSMSEC_MIN && reason <= WLAN_REASON_CODE_MSMSEC_MAX)
    {
        return REACH_WIFI_CONNECT_RESULT_INVALID_KEY;
    }
    if (reason == WLAN_REASON_CODE_NETWORK_NOT_AVAILABLE ||
        reason == WLAN_REASON_CODE_NO_VISIBLE_AP)
    {
        return REACH_WIFI_CONNECT_RESULT_NOT_FOUND;
    }
    return REACH_WIFI_CONNECT_RESULT_FAILED;
}

static void reach_wifi_complete_connect(reach_wifi_adapter *adapter,
                                        reach_wifi_connect_result result)
{
    {
        std::lock_guard<std::mutex> lock(adapter->mutex);
        if (!adapter->connect_pending)
        {
            return;
        }
        adapter->connect_result = result;
        adapter->connect_pending = 0;
    }
    if (adapter->connect_event != nullptr)
    {
        SetEvent(adapter->connect_event);
    }
}

static void WINAPI reach_wifi_notification(PWLAN_NOTIFICATION_DATA data, PVOID context)
{
    reach_wifi_adapter *adapter = static_cast<reach_wifi_adapter *>(context);
    if (adapter == nullptr || data == nullptr ||
        data->NotificationSource != WLAN_NOTIFICATION_SOURCE_ACM)
    {
        return;
    }

    uint32_t flags = 0;
    switch (data->NotificationCode)
    {
    case wlan_notification_acm_scan_complete:
        flags = REACH_WIFI_CHANGE_SCAN_COMPLETE | REACH_WIFI_CHANGE_NETWORKS;
        break;
    case wlan_notification_acm_scan_fail:
        flags = REACH_WIFI_CHANGE_SCAN_FAILED;
        break;
    case wlan_notification_acm_connection_complete:
    case wlan_notification_acm_connection_attempt_fail:
    {
        WLAN_REASON_CODE reason = WLAN_REASON_CODE_UNKNOWN;
        if (data->pData != nullptr && data->dwDataSize >= sizeof(WLAN_CONNECTION_NOTIFICATION_DATA))
        {
            reason = static_cast<PWLAN_CONNECTION_NOTIFICATION_DATA>(data->pData)->wlanReasonCode;
        }
        reach_wifi_complete_connect(adapter, reach_wifi_result_from_reason(reason));
        flags = REACH_WIFI_CHANGE_CONNECTION | REACH_WIFI_CHANGE_NETWORKS;
        break;
    }
    case wlan_notification_acm_disconnected:
    case wlan_notification_acm_profile_change:
    case wlan_notification_acm_profile_name_change:
        flags = REACH_WIFI_CHANGE_CONNECTION | REACH_WIFI_CHANGE_NETWORKS;
        break;
    case wlan_notification_acm_interface_arrival:
    case wlan_notification_acm_interface_removal:
        flags = REACH_WIFI_CHANGE_RADIO | REACH_WIFI_CHANGE_NETWORKS;
        break;
    default:
        break;
    }

    reach_wifi_notify(adapter, flags);
}

static int32_t reach_wifi_refresh_interface(reach_wifi_adapter *adapter)
{
    adapter->has_interface = 0;
    if (adapter->wlan == nullptr)
    {
        return 0;
    }

    PWLAN_INTERFACE_INFO_LIST interfaces = nullptr;
    if (WlanEnumInterfaces(adapter->wlan, nullptr, &interfaces) != ERROR_SUCCESS ||
        interfaces == nullptr)
    {
        return 0;
    }

    for (DWORD index = 0; index < interfaces->dwNumberOfItems; ++index)
    {
        const WLAN_INTERFACE_INFO *info = &interfaces->InterfaceInfo[index];
        if (info->isState == wlan_interface_state_not_ready && adapter->has_interface)
        {
            continue;
        }
        adapter->interface_guid = info->InterfaceGuid;
        adapter->has_interface = 1;
        if (info->isState != wlan_interface_state_not_ready)
        {
            break;
        }
    }

    WlanFreeMemory(interfaces);
    return adapter->has_interface;
}

static int32_t reach_wifi_find_radio(winrt::Windows::Devices::Radios::Radio *out_radio)
{
    namespace radios = winrt::Windows::Devices::Radios;
    if (out_radio == nullptr)
    {
        return 0;
    }
    *out_radio = nullptr;
    try
    {
        auto radio_list = radios::Radio::GetRadiosAsync().get();
        for (auto const &radio : radio_list)
        {
            if (radio.Kind() == radios::RadioKind::WiFi)
            {
                *out_radio = radio;
                return 1;
            }
        }
    }
    catch (winrt::hresult_error const &)
    {
        *out_radio = nullptr;
    }
    return 0;
}

static void reach_wifi_thread_attach(void *)
{
    try
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    }
    catch (winrt::hresult_error const &)
    {
    }
}

static void reach_wifi_thread_detach(void *)
{
    winrt::uninit_apartment();
}

static reach_result reach_wifi_get_radio_state(void *userdata, reach_wifi_radio_state *out_state)
{
    reach_wifi_adapter *adapter = static_cast<reach_wifi_adapter *>(userdata);
    if (adapter == nullptr || out_state == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_state = REACH_WIFI_RADIO_UNAVAILABLE;
    if (!reach_wifi_refresh_interface(adapter))
    {
        return REACH_OK;
    }

    winrt::Windows::Devices::Radios::Radio radio = nullptr;
    if (reach_wifi_find_radio(&radio))
    {
        try
        {
            *out_state = radio.State() == winrt::Windows::Devices::Radios::RadioState::On
                             ? REACH_WIFI_RADIO_ON
                             : REACH_WIFI_RADIO_OFF;
            return REACH_OK;
        }
        catch (winrt::hresult_error const &)
        {
        }
    }

    PWLAN_RADIO_STATE radio_state = nullptr;
    DWORD size = 0;
    if (WlanQueryInterface(adapter->wlan, &adapter->interface_guid, wlan_intf_opcode_radio_state,
                           nullptr, &size, reinterpret_cast<PVOID *>(&radio_state),
                           nullptr) == ERROR_SUCCESS &&
        radio_state != nullptr)
    {
        *out_state = REACH_WIFI_RADIO_OFF;
        for (DWORD index = 0; index < radio_state->dwNumberOfPhys; ++index)
        {
            if (radio_state->PhyRadioState[index].dot11SoftwareRadioState == dot11_radio_state_on &&
                radio_state->PhyRadioState[index].dot11HardwareRadioState == dot11_radio_state_on)
            {
                *out_state = REACH_WIFI_RADIO_ON;
                break;
            }
        }
        WlanFreeMemory(radio_state);
        return REACH_OK;
    }

    *out_state = REACH_WIFI_RADIO_ON;
    return REACH_OK;
}

static reach_result reach_wifi_set_radio_enabled(void *userdata, int32_t enabled)
{
    namespace radios = winrt::Windows::Devices::Radios;
    reach_wifi_adapter *adapter = static_cast<reach_wifi_adapter *>(userdata);
    if (adapter == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    try
    {
        auto access = radios::Radio::RequestAccessAsync().get();
        if (access != radios::RadioAccessStatus::Allowed)
        {
            return REACH_ERROR;
        }
        radios::Radio radio = nullptr;
        if (!reach_wifi_find_radio(&radio))
        {
            return REACH_ERROR;
        }
        auto result =
            radio.SetStateAsync(enabled ? radios::RadioState::On : radios::RadioState::Off).get();
        return result == radios::RadioAccessStatus::Allowed ? REACH_OK : REACH_ERROR;
    }
    catch (winrt::hresult_error const &)
    {
        return REACH_ERROR;
    }
}

static reach_result reach_wifi_start_scan(void *userdata)
{
    reach_wifi_adapter *adapter = static_cast<reach_wifi_adapter *>(userdata);
    if (adapter == nullptr || adapter->wlan == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (!reach_wifi_refresh_interface(adapter))
    {
        return REACH_ERROR;
    }
    DWORD result = WlanScan(adapter->wlan, &adapter->interface_guid, nullptr, nullptr, nullptr);
    return result == ERROR_SUCCESS ? REACH_OK : REACH_ERROR;
}

static reach_wifi_security reach_wifi_security_from_auth(DOT11_AUTH_ALGORITHM authentication,
                                                         BOOL security_enabled)
{
    switch ((unsigned)authentication)
    {
    case DOT11_AUTH_ALGO_80211_OPEN:
        return security_enabled ? REACH_WIFI_SECURITY_WEP : REACH_WIFI_SECURITY_OPEN;
    case DOT11_AUTH_ALGO_80211_SHARED_KEY:
        return REACH_WIFI_SECURITY_WEP;
    case DOT11_AUTH_ALGO_WPA_PSK:
    case DOT11_AUTH_ALGO_WPA_NONE:
    case DOT11_AUTH_ALGO_RSNA_PSK:
        return REACH_WIFI_SECURITY_WPA2_PERSONAL;
    case DOT11_AUTH_ALGO_WPA:
    case DOT11_AUTH_ALGO_RSNA:
    case REACH_DOT11_AUTH_ALGO_WPA3_VALUE:
    case REACH_DOT11_AUTH_ALGO_WPA3_ENT_VALUE:
        return REACH_WIFI_SECURITY_ENTERPRISE;
    case REACH_DOT11_AUTH_ALGO_WPA3_SAE_VALUE:
        return REACH_WIFI_SECURITY_WPA3_PERSONAL;
    case REACH_DOT11_AUTH_ALGO_OWE_VALUE:
        return REACH_WIFI_SECURITY_OPEN;
    default:
        return security_enabled ? REACH_WIFI_SECURITY_UNKNOWN : REACH_WIFI_SECURITY_OPEN;
    }
}

static void reach_wifi_copy_ssid(uint16_t *destination, const DOT11_SSID *ssid)
{
    destination[0] = 0;
    if (ssid == nullptr || ssid->uSSIDLength == 0)
    {
        return;
    }
    ULONG source_count = ssid->uSSIDLength;
    if (source_count > sizeof(ssid->ucSSID))
    {
        source_count = sizeof(ssid->ucSSID);
    }
    int converted = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char *>(ssid->ucSSID),
                                        (int)source_count, reinterpret_cast<wchar_t *>(destination),
                                        (int)(REACH_WIFI_SSID_CAPACITY - 1));
    if (converted <= 0)
    {
        converted = MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char *>(ssid->ucSSID),
                                        (int)source_count, reinterpret_cast<wchar_t *>(destination),
                                        (int)(REACH_WIFI_SSID_CAPACITY - 1));
    }
    destination[converted > 0 ? (size_t)converted : 0] = 0;
}

static int32_t reach_wifi_profile_is_automatic(reach_wifi_adapter *adapter,
                                               const wchar_t *profile_name)
{
    LPWSTR xml = nullptr;
    DWORD flags = 0;
    DWORD access = 0;
    if (WlanGetProfile(adapter->wlan, &adapter->interface_guid, profile_name, nullptr, &xml, &flags,
                       &access) != ERROR_SUCCESS ||
        xml == nullptr)
    {
        return 0;
    }
    int32_t automatic = wcsstr(xml, L"<connectionMode>auto</connectionMode>") != nullptr;
    WlanFreeMemory(xml);
    return automatic;
}

static reach_result reach_wifi_read_networks(void *userdata, reach_wifi_network_list *out_networks)
{
    reach_wifi_adapter *adapter = static_cast<reach_wifi_adapter *>(userdata);
    if (adapter == nullptr || out_networks == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_networks = {};
    if (adapter->wlan == nullptr || !reach_wifi_refresh_interface(adapter))
    {
        return REACH_OK;
    }

    PWLAN_AVAILABLE_NETWORK_LIST available = nullptr;
    if (WlanGetAvailableNetworkList(adapter->wlan, &adapter->interface_guid,
                                    WLAN_AVAILABLE_NETWORK_INCLUDE_ALL_MANUAL_HIDDEN_PROFILES,
                                    nullptr, &available) == ERROR_SUCCESS &&
        available != nullptr)
    {
        for (DWORD index = 0;
             index < available->dwNumberOfItems && out_networks->count < REACH_WIFI_MAX_NETWORKS;
             ++index)
        {
            const WLAN_AVAILABLE_NETWORK *entry = &available->Network[index];
            reach_wifi_network *network = &out_networks->networks[out_networks->count];
            *network = {};
            reach_wifi_copy_ssid(network->ssid, &entry->dot11Ssid);
            if (network->ssid[0] == 0)
            {
                continue;
            }
            network->security = reach_wifi_security_from_auth(entry->dot11DefaultAuthAlgorithm,
                                                              entry->bSecurityEnabled);
            network->signal_strength = (int32_t)entry->wlanSignalQuality;
            network->in_range = 1;
            network->connected = (entry->dwFlags & WLAN_AVAILABLE_NETWORK_CONNECTED) != 0 ? 1 : 0;
            network->saved = (entry->dwFlags & WLAN_AVAILABLE_NETWORK_HAS_PROFILE) != 0 ? 1 : 0;
            if (network->saved && entry->strProfileName[0] != 0)
            {
                network->connect_automatically =
                    reach_wifi_profile_is_automatic(adapter, entry->strProfileName);
            }
            ++out_networks->count;
        }
        WlanFreeMemory(available);
    }

    PWLAN_PROFILE_INFO_LIST profiles = nullptr;
    if (WlanGetProfileList(adapter->wlan, &adapter->interface_guid, nullptr, &profiles) ==
            ERROR_SUCCESS &&
        profiles != nullptr)
    {
        for (DWORD index = 0;
             index < profiles->dwNumberOfItems && out_networks->count < REACH_WIFI_MAX_NETWORKS;
             ++index)
        {
            const wchar_t *name = profiles->ProfileInfo[index].strProfileName;
            if (name[0] == 0)
            {
                continue;
            }
            reach_wifi_network *network = &out_networks->networks[out_networks->count];
            *network = {};
            reach_copy_utf16(network->ssid, REACH_WIFI_SSID_CAPACITY,
                             reinterpret_cast<const uint16_t *>(name));
            network->security = REACH_WIFI_SECURITY_UNKNOWN;
            network->saved = 1;
            network->connect_automatically = reach_wifi_profile_is_automatic(adapter, name);
            ++out_networks->count;
        }
        WlanFreeMemory(profiles);
    }

    reach_wifi_network_list_normalize(out_networks);
    return REACH_OK;
}

static reach_result reach_wifi_apply_profile(reach_wifi_adapter *adapter,
                                             const reach_wifi_connect_request *request)
{
    uint16_t xml[REACH_WIFI_PROFILE_CAPACITY] = {};
    if (reach_wifi_build_profile_xml(xml, REACH_WIFI_PROFILE_CAPACITY, request->ssid, request->key,
                                     request->security, request->connect_automatically,
                                     request->hidden) == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD reason = 0;
    DWORD result = WlanSetProfile(adapter->wlan, &adapter->interface_guid, 0,
                                  reinterpret_cast<LPCWSTR>(xml), nullptr, TRUE, nullptr, &reason);
    return result == ERROR_SUCCESS ? REACH_OK : REACH_ERROR;
}

static reach_result reach_wifi_connect(void *userdata, const reach_wifi_connect_request *request,
                                       reach_wifi_connect_result *out_result)
{
    reach_wifi_adapter *adapter = static_cast<reach_wifi_adapter *>(userdata);
    if (adapter == nullptr || request == nullptr || out_result == nullptr || request->ssid[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_result = REACH_WIFI_CONNECT_RESULT_NONE;
    if (adapter->wlan == nullptr || !reach_wifi_refresh_interface(adapter))
    {
        return REACH_ERROR;
    }
    if (!reach_wifi_security_is_supported(request->security))
    {
        *out_result = REACH_WIFI_CONNECT_RESULT_FAILED;
        return REACH_ERROR;
    }

    if (request->key[0] != 0 || request->hidden || request->security == REACH_WIFI_SECURITY_OPEN)
    {
        reach_result applied = reach_wifi_apply_profile(adapter, request);
        if (applied != REACH_OK)
        {
            *out_result = REACH_WIFI_CONNECT_RESULT_INVALID_KEY;
            return applied;
        }
    }

    if (adapter->connect_event != nullptr)
    {
        ResetEvent(adapter->connect_event);
    }
    {
        std::lock_guard<std::mutex> lock(adapter->mutex);
        adapter->connect_pending = 1;
        adapter->connect_result = REACH_WIFI_CONNECT_RESULT_NONE;
    }

    WLAN_CONNECTION_PARAMETERS parameters = {};
    parameters.wlanConnectionMode = wlan_connection_mode_profile;
    parameters.strProfile = reinterpret_cast<LPCWSTR>(request->ssid);
    parameters.dot11BssType = dot11_BSS_type_infrastructure;

    if (WlanConnect(adapter->wlan, &adapter->interface_guid, &parameters, nullptr) != ERROR_SUCCESS)
    {
        std::lock_guard<std::mutex> lock(adapter->mutex);
        adapter->connect_pending = 0;
        *out_result = REACH_WIFI_CONNECT_RESULT_FAILED;
        return REACH_ERROR;
    }

    if (adapter->connect_event != nullptr &&
        WaitForSingleObject(adapter->connect_event, REACH_WIFI_CONNECT_TIMEOUT_MS) != WAIT_OBJECT_0)
    {
        std::lock_guard<std::mutex> lock(adapter->mutex);
        adapter->connect_pending = 0;
        *out_result = REACH_WIFI_CONNECT_RESULT_TIMED_OUT;
        return REACH_OK;
    }

    {
        std::lock_guard<std::mutex> lock(adapter->mutex);
        *out_result = adapter->connect_result;
    }
    return REACH_OK;
}

static reach_result reach_wifi_disconnect(void *userdata)
{
    reach_wifi_adapter *adapter = static_cast<reach_wifi_adapter *>(userdata);
    if (adapter == nullptr || adapter->wlan == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (!reach_wifi_refresh_interface(adapter))
    {
        return REACH_ERROR;
    }
    return WlanDisconnect(adapter->wlan, &adapter->interface_guid, nullptr) == ERROR_SUCCESS
               ? REACH_OK
               : REACH_ERROR;
}

static reach_result reach_wifi_forget(void *userdata, const uint16_t *ssid)
{
    reach_wifi_adapter *adapter = static_cast<reach_wifi_adapter *>(userdata);
    if (adapter == nullptr || adapter->wlan == nullptr || ssid == nullptr || ssid[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (!reach_wifi_refresh_interface(adapter))
    {
        return REACH_ERROR;
    }
    DWORD result = WlanDeleteProfile(adapter->wlan, &adapter->interface_guid,
                                     reinterpret_cast<LPCWSTR>(ssid), nullptr);
    return result == ERROR_SUCCESS ? REACH_OK : REACH_ERROR;
}

static reach_result reach_wifi_start_watching(void *userdata, reach_wifi_change_callback callback,
                                              void *callback_user)
{
    reach_wifi_adapter *adapter = static_cast<reach_wifi_adapter *>(userdata);
    if (adapter == nullptr || callback == nullptr || adapter->wlan == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    adapter->callback = callback;
    adapter->callback_user = callback_user;
    adapter->main_thread_id = GetCurrentThreadId();

    DWORD previous = 0;
    DWORD result = WlanRegisterNotification(adapter->wlan, WLAN_NOTIFICATION_SOURCE_ACM, FALSE,
                                            reach_wifi_notification, adapter, nullptr, &previous);
    return result == ERROR_SUCCESS ? REACH_OK : REACH_ERROR;
}

static void reach_wifi_stop_watching(void *userdata)
{
    reach_wifi_adapter *adapter = static_cast<reach_wifi_adapter *>(userdata);
    if (adapter == nullptr || adapter->wlan == nullptr)
    {
        return;
    }
    DWORD previous = 0;
    (void)WlanRegisterNotification(adapter->wlan, WLAN_NOTIFICATION_SOURCE_NONE, FALSE, nullptr,
                                   nullptr, nullptr, &previous);
    adapter->callback = nullptr;
    adapter->callback_user = nullptr;
    reach_wifi_complete_connect(adapter, REACH_WIFI_CONNECT_RESULT_FAILED);
}

static void reach_wifi_destroy(void *userdata)
{
    reach_wifi_adapter *adapter = static_cast<reach_wifi_adapter *>(userdata);
    if (adapter == nullptr)
    {
        return;
    }
    reach_wifi_stop_watching(adapter);
    if (adapter->connect_event != nullptr)
    {
        CloseHandle(adapter->connect_event);
    }
    if (adapter->wlan != nullptr)
    {
        WlanCloseHandle(adapter->wlan, nullptr);
    }
    delete adapter;
}

reach_result reach_windows_create_wifi(reach_wifi_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_port = {};

    reach_wifi_adapter *adapter = new (std::nothrow) reach_wifi_adapter();
    if (adapter == nullptr)
    {
        return REACH_ERROR;
    }

    DWORD negotiated_version = 0;
    if (WlanOpenHandle(2, nullptr, &negotiated_version, &adapter->wlan) != ERROR_SUCCESS)
    {
        adapter->wlan = nullptr;
    }
    adapter->connect_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    (void)reach_wifi_refresh_interface(adapter);

    out_port->userdata = adapter;
    out_port->thread_attach = reach_wifi_thread_attach;
    out_port->thread_detach = reach_wifi_thread_detach;
    out_port->get_radio_state = reach_wifi_get_radio_state;
    out_port->set_radio_enabled = reach_wifi_set_radio_enabled;
    out_port->start_scan = reach_wifi_start_scan;
    out_port->read_networks = reach_wifi_read_networks;
    out_port->connect = reach_wifi_connect;
    out_port->disconnect = reach_wifi_disconnect;
    out_port->forget = reach_wifi_forget;
    out_port->start_watching = reach_wifi_start_watching;
    out_port->stop_watching = reach_wifi_stop_watching;
    out_port->destroy = reach_wifi_destroy;
    return REACH_OK;
}
