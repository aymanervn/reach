#include "windows_adapters_internal.h"

#include "reach/ports/bluetooth.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <winrt/base.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <mutex>
#include <new>
#include <vector>

namespace enumeration = winrt::Windows::Devices::Enumeration;

/* Association-endpoint protocol ids for classic Bluetooth and Bluetooth LE. */
#define REACH_BLUETOOTH_CLASSIC_PROTOCOL L"{e0cbf06c-cd8b-4647-bb8a-263b43f0f974}"
#define REACH_BLUETOOTH_LE_PROTOCOL L"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}"

struct reach_bluetooth_entry
{
    reach_bluetooth_device device;
    enumeration::DeviceInformation information;
};

struct reach_bluetooth_adapter
{
    std::mutex mutex;
    std::vector<reach_bluetooth_entry> entries;
    enumeration::DeviceWatcher classic_watcher;
    enumeration::DeviceWatcher le_watcher;
    winrt::event_token classic_tokens[4];
    winrt::event_token le_tokens[4];
    enumeration::DeviceInformationCustomPairing pairing;
    winrt::event_token pairing_token;
    enumeration::DevicePairingRequestedEventArgs pairing_args;
    winrt::Windows::Foundation::Deferral pairing_deferral;
    reach_bluetooth_pairing_request pairing_request;
    uint16_t completed_device_id[REACH_BLUETOOTH_DEVICE_ID_CAPACITY];
    reach_bluetooth_pair_result completed_result;
    int32_t scanning;
    int32_t watchers_started;
    reach_bluetooth_change_callback callback;
    void *callback_user;
    DWORD main_thread_id;

    reach_bluetooth_adapter()
        : classic_watcher(nullptr), le_watcher(nullptr), pairing(nullptr), pairing_args(nullptr),
          pairing_deferral(nullptr), pairing_request{}, completed_device_id{},
          completed_result(REACH_BLUETOOTH_PAIR_RESULT_NONE), scanning(0), watchers_started(0),
          callback(nullptr), callback_user(nullptr), main_thread_id(0)
    {
    }
};

static void reach_bluetooth_notify(reach_bluetooth_adapter *adapter, uint32_t flags)
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

static void reach_bluetooth_copy_hstring(uint16_t *destination, size_t capacity,
                                         winrt::hstring const &value)
{
    reach_copy_utf16(destination, capacity, reinterpret_cast<const uint16_t *>(value.c_str()));
}

static int32_t reach_bluetooth_property_flag(enumeration::DeviceInformation const &information,
                                             winrt::hstring const &key)
{
    auto properties = information.Properties();
    if (!properties.HasKey(key))
    {
        return 0;
    }
    auto value = properties.Lookup(key);
    if (value == nullptr)
    {
        return 0;
    }
    try
    {
        return winrt::unbox_value<bool>(value) ? 1 : 0;
    }
    catch (winrt::hresult_error const &)
    {
        return 0;
    }
}

static reach_bluetooth_device_kind reach_bluetooth_kind_from_category(winrt::hstring const &value)
{
    const wchar_t *text = value.c_str();
    if (wcsncmp(text, L"Audio", 5) == 0)
    {
        return REACH_BLUETOOTH_DEVICE_AUDIO;
    }
    if (wcsncmp(text, L"Input.Keyboard", 14) == 0)
    {
        return REACH_BLUETOOTH_DEVICE_KEYBOARD;
    }
    if (wcsncmp(text, L"Input.Mouse", 11) == 0 || wcsncmp(text, L"Input.Pointing", 14) == 0)
    {
        return REACH_BLUETOOTH_DEVICE_MOUSE;
    }
    if (wcsncmp(text, L"Phone", 5) == 0)
    {
        return REACH_BLUETOOTH_DEVICE_PHONE;
    }
    if (wcsncmp(text, L"Computer", 8) == 0)
    {
        return REACH_BLUETOOTH_DEVICE_COMPUTER;
    }
    if (wcsncmp(text, L"Wearable", 8) == 0)
    {
        return REACH_BLUETOOTH_DEVICE_WEARABLE;
    }
    if (wcsncmp(text, L"Printer", 7) == 0 || wcsncmp(text, L"Imaging", 7) == 0)
    {
        return REACH_BLUETOOTH_DEVICE_PRINTER;
    }
    return REACH_BLUETOOTH_DEVICE_UNKNOWN;
}

static void reach_bluetooth_read_icon(enumeration::DeviceInformation const &information,
                                      uint16_t *out_path)
{
    out_path[0] = 0;
    auto properties = information.Properties();
    winrt::hstring key = L"System.Devices.Icon";
    if (!properties.HasKey(key))
    {
        return;
    }
    auto value = properties.Lookup(key);
    if (value == nullptr)
    {
        return;
    }
    try
    {
        winrt::hstring icon = winrt::unbox_value<winrt::hstring>(value);
        wchar_t expanded[REACH_BLUETOOTH_ICON_PATH_CAPACITY] = {};
        DWORD written =
            ExpandEnvironmentStringsW(icon.c_str(), expanded, REACH_BLUETOOTH_ICON_PATH_CAPACITY);
        const wchar_t *source =
            written > 0 && written <= REACH_BLUETOOTH_ICON_PATH_CAPACITY ? expanded : icon.c_str();
        reach_copy_utf16(out_path, REACH_BLUETOOTH_ICON_PATH_CAPACITY,
                         reinterpret_cast<const uint16_t *>(source));
    }
    catch (winrt::hresult_error const &)
    {
        out_path[0] = 0;
    }
}

static reach_bluetooth_device_kind
reach_bluetooth_read_kind(enumeration::DeviceInformation const &information)
{
    auto properties = information.Properties();
    winrt::hstring key = L"System.Devices.Aep.Category";
    if (!properties.HasKey(key))
    {
        return REACH_BLUETOOTH_DEVICE_UNKNOWN;
    }
    auto value = properties.Lookup(key);
    if (value == nullptr)
    {
        return REACH_BLUETOOTH_DEVICE_UNKNOWN;
    }
    try
    {
        auto categories = winrt::unbox_value<winrt::com_array<winrt::hstring>>(value);
        for (auto const &category : categories)
        {
            reach_bluetooth_device_kind kind = reach_bluetooth_kind_from_category(category);
            if (kind != REACH_BLUETOOTH_DEVICE_UNKNOWN)
            {
                return kind;
            }
        }
    }
    catch (winrt::hresult_error const &)
    {
    }
    return REACH_BLUETOOTH_DEVICE_UNKNOWN;
}

static void reach_bluetooth_fill_device(reach_bluetooth_device *device,
                                        enumeration::DeviceInformation const &information)
{
    *device = {};
    reach_bluetooth_copy_hstring(device->id, REACH_BLUETOOTH_DEVICE_ID_CAPACITY, information.Id());
    reach_bluetooth_copy_hstring(device->name, REACH_BLUETOOTH_NAME_CAPACITY, information.Name());
    reach_bluetooth_read_icon(information, device->icon_path);
    device->kind = reach_bluetooth_read_kind(information);
    device->connected = reach_bluetooth_property_flag(
        information, winrt::hstring(L"System.Devices.Aep.IsConnected"));
    try
    {
        device->paired = information.Pairing().IsPaired() ? 1 : 0;
        device->can_pair = information.Pairing().CanPair() ? 1 : 0;
    }
    catch (winrt::hresult_error const &)
    {
        device->paired = 0;
        device->can_pair = 0;
    }
}

static void reach_bluetooth_store(reach_bluetooth_adapter *adapter,
                                  enumeration::DeviceInformation const &information)
{
    reach_bluetooth_device device = {};
    reach_bluetooth_fill_device(&device, information);
    if (device.id[0] == 0 || device.name[0] == 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(adapter->mutex);
    for (size_t index = 0; index < adapter->entries.size(); ++index)
    {
        if (!reach_bluetooth_device_id_equal(adapter->entries[index].device.id, device.id))
        {
            continue;
        }
        adapter->entries[index].device = device;
        adapter->entries[index].information = information;
        return;
    }
    if (adapter->entries.size() >= REACH_BLUETOOTH_MAX_DEVICES)
    {
        return;
    }
    reach_bluetooth_entry entry = {device, information};
    adapter->entries.push_back(entry);
}

static void reach_bluetooth_update(reach_bluetooth_adapter *adapter,
                                   enumeration::DeviceInformationUpdate const &update)
{
    winrt::hstring id = update.Id();
    std::lock_guard<std::mutex> lock(adapter->mutex);
    for (size_t index = 0; index < adapter->entries.size(); ++index)
    {
        uint16_t candidate[REACH_BLUETOOTH_DEVICE_ID_CAPACITY] = {};
        reach_bluetooth_copy_hstring(candidate, REACH_BLUETOOTH_DEVICE_ID_CAPACITY, id);
        if (!reach_bluetooth_device_id_equal(adapter->entries[index].device.id, candidate))
        {
            continue;
        }
        try
        {
            adapter->entries[index].information.Update(update);
            reach_bluetooth_fill_device(&adapter->entries[index].device,
                                        adapter->entries[index].information);
        }
        catch (winrt::hresult_error const &)
        {
        }
        return;
    }
}

static void reach_bluetooth_remove(reach_bluetooth_adapter *adapter,
                                   enumeration::DeviceInformationUpdate const &update)
{
    uint16_t candidate[REACH_BLUETOOTH_DEVICE_ID_CAPACITY] = {};
    reach_bluetooth_copy_hstring(candidate, REACH_BLUETOOTH_DEVICE_ID_CAPACITY, update.Id());

    std::lock_guard<std::mutex> lock(adapter->mutex);
    for (size_t index = 0; index < adapter->entries.size(); ++index)
    {
        if (!reach_bluetooth_device_id_equal(adapter->entries[index].device.id, candidate))
        {
            continue;
        }
        adapter->entries.erase(adapter->entries.begin() + (long long)index);
        return;
    }
}

static enumeration::DeviceWatcher reach_bluetooth_create_watcher(const wchar_t *protocol)
{
    wchar_t selector[256] = {};
    wsprintfW(selector, L"System.Devices.Aep.ProtocolId:=\"%ls\"", protocol);

    auto properties = winrt::single_threaded_vector<winrt::hstring>(
        {L"System.Devices.Aep.IsConnected", L"System.Devices.Aep.IsPaired",
         L"System.Devices.Aep.CanPair", L"System.Devices.Aep.Category", L"System.Devices.Icon"});

    return enumeration::DeviceInformation::CreateWatcher(
        selector, properties, enumeration::DeviceInformationKind::AssociationEndpoint);
}

static void reach_bluetooth_attach_watcher(reach_bluetooth_adapter *adapter,
                                           enumeration::DeviceWatcher const &watcher,
                                           winrt::event_token *tokens)
{
    tokens[0] = watcher.Added(
        [adapter](enumeration::DeviceWatcher const &,
                  enumeration::DeviceInformation const &information)
        {
            reach_bluetooth_store(adapter, information);
            reach_bluetooth_notify(adapter, REACH_BLUETOOTH_CHANGE_DEVICES);
        });
    tokens[1] = watcher.Updated(
        [adapter](enumeration::DeviceWatcher const &,
                  enumeration::DeviceInformationUpdate const &update)
        {
            reach_bluetooth_update(adapter, update);
            reach_bluetooth_notify(adapter, REACH_BLUETOOTH_CHANGE_DEVICES);
        });
    tokens[2] = watcher.Removed(
        [adapter](enumeration::DeviceWatcher const &,
                  enumeration::DeviceInformationUpdate const &update)
        {
            reach_bluetooth_remove(adapter, update);
            reach_bluetooth_notify(adapter, REACH_BLUETOOTH_CHANGE_DEVICES);
        });
    tokens[3] = watcher.EnumerationCompleted(
        [adapter](enumeration::DeviceWatcher const &,
                  winrt::Windows::Foundation::IInspectable const &)
        {
            reach_bluetooth_notify(adapter, REACH_BLUETOOTH_CHANGE_SCAN_COMPLETE |
                                                REACH_BLUETOOTH_CHANGE_DEVICES);
        });
}

static void reach_bluetooth_detach_watcher(enumeration::DeviceWatcher const &watcher,
                                           winrt::event_token *tokens)
{
    if (watcher == nullptr)
    {
        return;
    }
    watcher.Added(tokens[0]);
    watcher.Updated(tokens[1]);
    watcher.Removed(tokens[2]);
    watcher.EnumerationCompleted(tokens[3]);
}

static void reach_bluetooth_stop_watchers(reach_bluetooth_adapter *adapter)
{
    if (!adapter->watchers_started)
    {
        return;
    }
    try
    {
        if (adapter->classic_watcher != nullptr)
        {
            adapter->classic_watcher.Stop();
        }
        if (adapter->le_watcher != nullptr)
        {
            adapter->le_watcher.Stop();
        }
    }
    catch (winrt::hresult_error const &)
    {
    }
    reach_bluetooth_detach_watcher(adapter->classic_watcher, adapter->classic_tokens);
    reach_bluetooth_detach_watcher(adapter->le_watcher, adapter->le_tokens);
    adapter->classic_watcher = nullptr;
    adapter->le_watcher = nullptr;
    adapter->watchers_started = 0;
}

static void reach_bluetooth_thread_attach(void *)
{
    try
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    }
    catch (winrt::hresult_error const &)
    {
    }
}

static void reach_bluetooth_thread_detach(void *)
{
    winrt::uninit_apartment();
}

static reach_result reach_bluetooth_set_scan_enabled(void *userdata, int32_t enabled)
{
    reach_bluetooth_adapter *adapter = static_cast<reach_bluetooth_adapter *>(userdata);
    if (adapter == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (!enabled)
    {
        reach_bluetooth_stop_watchers(adapter);
        adapter->scanning = 0;
        return REACH_OK;
    }
    if (adapter->watchers_started)
    {
        return REACH_OK;
    }

    try
    {
        adapter->classic_watcher = reach_bluetooth_create_watcher(REACH_BLUETOOTH_CLASSIC_PROTOCOL);
        adapter->le_watcher = reach_bluetooth_create_watcher(REACH_BLUETOOTH_LE_PROTOCOL);
        reach_bluetooth_attach_watcher(adapter, adapter->classic_watcher, adapter->classic_tokens);
        reach_bluetooth_attach_watcher(adapter, adapter->le_watcher, adapter->le_tokens);
        adapter->classic_watcher.Start();
        adapter->le_watcher.Start();
        adapter->watchers_started = 1;
        adapter->scanning = 1;
        return REACH_OK;
    }
    catch (winrt::hresult_error const &)
    {
        reach_bluetooth_stop_watchers(adapter);
        return REACH_ERROR;
    }
}

static reach_result reach_bluetooth_read_devices(void *userdata,
                                                 reach_bluetooth_device_list *out_devices)
{
    reach_bluetooth_adapter *adapter = static_cast<reach_bluetooth_adapter *>(userdata);
    if (adapter == nullptr || out_devices == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_devices = {};
    {
        std::lock_guard<std::mutex> lock(adapter->mutex);
        for (size_t index = 0;
             index < adapter->entries.size() && out_devices->count < REACH_BLUETOOTH_MAX_DEVICES;
             ++index)
        {
            out_devices->devices[out_devices->count++] = adapter->entries[index].device;
        }
    }
    reach_bluetooth_device_list_normalize(out_devices);
    return REACH_OK;
}

static reach_result reach_bluetooth_read_pairing_request(void *userdata,
                                                         reach_bluetooth_pairing_request *out)
{
    reach_bluetooth_adapter *adapter = static_cast<reach_bluetooth_adapter *>(userdata);
    if (adapter == nullptr || out == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    std::lock_guard<std::mutex> lock(adapter->mutex);
    *out = adapter->pairing_request;
    return REACH_OK;
}

static void
reach_bluetooth_on_pairing_requested(reach_bluetooth_adapter *adapter,
                                     enumeration::DevicePairingRequestedEventArgs const &args)
{
    if (args.PairingKind() == enumeration::DevicePairingKinds::ConfirmOnly)
    {
        args.Accept();
        return;
    }
    if (args.PairingKind() != enumeration::DevicePairingKinds::ConfirmPinMatch)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(adapter->mutex);
        adapter->pairing_args = args;
        adapter->pairing_deferral = args.GetDeferral();
        adapter->pairing_request = {};
        adapter->pairing_request.active = 1;
        adapter->pairing_request.needs_confirmation = 1;
        reach_bluetooth_copy_hstring(adapter->pairing_request.device_id,
                                     REACH_BLUETOOTH_DEVICE_ID_CAPACITY,
                                     args.DeviceInformation().Id());
        reach_bluetooth_copy_hstring(adapter->pairing_request.pin, REACH_BLUETOOTH_PIN_CAPACITY,
                                     args.Pin());
    }
    reach_bluetooth_notify(adapter, REACH_BLUETOOTH_CHANGE_PAIRING);
}

static reach_bluetooth_pair_result
reach_bluetooth_result_from_status(enumeration::DevicePairingResultStatus status)
{
    switch (status)
    {
    case enumeration::DevicePairingResultStatus::Paired:
    case enumeration::DevicePairingResultStatus::AlreadyPaired:
        return REACH_BLUETOOTH_PAIR_RESULT_SUCCEEDED;
    case enumeration::DevicePairingResultStatus::RejectedByHandler:
    case enumeration::DevicePairingResultStatus::PairingCanceled:
        return REACH_BLUETOOTH_PAIR_RESULT_REJECTED;
    case enumeration::DevicePairingResultStatus::ConnectionRejected:
    case enumeration::DevicePairingResultStatus::Failed:
        return REACH_BLUETOOTH_PAIR_RESULT_FAILED;
    default:
        return REACH_BLUETOOTH_PAIR_RESULT_FAILED;
    }
}

static reach_result reach_bluetooth_pair(void *userdata, const uint16_t *device_id)
{
    reach_bluetooth_adapter *adapter = static_cast<reach_bluetooth_adapter *>(userdata);
    if (adapter == nullptr || device_id == nullptr || device_id[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    enumeration::DeviceInformation information(nullptr);
    {
        std::lock_guard<std::mutex> lock(adapter->mutex);
        for (size_t index = 0; index < adapter->entries.size(); ++index)
        {
            if (reach_bluetooth_device_id_equal(adapter->entries[index].device.id, device_id))
            {
                information = adapter->entries[index].information;
                break;
            }
        }
    }
    if (information == nullptr)
    {
        return REACH_ERROR;
    }

    try
    {
        if (adapter->pairing != nullptr && adapter->pairing_token.value != 0)
        {
            adapter->pairing.PairingRequested(adapter->pairing_token);
            adapter->pairing_token = {};
        }
        adapter->pairing = information.Pairing().Custom();
        adapter->pairing_token = adapter->pairing.PairingRequested(
            [adapter](enumeration::DeviceInformationCustomPairing const &,
                      enumeration::DevicePairingRequestedEventArgs const &args)
            { reach_bluetooth_on_pairing_requested(adapter, args); });

        uint16_t requested_id[REACH_BLUETOOTH_DEVICE_ID_CAPACITY] = {};
        reach_copy_utf16(requested_id, REACH_BLUETOOTH_DEVICE_ID_CAPACITY, device_id);

        auto operation =
            adapter->pairing.PairAsync(enumeration::DevicePairingKinds::ConfirmOnly |
                                           enumeration::DevicePairingKinds::ConfirmPinMatch,
                                       enumeration::DevicePairingProtectionLevel::Default);
        operation.Completed(
            [adapter, requested_id](
                winrt::Windows::Foundation::IAsyncOperation<enumeration::DevicePairingResult> const
                    &async,
                winrt::Windows::Foundation::AsyncStatus)
            {
                reach_bluetooth_pair_result result = REACH_BLUETOOTH_PAIR_RESULT_FAILED;
                try
                {
                    result = reach_bluetooth_result_from_status(async.GetResults().Status());
                }
                catch (winrt::hresult_error const &)
                {
                }
                {
                    std::lock_guard<std::mutex> lock(adapter->mutex);
                    reach_copy_utf16(adapter->completed_device_id,
                                     REACH_BLUETOOTH_DEVICE_ID_CAPACITY, requested_id);
                    adapter->completed_result = result;
                    adapter->pairing_request = {};
                    adapter->pairing_args = nullptr;
                    adapter->pairing_deferral = nullptr;
                }
                reach_bluetooth_notify(adapter, REACH_BLUETOOTH_CHANGE_PAIRING |
                                                    REACH_BLUETOOTH_CHANGE_DEVICES);
            });
        return REACH_OK;
    }
    catch (winrt::hresult_error const &)
    {
        return REACH_ERROR;
    }
}

static reach_result reach_bluetooth_respond_pairing(void *userdata, int32_t accept)
{
    reach_bluetooth_adapter *adapter = static_cast<reach_bluetooth_adapter *>(userdata);
    if (adapter == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    enumeration::DevicePairingRequestedEventArgs args(nullptr);
    winrt::Windows::Foundation::Deferral deferral(nullptr);
    {
        std::lock_guard<std::mutex> lock(adapter->mutex);
        if (!adapter->pairing_request.active)
        {
            return REACH_ERROR;
        }
        args = adapter->pairing_args;
        deferral = adapter->pairing_deferral;
        adapter->pairing_request = {};
        adapter->pairing_args = nullptr;
        adapter->pairing_deferral = nullptr;
    }

    try
    {
        if (accept && args != nullptr)
        {
            args.Accept();
        }
        if (deferral != nullptr)
        {
            deferral.Complete();
        }
    }
    catch (winrt::hresult_error const &)
    {
        return REACH_ERROR;
    }
    return REACH_OK;
}

static reach_result reach_bluetooth_take_pair_result(void *userdata, uint16_t *out_device_id,
                                                     size_t device_id_capacity,
                                                     reach_bluetooth_pair_result *out_result)
{
    reach_bluetooth_adapter *adapter = static_cast<reach_bluetooth_adapter *>(userdata);
    if (adapter == nullptr || out_result == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(adapter->mutex);
    *out_result = adapter->completed_result;
    if (out_device_id != nullptr && device_id_capacity > 0)
    {
        reach_copy_utf16(out_device_id, device_id_capacity, adapter->completed_device_id);
    }
    adapter->completed_result = REACH_BLUETOOTH_PAIR_RESULT_NONE;
    adapter->completed_device_id[0] = 0;
    return REACH_OK;
}

static reach_result reach_bluetooth_unpair(void *userdata, const uint16_t *device_id)
{
    reach_bluetooth_adapter *adapter = static_cast<reach_bluetooth_adapter *>(userdata);
    if (adapter == nullptr || device_id == nullptr || device_id[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    enumeration::DeviceInformation information(nullptr);
    {
        std::lock_guard<std::mutex> lock(adapter->mutex);
        for (size_t index = 0; index < adapter->entries.size(); ++index)
        {
            if (reach_bluetooth_device_id_equal(adapter->entries[index].device.id, device_id))
            {
                information = adapter->entries[index].information;
                break;
            }
        }
    }
    if (information == nullptr)
    {
        return REACH_ERROR;
    }

    try
    {
        auto result = information.Pairing().UnpairAsync().get();
        reach_bluetooth_notify(adapter, REACH_BLUETOOTH_CHANGE_DEVICES);
        return result.Status() == enumeration::DeviceUnpairingResultStatus::Unpaired ||
                       result.Status() == enumeration::DeviceUnpairingResultStatus::AlreadyUnpaired
                   ? REACH_OK
                   : REACH_ERROR;
    }
    catch (winrt::hresult_error const &)
    {
        return REACH_ERROR;
    }
}

static reach_result reach_bluetooth_start_watching(void *userdata,
                                                   reach_bluetooth_change_callback callback,
                                                   void *callback_user)
{
    reach_bluetooth_adapter *adapter = static_cast<reach_bluetooth_adapter *>(userdata);
    if (adapter == nullptr || callback == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    adapter->callback = callback;
    adapter->callback_user = callback_user;
    adapter->main_thread_id = GetCurrentThreadId();
    return REACH_OK;
}

static void reach_bluetooth_stop_watching(void *userdata)
{
    reach_bluetooth_adapter *adapter = static_cast<reach_bluetooth_adapter *>(userdata);
    if (adapter == nullptr)
    {
        return;
    }
    reach_bluetooth_stop_watchers(adapter);
    if (adapter->pairing != nullptr && adapter->pairing_token.value != 0)
    {
        try
        {
            adapter->pairing.PairingRequested(adapter->pairing_token);
        }
        catch (winrt::hresult_error const &)
        {
        }
        adapter->pairing_token = {};
    }
    adapter->pairing = nullptr;
    adapter->callback = nullptr;
    adapter->callback_user = nullptr;
}

static void reach_bluetooth_destroy(void *userdata)
{
    reach_bluetooth_adapter *adapter = static_cast<reach_bluetooth_adapter *>(userdata);
    if (adapter == nullptr)
    {
        return;
    }
    reach_bluetooth_stop_watching(adapter);
    {
        std::lock_guard<std::mutex> lock(adapter->mutex);
        adapter->entries.clear();
    }
    delete adapter;
}

reach_result reach_windows_create_bluetooth(reach_bluetooth_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_port = {};

    reach_bluetooth_adapter *adapter = new (std::nothrow) reach_bluetooth_adapter();
    if (adapter == nullptr)
    {
        return REACH_ERROR;
    }

    out_port->userdata = adapter;
    out_port->thread_attach = reach_bluetooth_thread_attach;
    out_port->thread_detach = reach_bluetooth_thread_detach;
    out_port->set_scan_enabled = reach_bluetooth_set_scan_enabled;
    out_port->read_devices = reach_bluetooth_read_devices;
    out_port->read_pairing_request = reach_bluetooth_read_pairing_request;
    out_port->pair = reach_bluetooth_pair;
    out_port->respond_pairing = reach_bluetooth_respond_pairing;
    out_port->take_pair_result = reach_bluetooth_take_pair_result;
    out_port->unpair = reach_bluetooth_unpair;
    out_port->start_watching = reach_bluetooth_start_watching;
    out_port->stop_watching = reach_bluetooth_stop_watching;
    out_port->destroy = reach_bluetooth_destroy;
    return REACH_OK;
}
