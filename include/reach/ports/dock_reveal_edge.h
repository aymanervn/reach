#ifndef REACH_PORTS_DOCK_REVEAL_EDGE_H
#define REACH_PORTS_DOCK_REVEAL_EDGE_H

#include <stdint.h>

#include "reach/core/ui_state.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_dock_reveal_edge reach_dock_reveal_edge;

typedef void (*reach_dock_reveal_edge_callback)(void *user);

typedef struct reach_dock_reveal_edge_ops {
    reach_result (*set_bounds)(
        reach_dock_reveal_edge *edge,
        reach_rect_f32 bounds
    );

    reach_result (*show)(
        reach_dock_reveal_edge *edge
    );

    reach_result (*hide)(
        reach_dock_reveal_edge *edge
    );

    reach_result (*set_callback)(
        reach_dock_reveal_edge *edge,
        reach_dock_reveal_edge_callback callback,
        void *user
    );

    void (*destroy)(
        reach_dock_reveal_edge *edge
    );
} reach_dock_reveal_edge_ops;

typedef struct reach_dock_reveal_edge_port {
    reach_dock_reveal_edge *edge;
    reach_dock_reveal_edge_ops ops;
} reach_dock_reveal_edge_port;

#ifdef __cplusplus
}
#endif

#endif
