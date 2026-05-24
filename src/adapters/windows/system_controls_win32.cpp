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

#include <new>

struct reach_system_controls_adapter {
    HANDLE wlan;
    HANDLE addr_change;
    HANDLE addr_change_thread;
    HANDLE addr_change_stop;
    OVERLAPPED addr_change_overlapped;
    DWORD main_thread_id;
    reach_system_controls_change_callback callback;
    void *callback_user;
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
    (void)userdata;

    if (out_state == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    out_state->available = 0;
    out_state->enabled = 0;
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

static reach_result reach_system_controls_get_brightness_state(
    void *userdata,
    reach_brightness_state *out_state
)
{
    (void)userdata;

    if (out_state == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    out_state->available = 0;
    out_state->level = 0.0f;
    return REACH_OK;
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

    out_port->userdata = adapter;
    out_port->get_network_state = reach_system_controls_get_network_state;
    out_port->get_bluetooth_state = reach_system_controls_get_bluetooth_state;
    out_port->get_power_state = reach_system_controls_get_power_state;
    out_port->get_brightness_state = reach_system_controls_get_brightness_state;
    out_port->open_project_menu = reach_system_controls_open_project_menu;
    out_port->start_watching = reach_system_controls_start_watching;
    out_port->stop_watching = reach_system_controls_stop_watching;
    out_port->destroy = reach_system_controls_destroy;
    return REACH_OK;
}
