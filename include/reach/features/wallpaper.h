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

    /*
     * Wallpaper capsule. Owns the wallpaper state (cached path + applied desktop
     * bounds) and the seed/apply/reload/bounds logic that used to live in
     * composition (shell_config / shell_dock_layout). It borrows the wallpaper
     * service/surface/config ports (composition still owns their lifetime and the
     * surface show/hide/destroy policy); destroying the capsule frees only its own
     * state. Composition just wires: create it, call apply/reload/set_bounds on a
     * condition, destroy it.
     */
    typedef struct reach_wallpaper reach_wallpaper;

    reach_result reach_wallpaper_create(reach_wallpaper_service_port service,
                                        reach_wallpaper_surface_port surface,
                                        reach_config_store_port config_store,
                                        reach_wallpaper **out_wallpaper);
    void reach_wallpaper_destroy(reach_wallpaper *wallpaper);

    /* Seed-or-apply from a (writable) snapshot, persisting via the config store
       when seeding changed the snapshot. */
    void reach_wallpaper_apply_snapshot(reach_wallpaper *wallpaper,
                                        reach_config_snapshot *snapshot);

    /* Reload wallpaper paths from the config store and apply to the surface when
       the primary path changed (or force is set). */
    void reach_wallpaper_reload(reach_wallpaper *wallpaper, int32_t force);

    /* Apply new desktop bounds to the surface when they differ from the last
       applied bounds. */
    reach_result reach_wallpaper_set_bounds(reach_wallpaper *wallpaper, reach_rect_f32 bounds);

#ifdef __cplusplus
}
#endif

#endif
