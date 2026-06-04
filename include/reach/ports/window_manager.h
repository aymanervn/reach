#ifndef REACH_PORTS_WINDOW_MANAGER_H
#define REACH_PORTS_WINDOW_MANAGER_H

#include <stdint.h>

#include "reach/core/pinned_app.h"
#include "reach/core/window_id.h"
#include "reach/support/layout.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_window_manager reach_window_manager;

    typedef struct reach_window_snapshot
    {
        reach_window_id id;
        uint16_t title[260];
        uint16_t path[260];
        uint16_t app_user_model_id[260];
        reach_rect_i32 bounds;
        int32_t visible;
        int32_t maximized;
        int32_t minimized;
    } reach_window_snapshot;

    typedef struct reach_window_manager_ops
    {
        reach_result (*start)(reach_window_manager *manager);
        reach_result (*stop)(reach_window_manager *manager);
        reach_result (*refresh)(reach_window_manager *manager);
        reach_result (*snap)(reach_window_manager *manager, reach_window_id window_id,
                             reach_split_mode mode);
        reach_window_id (*foreground)(const reach_window_manager *manager);
        int32_t (*foreground_is_maximized)(const reach_window_manager *manager);
        int32_t (*foreground_is_fullscreen)(const reach_window_manager *manager);
        int32_t (*foreground_is_exclusive_fullscreen)(const reach_window_manager *manager);
        int32_t (*dock_should_auto_hide)(const reach_window_manager *manager);
        int32_t (*needs_refresh)(const reach_window_manager *manager);
        size_t (*window_count)(const reach_window_manager *manager);
        reach_result (*window_at)(const reach_window_manager *manager, size_t index,
                                  reach_window_snapshot *out_window);
        reach_result (*pin_app_for_window)(reach_window_manager *manager, reach_window_id window_id,
                                           const reach_window_snapshot *snapshot,
                                           reach_pinned_app_model *out_app);
        int32_t (*privileged_control_available)(const reach_window_manager *manager);
        int32_t (*confirm_privileged_control_restart)(reach_window_manager *manager);
        reach_result (*start_privileged_control)(reach_window_manager *manager);
        reach_result (*activate)(reach_window_manager *manager, reach_window_id window_id);
        reach_result (*minimize)(reach_window_manager *manager, reach_window_id window_id);
        reach_result (*close)(reach_window_manager *manager, reach_window_id window_id);
        reach_result (*kill_process)(reach_window_manager *manager, reach_window_id window_id);
        void (*destroy)(reach_window_manager *manager);
    } reach_window_manager_ops;

    typedef struct reach_window_manager_port
    {
        reach_window_manager *manager;
        reach_window_manager_ops ops;
    } reach_window_manager_port;

#ifdef __cplusplus
}
#endif

#endif
