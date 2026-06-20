#ifndef REACH_PORTS_DOCK_REVEAL_EDGE_H
#define REACH_PORTS_DOCK_REVEAL_EDGE_H

#include <stdint.h>

#include "reach/core/geometry.h"
#include "reach/core/window_id.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_dock_reveal_edge reach_dock_reveal_edge;

    typedef enum reach_dock_reveal_edge_event
    {
        REACH_DOCK_REVEAL_EDGE_ENTER = 1,
        REACH_DOCK_REVEAL_EDGE_LEAVE = 2
    } reach_dock_reveal_edge_event;

    typedef void (*reach_dock_reveal_edge_callback)(void *user, reach_dock_reveal_edge_event event);

    typedef struct reach_dock_reveal_edge_ops
    {
        reach_result (*set_bounds)(reach_dock_reveal_edge *edge, reach_rect_f32 bounds);

        reach_result (*show)(reach_dock_reveal_edge *edge);

        reach_result (*hide)(reach_dock_reveal_edge *edge);

        reach_result (*place_behind)(reach_dock_reveal_edge *edge, reach_window_id window);

        reach_result (*set_callback)(reach_dock_reveal_edge *edge,
                                     reach_dock_reveal_edge_callback callback, void *user);

        int32_t (*has_pending_events)(const reach_dock_reveal_edge *edge);

        reach_result (*dispatch_events)(reach_dock_reveal_edge *edge);

        void (*destroy)(reach_dock_reveal_edge *edge);
    } reach_dock_reveal_edge_ops;

    typedef struct reach_dock_reveal_edge_port
    {
        reach_dock_reveal_edge *edge;
        reach_dock_reveal_edge_ops ops;
    } reach_dock_reveal_edge_port;

#ifdef __cplusplus
}
#endif

#endif
