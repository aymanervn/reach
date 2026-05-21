#ifndef REACH_PORTS_WALLPAPER_SURFACE_H
#define REACH_PORTS_WALLPAPER_SURFACE_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/ui_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_wallpaper_surface reach_wallpaper_surface;

typedef struct reach_wallpaper_surface_ops {
    reach_result (*show)(reach_wallpaper_surface *surface);
    reach_result (*hide)(reach_wallpaper_surface *surface);
    reach_result (*set_bounds)(reach_wallpaper_surface *surface, reach_rect_f32 bounds);
    reach_result (*set_wallpaper)(reach_wallpaper_surface *surface, const uint16_t *path);
    reach_result (*set_monitor_wallpaper)(reach_wallpaper_surface *surface, size_t monitor_index, const uint16_t *path);
    reach_result (*clear_monitor_wallpaper)(reach_wallpaper_surface *surface, size_t monitor_index);
    reach_result (*clear)(reach_wallpaper_surface *surface);
    void (*destroy)(reach_wallpaper_surface *surface);
} reach_wallpaper_surface_ops;

typedef struct reach_wallpaper_surface_port {
    reach_wallpaper_surface *surface;
    reach_wallpaper_surface_ops ops;
} reach_wallpaper_surface_port;

#ifdef __cplusplus
}
#endif

#endif
