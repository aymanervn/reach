#ifndef REACH_FEATURES_COMMON_DRAGGABLE_H
#define REACH_FEATURES_COMMON_DRAGGABLE_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/geometry.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_DRAGGABLE_TARGET_NONE UINT64_MAX

    typedef struct reach_draggable
    {
        uint64_t target;
        int32_t start_x;
        int32_t start_y;
        int32_t tracking;
        int32_t moved;
    } reach_draggable;

    typedef struct reach_draggable_result
    {
        uint64_t target;
        int32_t started;
        int32_t moved;
        int32_t ended;
    } reach_draggable_result;

    void reach_draggable_init(reach_draggable *draggable);
    void reach_draggable_begin(reach_draggable *draggable, uint64_t target, int32_t x, int32_t y);
    void reach_draggable_update(reach_draggable *draggable, int32_t x, int32_t y,
                                int32_t threshold_squared, reach_draggable_result *out);
    void reach_draggable_end(reach_draggable *draggable, reach_draggable_result *out);
    int32_t reach_draggable_tracking(const reach_draggable *draggable);
    int32_t reach_draggable_moved(const reach_draggable *draggable);

    size_t reach_horizontal_reorder_target(const reach_rect_f32 *slots, size_t count,
                                           size_t current_index, float dragged_x,
                                           float neighbor_threshold_ratio);

#ifdef __cplusplus
}
#endif

#endif
