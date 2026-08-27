#include "windows_adapters_internal.h"
#include "windows_icon_handle_internal.h"

#include "reach/ports/audio_volume.h"

#include <windows.h>
#include <shellapi.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <devpkey.h>
#include <propvarutil.h>

#include <new>
#include <wchar.h>

struct reach_audio_volume_adapter
{
    uint64_t session_icon_ids[REACH_AUDIO_VOLUME_MAX_SESSIONS];
    uint64_t output_device_icon_ids[REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES];
    size_t session_icon_count;
    size_t output_device_icon_count;
    int32_t com_initialized;

    reach_audio_volume_change_callback callback;
    void *callback_user;
    DWORD main_thread_id;
    HANDLE watch_thread;
    HANDLE watch_stop;
    HANDLE watch_rebind;
};

struct IPolicyConfig : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(LPCWSTR, WAVEFORMATEX **) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(LPCWSTR, INT, WAVEFORMATEX **) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(LPCWSTR, WAVEFORMATEX *, WAVEFORMATEX *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(LPCWSTR, INT, PINT64, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(LPCWSTR, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(LPCWSTR, void *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(LPCWSTR, void *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(LPCWSTR, const PROPERTYKEY &,
                                                       PROPVARIANT *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(LPCWSTR, const PROPERTYKEY &,
                                                       PROPVARIANT *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(LPCWSTR, ERole) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(LPCWSTR, INT) = 0;
};

static const GUID REACH_CLSID_PolicyConfigClient = {
    0x870af99c, 0x171d, 0x4f9e, {0xaf, 0x0d, 0xe6, 0x3d, 0xf4, 0x0c, 0x2b, 0xc9}};

static const GUID REACH_IID_IPolicyConfig = {
    0xf8679f50, 0x850a, 0x41cf, {0x9c, 0x72, 0x43, 0x0f, 0x29, 0x02, 0x90, 0xc8}};

static float reach_audio_volume_clamp01(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    if (value > 1.0f)
    {
        return 1.0f;
    }
    return value;
}

static void reach_audio_volume_copy_wide(uint16_t *dst, size_t dst_count, const wchar_t *src)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }
    if (src == nullptr)
    {
        dst[0] = 0;
        return;
    }
    (void)reach_copy_utf16(dst, dst_count, reinterpret_cast<const uint16_t *>(src));
}

static const wchar_t *reach_audio_volume_basename(const wchar_t *path)
{
    if (path == nullptr)
    {
        return nullptr;
    }

    const wchar_t *name = path;
    for (const wchar_t *cursor = path; *cursor != 0; ++cursor)
    {
        if (*cursor == L'\\' || *cursor == L'/')
        {
            name = cursor + 1;
        }
    }
    return name;
}

static void reach_audio_volume_destroy_session_icons(reach_audio_volume_adapter *adapter)
{
    if (adapter == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < adapter->session_icon_count; ++index)
    {
        reach_windows_icon_id_release(adapter->session_icon_ids[index]);
        adapter->session_icon_ids[index] = 0;
    }

    adapter->session_icon_count = 0;
}

static void reach_audio_volume_destroy_output_device_icons(reach_audio_volume_adapter *adapter)
{
    if (adapter == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < adapter->output_device_icon_count; ++index)
    {
        reach_windows_icon_id_release(adapter->output_device_icon_ids[index]);
        adapter->output_device_icon_ids[index] = 0;
    }

    adapter->output_device_icon_count = 0;
}

static reach_result
reach_audio_volume_create_default_endpoint_volume(IAudioEndpointVolume **out_endpoint)
{
    if (out_endpoint == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_endpoint = nullptr;

    IMMDeviceEnumerator *enumerator = nullptr;
    IMMDevice *device = nullptr;
    IAudioEndpointVolume *endpoint = nullptr;

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&enumerator));

    if (SUCCEEDED(hr))
    {
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device);
        if (FAILED(hr))
        {
            hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        }
    }

    if (SUCCEEDED(hr))
    {
        hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                              reinterpret_cast<void **>(&endpoint));
    }

    if (device != nullptr)
    {
        device->Release();
    }
    if (enumerator != nullptr)
    {
        enumerator->Release();
    }

    if (FAILED(hr) || endpoint == nullptr)
    {
        if (endpoint != nullptr)
        {
            endpoint->Release();
        }
        return REACH_ERROR;
    }

    *out_endpoint = endpoint;
    return REACH_OK;
}

static reach_result reach_audio_volume_create_session_manager(IAudioSessionManager2 **out_manager)
{
    if (out_manager == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_manager = nullptr;

    IMMDeviceEnumerator *enumerator = nullptr;
    IMMDevice *device = nullptr;
    IAudioSessionManager2 *manager = nullptr;

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&enumerator));

    if (SUCCEEDED(hr))
    {
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    }
    if (SUCCEEDED(hr))
    {
        hr = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                              reinterpret_cast<void **>(&manager));
    }

    if (enumerator != nullptr)
    {
        enumerator->Release();
    }
    if (device != nullptr)
    {
        device->Release();
    }

    if (FAILED(hr))
    {
        if (manager != nullptr)
        {
            manager->Release();
        }
        return REACH_ERROR;
    }

    *out_manager = manager;
    return REACH_OK;
}

static int reach_audio_volume_path_for_process(DWORD process_id, wchar_t *out_path,
                                               DWORD out_path_count)
{
    if (out_path == nullptr || out_path_count == 0)
    {
        return 0;
    }

    out_path[0] = 0;
    if (process_id == 0)
    {
        return 0;
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr)
    {
        return 0;
    }

    DWORD path_count = out_path_count;
    int ok = QueryFullProcessImageNameW(process, 0, out_path, &path_count) ? 1 : 0;

    CloseHandle(process);
    return ok;
}

static void reach_audio_volume_label_for_process(DWORD process_id, uint16_t *out_label,
                                                 size_t out_label_count)
{
    if (out_label == nullptr || out_label_count == 0)
    {
        return;
    }

    out_label[0] = 0;

    wchar_t path[MAX_PATH] = {};
    if (reach_audio_volume_path_for_process(process_id, path, MAX_PATH))
    {
        reach_audio_volume_copy_wide(out_label, out_label_count, reach_audio_volume_basename(path));
    }
}

static HICON reach_audio_volume_icon_for_process(DWORD process_id)
{
    wchar_t path[MAX_PATH] = {};
    if (!reach_audio_volume_path_for_process(process_id, path, MAX_PATH))
    {
        return nullptr;
    }

    HICON large_icon = nullptr;
    HICON small_icon = nullptr;
    UINT count = ExtractIconExW(path, 0, &large_icon, &small_icon, 1);
    if (large_icon != nullptr)
    {
        DestroyIcon(large_icon);
    }
    if (count == 0 || small_icon == nullptr)
    {
        if (small_icon != nullptr)
        {
            DestroyIcon(small_icon);
        }
        return nullptr;
    }
    return small_icon;
}

static void reach_audio_volume_copy_device_id(uint16_t *dst, size_t dst_count, const wchar_t *src)
{
    reach_audio_volume_copy_wide(dst, dst_count, src);
}

static void reach_audio_volume_utf16_to_wchar(wchar_t *dst, size_t dst_count, const uint16_t *src)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }

    size_t index = 0;
    if (src != nullptr)
    {
        while (index + 1 < dst_count && src[index] != 0)
        {
            dst[index] = (wchar_t)src[index];
            ++index;
        }
    }
    dst[index] = 0;
}

static void reach_audio_volume_fill_fallback_device_label(reach_audio_output_device *device)
{
    if (device == nullptr || device->label[0] != 0)
    {
        return;
    }

    reach_audio_volume_copy_wide(device->label, REACH_AUDIO_VOLUME_DEVICE_LABEL_CAPACITY,
                                 L"Output device");
}

static void reach_audio_volume_read_property_string(IPropertyStore *store, const PROPERTYKEY &key,
                                                    wchar_t *out_value, size_t out_value_count)
{
    if (store == nullptr || out_value == nullptr || out_value_count == 0)
    {
        return;
    }

    out_value[0] = 0;

    PROPVARIANT value = {};
    PropVariantInit(&value);
    if (SUCCEEDED(store->GetValue(key, &value)) && value.vt == VT_LPWSTR &&
        value.pwszVal != nullptr)
    {
        wcsncpy_s(out_value, out_value_count, value.pwszVal, _TRUNCATE);
    }
    PropVariantClear(&value);
}

static HICON reach_audio_volume_icon_from_path(const wchar_t *icon_path)
{
    if (icon_path == nullptr || icon_path[0] == 0)
    {
        return nullptr;
    }

    wchar_t expanded[512] = {};
    DWORD expanded_count = ExpandEnvironmentStringsW(
        icon_path, expanded, (DWORD)(sizeof(expanded) / sizeof(expanded[0])));
    const wchar_t *source =
        expanded_count > 0 && expanded_count <= sizeof(expanded) / sizeof(expanded[0]) ? expanded
                                                                                       : icon_path;

    wchar_t path[512] = {};
    wcsncpy_s(path, source, _TRUNCATE);

    int icon_index = 0;
    wchar_t *comma = wcsrchr(path, L',');
    if (comma != nullptr)
    {
        *comma = 0;
        icon_index = _wtoi(comma + 1);
    }

    HICON large_icon = nullptr;
    HICON small_icon = nullptr;
    UINT count = ExtractIconExW(path, icon_index, &large_icon, &small_icon, 1);
    if (large_icon != nullptr)
    {
        DestroyIcon(large_icon);
    }
    if (count == 0 || small_icon == nullptr)
    {
        if (small_icon != nullptr)
        {
            DestroyIcon(small_icon);
        }
        return nullptr;
    }
    return small_icon;
}

static void reach_audio_volume_fill_fallback_label(reach_audio_volume_session *session)
{
    if (session == nullptr || session->label[0] != 0)
    {
        return;
    }

    if (session->is_system_sounds)
    {
        reach_audio_volume_copy_wide(session->label, REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY,
                                     L"System Sounds");
        return;
    }

    reach_audio_volume_copy_wide(session->label, REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY, L"App");
}

static reach_result reach_audio_volume_read_session(IAudioSessionControl *control,
                                                    reach_audio_volume_session *out_session)
{
    if (control == nullptr || out_session == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_session = {};

    AudioSessionState state = AudioSessionStateInactive;
    HRESULT state_hr = control->GetState(&state);
    if (FAILED(state_hr) || state != AudioSessionStateActive)
    {
        return REACH_ERROR;
    }

    IAudioSessionControl2 *control2 = nullptr;
    ISimpleAudioVolume *simple_volume = nullptr;
    LPWSTR instance_id = nullptr;
    LPWSTR display_name = nullptr;

    HRESULT hr = control->QueryInterface(IID_PPV_ARGS(&control2));
    if (SUCCEEDED(hr))
    {
        hr = control->QueryInterface(IID_PPV_ARGS(&simple_volume));
    }
    if (SUCCEEDED(hr))
    {
        hr = control2->GetSessionInstanceIdentifier(&instance_id);
    }

    if (SUCCEEDED(hr) && instance_id != nullptr && instance_id[0] != 0)
    {
        reach_audio_volume_copy_wide(out_session->session_instance_id,
                                     REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY, instance_id);
    }
    else if (SUCCEEDED(hr))
    {
        hr = E_FAIL;
    }

    if (SUCCEEDED(hr))
    {
        DWORD process_id = 0;
        if (SUCCEEDED(control2->GetProcessId(&process_id)))
        {
            out_session->process_id = process_id;
        }

        HRESULT system_hr = control2->IsSystemSoundsSession();
        out_session->is_system_sounds = system_hr == S_OK ? 1 : 0;

        float level = 0.0f;
        BOOL muted = FALSE;
        if (SUCCEEDED(simple_volume->GetMasterVolume(&level)))
        {
            out_session->level = reach_audio_volume_clamp01(level);
        }
        if (SUCCEEDED(simple_volume->GetMute(&muted)))
        {
            out_session->muted = muted ? 1 : 0;
        }

        if (out_session->is_system_sounds)
        {
            reach_audio_volume_copy_wide(
                out_session->label, REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY, L"System Sounds");
        }
        else if (SUCCEEDED(control->GetDisplayName(&display_name)) && display_name != nullptr &&
                 display_name[0] != 0)
        {
            reach_audio_volume_copy_wide(out_session->label,
                                         REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY, display_name);
        }
        else
        {
            reach_audio_volume_label_for_process(out_session->process_id, out_session->label,
                                                 REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY);
        }

        reach_audio_volume_fill_fallback_label(out_session);
    }

    if (display_name != nullptr)
    {
        CoTaskMemFree(display_name);
    }
    if (instance_id != nullptr)
    {
        CoTaskMemFree(instance_id);
    }
    if (simple_volume != nullptr)
    {
        simple_volume->Release();
    }
    if (control2 != nullptr)
    {
        control2->Release();
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}
static reach_result reach_audio_volume_get_state(void *userdata,
                                                 reach_audio_volume_state *out_state)
{
    reach_audio_volume_adapter *adapter = static_cast<reach_audio_volume_adapter *>(userdata);

    if (adapter == nullptr || out_state == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    IAudioEndpointVolume *endpoint = nullptr;
    reach_result result = reach_audio_volume_create_default_endpoint_volume(&endpoint);
    if (result != REACH_OK)
    {
        return result;
    }

    float level = 0.0f;
    BOOL muted = FALSE;

    HRESULT hr = endpoint->GetMasterVolumeLevelScalar(&level);
    if (SUCCEEDED(hr))
    {
        hr = endpoint->GetMute(&muted);
    }

    endpoint->Release();

    if (FAILED(hr))
    {
        return REACH_ERROR;
    }

    out_state->level = reach_audio_volume_clamp01(level);
    out_state->muted = muted ? 1 : 0;

    return REACH_OK;
}

static reach_result reach_audio_volume_set_level(void *userdata, float level)
{
    reach_audio_volume_adapter *adapter = static_cast<reach_audio_volume_adapter *>(userdata);

    if (adapter == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    IAudioEndpointVolume *endpoint = nullptr;
    reach_result result = reach_audio_volume_create_default_endpoint_volume(&endpoint);
    if (result != REACH_OK)
    {
        return result;
    }

    HRESULT hr = endpoint->SetMasterVolumeLevelScalar(reach_audio_volume_clamp01(level), nullptr);
    endpoint->Release();

    if (FAILED(hr))
    {
        return REACH_ERROR;
    }

    return REACH_OK;
}

static reach_result reach_audio_volume_set_muted(void *userdata, int32_t muted)
{
    reach_audio_volume_adapter *adapter = static_cast<reach_audio_volume_adapter *>(userdata);

    if (adapter == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    IAudioEndpointVolume *endpoint = nullptr;
    reach_result result = reach_audio_volume_create_default_endpoint_volume(&endpoint);
    if (result != REACH_OK)
    {
        return result;
    }

    HRESULT hr = endpoint->SetMute(muted ? TRUE : FALSE, nullptr);
    endpoint->Release();

    if (FAILED(hr))
    {
        return REACH_ERROR;
    }

    return REACH_OK;
}

static reach_result reach_audio_volume_list_sessions(void *userdata,
                                                     reach_audio_volume_session_list *out_list)
{
    reach_audio_volume_adapter *adapter = static_cast<reach_audio_volume_adapter *>(userdata);

    if (adapter == nullptr || out_list == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_audio_volume_destroy_session_icons(adapter);
    *out_list = {};

    IAudioSessionManager2 *manager = nullptr;
    IAudioSessionEnumerator *enumerator = nullptr;
    reach_result result = reach_audio_volume_create_session_manager(&manager);
    if (result != REACH_OK)
    {
        return result;
    }

    HRESULT hr = manager->GetSessionEnumerator(&enumerator);
    int count = 0;
    if (SUCCEEDED(hr))
    {
        hr = enumerator->GetCount(&count);
    }

    for (int index = 0;
         SUCCEEDED(hr) && index < count && out_list->count < REACH_AUDIO_VOLUME_MAX_SESSIONS;
         ++index)
    {
        IAudioSessionControl *control = nullptr;
        hr = enumerator->GetSession(index, &control);
        if (SUCCEEDED(hr) && control != nullptr)
        {
            reach_audio_volume_session session = {};
            if (reach_audio_volume_read_session(control, &session) == REACH_OK &&
                session.session_instance_id[0] != 0)
            {
                HICON icon = reach_audio_volume_icon_for_process(session.process_id);
                if (icon != nullptr)
                {
                    if (adapter->session_icon_count < REACH_AUDIO_VOLUME_MAX_SESSIONS)
                    {
                        uint64_t icon_id = reach_windows_icon_id_from_hicon(icon);
                        if (icon_id != 0)
                        {
                            adapter->session_icon_ids[adapter->session_icon_count++] = icon_id;
                            session.icon_id = icon_id;
                        }
                    }
                    else
                    {
                        DestroyIcon(icon);
                    }
                }
                out_list->sessions[out_list->count++] = session;
            }
        }
        if (control != nullptr)
        {
            control->Release();
        }
    }

    if (enumerator != nullptr)
    {
        enumerator->Release();
    }
    if (manager != nullptr)
    {
        manager->Release();
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_audio_volume_read_output_device(reach_audio_volume_adapter *adapter,
                                                          IMMDevice *device,
                                                          const wchar_t *default_device_id,
                                                          reach_audio_output_device *out_device)
{
    if (adapter == nullptr || device == nullptr || out_device == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_device = {};

    LPWSTR device_id = nullptr;
    IPropertyStore *store = nullptr;

    HRESULT hr = device->GetId(&device_id);
    if (SUCCEEDED(hr) && device_id != nullptr)
    {
        reach_audio_volume_copy_device_id(out_device->device_id,
                                          REACH_AUDIO_VOLUME_DEVICE_ID_CAPACITY, device_id);
        out_device->is_default =
            default_device_id != nullptr && wcscmp(device_id, default_device_id) == 0 ? 1 : 0;
    }

    if (SUCCEEDED(hr))
    {
        hr = device->OpenPropertyStore(STGM_READ, &store);
    }

    if (SUCCEEDED(hr) && store != nullptr)
    {
        wchar_t label[REACH_AUDIO_VOLUME_DEVICE_LABEL_CAPACITY] = {};
        reach_audio_volume_read_property_string(store, PKEY_Device_FriendlyName, label,
                                                REACH_AUDIO_VOLUME_DEVICE_LABEL_CAPACITY);
        reach_audio_volume_copy_wide(out_device->label, REACH_AUDIO_VOLUME_DEVICE_LABEL_CAPACITY,
                                     label);

        wchar_t icon_path[512] = {};
        reach_audio_volume_read_property_string(store, PKEY_DeviceClass_IconPath, icon_path,
                                                sizeof(icon_path) / sizeof(icon_path[0]));
        HICON icon = reach_audio_volume_icon_from_path(icon_path);
        if (icon != nullptr)
        {
            if (adapter->output_device_icon_count < REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES)
            {
                uint64_t icon_id = reach_windows_icon_id_from_hicon(icon);
                if (icon_id != 0)
                {
                    adapter->output_device_icon_ids[adapter->output_device_icon_count++] = icon_id;
                    out_device->icon_id = icon_id;
                }
            }
            else
            {
                DestroyIcon(icon);
            }
        }
    }

    reach_audio_volume_fill_fallback_device_label(out_device);

    if (store != nullptr)
    {
        store->Release();
    }
    if (device_id != nullptr)
    {
        CoTaskMemFree(device_id);
    }

    return out_device->device_id[0] != 0 ? REACH_OK : REACH_ERROR;
}

static reach_result reach_audio_volume_list_output_devices(void *userdata,
                                                           reach_audio_output_device_list *out_list)
{
    reach_audio_volume_adapter *adapter = static_cast<reach_audio_volume_adapter *>(userdata);

    if (adapter == nullptr || out_list == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_audio_volume_destroy_output_device_icons(adapter);
    *out_list = {};

    IMMDeviceEnumerator *enumerator = nullptr;
    IMMDeviceCollection *collection = nullptr;
    IMMDevice *default_device = nullptr;
    LPWSTR default_device_id = nullptr;

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&enumerator));

    if (SUCCEEDED(hr))
    {
        hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
    }

    HRESULT default_hr = E_FAIL;
    if (SUCCEEDED(hr))
    {
        default_hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &default_device);
        if (SUCCEEDED(default_hr) && default_device != nullptr)
        {
            default_hr = default_device->GetId(&default_device_id);
        }
    }

    UINT count = 0;
    if (SUCCEEDED(hr))
    {
        hr = collection->GetCount(&count);
    }

    for (UINT index = 0;
         SUCCEEDED(hr) && index < count && out_list->count < REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES;
         ++index)
    {
        IMMDevice *device = nullptr;
        hr = collection->Item(index, &device);
        if (SUCCEEDED(hr) && device != nullptr)
        {
            reach_audio_output_device output = {};
            if (reach_audio_volume_read_output_device(adapter, device, default_device_id,
                                                      &output) == REACH_OK)
            {
                out_list->devices[out_list->count++] = output;
            }
        }
        if (device != nullptr)
        {
            device->Release();
        }
    }

    if (default_device_id != nullptr)
    {
        CoTaskMemFree(default_device_id);
    }
    if (default_device != nullptr)
    {
        default_device->Release();
    }
    if (collection != nullptr)
    {
        collection->Release();
    }
    if (enumerator != nullptr)
    {
        enumerator->Release();
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_audio_volume_with_session_volume(const uint16_t *session_instance_id,
                                                           ISimpleAudioVolume **out_volume)
{
    if (session_instance_id == nullptr || out_volume == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_volume = nullptr;

    IAudioSessionManager2 *manager = nullptr;
    IAudioSessionEnumerator *enumerator = nullptr;
    reach_result result = reach_audio_volume_create_session_manager(&manager);
    if (result != REACH_OK)
    {
        return result;
    }

    HRESULT hr = manager->GetSessionEnumerator(&enumerator);
    int count = 0;
    if (SUCCEEDED(hr))
    {
        hr = enumerator->GetCount(&count);
    }

    for (int index = 0; SUCCEEDED(hr) && index < count; ++index)
    {
        IAudioSessionControl *control = nullptr;
        hr = enumerator->GetSession(index, &control);
        if (FAILED(hr) || control == nullptr)
        {
            if (control != nullptr)
            {
                control->Release();
            }
            continue;
        }

        reach_audio_volume_session session = {};
        if (reach_audio_volume_read_session(control, &session) == REACH_OK &&
            reach_utf16_equal(session.session_instance_id, session_instance_id))
        {
            ISimpleAudioVolume *volume = nullptr;
            hr = control->QueryInterface(IID_PPV_ARGS(&volume));
            control->Release();
            if (SUCCEEDED(hr))
            {
                *out_volume = volume;
                break;
            }
        }
        else
        {
            control->Release();
        }
    }

    if (enumerator != nullptr)
    {
        enumerator->Release();
    }
    if (manager != nullptr)
    {
        manager->Release();
    }

    return *out_volume != nullptr ? REACH_OK : REACH_ERROR;
}

static reach_result reach_audio_volume_set_session_level(void *userdata,
                                                         const uint16_t *session_instance_id,
                                                         float level)
{
    (void)userdata;

    ISimpleAudioVolume *volume = nullptr;
    reach_result result = reach_audio_volume_with_session_volume(session_instance_id, &volume);
    if (result != REACH_OK)
    {
        return result;
    }

    HRESULT hr = volume->SetMasterVolume(reach_audio_volume_clamp01(level), nullptr);
    volume->Release();
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_audio_volume_set_session_muted(void *userdata,
                                                         const uint16_t *session_instance_id,
                                                         int32_t muted)
{
    (void)userdata;

    ISimpleAudioVolume *volume = nullptr;
    reach_result result = reach_audio_volume_with_session_volume(session_instance_id, &volume);
    if (result != REACH_OK)
    {
        return result;
    }

    HRESULT hr = volume->SetMute(muted ? TRUE : FALSE, nullptr);
    volume->Release();
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_audio_volume_set_default_output_device(void *userdata,
                                                                 const uint16_t *device_id)
{
    (void)userdata;

    if (device_id == nullptr || device_id[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    wchar_t id[REACH_AUDIO_VOLUME_DEVICE_ID_CAPACITY] = {};
    reach_audio_volume_utf16_to_wchar(id, REACH_AUDIO_VOLUME_DEVICE_ID_CAPACITY, device_id);
    if (id[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    IPolicyConfig *policy = nullptr;
    HRESULT hr = CoCreateInstance(REACH_CLSID_PolicyConfigClient, nullptr, CLSCTX_ALL,
                                  REACH_IID_IPolicyConfig, reinterpret_cast<void **>(&policy));

    if (FAILED(hr) || policy == nullptr)
    {
        return REACH_ERROR;
    }

    HRESULT multimedia_hr = policy->SetDefaultEndpoint(id, eMultimedia);
    HRESULT console_hr = policy->SetDefaultEndpoint(id, eConsole);
    HRESULT communications_hr = policy->SetDefaultEndpoint(id, eCommunications);

    policy->Release();

    return SUCCEEDED(multimedia_hr) || SUCCEEDED(console_hr) || SUCCEEDED(communications_hr)
               ? REACH_OK
               : REACH_ERROR;
}

static void reach_audio_volume_notify(reach_audio_volume_adapter *adapter)
{
    if (adapter == nullptr || adapter->callback == nullptr)
    {
        return;
    }

    adapter->callback(adapter->callback_user);
    if (adapter->main_thread_id != 0)
    {
        PostThreadMessageW(adapter->main_thread_id, WM_NULL, 0, 0);
    }
}

class reach_audio_volume_endpoint_watcher : public IAudioEndpointVolumeCallback
{
  public:
    explicit reach_audio_volume_endpoint_watcher(reach_audio_volume_adapter *adapter)
        : references(1), adapter(adapter)
    {
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return (ULONG)InterlockedIncrement(&references);
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        LONG remaining = InterlockedDecrement(&references);
        if (remaining == 0)
        {
            delete this;
        }
        return (ULONG)remaining;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **out_interface) override
    {
        if (out_interface == nullptr)
        {
            return E_POINTER;
        }
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IAudioEndpointVolumeCallback))
        {
            *out_interface = static_cast<IAudioEndpointVolumeCallback *>(this);
            AddRef();
            return S_OK;
        }
        *out_interface = nullptr;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA data) override
    {
        (void)data;
        reach_audio_volume_notify(adapter);
        return S_OK;
    }

  private:
    LONG references;
    reach_audio_volume_adapter *adapter;
};

class reach_audio_volume_device_watcher : public IMMNotificationClient
{
  public:
    explicit reach_audio_volume_device_watcher(HANDLE rebind) : references(1), rebind(rebind)
    {
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return (ULONG)InterlockedIncrement(&references);
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        LONG remaining = InterlockedDecrement(&references);
        if (remaining == 0)
        {
            delete this;
        }
        return (ULONG)remaining;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **out_interface) override
    {
        if (out_interface == nullptr)
        {
            return E_POINTER;
        }
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IMMNotificationClient))
        {
            *out_interface = static_cast<IMMNotificationClient *>(this);
            AddRef();
            return S_OK;
        }
        *out_interface = nullptr;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role,
                                                     LPCWSTR device_id) override
    {
        (void)role;
        (void)device_id;
        if (flow == eRender && rebind != nullptr)
        {
            SetEvent(rebind);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override
    {
        return S_OK;
    }

  private:
    LONG references;
    HANDLE rebind;
};

static void reach_audio_volume_unbind_endpoint(IAudioEndpointVolume **endpoint,
                                               reach_audio_volume_endpoint_watcher **watcher)
{
    if (*endpoint != nullptr && *watcher != nullptr)
    {
        (void)(*endpoint)->UnregisterControlChangeNotify(*watcher);
    }
    if (*watcher != nullptr)
    {
        (*watcher)->Release();
        *watcher = nullptr;
    }
    if (*endpoint != nullptr)
    {
        (*endpoint)->Release();
        *endpoint = nullptr;
    }
}

static void reach_audio_volume_bind_endpoint(reach_audio_volume_adapter *adapter,
                                             IAudioEndpointVolume **endpoint,
                                             reach_audio_volume_endpoint_watcher **watcher)
{
    if (reach_audio_volume_create_default_endpoint_volume(endpoint) != REACH_OK)
    {
        return;
    }

    *watcher = new (std::nothrow) reach_audio_volume_endpoint_watcher(adapter);
    if (*watcher == nullptr || FAILED((*endpoint)->RegisterControlChangeNotify(*watcher)))
    {
        reach_audio_volume_unbind_endpoint(endpoint, watcher);
    }
}

static DWORD WINAPI reach_audio_volume_watch_thread(LPVOID parameter)
{
    reach_audio_volume_adapter *adapter = static_cast<reach_audio_volume_adapter *>(parameter);

    HRESULT com_hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    int32_t uninitialize = com_hr == S_OK;
    if (FAILED(com_hr) && com_hr != RPC_E_CHANGED_MODE)
    {
        return 0;
    }

    IMMDeviceEnumerator *enumerator = nullptr;
    reach_audio_volume_device_watcher *device_watcher = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS(&enumerator))))
    {
        device_watcher =
            new (std::nothrow) reach_audio_volume_device_watcher(adapter->watch_rebind);
        if (device_watcher != nullptr &&
            FAILED(enumerator->RegisterEndpointNotificationCallback(device_watcher)))
        {
            device_watcher->Release();
            device_watcher = nullptr;
        }
    }

    IAudioEndpointVolume *endpoint = nullptr;
    reach_audio_volume_endpoint_watcher *endpoint_watcher = nullptr;
    reach_audio_volume_bind_endpoint(adapter, &endpoint, &endpoint_watcher);

    HANDLE waits[2] = {adapter->watch_stop, adapter->watch_rebind};
    for (;;)
    {
        DWORD signalled = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
        if (signalled != WAIT_OBJECT_0 + 1)
        {
            break;
        }

        reach_audio_volume_unbind_endpoint(&endpoint, &endpoint_watcher);
        reach_audio_volume_bind_endpoint(adapter, &endpoint, &endpoint_watcher);
        reach_audio_volume_notify(adapter);
    }

    reach_audio_volume_unbind_endpoint(&endpoint, &endpoint_watcher);
    if (enumerator != nullptr)
    {
        if (device_watcher != nullptr)
        {
            (void)enumerator->UnregisterEndpointNotificationCallback(device_watcher);
            device_watcher->Release();
        }
        enumerator->Release();
    }

    if (uninitialize)
    {
        CoUninitialize();
    }
    return 0;
}

static void reach_audio_volume_stop_watching(void *userdata)
{
    reach_audio_volume_adapter *adapter = static_cast<reach_audio_volume_adapter *>(userdata);
    if (adapter == nullptr)
    {
        return;
    }

    if (adapter->watch_thread != nullptr)
    {
        SetEvent(adapter->watch_stop);
        WaitForSingleObject(adapter->watch_thread, 2000);
        CloseHandle(adapter->watch_thread);
        adapter->watch_thread = nullptr;
    }
    if (adapter->watch_stop != nullptr)
    {
        CloseHandle(adapter->watch_stop);
        adapter->watch_stop = nullptr;
    }
    if (adapter->watch_rebind != nullptr)
    {
        CloseHandle(adapter->watch_rebind);
        adapter->watch_rebind = nullptr;
    }

    adapter->callback = nullptr;
    adapter->callback_user = nullptr;
    adapter->main_thread_id = 0;
}

static reach_result reach_audio_volume_start_watching(void *userdata,
                                                      reach_audio_volume_change_callback callback,
                                                      void *callback_user)
{
    reach_audio_volume_adapter *adapter = static_cast<reach_audio_volume_adapter *>(userdata);
    if (adapter == nullptr || callback == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (adapter->watch_thread != nullptr)
    {
        return REACH_OK;
    }

    adapter->callback = callback;
    adapter->callback_user = callback_user;
    adapter->main_thread_id = GetCurrentThreadId();

    adapter->watch_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    adapter->watch_rebind = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (adapter->watch_stop != nullptr && adapter->watch_rebind != nullptr)
    {
        adapter->watch_thread =
            CreateThread(nullptr, 0, reach_audio_volume_watch_thread, adapter, 0, nullptr);
    }

    if (adapter->watch_thread == nullptr)
    {
        reach_audio_volume_stop_watching(adapter);
        return REACH_ERROR;
    }
    return REACH_OK;
}

static void reach_audio_volume_destroy(void *userdata)
{
    reach_audio_volume_adapter *adapter = static_cast<reach_audio_volume_adapter *>(userdata);

    if (adapter == nullptr)
    {
        return;
    }

    reach_audio_volume_stop_watching(adapter);
    reach_audio_volume_destroy_session_icons(adapter);
    reach_audio_volume_destroy_output_device_icons(adapter);

    if (adapter->com_initialized)
    {
        CoUninitialize();
        adapter->com_initialized = 0;
    }

    delete adapter;
}

extern "C" reach_result reach_windows_create_audio_volume(reach_audio_volume_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};

    reach_audio_volume_adapter *adapter = new (std::nothrow) reach_audio_volume_adapter();

    if (adapter == nullptr)
    {
        return REACH_ERROR;
    }

    adapter->session_icon_count = 0;
    adapter->session_icon_count = 0;
    adapter->output_device_icon_count = 0;
    adapter->com_initialized = 0;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (hr == S_OK)
    {
        adapter->com_initialized = 1;
    }
    else if (hr == S_FALSE || hr == RPC_E_CHANGED_MODE)
    {
        hr = S_OK;
    }

    if (FAILED(hr))
    {
        delete adapter;
        return REACH_ERROR;
    }

    out_port->userdata = adapter;
    out_port->get_state = reach_audio_volume_get_state;
    out_port->set_level = reach_audio_volume_set_level;
    out_port->set_muted = reach_audio_volume_set_muted;
    out_port->list_sessions = reach_audio_volume_list_sessions;
    out_port->list_output_devices = reach_audio_volume_list_output_devices;
    out_port->set_session_level = reach_audio_volume_set_session_level;
    out_port->set_session_muted = reach_audio_volume_set_session_muted;
    out_port->set_default_output_device = reach_audio_volume_set_default_output_device;
    out_port->start_watching = reach_audio_volume_start_watching;
    out_port->stop_watching = reach_audio_volume_stop_watching;
    out_port->destroy = reach_audio_volume_destroy;

    return REACH_OK;
}
