#include "host_internal.h"

const uint16_t *reach_host_dock_item_path(const reach_host *host, size_t item_index)
{
    if (host == nullptr || item_index >= reach_dock_item_count(host->dock_capsule))
    {
        return nullptr;
    }
    if (reach_dock_item_at(host->dock_capsule, item_index)->pinned)
    {
        size_t pinned_index = reach_dock_item_at(host->dock_capsule, item_index)->pinned_index;
        return pinned_index < host->pinned_app_count ? host->pinned_apps[pinned_index].path
                                                     : nullptr;
    }

    const reach_window_snapshot *window = reach_window_tracking_window_by_id(
        host->window_tracking, reach_dock_item_at(host->dock_capsule, item_index)->window);
    return window != nullptr ? window->path : nullptr;
}

reach_result reach_host_launch_dock_item(reach_host *host, size_t item_index,
                                         int32_t force_new_instance)
{
    if (host == nullptr || item_index >= reach_dock_item_count(host->dock_capsule))
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (reach_dock_item_at(host->dock_capsule, item_index)->pinned)
    {
        size_t pinned_index = reach_dock_item_at(host->dock_capsule, item_index)->pinned_index;
        return reach_host_open_pinned_app(host, pinned_index, force_new_instance, 0);
    }

    const uint16_t *path = reach_host_dock_item_path(host, item_index);
    return reach_host_open_app(host, path, nullptr, nullptr, force_new_instance, 0);
}
