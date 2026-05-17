#include "reach/platform/windows_adapters.h"

#include <windows.h>

#include <new>

struct reach_wallpaper_service {
    int unused;
};

static reach_result reach_wallpaper_set(reach_wallpaper_service *service, const uint16_t *path)
{
    (void)service;
    if (path == nullptr || path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }
    DWORD attributes = GetFileAttributesW(reinterpret_cast<const wchar_t *>(path));
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return REACH_INVALID_ARGUMENT;
    }

    BOOL ok = SystemParametersInfoW(
        SPI_SETDESKWALLPAPER,
        0,
        const_cast<wchar_t *>(reinterpret_cast<const wchar_t *>(path)),
        SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    return ok ? REACH_OK : REACH_ERROR;
}

static reach_result reach_wallpaper_clear(reach_wallpaper_service *service)
{
    (void)service;
    wchar_t empty[] = L"";
    BOOL ok = SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, empty, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    return ok ? REACH_OK : REACH_ERROR;
}

static reach_result reach_wallpaper_current(reach_wallpaper_service *service, uint16_t *out_path, size_t out_path_count)
{
    (void)service;
    if (out_path == nullptr || out_path_count == 0) {
        return REACH_INVALID_ARGUMENT;
    }
    out_path[0] = 0;
    wchar_t path[260] = {};
    BOOL ok = SystemParametersInfoW(SPI_GETDESKWALLPAPER, 260, path, 0);
    if (!ok || path[0] == 0) {
        return REACH_ERROR;
    }
    return reach_copy_utf16(out_path, out_path_count, reinterpret_cast<const uint16_t *>(path));
}

static void reach_wallpaper_destroy(reach_wallpaper_service *service)
{
    delete service;
}

reach_result reach_windows_create_wallpaper_service(reach_wallpaper_service_port *out_port)
{
    if (out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_wallpaper_service *service = new (std::nothrow) reach_wallpaper_service();
    if (service == nullptr) {
        return REACH_ERROR;
    }

    out_port->service = service;
    out_port->ops.set_wallpaper = reach_wallpaper_set;
    out_port->ops.clear_wallpaper = reach_wallpaper_clear;
    out_port->ops.current_wallpaper = reach_wallpaper_current;
    out_port->ops.destroy = reach_wallpaper_destroy;
    return REACH_OK;
}
