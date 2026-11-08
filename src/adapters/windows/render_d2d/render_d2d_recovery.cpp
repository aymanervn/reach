#include "../windows_adapters_internal.h"

#include "render_d2d_internal.h"

#include "reach/adapters/windows/diagnostics_log.h"
#include "reach/support/render_device_loss.h"

#include <stdio.h>

static const char *reach_d2d_adapter_name(bool warp)
{
    return warp ? "warp" : "hardware";
}

static void reach_d2d_adapter_description(ID3D11Device *d3d_device, char *out, size_t capacity)
{
    out[0] = 0;
    IDXGIDevice *dxgi_device = nullptr;
    IDXGIAdapter *adapter = nullptr;
    if (d3d_device != nullptr && SUCCEEDED(d3d_device->QueryInterface(IID_PPV_ARGS(&dxgi_device))))
    {
        if (SUCCEEDED(dxgi_device->GetAdapter(&adapter)))
        {
            DXGI_ADAPTER_DESC desc = {};
            if (SUCCEEDED(adapter->GetDesc(&desc)))
            {
                WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, out, (int)capacity, nullptr,
                                    nullptr);
                out[capacity - 1] = 0;
            }
            adapter->Release();
        }
        dxgi_device->Release();
    }
}

static reach_result reach_d2d_device_rebuild(reach_render_device *device, bool warp,
                                             HRESULT *out_hr)
{
    ID3D11Device *d3d_device = nullptr;
    IDXGIDevice *dxgi_device = nullptr;
    ID2D1Device *d2d_device = nullptr;
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;

    HRESULT hr = reach_d3d11_create_adapter_device(warp, &d3d_device, &level);
    if (SUCCEEDED(hr))
    {
        hr = d3d_device->QueryInterface(IID_PPV_ARGS(&dxgi_device));
    }
    if (SUCCEEDED(hr))
    {
        hr = device->factory->CreateDevice(dxgi_device, &d2d_device);
    }
    if (SUCCEEDED(hr) && device->composition_graphics_device != nullptr)
    {
        ComPtr<ABI::Windows::UI::Composition::ICompositionGraphicsDeviceInterop> interop;
        hr = device->composition_graphics_device.As(&interop);
        if (SUCCEEDED(hr))
        {
            hr = interop->SetRenderingDevice(d2d_device);
        }
    }

    *out_hr = hr;
    if (FAILED(hr))
    {
        if (d2d_device != nullptr)
        {
            d2d_device->Release();
        }
        if (dxgi_device != nullptr)
        {
            dxgi_device->Release();
        }
        if (d3d_device != nullptr)
        {
            d3d_device->Release();
        }
        return REACH_ERROR;
    }

    reach_d2d_device_release_device_resources(device);
    reach_d2d_device_unregister_removed_event(device);
    device->d2d_device->Release();
    device->dxgi_device->Release();
    device->d3d_device->Release();
    device->d3d_device = d3d_device;
    device->dxgi_device = dxgi_device;
    device->d2d_device = d2d_device;
    reach_d2d_device_register_removed_event(device);

    device->lost = false;
    device->warp_active = warp;
    device->generation += 1;
    return REACH_OK;
}

static void reach_d2d_recovery_log(const char *level, const char *event,
                                   const reach_render_device *device, uint64_t elapsed_ms,
                                   bool warp)
{
    char message[256] = {};
    sprintf_s(message, "%s losses=%u recoveries=%u attempts=%u ms=%llu adapter=%s gen=%llu", event,
              (unsigned)device->losses, (unsigned)device->recoveries,
              (unsigned)device->recovery.failed_attempts, (unsigned long long)elapsed_ms,
              reach_d2d_adapter_name(warp), (unsigned long long)device->generation);
    reach_diagnostics_log_write(level, message);
}

static const char *reach_d2d_hint_name(reach_render_device_hint hint)
{
    switch (hint)
    {
    case REACH_RENDER_DEVICE_HINT_DISPLAY:
        return "display";
    case REACH_RENDER_DEVICE_HINT_RESUME:
        return "resume";
    case REACH_RENDER_DEVICE_HINT_UNLOCK:
        return "unlock";
    default:
        return "adapters";
    }
}

static reach_render_device_status reach_d2d_port_status(reach_render_device *device)
{
    reach_render_device_status status = {};
    status.state = REACH_RENDER_DEVICE_READY;
    if (device == nullptr)
    {
        return status;
    }

    status.generation = device->generation;
    if (device->lost)
    {
        status.state = REACH_RENDER_DEVICE_LOST;
        reach_render_recovery_step step =
            reach_render_recovery_step_at(&device->recovery, GetTickCount64());
        status.retry_wait_ms = step.action == REACH_RENDER_RECOVERY_WAIT ? step.wait_ms : 0;
    }
    return status;
}

static reach_result reach_d2d_port_recover(reach_render_device *device)
{
    if (device == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_render_device_hint hint = reach_d2d_device_take_hint();
    if (!device->lost)
    {
        HRESULT reason =
            device->d3d_device != nullptr ? device->d3d_device->GetDeviceRemovedReason() : S_OK;
        if (SUCCEEDED(reason))
        {
            if (reach_render_recovery_should_leave_warp(device->warp_active, device->lost,
                                                        device->recovery.recovering))
            {
                uint64_t switch_start = GetTickCount64();
                HRESULT switch_hr = S_OK;
                bool switched = reach_d2d_device_rebuild(device, false, &switch_hr) == REACH_OK;
                char adapter_description[128] = {};
                reach_d2d_adapter_description(device->d3d_device, adapter_description,
                                              sizeof(adapter_description));
                char message[256] = {};
                sprintf_s(message,
                          "device_adapter_switch from=warp to=hardware reason=%s ok=%d ms=%llu "
                          "adapter_description=\"%s\"",
                          reach_d2d_hint_name(hint), switched ? 1 : 0,
                          (unsigned long long)(GetTickCount64() - switch_start),
                          adapter_description);
                reach_diagnostics_log_write("info", message);
            }
            return REACH_OK;
        }
        (void)reach_d2d_device_check(device, reason, L"DeviceRemovedEvent");
    }

    uint64_t now = GetTickCount64();
    if (!device->recovery.recovering && !reach_render_recovery_begin(&device->recovery, now))
    {
        reach_d2d_recovery_log("error", "device_recovery_gave_up reason=loss_rate", device, 0,
                               device->warp_active);
        return REACH_ERROR;
    }
    if (hint != REACH_RENDER_DEVICE_HINT_NONE)
    {
        reach_render_recovery_attempt_now(&device->recovery, now);
    }

    reach_render_recovery_step step = reach_render_recovery_step_at(&device->recovery, now);
    if (step.action == REACH_RENDER_RECOVERY_GIVE_UP)
    {
        reach_d2d_recovery_log("error", "device_recovery_gave_up reason=warp_failed", device,
                               now - device->recovery.started_ms, true);
        return REACH_ERROR;
    }
    if (step.action != REACH_RENDER_RECOVERY_ATTEMPT)
    {
        return REACH_DEVICE_LOST;
    }

    HRESULT hr = S_OK;
    bool warp = step.use_warp != 0;
    if (warp)
    {
        char message[128] = {};
        sprintf_s(message, "device_warp_selected reason=%s attempts=%u ms=%llu",
                  device->recovery.hardware_unusable ? "no_hardware" : "hardware_timeout",
                  (unsigned)device->recovery.failed_attempts,
                  (unsigned long long)(now - device->recovery.started_ms));
        reach_diagnostics_log_write("warn", message);
    }
    if (reach_d2d_device_rebuild(device, warp, &hr) != REACH_OK)
    {
        reach_render_recovery_attempt_failed(&device->recovery, now, warp);
        if (!warp && reach_render_hardware_unavailable_hresult((int32_t)hr))
        {
            reach_render_recovery_hardware_unusable(&device->recovery, now);
        }
        char message[128] = {};
        sprintf_s(message, "device_recovery_failed attempt=%u hr=0x%08X adapter=%s",
                  (unsigned)device->recovery.failed_attempts, (unsigned)hr,
                  reach_d2d_adapter_name(warp));
        reach_diagnostics_log_write("error", message);
        return REACH_DEVICE_LOST;
    }

    uint64_t elapsed_ms = GetTickCount64() - device->recovery.started_ms;
    reach_render_recovery_succeeded(&device->recovery);
    device->recoveries += 1;
    reach_d2d_recovery_log("info", "device_recovered", device, elapsed_ms, warp);
    return REACH_OK;
}

static void *reach_d2d_port_wait_handle(reach_render_device *device)
{
    return device != nullptr ? device->removed_event : nullptr;
}

static void reach_d2d_port_diagnostics(reach_render_device *device,
                                       reach_render_device_diagnostics *out)
{
    if (out == nullptr)
    {
        return;
    }

    *out = {};
    if (device == nullptr)
    {
        return;
    }

    out->losses = device->losses;
    out->recoveries = device->recoveries;
    out->warp_active = device->warp_active ? 1 : 0;
    out->last_hr = (int32_t)device->lost_hr;
    out->last_reason = (int32_t)device->lost_reason;
    out->generation = device->generation;
}

void reach_windows_render_device_port(reach_render_device *device,
                                      reach_render_device_port *out_port)
{
    REACH_ASSERT(out_port != nullptr);
    if (out_port == nullptr)
    {
        return;
    }

    *out_port = {};
    out_port->device = device;
    out_port->ops.status = reach_d2d_port_status;
    out_port->ops.recover = reach_d2d_port_recover;
    out_port->ops.wait_handle = reach_d2d_port_wait_handle;
    out_port->ops.diagnostics = reach_d2d_port_diagnostics;
}
