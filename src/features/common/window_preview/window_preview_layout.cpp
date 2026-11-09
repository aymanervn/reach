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

float reach_window_preview_row_fit(const float *aspects, size_t count, float height, float gap,
                                   float available_width)
{
    float row_width = count > 1 ? gap * (float)(count - 1) : 0.0f;
    for (size_t index = 0; index < count; ++index)
    {
        row_width += aspects[index] * height;
    }
    return reach_window_preview_row_scale(row_width, available_width);
}
