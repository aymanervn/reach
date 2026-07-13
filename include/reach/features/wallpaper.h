#ifndef REACH_FEATURES_WALLPAPER_H
#define REACH_FEATURES_WALLPAPER_H

#include <stddef.h>
#include <stdint.h>

#include "reach/ports/wallpaper_service.h"
#include "reach/ports/wallpaper_surface.h"
#include "reach/ports/config_store.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    int32_t reach_wallpaper_seed_or_apply(reach_wallpaper_service_port *wallpaper_service,
                                          reach_wallpaper_surface_port *wallpaper_surface,
                                          uint16_t *wallpaper_path, size_t wallpaper_path_count,
                                          uint16_t monitor_wallpaper_paths[][260],
                                          size_t monitor_wallpaper_count,
                                          uint16_t *cached_wallpaper_path,
                                          size_t cached_wallpaper_path_count);

    typedef struct reach_wallpaper reach_wallpaper;

    reach_result reach_wallpaper_create(reach_wallpaper_service_port service,
                                        reach_wallpaper_surface_port surface,
                                        reach_config_store_port config_store,
                                        reach_wallpaper **out_wallpaper);
    void reach_wallpaper_destroy(reach_wallpaper *wallpaper);

    void reach_wallpaper_apply_snapshot(reach_wallpaper *wallpaper,
                                        reach_config_snapshot *snapshot);

    void reach_wallpaper_reload(reach_wallpaper *wallpaper, int32_t force);

    reach_result reach_wallpaper_set_bounds(reach_wallpaper *wallpaper, reach_rect_f32 bounds);

#ifdef __cplusplus
}
#endif

#endif
