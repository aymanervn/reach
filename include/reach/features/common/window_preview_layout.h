#ifndef REACH_FEATURES_COMMON_WINDOW_PREVIEW_LAYOUT_H
#define REACH_FEATURES_COMMON_WINDOW_PREVIEW_LAYOUT_H

#include <stddef.h>
#include "reach/core/geometry.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_window_preview_range
    {
        size_t start;
        size_t end;
        float width;
    } reach_window_preview_range;

    float reach_window_preview_aspect(reach_rect_f32 frame);
    float reach_window_preview_row_scale(float row_width, float available_width);
    float reach_window_preview_height(const float *aspects, size_t count, float requested_height,
                                      float maximum_height, float maximum_width);
    reach_window_preview_range reach_window_preview_pack(const float *widths, size_t count,
                                                         size_t start, size_t capacity, float room,
                                                         float gap);

#ifdef __cplusplus
}
#endif

#endif
