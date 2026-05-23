#include "reach/platform/windows_adapters.h"

#include "reach/ports/audio_volume.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audiopolicy.h>

#include <new>
#include <wchar.h>

struct reach_audio_volume_adapter {
    IMMDeviceEnumerator *enumerator;
    IMMDevice *device;
    IAudioEndpointVolume *endpoint;
    int32_t com_initialized;
};

static float reach_audio_volume_clamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static void reach_audio_volume_copy_utf16(
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

static int reach_audio_volume_utf16_equal(
    const uint16_t *a,
    const uint16_t *b
)
{
    if (a == nullptr || b == nullptr) {
        return a == b;
    }

    size_t index = 0;
    while (a[index] != 0 || b[index] != 0) {
        if (a[index] != b[index]) {
            return 0;
        }
        ++index;
    }
    return 1;
}

static const wchar_t *reach_audio_volume_basename(const wchar_t *path)
{
    if (path == nullptr) {
        return nullptr;
    }

    const wchar_t *name = path;
    for (const wchar_t *cursor = path; *cursor != 0; ++cursor) {
        if (*cursor == L'\\' || *cursor == L'/') {
            name = cursor + 1;
        }
    }
    return name;
}

static void reach_audio_volume_release_endpoint(
    reach_audio_volume_adapter *adapter
)
{
    if (adapter == nullptr) {
        return;
    }

    if (adapter->endpoint != nullptr) {
        adapter->endpoint->Release();
        adapter->endpoint = nullptr;
    }
    if (adapter->device != nullptr) {
        adapter->device->Release();
        adapter->device = nullptr;
    }
    if (adapter->enumerator != nullptr) {
        adapter->enumerator->Release();
        adapter->enumerator = nullptr;
    }
}

static reach_result reach_audio_volume_ensure_endpoint(
    reach_audio_volume_adapter *adapter
)
{
    if (adapter == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    if (adapter->endpoint != nullptr) {
        return REACH_OK;
    }

    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(&adapter->enumerator));

    if (SUCCEEDED(hr)) {
        hr = adapter->enumerator->GetDefaultAudioEndpoint(
            eRender,
            eConsole,
            &adapter->device);
    }

    if (SUCCEEDED(hr)) {
        hr = adapter->device->Activate(
            __uuidof(IAudioEndpointVolume),
            CLSCTX_ALL,
            nullptr,
            reinterpret_cast<void **>(&adapter->endpoint));
    }

    if (FAILED(hr)) {
        reach_audio_volume_release_endpoint(adapter);
        return REACH_ERROR;
    }

    return REACH_OK;
}

static reach_result reach_audio_volume_create_session_manager(
    IAudioSessionManager2 **out_manager
)
{
    if (out_manager == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_manager = nullptr;

    IMMDeviceEnumerator *enumerator = nullptr;
    IMMDevice *device = nullptr;
    IAudioSessionManager2 *manager = nullptr;

    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(&enumerator));

    if (SUCCEEDED(hr)) {
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    }
    if (SUCCEEDED(hr)) {
        hr = device->Activate(
            __uuidof(IAudioSessionManager2),
            CLSCTX_ALL,
            nullptr,
            reinterpret_cast<void **>(&manager));
    }

    if (enumerator != nullptr) {
        enumerator->Release();
    }
    if (device != nullptr) {
        device->Release();
    }

    if (FAILED(hr)) {
        if (manager != nullptr) {
            manager->Release();
        }
        return REACH_ERROR;
    }

    *out_manager = manager;
    return REACH_OK;
}

static void reach_audio_volume_label_for_process(
    DWORD process_id,
    uint16_t *out_label,
    size_t out_label_count
)
{
    if (out_label == nullptr || out_label_count == 0) {
        return;
    }

    out_label[0] = 0;
    if (process_id == 0) {
        return;
    }

    HANDLE process = OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION,
        FALSE,
        process_id);
    if (process == nullptr) {
        return;
    }

    wchar_t path[MAX_PATH] = {};
    DWORD path_count = MAX_PATH;
    if (QueryFullProcessImageNameW(process, 0, path, &path_count)) {
        reach_audio_volume_copy_utf16(
            out_label,
            out_label_count,
            reach_audio_volume_basename(path));
    }

    CloseHandle(process);
}

static void reach_audio_volume_fill_fallback_label(
    reach_audio_volume_session *session
)
{
    if (session == nullptr || session->label[0] != 0) {
        return;
    }

    if (session->is_system_sounds) {
        reach_audio_volume_copy_utf16(
            session->label,
            REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY,
            L"System Sounds");
        return;
    }

    reach_audio_volume_copy_utf16(
        session->label,
        REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY,
        L"App");
}

static reach_result reach_audio_volume_read_session(
    IAudioSessionControl *control,
    reach_audio_volume_session *out_session
)
{
    if (control == nullptr || out_session == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_session = {};

    AudioSessionState state = AudioSessionStateInactive;
    HRESULT state_hr = control->GetState(&state);
    if (FAILED(state_hr) || state != AudioSessionStateActive) {
        return REACH_ERROR;
    }

    IAudioSessionControl2 *control2 = nullptr;
    ISimpleAudioVolume *simple_volume = nullptr;
    LPWSTR instance_id = nullptr;
    LPWSTR display_name = nullptr;

    HRESULT hr = control->QueryInterface(IID_PPV_ARGS(&control2));
    if (SUCCEEDED(hr)) {
        hr = control->QueryInterface(IID_PPV_ARGS(&simple_volume));
    }
    if (SUCCEEDED(hr)) {
        hr = control2->GetSessionInstanceIdentifier(&instance_id);
    }

    if (SUCCEEDED(hr) && instance_id != nullptr && instance_id[0] != 0) {
        reach_audio_volume_copy_utf16(
            out_session->session_instance_id,
            REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY,
            instance_id);
    } else if (SUCCEEDED(hr)) {
        hr = E_FAIL;
    }

    if (SUCCEEDED(hr)) {
        DWORD process_id = 0;
        if (SUCCEEDED(control2->GetProcessId(&process_id))) {
            out_session->process_id = process_id;
        }

        HRESULT system_hr = control2->IsSystemSoundsSession();
        out_session->is_system_sounds = system_hr == S_OK ? 1 : 0;

        float level = 0.0f;
        BOOL muted = FALSE;
        if (SUCCEEDED(simple_volume->GetMasterVolume(&level))) {
            out_session->level = reach_audio_volume_clamp01(level);
        }
        if (SUCCEEDED(simple_volume->GetMute(&muted))) {
            out_session->muted = muted ? 1 : 0;
        }

        if (out_session->is_system_sounds) {
            reach_audio_volume_copy_utf16(
                out_session->label,
                REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY,
                L"System Sounds");
        } else if (SUCCEEDED(control->GetDisplayName(&display_name)) &&
            display_name != nullptr &&
            display_name[0] != 0) {
            reach_audio_volume_copy_utf16(
                out_session->label,
                REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY,
                display_name);
        } else {
            reach_audio_volume_label_for_process(
                out_session->process_id,
                out_session->label,
                REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY);
        }

        reach_audio_volume_fill_fallback_label(out_session);
    }

    if (display_name != nullptr) {
        CoTaskMemFree(display_name);
    }
    if (instance_id != nullptr) {
        CoTaskMemFree(instance_id);
    }
    if (simple_volume != nullptr) {
        simple_volume->Release();
    }
    if (control2 != nullptr) {
        control2->Release();
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}
static reach_result reach_audio_volume_get_state(
    void *userdata,
    reach_audio_volume_state *out_state
)
{
    reach_audio_volume_adapter *adapter =
        static_cast<reach_audio_volume_adapter *>(userdata);

    if (adapter == nullptr || out_state == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = reach_audio_volume_ensure_endpoint(adapter);
    if (result != REACH_OK) {
        return result;
    }

    float level = 0.0f;
    BOOL muted = FALSE;

    HRESULT hr = adapter->endpoint->GetMasterVolumeLevelScalar(&level);
    if (SUCCEEDED(hr)) {
        hr = adapter->endpoint->GetMute(&muted);
    }

    if (FAILED(hr)) {
        reach_audio_volume_release_endpoint(adapter);
        return REACH_ERROR;
    }

    out_state->level = reach_audio_volume_clamp01(level);
    out_state->muted = muted ? 1 : 0;

    return REACH_OK;
}

static reach_result reach_audio_volume_set_level(
    void *userdata,
    float level
)
{
    reach_audio_volume_adapter *adapter =
        static_cast<reach_audio_volume_adapter *>(userdata);

    if (adapter == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = reach_audio_volume_ensure_endpoint(adapter);
    if (result != REACH_OK) {
        return result;
    }

    HRESULT hr = adapter->endpoint->SetMasterVolumeLevelScalar(
        reach_audio_volume_clamp01(level),
        nullptr);

    if (FAILED(hr)) {
        reach_audio_volume_release_endpoint(adapter);
        return REACH_ERROR;
    }

    return REACH_OK;
}

static reach_result reach_audio_volume_set_muted(
    void *userdata,
    int32_t muted
)
{
    reach_audio_volume_adapter *adapter =
        static_cast<reach_audio_volume_adapter *>(userdata);

    if (adapter == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = reach_audio_volume_ensure_endpoint(adapter);
    if (result != REACH_OK) {
        return result;
    }

    HRESULT hr = adapter->endpoint->SetMute(muted ? TRUE : FALSE, nullptr);

    if (FAILED(hr)) {
        reach_audio_volume_release_endpoint(adapter);
        return REACH_ERROR;
    }

    return REACH_OK;
}

static reach_result reach_audio_volume_list_sessions(
    void *userdata,
    reach_audio_volume_session_list *out_list
)
{
    (void)userdata;

    if (out_list == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_list = {};

    IAudioSessionManager2 *manager = nullptr;
    IAudioSessionEnumerator *enumerator = nullptr;
    reach_result result = reach_audio_volume_create_session_manager(&manager);
    if (result != REACH_OK) {
        return result;
    }

    HRESULT hr = manager->GetSessionEnumerator(&enumerator);
    int count = 0;
    if (SUCCEEDED(hr)) {
        hr = enumerator->GetCount(&count);
    }

    for (int index = 0;
        SUCCEEDED(hr) &&
        index < count &&
        out_list->count < REACH_AUDIO_VOLUME_MAX_SESSIONS;
        ++index) {
        IAudioSessionControl *control = nullptr;
        hr = enumerator->GetSession(index, &control);
        if (SUCCEEDED(hr) && control != nullptr) {
            reach_audio_volume_session session = {};
            if (reach_audio_volume_read_session(control, &session) == REACH_OK &&
                session.session_instance_id[0] != 0) {
                out_list->sessions[out_list->count++] = session;
            }
        }
        if (control != nullptr) {
            control->Release();
        }
    }

    if (enumerator != nullptr) {
        enumerator->Release();
    }
    if (manager != nullptr) {
        manager->Release();
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_audio_volume_with_session_volume(
    const uint16_t *session_instance_id,
    ISimpleAudioVolume **out_volume
)
{
    if (session_instance_id == nullptr || out_volume == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_volume = nullptr;

    IAudioSessionManager2 *manager = nullptr;
    IAudioSessionEnumerator *enumerator = nullptr;
    reach_result result = reach_audio_volume_create_session_manager(&manager);
    if (result != REACH_OK) {
        return result;
    }

    HRESULT hr = manager->GetSessionEnumerator(&enumerator);
    int count = 0;
    if (SUCCEEDED(hr)) {
        hr = enumerator->GetCount(&count);
    }

    for (int index = 0; SUCCEEDED(hr) && index < count; ++index) {
        IAudioSessionControl *control = nullptr;
        hr = enumerator->GetSession(index, &control);
        if (FAILED(hr) || control == nullptr) {
            if (control != nullptr) {
                control->Release();
            }
            continue;
        }

        reach_audio_volume_session session = {};
        if (reach_audio_volume_read_session(control, &session) == REACH_OK &&
            reach_audio_volume_utf16_equal(session.session_instance_id, session_instance_id)) {
            ISimpleAudioVolume *volume = nullptr;
            hr = control->QueryInterface(IID_PPV_ARGS(&volume));
            control->Release();
            if (SUCCEEDED(hr)) {
                *out_volume = volume;
                break;
            }
        } else {
            control->Release();
        }
    }

    if (enumerator != nullptr) {
        enumerator->Release();
    }
    if (manager != nullptr) {
        manager->Release();
    }

    return *out_volume != nullptr ? REACH_OK : REACH_ERROR;
}

static reach_result reach_audio_volume_set_session_level(
    void *userdata,
    const uint16_t *session_instance_id,
    float level
)
{
    (void)userdata;

    ISimpleAudioVolume *volume = nullptr;
    reach_result result = reach_audio_volume_with_session_volume(
        session_instance_id,
        &volume);
    if (result != REACH_OK) {
        return result;
    }

    HRESULT hr = volume->SetMasterVolume(reach_audio_volume_clamp01(level), nullptr);
    volume->Release();
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_audio_volume_set_session_muted(
    void *userdata,
    const uint16_t *session_instance_id,
    int32_t muted
)
{
    (void)userdata;

    ISimpleAudioVolume *volume = nullptr;
    reach_result result = reach_audio_volume_with_session_volume(
        session_instance_id,
        &volume);
    if (result != REACH_OK) {
        return result;
    }

    HRESULT hr = volume->SetMute(muted ? TRUE : FALSE, nullptr);
    volume->Release();
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static void reach_audio_volume_destroy(void *userdata)
{
    reach_audio_volume_adapter *adapter =
        static_cast<reach_audio_volume_adapter *>(userdata);

    if (adapter == nullptr) {
        return;
    }

    reach_audio_volume_release_endpoint(adapter);

    if (adapter->com_initialized) {
        CoUninitialize();
        adapter->com_initialized = 0;
    }

    delete adapter;
}

extern "C" reach_result reach_windows_create_audio_volume(
    reach_audio_volume_port *out_port
)
{
    if (out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};

    reach_audio_volume_adapter *adapter = new (std::nothrow)
        reach_audio_volume_adapter();

    if (adapter == nullptr) {
        return REACH_ERROR;
    }

    adapter->enumerator = nullptr;
    adapter->device = nullptr;
    adapter->endpoint = nullptr;
    adapter->com_initialized = 0;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (hr == S_OK) {
        adapter->com_initialized = 1;
    } else if (hr == S_FALSE || hr == RPC_E_CHANGED_MODE) {
        hr = S_OK;
    }

    if (FAILED(hr)) {
        delete adapter;
        return REACH_ERROR;
    }

    out_port->userdata = adapter;
    out_port->get_state = reach_audio_volume_get_state;
    out_port->set_level = reach_audio_volume_set_level;
    out_port->set_muted = reach_audio_volume_set_muted;
    out_port->list_sessions = reach_audio_volume_list_sessions;
    out_port->set_session_level = reach_audio_volume_set_session_level;
    out_port->set_session_muted = reach_audio_volume_set_session_muted;
    out_port->destroy = reach_audio_volume_destroy;

    return REACH_OK;
}
