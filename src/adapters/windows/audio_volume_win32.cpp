#include "reach/platform/windows_adapters.h"

#include "reach/ports/audio_volume.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>

#include <new>

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
    out_port->destroy = reach_audio_volume_destroy;

    return REACH_OK;
}
