#include "reach/features/common/window_preview_layout.h"

#include <math.h>

float reach_window_preview_aspect(reach_rect_f32 frame)
{
    if (!isfinite(frame.width) || !isfinite(frame.height) || frame.width <= 0.0f ||
        frame.height <= 0.0f)
    {
        return 1.0f;
    }
    float aspect = frame.width / frame.height;
    return isfinite(aspect) && aspect > 0.0f ? aspect : 1.0f;
}

float reach_window_preview_height(const float *aspects, size_t count, float requested_height,
                                  float maximum_height, float maximum_width)
{
    float widest = 1.0f;
    for (size_t index = 0; index < count; ++index)
    {
        widest = fmaxf(widest, aspects[index]);
    }
    return fmaxf(0.0f, fminf(requested_height, fminf(maximum_height, maximum_width / widest)));
}

float reach_window_preview_row_scale(float row_width, float available_width)
{
    return row_width > 0.0f ? fminf(1.0f, fmaxf(0.0f, available_width) / row_width) : 1.0f;
}

reach_window_preview_range reach_window_preview_pack(const float *widths, size_t count,
                                                     size_t start, size_t capacity, float room,
                                                     float gap)
{
    reach_window_preview_range range = {};
    range.start = start < count ? start : count;
    range.end = range.start;
    while (range.end < count && range.end - range.start < capacity)
    {
        float next = range.width + (range.end > range.start ? gap : 0.0f) + widths[range.end];
        if (range.end > range.start && next > room + 0.01f)
        {
            break;
        }
        range.width = next;
        ++range.end;
    }
    return range;
}
