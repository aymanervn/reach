#ifndef REACH_FEATURES_COMMON_SELECTION_PILL_H
#define REACH_FEATURES_COMMON_SELECTION_PILL_H

#include "reach/core/animated_rect.h"
#include "reach/core/theme.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_selection_pill_mode
    {
        REACH_SELECTION_PILL_STEP = 0,
        REACH_SELECTION_PILL_JUMP,
        REACH_SELECTION_PILL_SET
    } reach_selection_pill_mode;

    typedef struct reach_selection_pill
    {
        reach_rect_motion rect;
        size_t slot;
    } reach_selection_pill;

    reach_selection_pill_mode reach_selection_pill_mode_for_distance(size_t from_slot,
                                                                     size_t to_slot);
    void reach_selection_pill_retarget(reach_selection_pill *pill, const reach_theme *theme,
                                       reach_rect_f32 target, size_t slot,
                                       reach_selection_pill_mode mode, double now_seconds);

#ifdef __cplusplus
}
#endif

#endif
