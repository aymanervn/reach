#include "window_filter_win32.h"

#include "window_query_win32.h"

#include <dwmapi.h>

static int32_t reach_window_is_reach_window(HWND hwnd)
{
    wchar_t class_name[64] = {};
    GetClassNameW(hwnd, class_name, 64);
    return lstrcmpiW(class_name, L"ReachPlatformWindow") == 0 ||
           lstrcmpiW(class_name, L"ReachInputMessageWindow") == 0 ||
           lstrcmpiW(class_name, L"ReachWallpaperWindow") == 0;
}

static int32_t reach_window_process_basename_matches(HWND hwnd, const wchar_t *name_a,
                                                     const wchar_t *name_b)
{
    uint16_t path_u16[512] = {};
    if (!reach_window_query_process_path(hwnd, path_u16, 512))
    {
        return 0;
    }

    const wchar_t *path = reinterpret_cast<const wchar_t *>(path_u16);
    const wchar_t *base = wcsrchr(path, L'\\');
    base = base != nullptr ? base + 1 : path;

    return lstrcmpiW(base, name_a) == 0 || lstrcmpiW(base, name_b) == 0;
}

static int32_t reach_window_is_wallpaper_engine_render_window(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return 0;
    }

    wchar_t class_name[128] = {};
    wchar_t title[128] = {};

    GetClassNameW(hwnd, class_name, 128);
    GetWindowTextW(hwnd, title, 128);

    if (lstrcmpiW(class_name, L"WPEDesktopDX11Window") != 0)
    {
        return 0;
    }

    if (lstrcmpW(title, L"WPELiveWallpaper") != 0)
    {
        return 0;
    }

    return reach_window_process_basename_matches(hwnd, L"wallpaper32.exe", L"wallpaper64.exe");
}

int32_t reach_window_is_desktop_surface(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return 0;
    }

    if (hwnd == GetShellWindow())
    {
        return 1;
    }

    if (reach_window_is_wallpaper_engine_render_window(hwnd))
    {
        return 1;
    }

    wchar_t class_name[64] = {};
    GetClassNameW(hwnd, class_name, 64);

    return lstrcmpiW(class_name, L"Progman") == 0 || lstrcmpiW(class_name, L"WorkerW") == 0 ||
           lstrcmpiW(class_name, L"ReachWallpaperWindow") == 0;
}

int32_t reach_window_is_app_candidate(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd) || reach_window_is_reach_window(hwnd) ||
        reach_window_is_desktop_surface(hwnd))
    {
        return 0;
    }

    if (GetWindow(hwnd, GW_OWNER) != nullptr)
    {
        return 0;
    }

    LONG_PTR ex_style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((ex_style & WS_EX_TOOLWINDOW) != 0)
    {
        return 0;
    }

    return 1;
}

int32_t reach_window_is_app(HWND hwnd)
{
    if (!reach_window_is_app_candidate(hwnd))
    {
        return 0;
    }
    if (!IsWindowVisible(hwnd) && !IsIconic(hwnd))
    {
        return 0;
    }
    return 1;
}

static int32_t reach_window_is_cloaked(HWND hwnd)
{
    DWORD cloaked = 0;
    HRESULT hr = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    return SUCCEEDED(hr) && cloaked != 0;
}

int32_t reach_window_is_displayed_app(HWND hwnd)
{
    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);
    if (GetWindowPlacement(hwnd, &placement) && placement.showCmd == SW_SHOWMINIMIZED)
    {
        return 0;
    }
    return reach_window_is_app(hwnd) && IsWindowVisible(hwnd) && !IsIconic(hwnd) &&
           !reach_window_is_cloaked(hwnd);
}
