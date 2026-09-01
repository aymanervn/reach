#ifndef REACH_PORTS_SCREEN_HOTSPOT_H
#define REACH_PORTS_SCREEN_HOTSPOT_H

#include <stdint.h>

#include "reach/core/geometry.h"
#include "reach/core/window_id.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_screen_hotspot reach_screen_hotspot;

    typedef enum reach_screen_hotspot_event
    {
        REACH_SCREEN_HOTSPOT_ENTER = 1,
        REACH_SCREEN_HOTSPOT_LEAVE = 2
    } reach_screen_hotspot_event;

    typedef void (*reach_screen_hotspot_callback)(void *user, reach_screen_hotspot_event event);

    typedef struct reach_screen_hotspot_ops
    {
        reach_result (*set_bounds)(reach_screen_hotspot *hotspot, reach_rect_f32 bounds);

        reach_result (*show)(reach_screen_hotspot *hotspot);

        reach_result (*hide)(reach_screen_hotspot *hotspot);

        reach_result (*set_topmost)(reach_screen_hotspot *hotspot, int32_t enabled);

        reach_window_id (*native_id)(const reach_screen_hotspot *hotspot);

        reach_result (*place_behind)(reach_screen_hotspot *hotspot, reach_window_id target);

        reach_result (*set_callback)(reach_screen_hotspot *hotspot,
                                     reach_screen_hotspot_callback callback, void *user);

        int32_t (*has_pending_events)(const reach_screen_hotspot *hotspot);

        reach_result (*dispatch_events)(reach_screen_hotspot *hotspot);

        void (*destroy)(reach_screen_hotspot *hotspot);
    } reach_screen_hotspot_ops;

    typedef struct reach_screen_hotspot_port
    {
        reach_screen_hotspot *hotspot;
        reach_screen_hotspot_ops ops;
    } reach_screen_hotspot_port;

    typedef struct reach_screen_hotspot_factory_ops
    {
        reach_result (*create)(void *factory, reach_screen_hotspot_port *out_port);
    } reach_screen_hotspot_factory_ops;

    typedef struct reach_screen_hotspot_factory_port
    {
        void *factory;
        reach_screen_hotspot_factory_ops ops;
    } reach_screen_hotspot_factory_port;

#ifdef __cplusplus
}
#endif

#endif
