#ifndef REACH_LAYOUT_H
#define REACH_LAYOUT_H

#include "reach/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum reach_split_mode {
    REACH_SPLIT_LEFT = 0,
    REACH_SPLIT_RIGHT = 1,
    REACH_SPLIT_TOP = 2,
    REACH_SPLIT_BOTTOM = 3,
    REACH_SPLIT_FULL = 4
} reach_split_mode;

reach_result reach_layout_compute_split(reach_rect_i32 work_area, reach_split_mode mode, reach_rect_i32 *out_rect);

#ifdef __cplusplus
}
#endif

#endif
