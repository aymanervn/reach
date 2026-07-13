#include "host_internal.h"

reach_result reach_host_set_pinned_apps(reach_host *host, const reach_pinned_app_model *apps,
                                         size_t count)
{
    if (host == nullptr || (apps == nullptr && count != 0))
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (count > REACH_MAX_PINNED_APPS)
    {
        count = REACH_MAX_PINNED_APPS;
    }
    for (size_t index = 0; index < count; ++index)
    {
        host->pinned_apps[index] = apps[index];
    }
    host->pinned_app_count = count;
    return REACH_OK;
}

static int32_t reach_host_utf16_equal(const uint16_t *a, const uint16_t *b)
{
    size_t index = 0;
    if (a == nullptr || b == nullptr)
    {
        return a == b;
    }
    while (a[index] != 0 || b[index] != 0)
    {
        if (a[index] != b[index])
        {
            return 0;
        }
        ++index;
    }
    return 1;
}

static int32_t reach_host_path_equals(const uint16_t *a, const uint16_t *b)
{
    if (a == nullptr || b == nullptr)
    {
        return 0;
    }

    size_t index = 0;
    while (a[index] != 0 && b[index] != 0)
    {
        uint16_t ca = a[index];
        uint16_t cb = b[index];
        if (ca >= 'A' && ca <= 'Z')
        {
            ca = (uint16_t)(ca + ('a' - 'A'));
        }
        if (cb >= 'A' && cb <= 'Z')
        {
            cb = (uint16_t)(cb + ('a' - 'A'));
        }
        if (ca != cb)
        {
            return 0;
        }
        ++index;
    }

    return a[index] == b[index];
}

/*
 * Config shims: the reload + pin-persistence worker lives in the config
 * SERVICE (services/config); composition applies the completed snapshots
 * (pins remap + wallpaper) on the main thread.
 */

reach_result reach_host_schedule_config_reload(reach_host *host)
{
    return host != nullptr ? reach_config_service_schedule_reload(host->config_service)
                           : REACH_INVALID_ARGUMENT;
}

reach_result reach_host_schedule_pin_app(reach_host *host, const reach_pinned_app_model *app)
{
    return host != nullptr ? reach_config_service_schedule_pin_app(host->config_service, app)
                           : REACH_INVALID_ARGUMENT;
}

reach_result reach_host_schedule_unpin_id(reach_host *host, uint32_t pin_id)
{
    return host != nullptr ? reach_config_service_schedule_unpin(host->config_service, pin_id)
                           : REACH_INVALID_ARGUMENT;
}

reach_result reach_host_schedule_move_pin(reach_host *host, uint32_t pin_id, size_t target_index)
{
    return host != nullptr ? reach_config_service_schedule_move_pin(host->config_service, pin_id,
                                                                    target_index)
                           : REACH_INVALID_ARGUMENT;
}

void reach_host_stop_config_reload_worker(reach_host *host)
{
    if (host != nullptr)
    {
        reach_config_service_stop(host->config_service);
    }
}

int32_t reach_host_config_reload_work_pending(const reach_host *host)
{
    return host != nullptr && reach_config_service_work_pending(host->config_service);
}

int32_t reach_host_apply_config_reload_result(reach_host *host)
{
    if (host == nullptr)
    {
        return 0;
    }

    reach_result result = REACH_OK;
    reach_config_snapshot snapshot = {};
    int32_t current = 0;
    if (!reach_config_service_take_completed(host->config_service, &result, &snapshot, &current))
    {
        return 0;
    }
    if (!current)
    {
        return 1; /* superseded — handled, a newer job's completion follows */
    }

    if (result == REACH_OK)
    {
        (void)reach_host_apply_config_snapshot(host, &snapshot, 1, 1);
    }
    return 1;
}

static reach_result reach_host_apply_pins_from_snapshot(reach_host *host,
                                                         const reach_config_snapshot *snapshot)
{
    if (host == nullptr || snapshot == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    /* Snapshot the user's dock ordering, remap pinned entries across the reload
     * by path (pin ids are reassigned), and hand the keys back to the capsule. */
    reach_dock_order_key old_order[REACH_MAX_PINNED_APPS] = {};
    uint16_t old_order_paths[REACH_MAX_PINNED_APPS][260] = {};
    size_t old_order_count = reach_dock_order_count(host->dock_capsule);
    for (size_t order_index = 0; order_index < old_order_count; ++order_index)
    {
        old_order[order_index] = reach_dock_order_key_at(host->dock_capsule, order_index);
        if (old_order[order_index].pinned)
        {
            for (size_t pin_index = 0; pin_index < host->pinned_app_count; ++pin_index)
            {
                if (host->pinned_apps[pin_index].id == old_order[order_index].pin_id)
                {
                    reach_copy_utf16(old_order_paths[order_index], 260,
                                     host->pinned_apps[pin_index].path);
                    break;
                }
            }
        }
    }

    reach_result result = reach_host_set_pinned_apps(host, snapshot->pinned_apps,
                                                      snapshot->pinned_app_count);
    if (result != REACH_OK)
    {
        return result;
    }
    for (size_t order_index = 0; order_index < old_order_count; ++order_index)
    {
        if (old_order[order_index].pinned && old_order_paths[order_index][0] != 0)
        {
            for (size_t pin_index = 0; pin_index < host->pinned_app_count; ++pin_index)
            {
                if (reach_host_path_equals(host->pinned_apps[pin_index].path,
                                            old_order_paths[order_index]))
                {
                    old_order[order_index].pin_id = host->pinned_apps[pin_index].id;
                    break;
                }
            }
        }
    }
    reach_dock_restore_order(host->dock_capsule, old_order, old_order_count);
    host->dirty.layout = 1;
    host->dirty.render = 1;
    host->dock.dirty_flags = 1;
    host->launcher.dirty_flags = 1;
    reach_dock_mark_items_changed(host->dock_capsule);
    reach_host_request_update(host);
    return result;
}

void reach_host_seed_or_apply_wallpaper(reach_host *host, reach_config_snapshot *snapshot)
{
    if (host == nullptr || snapshot == nullptr)
    {
        return;
    }
    reach_wallpaper_apply_snapshot(host->wallpaper, snapshot);
}

static int32_t reach_host_pinned_apps_equal(const reach_pinned_app_model *a, size_t a_count,
                                             const reach_pinned_app_model *b, size_t b_count)
{
    if (a_count != b_count)
    {
        return 0;
    }

    for (size_t index = 0; index < a_count; ++index)
    {
        if (a[index].id != b[index].id ||
            !reach_host_utf16_equal(a[index].title, b[index].title) ||
            !reach_host_utf16_equal(a[index].path, b[index].path) ||
            !reach_host_utf16_equal(a[index].arguments, b[index].arguments) ||
            !reach_host_utf16_equal(a[index].icon_ref, b[index].icon_ref) ||
            !reach_host_utf16_equal(a[index].app_user_model_id, b[index].app_user_model_id))
        {
            return 0;
        }
    }

    return 1;
}

reach_result reach_host_apply_config_snapshot(reach_host *host,
                                               const reach_config_snapshot *snapshot,
                                               int32_t apply_pins, int32_t apply_wallpaper)
{
    if (host == nullptr || snapshot == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (apply_pins &&
        !reach_host_pinned_apps_equal(host->pinned_apps, host->pinned_app_count,
                                       snapshot->pinned_apps, snapshot->pinned_app_count))
    {
        reach_result pin_result = reach_host_apply_pins_from_snapshot(host, snapshot);
        if (pin_result != REACH_OK)
        {
            return pin_result;
        }
    }

    if (apply_wallpaper)
    {
        reach_config_snapshot writable_snapshot = *snapshot;
        reach_host_seed_or_apply_wallpaper(host, &writable_snapshot);
    }
    return REACH_OK;
}

void reach_host_reload_wallpaper(reach_host *host, int32_t force)
{
    if (host == nullptr)
    {
        return;
    }
    reach_wallpaper_reload(host->wallpaper, force);
}


