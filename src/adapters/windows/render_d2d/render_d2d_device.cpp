#include "render_d2d_internal.h"

#include "reach/adapters/windows/diagnostics_log.h"

#include <stdio.h>

ID2D1RenderTarget *reach_d2d_target(reach_render_backend *backend)
{
    if (backend == nullptr)
    {
        return nullptr;
    }
    if (backend->command_target != nullptr)
    {
        return backend->command_target;
    }
    if (backend->d2d_context != nullptr)
    {
        return backend->d2d_context;
    }
    return nullptr;
}

D2D1_COLOR_F reach_d2d_color(reach_color color)
{
    return D2D1::ColorF(color.r, color.g, color.b, color.a);
}

struct reach_d2d_log_entry
{
    char operation[96];
    HRESULT hr;
    ULONGLONG logged_ms;
    uint32_t suppressed;
};

static const size_t REACH_D2D_LOG_ENTRY_COUNT = 24;
static const ULONGLONG REACH_D2D_LOG_REPEAT_MS = 60000;
static SRWLOCK reach_d2d_log_lock = SRWLOCK_INIT;
static reach_d2d_log_entry reach_d2d_log_entries[REACH_D2D_LOG_ENTRY_COUNT];
static size_t reach_d2d_log_next_slot = 0;

void reach_d2d_log_hresult(const wchar_t *context, HRESULT hr)
{
    if (SUCCEEDED(hr))
    {
        return;
    }

    char operation[96] = {};
    if (context != nullptr)
    {
        WideCharToMultiByte(CP_UTF8, 0, context, -1, operation, (int)sizeof(operation) - 1, nullptr,
                            nullptr);
    }
    if (operation[0] == 0)
    {
        strcpy_s(operation, "unknown");
    }

    ULONGLONG now = GetTickCount64();
    uint32_t suppressed = 0;

    AcquireSRWLockExclusive(&reach_d2d_log_lock);
    reach_d2d_log_entry *entry = nullptr;
    for (size_t index = 0; index < REACH_D2D_LOG_ENTRY_COUNT; ++index)
    {
        if (strcmp(reach_d2d_log_entries[index].operation, operation) == 0)
        {
            entry = &reach_d2d_log_entries[index];
            break;
        }
    }

    if (entry == nullptr)
    {
        entry = &reach_d2d_log_entries[reach_d2d_log_next_slot];
        reach_d2d_log_next_slot = (reach_d2d_log_next_slot + 1) % REACH_D2D_LOG_ENTRY_COUNT;
        strcpy_s(entry->operation, operation);
        entry->hr = 0;
        entry->logged_ms = 0;
        entry->suppressed = 0;
    }
    else if (entry->hr == hr && entry->logged_ms != 0 &&
             now - entry->logged_ms < REACH_D2D_LOG_REPEAT_MS)
    {
        entry->suppressed += 1;
        ReleaseSRWLockExclusive(&reach_d2d_log_lock);
        return;
    }

    suppressed = entry->suppressed;
    entry->hr = hr;
    entry->logged_ms = now;
    entry->suppressed = 0;
    ReleaseSRWLockExclusive(&reach_d2d_log_lock);

    char message[256] = {};
    if (suppressed > 0)
    {
        sprintf_s(message, "d2d op=%s hr=0x%08X repeated=%u", operation, (unsigned)hr,
                  (unsigned)suppressed);
    }
    else
    {
        sprintf_s(message, "d2d op=%s hr=0x%08X", operation, (unsigned)hr);
    }
    reach_diagnostics_log_write("error", message);
}

HRESULT reach_d3d11_create_device(D3D_DRIVER_TYPE driver_type, ID3D11Device **out_device,
                                  D3D_FEATURE_LEVEL *out_level)
{
    if (out_device == nullptr || out_level == nullptr)
    {
        return E_INVALIDARG;
    }

    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
                                  D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};

    HRESULT hr = D3D11CreateDevice(nullptr, driver_type, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                   levels, sizeof(levels) / sizeof(levels[0]), D3D11_SDK_VERSION,
                                   out_device, out_level, nullptr);

    if (hr == E_INVALIDARG)
    {
        hr = D3D11CreateDevice(nullptr, driver_type, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                               levels + 1, (sizeof(levels) / sizeof(levels[0])) - 1,
                               D3D11_SDK_VERSION, out_device, out_level, nullptr);
    }

    return hr;
}
