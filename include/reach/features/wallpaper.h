#ifndef REACH_FEATURES_WALLPAPER_H
#define REACH_FEATURES_WALLPAPER_H

#include <stddef.h>
#include <stdint.h>

#include "reach/ports/config_store.h"
#include "reach/ports/wallpaper_service.h"
#include "reach/ports/wallpaper_surface.h"

#ifdef __cplusplus
extern "C" {
#endif

void reach_wallpaper_seed_or_apply(
    reach_config_store_port *config_store,
    reach_wallpaper_service_port *wallpaper_service,
    reach_wallpaper_surface_port *wallpaper_surface,
    reach_config_snapshot *snapshot,
    uint16_t *cached_wallpaper_path,
    size_t cached_wallpaper_path_count);

#ifdef __cplusplus
}
#endif

#endif
