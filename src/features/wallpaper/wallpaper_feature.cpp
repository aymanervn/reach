#include "reach/features/wallpaper.h"

void reach_wallpaper_seed_or_apply(
    reach_config_store_port *config_store,
    reach_wallpaper_service_port *wallpaper_service,
    reach_wallpaper_surface_port *wallpaper_surface,
    reach_config_snapshot *snapshot,
    uint16_t *cached_wallpaper_path,
    size_t cached_wallpaper_path_count)
{
    if (snapshot == nullptr ||
        wallpaper_service == nullptr ||
        wallpaper_service->service == nullptr ||
        cached_wallpaper_path == nullptr ||
        cached_wallpaper_path_count == 0) {
        return;
    }
    cached_wallpaper_path[0] = 0;
    if (snapshot->wallpaper_path[0] != 0) {
        reach_copy_utf16(cached_wallpaper_path, cached_wallpaper_path_count, snapshot->wallpaper_path);
        if (wallpaper_service->ops.set_wallpaper != nullptr) {
            (void)wallpaper_service->ops.set_wallpaper(wallpaper_service->service, snapshot->wallpaper_path);
        }
        if (wallpaper_surface != nullptr && wallpaper_surface->ops.set_wallpaper != nullptr) {
            (void)wallpaper_surface->ops.set_wallpaper(wallpaper_surface->surface, snapshot->wallpaper_path);
        }
        if (wallpaper_surface != nullptr && wallpaper_surface->ops.set_monitor_wallpaper != nullptr) {
            for (size_t index = 0; index < REACH_MAX_WALLPAPER_MONITORS; ++index) {
                if (snapshot->monitor_wallpaper_paths[index][0] != 0) {
                    (void)wallpaper_surface->ops.set_monitor_wallpaper(
                        wallpaper_surface->surface,
                        index,
                        snapshot->monitor_wallpaper_paths[index]);
                }
            }
        }
        return;
    }
    if (config_store == nullptr ||
        wallpaper_service->ops.current_wallpaper == nullptr ||
        config_store->ops.save == nullptr) {
        return;
    }
    uint16_t current[260] = {};
    if (wallpaper_service->ops.current_wallpaper(wallpaper_service->service, current, 260) == REACH_OK &&
        current[0] != 0) {
        reach_copy_utf16(cached_wallpaper_path, cached_wallpaper_path_count, current);
        (void)reach_copy_utf16(snapshot->wallpaper_path, 260, current);
        (void)config_store->ops.save(config_store->store, snapshot);
        if (wallpaper_surface != nullptr && wallpaper_surface->ops.set_wallpaper != nullptr) {
            (void)wallpaper_surface->ops.set_wallpaper(wallpaper_surface->surface, current);
        }
    }
    if (wallpaper_surface != nullptr && wallpaper_surface->ops.set_monitor_wallpaper != nullptr) {
        for (size_t index = 0; index < REACH_MAX_WALLPAPER_MONITORS; ++index) {
            if (snapshot->monitor_wallpaper_paths[index][0] != 0) {
                (void)wallpaper_surface->ops.set_monitor_wallpaper(
                    wallpaper_surface->surface,
                    index,
                    snapshot->monitor_wallpaper_paths[index]);
            }
        }
    }
}
