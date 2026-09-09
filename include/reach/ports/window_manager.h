#ifndef REACH_PORTS_WINDOW_MANAGER_H
#define REACH_PORTS_WINDOW_MANAGER_H

#include <stdint.h>

#include "reach/core/geometry.h"
#include "reach/core/pinned_app.h"
#include "reach/core/process_id.h"
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
        reach_process_id process_id;
        uint16_t title[260];
        reach_application_identity identity;
        uint16_t icon_ref[260];
        int32_t visible;
        int32_t maximized;
        int32_t minimized;
    } reach_window_snapshot;

    typedef struct reach_window_move
    {
        reach_window_id window;
        reach_point_f32 position;
    } reach_window_move;

    typedef struct reach_window_manager_ops
    {
        reach_result (*start)(reach_window_manager *manager);
        reach_result (*stop)(reach_window_manager *manager);
        reach_result (*refresh)(reach_window_manager *manager);
        reach_result (*snap)(reach_window_manager *manager, reach_window_id window_id,
                             reach_split_mode mode);

        int32_t (*game_mode_active)(const reach_window_manager *manager);
        reach_window_id (*foreground_fullscreen_window)(const reach_window_manager *manager);
        int32_t (*needs_refresh)(const reach_window_manager *manager);
        size_t (*window_count)(const reach_window_manager *manager);
        reach_result (*window_at)(const reach_window_manager *manager, size_t index,
                                  reach_window_snapshot *out_window);
        reach_result (*frame_bounds)(const reach_window_manager *manager, reach_window_id window_id,
                                     reach_rect_f32 *out_bounds);
        reach_result (*outer_bounds)(const reach_window_manager *manager, reach_window_id window_id,
                                     reach_rect_f32 *out_bounds);
        reach_result (*move_windows)(reach_window_manager *manager,
                                     const reach_window_move *windows, size_t count);
        int32_t (*privileged_control_available)(const reach_window_manager *manager);
        reach_result (*start_privileged_control)(reach_window_manager *manager);
        reach_result (*activate)(reach_window_manager *manager, reach_window_id window_id);
        reach_result (*minimize)(reach_window_manager *manager, reach_window_id window_id);
        reach_result (*close)(reach_window_manager *manager, reach_window_id window_id);
        void (*destroy)(reach_window_manager *manager);
        int32_t (*is_foreground)(const reach_window_manager *manager, reach_window_id window);
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
