#include "windows_adapters_internal.h"

#include <windows.h>
#include <dwmapi.h>
#include <shellscalingapi.h>

#include <new>
#include <string>
#include <vector>

struct reach_monitor_list
{
    std::vector<reach_monitor_info> monitors;
    std::vector<std::wstring> devices;
    int32_t enumeration_failed;
    int32_t work_area_initialized;
    int32_t work_area_changed;
    reach_rect_i32 controlled_monitor;
    reach_rect_i32 controlled_work_area;
    std::wstring controlled_device;
};

static reach_result reach_monitor_refresh(reach_monitor_list *list);
static reach_result reach_monitor_set_work_area(reach_monitor_list *list,
                                                reach_rect_i32 monitor_bounds,
                                                reach_rect_i32 work_area);
static int32_t reach_monitor_rect_equal(reach_rect_i32 left, reach_rect_i32 right);

static reach_rect_i32 reach_rect_from_win32(const RECT &rect)
{
    reach_rect_i32 result = {};
    result.left = rect.left;
    result.top = rect.top;
    result.right = rect.right;
    result.bottom = rect.bottom;
    return result;
}

static uint32_t reach_monitor_device_id(const wchar_t *device)
{
    uint32_t hash = 2166136261u;
    if (device != nullptr)
    {
        while (*device != L'\0')
        {
            hash ^= (uint32_t)*device++;
            hash *= 16777619u;
        }
    }
    return hash != 0 ? hash : 1u;
}

static BOOL CALLBACK reach_monitor_enum_proc(HMONITOR monitor, HDC dc, LPRECT rect, LPARAM param)
{
    (void)dc;
    (void)rect;

    reach_monitor_list *list = reinterpret_cast<reach_monitor_list *>(param);
    MONITORINFOEXW info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info))
    {
        list->enumeration_failed = 1;
        return FALSE;
    }

    reach_monitor_info item = {};
    item.id = reach_monitor_device_id(info.szDevice);
    item.bounds = reach_rect_from_win32(info.rcMonitor);
    item.work_area = reach_rect_from_win32(info.rcWork);
    item.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    item.dpi_x = 96;
    item.dpi_y = 96;

    UINT dpi_x = 96;
    UINT dpi_y = 96;
    if (SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y)))
    {
        item.dpi_x = static_cast<int32_t>(dpi_x);
        item.dpi_y = static_cast<int32_t>(dpi_y);
    }

    DEVMODEW devmode = {};
    devmode.dmSize = sizeof(devmode);
    if (EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &devmode))
    {
        item.refresh_rate_hz = static_cast<int32_t>(devmode.dmDisplayFrequency);
    }

    list->monitors.push_back(item);
    list->devices.emplace_back(info.szDevice);
    return TRUE;
}

static reach_result reach_monitor_list_create(reach_monitor_list **out_list)
{
    if (out_list == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_monitor_list *list = new (std::nothrow) reach_monitor_list();
    if (list == nullptr)
    {
        *out_list = nullptr;
        return REACH_ERROR;
    }

    *out_list = list;
    return reach_monitor_refresh(list);
}

static void reach_monitor_list_destroy(reach_monitor_list *list)
{
    if (list != nullptr && list->work_area_changed)
    {
        for (size_t index = 0; index < list->devices.size(); ++index)
        {
            if (list->devices[index] == list->controlled_device)
            {
                (void)reach_monitor_set_work_area(list, list->monitors[index].bounds,
                                                  list->monitors[index].bounds);
                break;
            }
        }
    }
    delete list;
}

static reach_result reach_monitor_refresh(reach_monitor_list *list)
{
    if (list == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_monitor_list snapshot = {};
    if (!EnumDisplayMonitors(nullptr, nullptr, reach_monitor_enum_proc,
                             reinterpret_cast<LPARAM>(&snapshot)) ||
        snapshot.enumeration_failed ||
        snapshot.monitors.empty())
    {
        return REACH_ERROR;
    }

    if (list->work_area_changed && !list->controlled_device.empty())
    {
        size_t controlled_index = snapshot.devices.size();
        for (size_t index = 0; index < snapshot.devices.size(); ++index)
        {
            if (snapshot.devices[index] == list->controlled_device)
            {
                controlled_index = index;
                break;
            }
        }
        if (controlled_index < snapshot.monitors.size())
        {
            reach_rect_i32 next_monitor = snapshot.monitors[controlled_index].bounds;
            int32_t left_inset = list->controlled_work_area.left - list->controlled_monitor.left;
            int32_t top_inset = list->controlled_work_area.top - list->controlled_monitor.top;
            int32_t right_inset = list->controlled_monitor.right - list->controlled_work_area.right;
            int32_t bottom_inset =
                list->controlled_monitor.bottom - list->controlled_work_area.bottom;
            list->controlled_monitor = next_monitor;
            list->controlled_work_area = {
                next_monitor.left + left_inset, next_monitor.top + top_inset,
                next_monitor.right - right_inset, next_monitor.bottom - bottom_inset};
        }
        else
        {
            list->work_area_changed = 0;
            list->controlled_monitor = {};
            list->controlled_work_area = {};
            list->controlled_device.clear();
        }
    }

    int32_t work_area_initialized = 0;
    if (list->work_area_initialized && !list->controlled_device.empty())
    {
        for (size_t index = 0; index < snapshot.devices.size(); ++index)
        {
            if (snapshot.devices[index] == list->controlled_device &&
                reach_monitor_rect_equal(snapshot.monitors[index].bounds,
                                         list->controlled_monitor) &&
                reach_monitor_rect_equal(snapshot.monitors[index].work_area,
                                         list->controlled_work_area))
            {
                work_area_initialized = 1;
                break;
            }
        }
    }

    list->monitors.swap(snapshot.monitors);
    list->devices.swap(snapshot.devices);
    list->work_area_initialized = work_area_initialized;

    return REACH_OK;
}

static size_t reach_monitor_count(const reach_monitor_list *list)
{
    return list == nullptr ? 0 : list->monitors.size();
}

static const reach_monitor_info *reach_monitor_get(const reach_monitor_list *list, size_t index)
{
    if (list == nullptr || index >= list->monitors.size())
    {
        return nullptr;
    }

    return &list->monitors[index];
}

static const reach_monitor_info *reach_monitor_primary(const reach_monitor_list *list)
{
    if (list == nullptr || list->monitors.empty())
    {
        return nullptr;
    }

    for (const reach_monitor_info &monitor : list->monitors)
    {
        if (monitor.primary)
        {
            return &monitor;
        }
    }

    return &list->monitors[0];
}

static int32_t reach_monitor_rect_equal(reach_rect_i32 left, reach_rect_i32 right)
{
    return left.left == right.left && left.top == right.top && left.right == right.right &&
           left.bottom == right.bottom;
}

static int32_t reach_monitor_rect_valid(reach_rect_i32 rect)
{
    return rect.right > rect.left && rect.bottom > rect.top;
}

static RECT reach_monitor_win32_rect(reach_rect_i32 rect)
{
    return {rect.left, rect.top, rect.right, rect.bottom};
}

static int32_t reach_monitor_win32_rect_equal(const RECT &left, const RECT &right)
{
    return left.left == right.left && left.top == right.top && left.right == right.right &&
           left.bottom == right.bottom;
}

static int32_t reach_monitor_is_shell_window(HWND window)
{
    wchar_t class_name[64] = {};
    GetClassNameW(window, class_name, 64);
    return lstrcmpiW(class_name, L"Shell_TrayWnd") == 0 ||
           lstrcmpiW(class_name, L"Shell_SecondaryTrayWnd") == 0 ||
           lstrcmpiW(class_name, L"Progman") == 0 || lstrcmpiW(class_name, L"WorkerW") == 0;
}

static void reach_monitor_expand_for_invisible_borders(HWND window, RECT *bounds)
{
    RECT window_bounds = {};
    RECT frame_bounds = {};
    if (bounds == nullptr || !GetWindowRect(window, &window_bounds) ||
        FAILED(DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &frame_bounds,
                                     sizeof(frame_bounds))))
    {
        return;
    }

    LONG left = window_bounds.left - frame_bounds.left;
    LONG top = window_bounds.top - frame_bounds.top;
    LONG right = window_bounds.right - frame_bounds.right;
    LONG bottom = window_bounds.bottom - frame_bounds.bottom;
    if (left < -64 || left > 64 || top < -64 || top > 64 || right < -64 || right > 64 ||
        bottom < -64 || bottom > 64)
    {
        return;
    }

    bounds->left += left;
    bounds->top += top;
    bounds->right += right;
    bounds->bottom += bottom;
}

typedef struct reach_monitor_work_area_repair
{
    RECT monitor;
    RECT work_area;
} reach_monitor_work_area_repair;

static BOOL CALLBACK reach_monitor_repair_maximized_window(HWND window, LPARAM param)
{
    reach_monitor_work_area_repair *repair =
        reinterpret_cast<reach_monitor_work_area_repair *>(param);
    if (repair == nullptr || window == nullptr || !IsWindow(window) || !IsWindowVisible(window) ||
        !IsZoomed(window) || reach_monitor_is_shell_window(window))
    {
        return TRUE;
    }

    HMONITOR window_monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info = {};
    monitor_info.cbSize = sizeof(monitor_info);
    if (window_monitor == nullptr || !GetMonitorInfoW(window_monitor, &monitor_info) ||
        !reach_monitor_win32_rect_equal(monitor_info.rcMonitor, repair->monitor))
    {
        return TRUE;
    }

    RECT bounds = repair->work_area;
    reach_monitor_expand_for_invisible_borders(window, &bounds);
    (void)SetWindowPos(window, nullptr, bounds.left, bounds.top, bounds.right - bounds.left,
                       bounds.bottom - bounds.top,
                       SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    return TRUE;
}

static reach_result reach_monitor_apply_work_area(reach_rect_i32 monitor_bounds,
                                                  reach_rect_i32 work_area)
{
    RECT desired = reach_monitor_win32_rect(work_area);
    if (!SystemParametersInfoW(SPI_SETWORKAREA, 0, &desired, SPIF_SENDCHANGE))
    {
        return REACH_ERROR;
    }

    reach_monitor_work_area_repair repair = {};
    repair.monitor = reach_monitor_win32_rect(monitor_bounds);
    repair.work_area = desired;
    return EnumWindows(reach_monitor_repair_maximized_window, reinterpret_cast<LPARAM>(&repair))
               ? REACH_OK
               : REACH_ERROR;
}

static void reach_monitor_update_cached_work_area(reach_monitor_list *list,
                                                  reach_rect_i32 monitor_bounds,
                                                  reach_rect_i32 work_area)
{
    for (reach_monitor_info &monitor : list->monitors)
    {
        if (reach_monitor_rect_equal(monitor.bounds, monitor_bounds))
        {
            monitor.work_area = work_area;
            return;
        }
    }
}

static reach_result reach_monitor_set_work_area(reach_monitor_list *list,
                                                reach_rect_i32 monitor_bounds,
                                                reach_rect_i32 work_area)
{
    if (list == nullptr || !reach_monitor_rect_valid(monitor_bounds) ||
        !reach_monitor_rect_valid(work_area) || work_area.left < monitor_bounds.left ||
        work_area.top < monitor_bounds.top || work_area.right > monitor_bounds.right ||
        work_area.bottom > monitor_bounds.bottom)
    {
        return REACH_INVALID_ARGUMENT;
    }

    size_t target_index = list->monitors.size();
    for (size_t index = 0; index < list->monitors.size(); ++index)
    {
        if (reach_monitor_rect_equal(list->monitors[index].bounds, monitor_bounds))
        {
            target_index = index;
            break;
        }
    }
    if (target_index >= list->devices.size())
    {
        return REACH_INVALID_ARGUMENT;
    }
    const std::wstring &target_device = list->devices[target_index];

    if (list->work_area_initialized && list->controlled_device == target_device &&
        reach_monitor_rect_equal(list->controlled_work_area, work_area))
    {
        return REACH_OK;
    }

    if (list->work_area_changed && list->controlled_device != target_device)
    {
        for (size_t index = 0; index < list->devices.size(); ++index)
        {
            if (list->devices[index] == list->controlled_device &&
                reach_monitor_apply_work_area(list->monitors[index].bounds,
                                              list->monitors[index].bounds) != REACH_OK)
            {
                return REACH_ERROR;
            }
        }
    }

    if (reach_monitor_apply_work_area(monitor_bounds, work_area) != REACH_OK)
    {
        return REACH_ERROR;
    }

    list->work_area_initialized = 1;
    list->work_area_changed = !reach_monitor_rect_equal(monitor_bounds, work_area);
    list->controlled_monitor = monitor_bounds;
    list->controlled_work_area = work_area;
    list->controlled_device = target_device;
    reach_monitor_update_cached_work_area(list, monitor_bounds, work_area);
    return REACH_OK;
}

reach_result reach_windows_create_monitor_list(reach_monitor_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_result result = reach_monitor_list_create(&out_port->list);
    if (result != REACH_OK)
    {
        return result;
    }

    out_port->ops.destroy = reach_monitor_list_destroy;
    out_port->ops.refresh = reach_monitor_refresh;
    out_port->ops.count = reach_monitor_count;
    out_port->ops.get = reach_monitor_get;
    out_port->ops.primary = reach_monitor_primary;
    out_port->ops.set_work_area = reach_monitor_set_work_area;
    return REACH_OK;
}
