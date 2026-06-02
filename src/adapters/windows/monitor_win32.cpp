#include "windows_adapters_internal.h"

#include <windows.h>
#include <shellscalingapi.h>

#include <new>
#include <vector>

struct reach_monitor_list
{
    std::vector<reach_monitor_info> monitors;
};

static reach_result reach_monitor_refresh(reach_monitor_list *list);

static reach_rect_i32 reach_rect_from_win32(const RECT &rect)
{
    reach_rect_i32 result = {};
    result.left = rect.left;
    result.top = rect.top;
    result.right = rect.right;
    result.bottom = rect.bottom;
    return result;
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
        return TRUE;
    }

    reach_monitor_info item = {};
    item.id = static_cast<uint32_t>(list->monitors.size() + 1);
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

    list->monitors.push_back(item);
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
    delete list;
}

static reach_result reach_monitor_refresh(reach_monitor_list *list)
{
    if (list == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    list->monitors.clear();
    if (!EnumDisplayMonitors(nullptr, nullptr, reach_monitor_enum_proc,
                             reinterpret_cast<LPARAM>(list)))
    {
        return REACH_ERROR;
    }

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
    return REACH_OK;
}
