#ifndef REACH_PORTS_WINDOW_MANAGER_H
#define REACH_PORTS_WINDOW_MANAGER_H

#include <stdint.h>

#include "reach/layout.h"
#include "reach/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_window_manager reach_window_manager;

typedef struct reach_window_snapshot {
    uintptr_t id;
    uint16_t title[260];
    uint16_t path[260];
    reach_rect_i32 bounds;
    int32_t visible;
    int32_t maximized;
    int32_t minimized;
} reach_window_snapshot;

typedef struct reach_window_manager_ops {
    reach_result (*start)(reach_window_manager *manager);
    reach_result (*stop)(reach_window_manager *manager);
    reach_result (*refresh)(reach_window_manager *manager);
    reach_result (*snap)(reach_window_manager *manager, uintptr_t window_id, reach_split_mode mode);
    uintptr_t (*foreground)(const reach_window_manager *manager);
    int32_t (*foreground_is_maximized)(const reach_window_manager *manager);
    int32_t (*dock_should_auto_hide)(const reach_window_manager *manager);
    int32_t (*needs_refresh)(const reach_window_manager *manager);
    size_t (*window_count)(const reach_window_manager *manager);
    reach_result (*window_at)(const reach_window_manager *manager, size_t index, reach_window_snapshot *out_window);
    reach_result (*activate)(reach_window_manager *manager, uintptr_t window_id);
    reach_result (*minimize)(reach_window_manager *manager, uintptr_t window_id);
    reach_result (*close)(reach_window_manager *manager, uintptr_t window_id);
    void (*destroy)(reach_window_manager *manager);
} reach_window_manager_ops;

typedef struct reach_window_manager_port {
    reach_window_manager *manager;
    reach_window_manager_ops ops;
} reach_window_manager_port;

#ifdef __cplusplus
}
#endif

#endif
