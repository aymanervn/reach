#ifndef REACH_PORTS_WALLPAPER_SERVICE_H
#define REACH_PORTS_WALLPAPER_SERVICE_H

#include <stdint.h>

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_wallpaper_service reach_wallpaper_service;

    typedef struct reach_wallpaper_service_ops
    {
        reach_result (*set_wallpaper)(reach_wallpaper_service *service, const uint16_t *path);
        reach_result (*set_monitor_wallpaper)(reach_wallpaper_service *service,
                                              size_t monitor_index, const uint16_t *path);
        reach_result (*clear_wallpaper)(reach_wallpaper_service *service);
        reach_result (*current_wallpaper)(reach_wallpaper_service *service, uint16_t *out_path,
                                          size_t out_path_count);
        reach_result (*current_monitor_wallpaper)(reach_wallpaper_service *service,
                                                  size_t monitor_index, uint16_t *out_path,
                                                  size_t out_path_count);
        void (*destroy)(reach_wallpaper_service *service);
    } reach_wallpaper_service_ops;

    typedef struct reach_wallpaper_service_port
    {
        reach_wallpaper_service *service;
        reach_wallpaper_service_ops ops;
    } reach_wallpaper_service_port;

#ifdef __cplusplus
}
#endif

#endif
