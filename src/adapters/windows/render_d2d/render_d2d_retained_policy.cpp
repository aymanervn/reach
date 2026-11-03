#include "render_d2d_retained_policy.h"

#include <math.h>

static uint64_t reach_d2d_surface_extent(float value)
{
    if (!(value > 0.0f))
        return 0;
    return (uint64_t)fmin(ceil((double)value), (double)(UINT64_C(1) << 20));
}

uint64_t reach_d2d_layer_bytes(float width, float height, bool has_paint, bool has_shadow)
{
    uint64_t surface = reach_d2d_surface_extent(width) * reach_d2d_surface_extent(height) * 4;
    return surface * ((has_paint ? 1 : 0) + (has_shadow ? 1 : 0));
}
