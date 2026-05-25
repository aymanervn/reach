#include "reach/platform/windows_adapters.h"

#include "reach/ports/system_controls.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <wlanapi.h>
#include <iphlpapi.h>
#include <wbemidl.h>
#include <roapi.h>
#include <winrt/base.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <wrl/client.h>

#include <new>

using Microsoft::WRL::ComPtr;

typedef enum reach_bluetooth_request_type {
    REACH_BLUETOOTH_REQUEST_NONE = 0,
    REACH_BLUETOOTH_REQUEST_GET_STATE,
    REACH_BLUETOOTH_REQUEST_SET_ENABLED,
    REACH_BLUETOOTH_REQUEST_SET_ENABLED_ASYNC
} reach_bluetooth_request_type;

struct reach_system_controls_adapter {
    HANDLE wlan;
    HANDLE addr_change;
    HANDLE addr_change_thread;
    HANDLE addr_change_stop;
    OVERLAPPED addr_change_overlapped;
    DWORD main_thread_id;
    reach_system_controls_change_callback callback;
    void *callback_user;
    HANDLE bluetooth_thread;
    HANDLE bluetooth_stop;
    HANDLE bluetooth_request;
    HANDLE bluetooth_complete;
    CRITICAL_SECTION bluetooth_lock;
    int32_t bluetooth_lock_initialized;
    reach_bluetooth_request_type bluetooth_request_type;
    int32_t bluetooth_request_pending;
    int32_t bluetooth_request_enabled;
    reach_result bluetooth_request_result;
    reach_bluetooth_state bluetooth_request_state;
};

struct reach_com_scope {
    int32_t uninitialize;
    reach_result result;

    reach_com_scope()
        : uninitialize(0),
          result(REACH_OK)
    {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (hr == S_OK) {
            uninitialize = 1;
        } else if (hr == S_FALSE || hr == RPC_E_CHANGED_MODE) {
            result = REACH_OK;
        } else {
            result = REACH_ERROR;
        }
    }

    ~reach_com_scope()
    {
        if (uninitialize) {
            CoUninitialize();
        }
    }
};

static void reach_system_controls_notify(
    reach_system_controls_adapter *adapter,
    uint32_t flags
)
{
    if (adapter == nullptr || adapter->callback == nullptr || flags == 0) {
        return;
    }

    adapter->callback(adapter->callback_user, flags);
    if (adapter->main_thread_id != 0) {
        PostThreadMessageW(adapter->main_thread_id, WM_NULL, 0, 0);
    }
}

static int32_t reach_bluetooth_worker_find_radio(
    winrt::Windows::Devices::Radios::Radio *out_radio
)
{
    namespace radios = winrt::Windows::Devices::Radios;

    if (out_radio == nullptr) {
        return 0;
    }

    *out_radio = nullptr;

    try {
        auto radio_list = radios::Radio::GetRadiosAsync().get();
        for (auto const &radio : radio_list) {
            if (radio.Kind() == radios::RadioKind::Bluetooth) {
                *out_radio = radio;
                return 1;
            }
        }
    } catch (winrt::hresult_error const &) {
        *out_radio = nullptr;
    }

    return 0;
}

static reach_result reach_bluetooth_worker_get_state(
    winrt::Windows::Devices::Radios::Radio *radio,
    reach_bluetooth_state *out_state)
{
    if (radio == nullptr || out_state == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_state = {};
    if (*radio == nullptr && !reach_bluetooth_worker_find_radio(radio)) {
        return REACH_OK;
    }

    try {
        out_state->available = 1;
        out_state->enabled =
            radio->State() == winrt::Windows::Devices::Radios::RadioState::On
                ? 1
                : 0;
    } catch (winrt::hresult_error const &) {
        *radio = nullptr;
        *out_state = {};
        return REACH_ERROR;
    }

    return REACH_OK;
}

static reach_result reach_bluetooth_worker_set_enabled(
    winrt::Windows::Devices::Radios::Radio *radio,
    int32_t enabled)
{
    namespace radios = winrt::Windows::Devices::Radios;

    if (radio == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    try {
        auto access = radios::Radio::RequestAccessAsync().get();
        if (access != radios::RadioAccessStatus::Allowed) {
            return REACH_ERROR;
        }

        if (*radio == nullptr && !reach_bluetooth_worker_find_radio(radio)) {
            return REACH_ERROR;
        }

        auto result = radio->SetStateAsync(
            enabled ? radios::RadioState::On : radios::RadioState::Off).get();
        return result == radios::RadioAccessStatus::Allowed
            ? REACH_OK
            : REACH_ERROR;
    } catch (winrt::hresult_error const &) {
        *radio = nullptr;
        return REACH_ERROR;
    }
}

static DWORD WINAPI reach_bluetooth_worker_thread(
    void *context
);

static void reach_system_controls_stop_bluetooth_worker(
    reach_system_controls_adapter *adapter
);

static reach_result reach_system_controls_start_bluetooth_worker(
    reach_system_controls_adapter *adapter
)
{
    if (adapter == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }
    if (adapter->bluetooth_thread != nullptr) {
        return REACH_OK;
    }

    InitializeCriticalSection(&adapter->bluetooth_lock);
    adapter->bluetooth_lock_initialized = 1;

    adapter->bluetooth_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    adapter->bluetooth_request = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    adapter->bluetooth_complete = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (adapter->bluetooth_stop == nullptr ||
        adapter->bluetooth_request == nullptr ||
        adapter->bluetooth_complete == nullptr) {
        reach_system_controls_stop_bluetooth_worker(adapter);
        return REACH_ERROR;
    }

    adapter->bluetooth_thread = CreateThread(
        nullptr,
        0,
        reach_bluetooth_worker_thread,
        adapter,
        0,
        nullptr);
    if (adapter->bluetooth_thread == nullptr) {
        reach_system_controls_stop_bluetooth_worker(adapter);
        return REACH_ERROR;
    }
    return REACH_OK;
}

static void reach_system_controls_stop_bluetooth_worker(
    reach_system_controls_adapter *adapter
)
{
    if (adapter == nullptr) {
        return;
    }

    if (adapter->bluetooth_stop != nullptr) {
        SetEvent(adapter->bluetooth_stop);
    }
    if (adapter->bluetooth_request != nullptr) {
        SetEvent(adapter->bluetooth_request);
    }
    if (adapter->bluetooth_thread != nullptr) {
        WaitForSingleObject(adapter->bluetooth_thread, INFINITE);
        CloseHandle(adapter->bluetooth_thread);
        adapter->bluetooth_thread = nullptr;
    }
    if (adapter->bluetooth_stop != nullptr) {
        CloseHandle(adapter->bluetooth_stop);
        adapter->bluetooth_stop = nullptr;
    }
    if (adapter->bluetooth_request != nullptr) {
        CloseHandle(adapter->bluetooth_request);
        adapter->bluetooth_request = nullptr;
    }
    if (adapter->bluetooth_complete != nullptr) {
        CloseHandle(adapter->bluetooth_complete);
        adapter->bluetooth_complete = nullptr;
    }
    if (adapter->bluetooth_lock_initialized) {
        DeleteCriticalSection(&adapter->bluetooth_lock);
        adapter->bluetooth_lock_initialized = 0;
    }
    adapter->bluetooth_request_type = REACH_BLUETOOTH_REQUEST_NONE;
    adapter->bluetooth_request_pending = 0;
    adapter->bluetooth_request_result = REACH_ERROR;
    adapter->bluetooth_request_state = {};
}

static void WINAPI reach_system_controls_wlan_notification(
    PWLAN_NOTIFICATION_DATA data,
    PVOID context
)
{
    (void)data;
    reach_system_controls_notify(
        static_cast<reach_system_controls_adapter *>(context),
        REACH_SYSTEM_CONTROLS_CHANGE_NETWORK);
}

static void reach_system_controls_copy_ascii(
    uint16_t *dst,
    size_t dst_count,
    const char *src
)
{
    if (dst == nullptr || dst_count == 0) {
        return;
    }

    size_t index = 0;
    if (src != nullptr) {
        while (index + 1 < dst_count && src[index] != 0) {
            dst[index] = (uint16_t)(unsigned char)src[index];
            ++index;
        }
    }
    dst[index] = 0;
}

static DWORD WINAPI reach_system_controls_addr_change_thread(
    void *context
)
{
    reach_system_controls_adapter *adapter =
        static_cast<reach_system_controls_adapter *>(context);
    if (adapter == nullptr ||
        adapter->addr_change == nullptr ||
        adapter->addr_change_stop == nullptr) {
        return 0;
    }

    HANDLE waits[2] = {
        adapter->addr_change_stop,
        adapter->addr_change
    };

    while (WaitForSingleObject(adapter->addr_change_stop, 0) == WAIT_TIMEOUT) {
        ResetEvent(adapter->addr_change);
        adapter->addr_change_overlapped = {};
        adapter->addr_change_overlapped.hEvent = adapter->addr_change;

        HANDLE notification_handle = nullptr;
        DWORD result = NotifyAddrChange(
            &notification_handle,
            &adapter->addr_change_overlapped);
        if (result != NO_ERROR && result != ERROR_IO_PENDING) {
            return 0;
        }

        DWORD wait_result = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
        if (wait_result == WAIT_OBJECT_0) {
            CancelIPChangeNotify(&adapter->addr_change_overlapped);
            return 0;
        }
        if (wait_result == WAIT_OBJECT_0 + 1) {
            reach_system_controls_notify(
                adapter,
                REACH_SYSTEM_CONTROLS_CHANGE_NETWORK);
        } else {
            CancelIPChangeNotify(&adapter->addr_change_overlapped);
            return 0;
        }
    }

    return 0;
}

static DWORD WINAPI reach_bluetooth_worker_thread(
    void *context
)
{
    reach_system_controls_adapter *adapter =
        static_cast<reach_system_controls_adapter *>(context);
    if (adapter == nullptr || adapter->bluetooth_stop == nullptr ||
        adapter->bluetooth_request == nullptr) {
        return 0;
    }

    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    } catch (winrt::hresult_error const &) {
        return 0;
    }

    winrt::Windows::Devices::Radios::Radio radio = nullptr;
    winrt::event_token state_token{};
    int32_t state_subscribed = 0;

    if (reach_bluetooth_worker_find_radio(&radio)) {
        try {
            state_token = radio.StateChanged(
                [adapter](
                    winrt::Windows::Devices::Radios::Radio const &,
                    winrt::Windows::Foundation::IInspectable const &) {
                    reach_system_controls_notify(
                        adapter,
                        REACH_SYSTEM_CONTROLS_CHANGE_BLUETOOTH);
                });
            state_subscribed = 1;
        } catch (winrt::hresult_error const &) {
            state_subscribed = 0;
        }
    }

    HANDLE waits[2] = {
        adapter->bluetooth_stop,
        adapter->bluetooth_request
    };

    int32_t running = 1;
    while (running) {
        DWORD wait_result = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
        if (wait_result == WAIT_OBJECT_0) {
            running = 0;
            break;
        }
        if (wait_result != WAIT_OBJECT_0 + 1) {
            continue;
        }

        reach_bluetooth_request_type request_type = REACH_BLUETOOTH_REQUEST_NONE;
        int32_t request_enabled = 0;
        if (adapter->bluetooth_lock_initialized) {
            EnterCriticalSection(&adapter->bluetooth_lock);
            request_type = adapter->bluetooth_request_type;
            request_enabled = adapter->bluetooth_request_enabled;
            LeaveCriticalSection(&adapter->bluetooth_lock);
        }

        reach_result result = REACH_ERROR;
        reach_bluetooth_state state = {};
        int32_t notify_bluetooth = 0;

        if (request_type == REACH_BLUETOOTH_REQUEST_GET_STATE) {
            result = reach_bluetooth_worker_get_state(&radio, &state);
        } else if (request_type == REACH_BLUETOOTH_REQUEST_SET_ENABLED) {
            result = reach_bluetooth_worker_set_enabled(&radio, request_enabled);
        } else if (request_type == REACH_BLUETOOTH_REQUEST_SET_ENABLED_ASYNC) {
            result = reach_bluetooth_worker_set_enabled(&radio, request_enabled);
            notify_bluetooth = 1;
        }

        if (adapter->bluetooth_lock_initialized) {
            EnterCriticalSection(&adapter->bluetooth_lock);
            adapter->bluetooth_request_result = result;
            adapter->bluetooth_request_state = state;
            adapter->bluetooth_request_type = REACH_BLUETOOTH_REQUEST_NONE;
            adapter->bluetooth_request_pending = 0;
            LeaveCriticalSection(&adapter->bluetooth_lock);
        }
        if (adapter->bluetooth_complete != nullptr) {
            SetEvent(adapter->bluetooth_complete);
        }
        if (notify_bluetooth) {
            reach_system_controls_notify(
                adapter,
                REACH_SYSTEM_CONTROLS_CHANGE_BLUETOOTH);
        }
    }

    if (state_subscribed) {
        try {
            radio.StateChanged(state_token);
        } catch (winrt::hresult_error const &) {
        }
    }
    radio = nullptr;
    winrt::uninit_apartment();
    return 0;
}

static void reach_system_controls_copy_utf16(
    uint16_t *dst,
    size_t dst_count,
    const wchar_t *src
)
{
    if (dst == nullptr || dst_count == 0) {
        return;
    }

    size_t index = 0;
    if (src != nullptr) {
        while (index + 1 < dst_count && src[index] != 0) {
            dst[index] = (uint16_t)src[index];
            ++index;
        }
    }
    dst[index] = 0;
}

static void reach_system_controls_copy_ssid(
    uint16_t *dst,
    size_t dst_count,
    const DOT11_SSID *ssid
)
{
    if (dst == nullptr || dst_count == 0) {
        return;
    }

    dst[0] = 0;
    if (ssid == nullptr) {
        return;
    }

    ULONG source_count = ssid->uSSIDLength;
    if (source_count > sizeof(ssid->ucSSID)) {
        source_count = sizeof(ssid->ucSSID);
    }

    size_t index = 0;
    while (index + 1 < dst_count && index < source_count) {
        dst[index] = (uint16_t)ssid->ucSSID[index];
        ++index;
    }
    dst[index] = 0;
}

static int32_t reach_system_controls_has_ethernet_connection(void)
{
    ULONG buffer_size = 0;
    ULONG flags = GAA_FLAG_SKIP_ANYCAST |
        GAA_FLAG_SKIP_MULTICAST |
        GAA_FLAG_SKIP_DNS_SERVER;

    ULONG result = GetAdaptersAddresses(
        AF_UNSPEC,
        flags,
        nullptr,
        nullptr,
        &buffer_size);
    if (result != ERROR_BUFFER_OVERFLOW || buffer_size == 0) {
        return 0;
    }

    IP_ADAPTER_ADDRESSES *addresses =
        reinterpret_cast<IP_ADAPTER_ADDRESSES *>(new (std::nothrow) unsigned char[buffer_size]);
    if (addresses == nullptr) {
        return 0;
    }

    result = GetAdaptersAddresses(
        AF_UNSPEC,
        flags,
        nullptr,
        addresses,
        &buffer_size);

    int32_t connected = 0;
    if (result == NO_ERROR) {
        for (IP_ADAPTER_ADDRESSES *adapter = addresses;
            adapter != nullptr;
            adapter = adapter->Next) {
            if (adapter->IfType == IF_TYPE_ETHERNET_CSMACD &&
                adapter->OperStatus == IfOperStatusUp &&
                adapter->FirstUnicastAddress != nullptr) {
                connected = 1;
                break;
            }
        }
    }

    delete[] reinterpret_cast<unsigned char *>(addresses);
    return connected;
}

static reach_result reach_system_controls_get_network_state(
    void *userdata,
    reach_network_state *out_state
)
{
    (void)userdata;

    if (out_state == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_state = {};
    out_state->kind = REACH_NETWORK_KIND_NONE;
    out_state->connected = 0;
    out_state->signal_strength = 0;
    reach_system_controls_copy_ascii(
        out_state->label,
        REACH_SYSTEM_NETWORK_LABEL_CAPACITY,
        "No internet");

    HANDLE wlan = nullptr;
    DWORD negotiated_version = 0;
    DWORD wlan_result = WlanOpenHandle(
        2,
        nullptr,
        &negotiated_version,
        &wlan);

    if (wlan_result == ERROR_SUCCESS && wlan != nullptr) {
        PWLAN_INTERFACE_INFO_LIST interfaces = nullptr;
        wlan_result = WlanEnumInterfaces(wlan, nullptr, &interfaces);
        if (wlan_result == ERROR_SUCCESS && interfaces != nullptr) {
            for (DWORD index = 0; index < interfaces->dwNumberOfItems; ++index) {
                WLAN_INTERFACE_INFO *info = &interfaces->InterfaceInfo[index];
                if (info->isState != wlan_interface_state_connected) {
                    continue;
                }

                DWORD data_size = 0;
                PWLAN_CONNECTION_ATTRIBUTES attributes = nullptr;
                WLAN_OPCODE_VALUE_TYPE opcode_type = wlan_opcode_value_type_invalid;
                DWORD query_result = WlanQueryInterface(
                    wlan,
                    &info->InterfaceGuid,
                    wlan_intf_opcode_current_connection,
                    nullptr,
                    &data_size,
                    reinterpret_cast<PVOID *>(&attributes),
                    &opcode_type);
                if (query_result == ERROR_SUCCESS && attributes != nullptr) {
                    out_state->kind = REACH_NETWORK_KIND_WIFI;
                    out_state->connected = 1;
                    out_state->signal_strength =
                        (int32_t)attributes->wlanAssociationAttributes.wlanSignalQuality;
                    if (out_state->signal_strength < 0) {
                        out_state->signal_strength = 0;
                    }
                    if (out_state->signal_strength > 100) {
                        out_state->signal_strength = 100;
                    }
                    reach_system_controls_copy_ssid(
                        out_state->label,
                        REACH_SYSTEM_NETWORK_LABEL_CAPACITY,
                        &attributes->wlanAssociationAttributes.dot11Ssid);
                    if (out_state->label[0] == 0) {
                        reach_system_controls_copy_ascii(
                            out_state->label,
                            REACH_SYSTEM_NETWORK_LABEL_CAPACITY,
                            "Wi-Fi");
                    }
                    WlanFreeMemory(attributes);
                    break;
                }
                if (attributes != nullptr) {
                    WlanFreeMemory(attributes);
                }
            }
        }
        if (interfaces != nullptr) {
            WlanFreeMemory(interfaces);
        }
        WlanCloseHandle(wlan, nullptr);
    }

    if (!out_state->connected && reach_system_controls_has_ethernet_connection()) {
        out_state->kind = REACH_NETWORK_KIND_ETHERNET;
        out_state->connected = 1;
        out_state->signal_strength = 100;
        reach_system_controls_copy_ascii(
            out_state->label,
            REACH_SYSTEM_NETWORK_LABEL_CAPACITY,
            "Ethernet");
    }

    return REACH_OK;
}

static reach_result reach_system_controls_get_bluetooth_state(
    void *userdata,
    reach_bluetooth_state *out_state
)
{
    if (out_state == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_state = {};
    reach_system_controls_adapter *adapter =
        static_cast<reach_system_controls_adapter *>(userdata);
    if (adapter == nullptr ||
        adapter->bluetooth_thread == nullptr ||
        adapter->bluetooth_request == nullptr ||
        adapter->bluetooth_complete == nullptr ||
        adapter->bluetooth_stop == nullptr ||
        !adapter->bluetooth_lock_initialized) {
        return REACH_OK;
    }
    if (WaitForSingleObject(adapter->bluetooth_thread, 0) == WAIT_OBJECT_0) {
        return REACH_ERROR;
    }

    ResetEvent(adapter->bluetooth_complete);
    EnterCriticalSection(&adapter->bluetooth_lock);
    if (adapter->bluetooth_request_pending) {
        LeaveCriticalSection(&adapter->bluetooth_lock);
        return REACH_ERROR;
    }
    adapter->bluetooth_request_pending = 1;
    adapter->bluetooth_request_type = REACH_BLUETOOTH_REQUEST_GET_STATE;
    adapter->bluetooth_request_state = {};
    adapter->bluetooth_request_result = REACH_ERROR;
    LeaveCriticalSection(&adapter->bluetooth_lock);
    SetEvent(adapter->bluetooth_request);

    HANDLE waits[3] = {
        adapter->bluetooth_complete,
        adapter->bluetooth_stop,
        adapter->bluetooth_thread
    };
    DWORD wait_result = WaitForMultipleObjects(3, waits, FALSE, INFINITE);
    if (wait_result != WAIT_OBJECT_0) {
        return REACH_ERROR;
    }

    reach_result result = REACH_ERROR;
    EnterCriticalSection(&adapter->bluetooth_lock);
    result = adapter->bluetooth_request_result;
    *out_state = adapter->bluetooth_request_state;
    LeaveCriticalSection(&adapter->bluetooth_lock);
    return result;
}

static reach_result reach_system_controls_set_bluetooth_enabled(
    void *userdata,
    int32_t enabled
)
{
    reach_system_controls_adapter *adapter =
        static_cast<reach_system_controls_adapter *>(userdata);
    if (adapter == nullptr ||
        adapter->bluetooth_thread == nullptr ||
        adapter->bluetooth_request == nullptr ||
        adapter->bluetooth_complete == nullptr ||
        adapter->bluetooth_stop == nullptr ||
        !adapter->bluetooth_lock_initialized) {
        return REACH_ERROR;
    }
    if (WaitForSingleObject(adapter->bluetooth_thread, 0) == WAIT_OBJECT_0) {
        return REACH_ERROR;
    }

    ResetEvent(adapter->bluetooth_complete);
    EnterCriticalSection(&adapter->bluetooth_lock);
    if (adapter->bluetooth_request_pending) {
        LeaveCriticalSection(&adapter->bluetooth_lock);
        return REACH_ERROR;
    }
    adapter->bluetooth_request_pending = 1;
    adapter->bluetooth_request_type = REACH_BLUETOOTH_REQUEST_SET_ENABLED;
    adapter->bluetooth_request_enabled = enabled ? 1 : 0;
    adapter->bluetooth_request_result = REACH_ERROR;
    LeaveCriticalSection(&adapter->bluetooth_lock);
    SetEvent(adapter->bluetooth_request);

    HANDLE waits[3] = {
        adapter->bluetooth_complete,
        adapter->bluetooth_stop,
        adapter->bluetooth_thread
    };
    DWORD wait_result = WaitForMultipleObjects(3, waits, FALSE, INFINITE);
    if (wait_result != WAIT_OBJECT_0) {
        return REACH_ERROR;
    }

    reach_result result = REACH_ERROR;
    EnterCriticalSection(&adapter->bluetooth_lock);
    result = adapter->bluetooth_request_result;
    LeaveCriticalSection(&adapter->bluetooth_lock);
    return result;
}

static reach_result reach_system_controls_request_bluetooth_enabled(
    void *userdata,
    int32_t enabled
)
{
    reach_system_controls_adapter *adapter =
        static_cast<reach_system_controls_adapter *>(userdata);
    if (adapter == nullptr ||
        adapter->bluetooth_thread == nullptr ||
        adapter->bluetooth_request == nullptr ||
        adapter->bluetooth_stop == nullptr ||
        !adapter->bluetooth_lock_initialized) {
        return REACH_ERROR;
    }
    if (WaitForSingleObject(adapter->bluetooth_thread, 0) == WAIT_OBJECT_0) {
        return REACH_ERROR;
    }

    EnterCriticalSection(&adapter->bluetooth_lock);
    if (adapter->bluetooth_request_pending) {
        LeaveCriticalSection(&adapter->bluetooth_lock);
        return REACH_ERROR;
    }
    adapter->bluetooth_request_pending = 1;
    adapter->bluetooth_request_type = REACH_BLUETOOTH_REQUEST_SET_ENABLED_ASYNC;
    adapter->bluetooth_request_enabled = enabled ? 1 : 0;
    adapter->bluetooth_request_result = REACH_ERROR;
    LeaveCriticalSection(&adapter->bluetooth_lock);
    SetEvent(adapter->bluetooth_request);
    return REACH_OK;
}

static reach_result reach_system_controls_get_power_state(
    void *userdata,
    reach_power_state *out_state
)
{
    (void)userdata;

    if (out_state == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_state = {};

    SYSTEM_POWER_STATUS status = {};
    if (!GetSystemPowerStatus(&status)) {
        return REACH_ERROR;
    }

    out_state->has_battery =
        (status.BatteryFlag & 128) == 0 &&
        status.BatteryFlag != 255
            ? 1
            : 0;
    out_state->battery_percent =
        status.BatteryLifePercent == 255
            ? -1
            : (int32_t)status.BatteryLifePercent;
    out_state->battery_saver_on = status.SystemStatusFlag == 1 ? 1 : 0;
    return REACH_OK;
}

static int32_t reach_system_controls_variant_bool(const VARIANT *variant)
{
    if (variant == nullptr) {
        return 0;
    }
    if (variant->vt == VT_BOOL) {
        return variant->boolVal == VARIANT_TRUE ? 1 : 0;
    }
    if (variant->vt == VT_I4 || variant->vt == VT_INT) {
        return variant->lVal != 0 ? 1 : 0;
    }
    return 0;
}

static int32_t reach_system_controls_variant_i32(
    const VARIANT *variant,
    int32_t *out_value
)
{
    if (variant == nullptr || out_value == nullptr) {
        return 0;
    }

    if (variant->vt == VT_UI1) {
        *out_value = (int32_t)variant->bVal;
        return 1;
    }
    if (variant->vt == VT_I4 || variant->vt == VT_INT) {
        *out_value = (int32_t)variant->lVal;
        return 1;
    }
    if (variant->vt == VT_UI4 || variant->vt == VT_UINT) {
        *out_value = (int32_t)variant->ulVal;
        return 1;
    }
    return 0;
}

static HRESULT reach_system_controls_connect_wmi(
    IWbemServices **out_services
)
{
    if (out_services == nullptr) {
        return E_INVALIDARG;
    }
    *out_services = nullptr;

    ComPtr<IWbemLocator> locator;
    HRESULT hr = CoCreateInstance(
        CLSID_WbemLocator,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&locator));
    if (FAILED(hr)) {
        return hr;
    }

    BSTR namespace_name = SysAllocString(L"ROOT\\WMI");
    if (namespace_name == nullptr) {
        return E_OUTOFMEMORY;
    }

    ComPtr<IWbemServices> services;
    hr = locator->ConnectServer(
        namespace_name,
        nullptr,
        nullptr,
        nullptr,
        0,
        nullptr,
        nullptr,
        &services);
    SysFreeString(namespace_name);
    if (FAILED(hr)) {
        return hr;
    }

    (void)CoSetProxyBlanket(
        services.Get(),
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        nullptr,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr,
        EOAC_NONE);

    *out_services = services.Detach();
    return S_OK;
}

static HRESULT reach_system_controls_find_brightness_instance(
    IWbemServices *services,
    int32_t *out_percent,
    BSTR *out_instance_name
)
{
    if (services == nullptr) {
        return E_INVALIDARG;
    }
    if (out_percent != nullptr) {
        *out_percent = 0;
    }
    if (out_instance_name != nullptr) {
        *out_instance_name = nullptr;
    }

    BSTR language = SysAllocString(L"WQL");
    BSTR query = SysAllocString(
        L"SELECT Active, CurrentBrightness, InstanceName FROM WmiMonitorBrightness");
    if (language == nullptr || query == nullptr) {
        if (language != nullptr) SysFreeString(language);
        if (query != nullptr) SysFreeString(query);
        return E_OUTOFMEMORY;
    }

    ComPtr<IEnumWbemClassObject> enumerator;
    HRESULT hr = services->ExecQuery(
        language,
        query,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr,
        &enumerator);
    SysFreeString(language);
    SysFreeString(query);
    if (FAILED(hr)) {
        return hr;
    }

    int32_t found = 0;
    int32_t selected_percent = 0;
    BSTR selected_instance = nullptr;

    while (true) {
        ComPtr<IWbemClassObject> object;
        ULONG returned = 0;
        hr = enumerator->Next(WBEM_INFINITE, 1, &object, &returned);
        if (FAILED(hr) || returned == 0) {
            break;
        }

        VARIANT active_variant = {};
        VARIANT brightness_variant = {};
        VARIANT instance_variant = {};
        VariantInit(&active_variant);
        VariantInit(&brightness_variant);
        VariantInit(&instance_variant);

        int32_t active = 0;
        int32_t percent = 0;
        HRESULT active_hr = object->Get(L"Active", 0, &active_variant, nullptr, nullptr);
        HRESULT brightness_hr = object->Get(L"CurrentBrightness", 0, &brightness_variant, nullptr, nullptr);
        HRESULT instance_hr = object->Get(L"InstanceName", 0, &instance_variant, nullptr, nullptr);
        if (SUCCEEDED(active_hr)) {
            active = reach_system_controls_variant_bool(&active_variant);
        }
        if (SUCCEEDED(brightness_hr) &&
            reach_system_controls_variant_i32(&brightness_variant, &percent)) {
            if (percent < 0) percent = 0;
            if (percent > 100) percent = 100;
            if (!found || active) {
                found = 1;
                selected_percent = percent;
                if (selected_instance != nullptr) {
                    SysFreeString(selected_instance);
                    selected_instance = nullptr;
                }
                if (SUCCEEDED(instance_hr) &&
                    instance_variant.vt == VT_BSTR &&
                    instance_variant.bstrVal != nullptr) {
                    selected_instance = SysAllocString(instance_variant.bstrVal);
                }
            }
        }

        VariantClear(&active_variant);
        VariantClear(&brightness_variant);
        VariantClear(&instance_variant);

        if (found && active) {
            break;
        }
    }

    if (!found) {
        if (selected_instance != nullptr) {
            SysFreeString(selected_instance);
        }
        return WBEM_E_NOT_FOUND;
    }

    if (out_percent != nullptr) {
        *out_percent = selected_percent;
    }
    if (out_instance_name != nullptr) {
        *out_instance_name = selected_instance;
    } else if (selected_instance != nullptr) {
        SysFreeString(selected_instance);
    }
    return S_OK;
}

static reach_result reach_system_controls_get_brightness_state(
    void *userdata,
    reach_brightness_state *out_state
)
{
    (void)userdata;

    if (out_state == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_state = {};

    reach_com_scope com_scope;
    if (com_scope.result != REACH_OK) {
        return REACH_ERROR;
    }

    ComPtr<IWbemServices> services;
    HRESULT hr = reach_system_controls_connect_wmi(&services);
    if (FAILED(hr)) {
        return REACH_ERROR;
    }

    int32_t percent = 0;
    hr = reach_system_controls_find_brightness_instance(
        services.Get(),
        &percent,
        nullptr);
    if (FAILED(hr)) {
        out_state->available = 0;
        out_state->level = 0.0f;
        return REACH_OK;
    }

    out_state->available = 1;
    out_state->level = (float)percent / 100.0f;
    return REACH_OK;
}

static reach_result reach_system_controls_set_brightness_level(
    void *userdata,
    float level
)
{
    (void)userdata;

    if (level < 0.0f) {
        level = 0.0f;
    }
    if (level > 1.0f) {
        level = 1.0f;
    }

    reach_com_scope com_scope;
    if (com_scope.result != REACH_OK) {
        return REACH_ERROR;
    }

    ComPtr<IWbemServices> services;
    HRESULT hr = reach_system_controls_connect_wmi(&services);
    if (FAILED(hr)) {
        return REACH_ERROR;
    }

    BSTR instance_name = nullptr;
    hr = reach_system_controls_find_brightness_instance(
        services.Get(),
        nullptr,
        &instance_name);
    if (FAILED(hr) || instance_name == nullptr) {
        if (instance_name != nullptr) {
            SysFreeString(instance_name);
        }
        return REACH_ERROR;
    }

    BSTR language = SysAllocString(L"WQL");
    BSTR query = SysAllocString(
        L"SELECT __PATH, InstanceName FROM WmiMonitorBrightnessMethods");
    if (language == nullptr || query == nullptr) {
        SysFreeString(instance_name);
        if (language != nullptr) SysFreeString(language);
        if (query != nullptr) SysFreeString(query);
        return REACH_ERROR;
    }

    ComPtr<IEnumWbemClassObject> enumerator;
    hr = services->ExecQuery(
        language,
        query,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr,
        &enumerator);
    SysFreeString(language);
    SysFreeString(query);
    if (FAILED(hr)) {
        SysFreeString(instance_name);
        return REACH_ERROR;
    }

    BSTR method_path = nullptr;
    while (true) {
        ComPtr<IWbemClassObject> object;
        ULONG returned = 0;
        hr = enumerator->Next(WBEM_INFINITE, 1, &object, &returned);
        if (FAILED(hr) || returned == 0) {
            break;
        }

        VARIANT path_variant = {};
        VARIANT instance_variant = {};
        VariantInit(&path_variant);
        VariantInit(&instance_variant);
        HRESULT path_hr = object->Get(L"__PATH", 0, &path_variant, nullptr, nullptr);
        HRESULT instance_hr = object->Get(L"InstanceName", 0, &instance_variant, nullptr, nullptr);
        if (SUCCEEDED(path_hr) &&
            SUCCEEDED(instance_hr) &&
            path_variant.vt == VT_BSTR &&
            instance_variant.vt == VT_BSTR &&
            path_variant.bstrVal != nullptr &&
            instance_variant.bstrVal != nullptr &&
            wcscmp(instance_variant.bstrVal, instance_name) == 0) {
            method_path = SysAllocString(path_variant.bstrVal);
            VariantClear(&path_variant);
            VariantClear(&instance_variant);
            break;
        }
        VariantClear(&path_variant);
        VariantClear(&instance_variant);
    }
    SysFreeString(instance_name);

    if (method_path == nullptr) {
        return REACH_ERROR;
    }

    BSTR class_name = SysAllocString(L"WmiMonitorBrightnessMethods");
    BSTR method_name = SysAllocString(L"WmiSetBrightness");
    if (class_name == nullptr || method_name == nullptr) {
        SysFreeString(method_path);
        if (class_name != nullptr) SysFreeString(class_name);
        if (method_name != nullptr) SysFreeString(method_name);
        return REACH_ERROR;
    }

    ComPtr<IWbemClassObject> class_object;
    ComPtr<IWbemClassObject> method_definition;
    ComPtr<IWbemClassObject> in_parameters;
    hr = services->GetObject(class_name, 0, nullptr, &class_object, nullptr);
    if (SUCCEEDED(hr)) {
        hr = class_object->GetMethod(method_name, 0, &method_definition, nullptr);
    }
    if (SUCCEEDED(hr)) {
        hr = method_definition->SpawnInstance(0, &in_parameters);
    }
    if (SUCCEEDED(hr)) {
        VARIANT timeout = {};
        VARIANT brightness = {};
        VariantInit(&timeout);
        VariantInit(&brightness);
        timeout.vt = VT_I4;
        timeout.lVal = 1;
        brightness.vt = VT_UI1;
        brightness.bVal = (BYTE)(level * 100.0f + 0.5f);
        hr = in_parameters->Put(L"Timeout", 0, &timeout, 0);
        if (SUCCEEDED(hr)) {
            hr = in_parameters->Put(L"Brightness", 0, &brightness, 0);
        }
        VariantClear(&timeout);
        VariantClear(&brightness);
    }
    if (SUCCEEDED(hr)) {
        hr = services->ExecMethod(
            method_path,
            method_name,
            0,
            nullptr,
            in_parameters.Get(),
            nullptr,
            nullptr);
    }

    SysFreeString(method_path);
    SysFreeString(class_name);
    SysFreeString(method_name);
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_system_controls_open_project_menu(
    void *userdata
)
{
    (void)userdata;

    HINSTANCE result = ShellExecuteW(
        nullptr,
        L"open",
        L"DisplaySwitch.exe",
        nullptr,
        nullptr,
        SW_SHOWNORMAL);
    return (INT_PTR)result > 32 ? REACH_OK : REACH_ERROR;
}

static reach_result reach_system_controls_start_watching(
    void *userdata,
    reach_system_controls_change_callback callback,
    void *callback_user
)
{
    reach_system_controls_adapter *adapter =
        static_cast<reach_system_controls_adapter *>(userdata);
    if (adapter == nullptr || callback == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    if (adapter->callback != nullptr) {
        return REACH_OK;
    }

    adapter->callback = callback;
    adapter->callback_user = callback_user;
    adapter->main_thread_id = GetCurrentThreadId();

    DWORD negotiated_version = 0;
    DWORD wlan_result = WlanOpenHandle(
        2,
        nullptr,
        &negotiated_version,
        &adapter->wlan);
    if (wlan_result == ERROR_SUCCESS && adapter->wlan != nullptr) {
        DWORD previous_sources = 0;
        wlan_result = WlanRegisterNotification(
            adapter->wlan,
            WLAN_NOTIFICATION_SOURCE_ACM | WLAN_NOTIFICATION_SOURCE_MSM,
            FALSE,
            reach_system_controls_wlan_notification,
            adapter,
            nullptr,
            &previous_sources);
        if (wlan_result != ERROR_SUCCESS) {
            WlanCloseHandle(adapter->wlan, nullptr);
            adapter->wlan = nullptr;
        }
    }

    adapter->addr_change = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    adapter->addr_change_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (adapter->addr_change != nullptr && adapter->addr_change_stop != nullptr) {
        adapter->addr_change_thread = CreateThread(
            nullptr,
            0,
            reach_system_controls_addr_change_thread,
            adapter,
            0,
            nullptr);
    }

    if (adapter->wlan == nullptr && adapter->addr_change_thread == nullptr) {
        if (adapter->addr_change != nullptr) {
            CloseHandle(adapter->addr_change);
            adapter->addr_change = nullptr;
        }
        if (adapter->addr_change_stop != nullptr) {
            CloseHandle(adapter->addr_change_stop);
            adapter->addr_change_stop = nullptr;
        }
        adapter->callback = nullptr;
        adapter->callback_user = nullptr;
        adapter->main_thread_id = 0;
        return REACH_ERROR;
    }

    return REACH_OK;
}

static void reach_system_controls_stop_watching(
    void *userdata
)
{
    reach_system_controls_adapter *adapter =
        static_cast<reach_system_controls_adapter *>(userdata);
    if (adapter == nullptr) {
        return;
    }

    adapter->callback = nullptr;
    adapter->callback_user = nullptr;
    adapter->main_thread_id = 0;
    reach_system_controls_stop_bluetooth_worker(adapter);

    if (adapter->addr_change_stop != nullptr) {
        SetEvent(adapter->addr_change_stop);
    }
    if (adapter->addr_change_thread != nullptr) {
        WaitForSingleObject(adapter->addr_change_thread, 2000);
        CloseHandle(adapter->addr_change_thread);
        adapter->addr_change_thread = nullptr;
    }
    if (adapter->addr_change != nullptr) {
        CloseHandle(adapter->addr_change);
        adapter->addr_change = nullptr;
    }
    if (adapter->addr_change_stop != nullptr) {
        CloseHandle(adapter->addr_change_stop);
        adapter->addr_change_stop = nullptr;
    }
    adapter->addr_change_overlapped = {};
    if (adapter->wlan != nullptr) {
        (void)WlanRegisterNotification(
            adapter->wlan,
            WLAN_NOTIFICATION_SOURCE_NONE,
            FALSE,
            nullptr,
            nullptr,
            nullptr,
            nullptr);
        WlanCloseHandle(adapter->wlan, nullptr);
        adapter->wlan = nullptr;
    }
}

static void reach_system_controls_destroy(
    void *userdata
)
{
    reach_system_controls_adapter *adapter =
        static_cast<reach_system_controls_adapter *>(userdata);
    reach_system_controls_stop_watching(adapter);
    delete adapter;
}

extern "C" reach_result reach_windows_create_system_controls(
    reach_system_controls_port *out_port
)
{
    if (out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};

    reach_system_controls_adapter *adapter = new (std::nothrow)
        reach_system_controls_adapter();
    if (adapter == nullptr) {
        return REACH_ERROR;
    }
    if (reach_system_controls_start_bluetooth_worker(adapter) != REACH_OK) {
        reach_system_controls_stop_bluetooth_worker(adapter);
    }

    out_port->userdata = adapter;
    out_port->get_network_state = reach_system_controls_get_network_state;
    out_port->get_bluetooth_state = reach_system_controls_get_bluetooth_state;
    out_port->set_bluetooth_enabled = reach_system_controls_set_bluetooth_enabled;
    out_port->request_bluetooth_enabled = reach_system_controls_request_bluetooth_enabled;
    out_port->get_power_state = reach_system_controls_get_power_state;
    out_port->get_brightness_state = reach_system_controls_get_brightness_state;
    out_port->set_brightness_level = reach_system_controls_set_brightness_level;
    out_port->open_project_menu = reach_system_controls_open_project_menu;
    out_port->start_watching = reach_system_controls_start_watching;
    out_port->stop_watching = reach_system_controls_stop_watching;
    out_port->destroy = reach_system_controls_destroy;
    return REACH_OK;
}
