#include "windows_adapters_internal.h"

#include <windows.h>
#include <shobjidl.h>

#include <new>

struct reach_wallpaper_service
{
    IDesktopWallpaper *desktop;
    int32_t com_initialized;
};

static reach_result reach_wallpaper_set(reach_wallpaper_service *service, const uint16_t *path)
{
    if (service == nullptr || path == nullptr || path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    DWORD attributes = GetFileAttributesW(reinterpret_cast<const wchar_t *>(path));
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (service->desktop != nullptr)
    {
        HRESULT hr =
            service->desktop->SetWallpaper(nullptr, reinterpret_cast<const wchar_t *>(path));
        if (SUCCEEDED(hr))
        {
            return REACH_OK;
        }
    }

    BOOL ok = SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0,
                                    const_cast<wchar_t *>(reinterpret_cast<const wchar_t *>(path)),
                                    SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    return ok ? REACH_OK : REACH_ERROR;
}

static reach_result reach_wallpaper_set_monitor(reach_wallpaper_service *service,
                                                size_t monitor_index, const uint16_t *path)
{
    if (service == nullptr || path == nullptr || path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    DWORD attributes = GetFileAttributesW(reinterpret_cast<const wchar_t *>(path));
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (service->desktop == nullptr || monitor_index > 0xFFFFFFFFu)
    {
        return REACH_ERROR;
    }

    UINT count = 0;
    HRESULT hr = service->desktop->GetMonitorDevicePathCount(&count);
    if (FAILED(hr) || monitor_index >= count)
    {
        return REACH_INVALID_ARGUMENT;
    }

    LPWSTR monitor_id = nullptr;
    hr = service->desktop->GetMonitorDevicePathAt((UINT)monitor_index, &monitor_id);
    if (SUCCEEDED(hr) && monitor_id != nullptr)
    {
        hr = service->desktop->SetWallpaper(monitor_id, reinterpret_cast<const wchar_t *>(path));
    }
    if (monitor_id != nullptr)
    {
        CoTaskMemFree(monitor_id);
    }
    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_wallpaper_clear(reach_wallpaper_service *service)
{
    if (service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (service->desktop != nullptr)
    {
        HRESULT hr = service->desktop->Enable(FALSE);
        if (SUCCEEDED(hr) || hr == S_FALSE)
        {
            return REACH_OK;
        }
    }

    wchar_t empty[] = L"";
    BOOL ok =
        SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, empty, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    return ok ? REACH_OK : REACH_ERROR;
}

static reach_result reach_wallpaper_current(reach_wallpaper_service *service, uint16_t *out_path,
                                            size_t out_path_count)
{
    if (service == nullptr || out_path == nullptr || out_path_count == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    out_path[0] = 0;

    if (service->desktop == nullptr)
    {
        wchar_t path[260] = {};
        BOOL ok = SystemParametersInfoW(SPI_GETDESKWALLPAPER, 260, path, 0);
        if (!ok || path[0] == 0)
        {
            return REACH_ERROR;
        }
        return reach_copy_utf16(out_path, out_path_count, reinterpret_cast<const uint16_t *>(path));
    }

    LPWSTR path = nullptr;
    HRESULT hr = service->desktop->GetWallpaper(nullptr, &path);
    if ((FAILED(hr) || path == nullptr || path[0] == 0) && service->desktop != nullptr)
    {
        if (path != nullptr)
        {
            CoTaskMemFree(path);
            path = nullptr;
        }

        UINT count = 0;
        hr = service->desktop->GetMonitorDevicePathCount(&count);
        for (UINT index = 0; SUCCEEDED(hr) && index < count && (path == nullptr || path[0] == 0);
             ++index)
        {
            LPWSTR monitor_id = nullptr;
            hr = service->desktop->GetMonitorDevicePathAt(index, &monitor_id);
            if (SUCCEEDED(hr) && monitor_id != nullptr)
            {
                hr = service->desktop->GetWallpaper(monitor_id, &path);
            }
            if (monitor_id != nullptr)
            {
                CoTaskMemFree(monitor_id);
            }
        }
    }
    if (FAILED(hr) || path == nullptr || path[0] == 0)
    {
        if (path != nullptr)
        {
            CoTaskMemFree(path);
        }
        return REACH_ERROR;
    }
    reach_result result =
        reach_copy_utf16(out_path, out_path_count, reinterpret_cast<const uint16_t *>(path));
    CoTaskMemFree(path);
    return result;
}

struct reach_wallpaper_monitor_rects
{
    RECT rects[REACH_MAX_WALLPAPER_MONITORS];
    size_t count;
};

static BOOL CALLBACK reach_wallpaper_monitor_rects_enum_proc(HMONITOR monitor, HDC dc, LPRECT rect,
                                                             LPARAM param)
{
    (void)monitor;
    (void)dc;

    reach_wallpaper_monitor_rects *state = reinterpret_cast<reach_wallpaper_monitor_rects *>(param);
    if (state == nullptr || rect == nullptr)
    {
        return TRUE;
    }
    if (rect->right - rect->left <= 0 || rect->bottom - rect->top <= 0)
    {
        return TRUE;
    }
    if (state->count < REACH_MAX_WALLPAPER_MONITORS)
    {
        state->rects[state->count++] = *rect;
    }
    return TRUE;
}

// monitor_index follows EnumDisplayMonitors order, matching the wallpaper surface's
// window order; IDesktopWallpaper device paths are matched to it by monitor RECT.
static reach_result reach_wallpaper_current_monitor(reach_wallpaper_service *service,
                                                    size_t monitor_index, uint16_t *out_path,
                                                    size_t out_path_count)
{
    if (service == nullptr || out_path == nullptr || out_path_count == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    out_path[0] = 0;
    if (service->desktop == nullptr)
    {
        return REACH_ERROR;
    }

    reach_wallpaper_monitor_rects monitors = {};
    EnumDisplayMonitors(nullptr, nullptr, reach_wallpaper_monitor_rects_enum_proc,
                        reinterpret_cast<LPARAM>(&monitors));
    if (monitor_index >= monitors.count)
    {
        return REACH_INVALID_ARGUMENT;
    }

    RECT target = monitors.rects[monitor_index];

    UINT count = 0;
    HRESULT hr = service->desktop->GetMonitorDevicePathCount(&count);
    if (FAILED(hr))
    {
        return REACH_ERROR;
    }

    reach_result result = REACH_ERROR;
    for (UINT index = 0; index < count && result != REACH_OK; ++index)
    {
        LPWSTR monitor_id = nullptr;
        hr = service->desktop->GetMonitorDevicePathAt(index, &monitor_id);
        if (FAILED(hr) || monitor_id == nullptr)
        {
            continue;
        }

        RECT rect = {};
        if (SUCCEEDED(service->desktop->GetMonitorRECT(monitor_id, &rect)) &&
            EqualRect(&rect, &target))
        {
            LPWSTR path = nullptr;
            if (SUCCEEDED(service->desktop->GetWallpaper(monitor_id, &path)) && path != nullptr &&
                path[0] != 0)
            {
                result = reach_copy_utf16(out_path, out_path_count,
                                          reinterpret_cast<const uint16_t *>(path));
            }
            if (path != nullptr)
            {
                CoTaskMemFree(path);
            }
        }
        CoTaskMemFree(monitor_id);
    }
    return result;
}

static void reach_wallpaper_destroy(reach_wallpaper_service *service)
{
    if (service != nullptr && service->desktop != nullptr)
    {
        service->desktop->Release();
    }
    if (service != nullptr && service->com_initialized)
    {
        CoUninitialize();
    }
    delete service;
}

reach_result reach_windows_create_wallpaper_service(reach_wallpaper_service_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_wallpaper_service *service = new (std::nothrow) reach_wallpaper_service();
    if (service == nullptr)
    {
        return REACH_ERROR;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
    {
        service->com_initialized = 1;
    }
    else if (hr != RPC_E_CHANGED_MODE)
    {
        delete service;
        return REACH_ERROR;
    }

    hr = CoCreateInstance(CLSID_DesktopWallpaper, nullptr, CLSCTX_ALL,
                          IID_PPV_ARGS(&service->desktop));

    out_port->service = service;
    out_port->ops.set_wallpaper = reach_wallpaper_set;
    out_port->ops.set_monitor_wallpaper = reach_wallpaper_set_monitor;
    out_port->ops.clear_wallpaper = reach_wallpaper_clear;
    out_port->ops.current_wallpaper = reach_wallpaper_current;
    out_port->ops.current_monitor_wallpaper = reach_wallpaper_current_monitor;
    out_port->ops.destroy = reach_wallpaper_destroy;
    return REACH_OK;
}
