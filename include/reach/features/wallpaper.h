#ifndef REACH_FEATURES_WALLPAPER_H
#define REACH_FEATURES_WALLPAPER_H

#include <stddef.h>
#include <stdint.h>

#include "reach/ports/wallpaper_service.h"
#include "reach/ports/wallpaper_surface.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t reach_wallpaper_seed_or_apply(
    reach_wallpaper_service_port *wallpaper_service,
    reach_wallpaper_surface_port *wallpaper_surface,
    uint16_t *wallpaper_path,
    size_t wallpaper_path_count,
    uint16_t monitor_wallpaper_paths[][260],
    size_t monitor_wallpaper_count,
    uint16_t *cached_wallpaper_path,
    size_t cached_wallpaper_path_count);

#ifdef __cplusplus
}
#endif

#endif
