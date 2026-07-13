#include "reach/features/wallpaper.h"

#include <new>

static int32_t reach_wallpaper_path_equals(const uint16_t *a, const uint16_t *b)
{
    size_t index = 0;
    while (a[index] != 0 && b[index] != 0)
    {
        if (a[index] != b[index])
        {
            return 0;
        }
        ++index;
    }
    return a[index] == b[index];
}

int32_t reach_wallpaper_seed_or_apply(reach_wallpaper_service_port *wallpaper_service,
                                      reach_wallpaper_surface_port *wallpaper_surface,
                                      uint16_t *wallpaper_path, size_t wallpaper_path_count,
                                      uint16_t monitor_wallpaper_paths[][260],
                                      size_t monitor_wallpaper_count,
                                      uint16_t *cached_wallpaper_path,
                                      size_t cached_wallpaper_path_count)
{
    if (wallpaper_path == nullptr || wallpaper_path_count == 0 || wallpaper_service == nullptr ||
        wallpaper_service->service == nullptr || cached_wallpaper_path == nullptr ||
        cached_wallpaper_path_count == 0)
    {
        return 0;
    }
    cached_wallpaper_path[0] = 0;
    if (wallpaper_path[0] != 0)
    {
        reach_copy_utf16(cached_wallpaper_path, cached_wallpaper_path_count, wallpaper_path);
        if (wallpaper_service->ops.set_wallpaper != nullptr)
        {
            (void)wallpaper_service->ops.set_wallpaper(wallpaper_service->service, wallpaper_path);
        }
        if (wallpaper_surface != nullptr && wallpaper_surface->ops.set_wallpaper != nullptr)
        {
            (void)wallpaper_surface->ops.set_wallpaper(wallpaper_surface->surface, wallpaper_path);
        }
        if (wallpaper_surface != nullptr &&
            wallpaper_surface->ops.set_monitor_wallpaper != nullptr &&
            monitor_wallpaper_paths != nullptr)
        {
            for (size_t index = 0; index < monitor_wallpaper_count; ++index)
            {
                if (monitor_wallpaper_paths[index][0] != 0)
                {
                    (void)wallpaper_surface->ops.set_monitor_wallpaper(
                        wallpaper_surface->surface, index, monitor_wallpaper_paths[index]);
                }
            }
        }
        return 0;
    }
    uint16_t current[260] = {};
    int32_t changed = 0;
    // Seed per monitor: the global query can return the stitched multi-monitor
    // composite (TranscodedWallpaper) when monitors have different wallpapers.
    if (wallpaper_service->ops.current_monitor_wallpaper != nullptr &&
        monitor_wallpaper_paths != nullptr)
    {
        for (size_t index = 0; index < monitor_wallpaper_count; ++index)
        {
            uint16_t monitor_current[260] = {};
            if (wallpaper_service->ops.current_monitor_wallpaper(
                    wallpaper_service->service, index, monitor_current, 260) != REACH_OK ||
                monitor_current[0] == 0)
            {
                continue;
            }
            if (current[0] == 0)
            {
                reach_copy_utf16(current, 260, monitor_current);
            }
            else if (monitor_wallpaper_paths[index][0] == 0 &&
                     !reach_wallpaper_path_equals(monitor_current, current) &&
                     reach_copy_utf16(monitor_wallpaper_paths[index], 260, monitor_current) ==
                         REACH_OK)
            {
                changed = 1;
            }
        }
    }
    if (current[0] == 0 && wallpaper_service->ops.current_wallpaper != nullptr &&
        wallpaper_service->ops.current_wallpaper(wallpaper_service->service, current, 260) !=
            REACH_OK)
    {
        current[0] = 0;
    }
    if (current[0] != 0)
    {
        reach_copy_utf16(cached_wallpaper_path, cached_wallpaper_path_count, current);
        if (reach_copy_utf16(wallpaper_path, wallpaper_path_count, current) == REACH_OK)
        {
            changed = 1;
        }
        if (wallpaper_surface != nullptr && wallpaper_surface->ops.set_wallpaper != nullptr)
        {
            (void)wallpaper_surface->ops.set_wallpaper(wallpaper_surface->surface, current);
        }
    }
    if (wallpaper_surface != nullptr && wallpaper_surface->ops.set_monitor_wallpaper != nullptr &&
        monitor_wallpaper_paths != nullptr)
    {
        for (size_t index = 0; index < monitor_wallpaper_count; ++index)
        {
            if (monitor_wallpaper_paths[index][0] != 0)
            {
                (void)wallpaper_surface->ops.set_monitor_wallpaper(
                    wallpaper_surface->surface, index, monitor_wallpaper_paths[index]);
            }
        }
    }
    return changed;
}

struct reach_wallpaper
{
    reach_wallpaper_service_port service;
    reach_wallpaper_surface_port surface;
    reach_config_store_port config_store;
    int32_t bounds_valid;
    reach_rect_f32 bounds;
    uint16_t path[260];
};

static int32_t reach_wallpaper_rect_equal(reach_rect_f32 a, reach_rect_f32 b)
{
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

reach_result reach_wallpaper_create(reach_wallpaper_service_port service,
                                    reach_wallpaper_surface_port surface,
                                    reach_config_store_port config_store,
                                    reach_wallpaper **out_wallpaper)
{
    if (out_wallpaper == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_wallpaper *wallpaper = new (std::nothrow) reach_wallpaper();
    if (wallpaper == nullptr)
    {
        return REACH_ERROR;
    }
    wallpaper->service = service;
    wallpaper->surface = surface;
    wallpaper->config_store = config_store;
    wallpaper->bounds_valid = 0;
    wallpaper->bounds = {};
    wallpaper->path[0] = 0;
    *out_wallpaper = wallpaper;
    return REACH_OK;
}

void reach_wallpaper_destroy(reach_wallpaper *wallpaper)
{

    delete wallpaper;
}

void reach_wallpaper_apply_snapshot(reach_wallpaper *wallpaper, reach_config_snapshot *snapshot)
{
    if (wallpaper == nullptr || snapshot == nullptr)
    {
        return;
    }
    int32_t changed = reach_wallpaper_seed_or_apply(
        &wallpaper->service, &wallpaper->surface, snapshot->wallpaper_path, 260,
        snapshot->monitor_wallpaper_paths, REACH_MAX_WALLPAPER_MONITORS, wallpaper->path, 260);
    if (changed && wallpaper->config_store.ops.save != nullptr)
    {
        (void)wallpaper->config_store.ops.save(wallpaper->config_store.store, snapshot);
    }
}

void reach_wallpaper_reload(reach_wallpaper *wallpaper, int32_t force)
{
    if (wallpaper == nullptr || wallpaper->config_store.ops.load == nullptr)
    {
        return;
    }

    reach_config_snapshot snapshot = {};
    if (wallpaper->config_store.ops.load(wallpaper->config_store.store, &snapshot) != REACH_OK)
    {
        return;
    }

    uint16_t new_path[260] = {};
    if (snapshot.wallpaper_path[0] != 0)
    {
        reach_copy_utf16(new_path, 260, snapshot.wallpaper_path);
    }

    if (!force && reach_wallpaper_path_equals(wallpaper->path, new_path))
    {
        return;
    }

    reach_copy_utf16(wallpaper->path, 260, new_path);
    if (new_path[0] != 0 && wallpaper->surface.ops.set_wallpaper != nullptr)
    {
        (void)wallpaper->surface.ops.set_wallpaper(wallpaper->surface.surface, new_path);
    }
    else if (new_path[0] == 0 && wallpaper->surface.ops.clear != nullptr)
    {
        (void)wallpaper->surface.ops.clear(wallpaper->surface.surface);
    }
    if (wallpaper->surface.ops.set_monitor_wallpaper != nullptr &&
        wallpaper->surface.ops.clear_monitor_wallpaper != nullptr)
    {
        for (size_t index = 0; index < REACH_MAX_WALLPAPER_MONITORS; ++index)
        {
            if (snapshot.monitor_wallpaper_paths[index][0] != 0)
            {
                (void)wallpaper->surface.ops.set_monitor_wallpaper(
                    wallpaper->surface.surface, index, snapshot.monitor_wallpaper_paths[index]);
            }
            else
            {
                (void)wallpaper->surface.ops.clear_monitor_wallpaper(wallpaper->surface.surface,
                                                                     index);
            }
        }
    }
}

reach_result reach_wallpaper_set_bounds(reach_wallpaper *wallpaper, reach_rect_f32 bounds)
{
    if (wallpaper == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (wallpaper->bounds_valid && reach_wallpaper_rect_equal(wallpaper->bounds, bounds))
    {
        return REACH_OK;
    }
    if (wallpaper->surface.ops.set_bounds != nullptr)
    {
        reach_result result = wallpaper->surface.ops.set_bounds(wallpaper->surface.surface, bounds);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    wallpaper->bounds = bounds;
    wallpaper->bounds_valid = 1;
    return REACH_OK;
}
