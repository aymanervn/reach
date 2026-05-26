#include "reach/features/wallpaper.h"

int32_t reach_wallpaper_seed_or_apply(
    reach_wallpaper_service_port *wallpaper_service,
    reach_wallpaper_surface_port *wallpaper_surface,
    uint16_t *wallpaper_path,
    size_t wallpaper_path_count,
    uint16_t monitor_wallpaper_paths[][260],
    size_t monitor_wallpaper_count,
    uint16_t *cached_wallpaper_path,
    size_t cached_wallpaper_path_count)
{
    if (wallpaper_path == nullptr ||
        wallpaper_path_count == 0 ||
        wallpaper_service == nullptr ||
        wallpaper_service->service == nullptr ||
        cached_wallpaper_path == nullptr ||
        cached_wallpaper_path_count == 0) {
        return 0;
    }
    cached_wallpaper_path[0] = 0;
    if (wallpaper_path[0] != 0) {
        reach_copy_utf16(cached_wallpaper_path, cached_wallpaper_path_count, wallpaper_path);
        if (wallpaper_service->ops.set_wallpaper != nullptr) {
            (void)wallpaper_service->ops.set_wallpaper(wallpaper_service->service, wallpaper_path);
        }
        if (wallpaper_surface != nullptr && wallpaper_surface->ops.set_wallpaper != nullptr) {
            (void)wallpaper_surface->ops.set_wallpaper(wallpaper_surface->surface, wallpaper_path);
        }
        if (wallpaper_surface != nullptr &&
            wallpaper_surface->ops.set_monitor_wallpaper != nullptr &&
            monitor_wallpaper_paths != nullptr) {
            for (size_t index = 0; index < monitor_wallpaper_count; ++index) {
                if (monitor_wallpaper_paths[index][0] != 0) {
                    (void)wallpaper_surface->ops.set_monitor_wallpaper(
                        wallpaper_surface->surface,
                        index,
                        monitor_wallpaper_paths[index]);
                }
            }
        }
        return 0;
    }
    if (wallpaper_service->ops.current_wallpaper == nullptr) {
        return 0;
    }
    uint16_t current[260] = {};
    int32_t changed = 0;
    if (wallpaper_service->ops.current_wallpaper(wallpaper_service->service, current, 260) == REACH_OK &&
        current[0] != 0) {
        reach_copy_utf16(cached_wallpaper_path, cached_wallpaper_path_count, current);
        if (reach_copy_utf16(wallpaper_path, wallpaper_path_count, current) == REACH_OK) {
            changed = 1;
        }
        if (wallpaper_surface != nullptr && wallpaper_surface->ops.set_wallpaper != nullptr) {
            (void)wallpaper_surface->ops.set_wallpaper(wallpaper_surface->surface, current);
        }
    }
    if (wallpaper_surface != nullptr &&
        wallpaper_surface->ops.set_monitor_wallpaper != nullptr &&
        monitor_wallpaper_paths != nullptr) {
        for (size_t index = 0; index < monitor_wallpaper_count; ++index) {
            if (monitor_wallpaper_paths[index][0] != 0) {
                (void)wallpaper_surface->ops.set_monitor_wallpaper(
                    wallpaper_surface->surface,
                    index,
                    monitor_wallpaper_paths[index]);
            }
        }
    }
    return changed;
}
