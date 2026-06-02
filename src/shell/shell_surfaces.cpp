#include "shell_internal.h"

#include <math.h>

int32_t reach_shell_rect_equal(reach_rect_f32 a, reach_rect_f32 b)
{
    return fabsf(a.x - b.x) < 0.5f && fabsf(a.y - b.y) < 0.5f && fabsf(a.width - b.width) < 0.5f &&
           fabsf(a.height - b.height) < 0.5f;
}

int32_t reach_shell_opacity_equal(float a, float b)
{
    return fabsf(a - b) < 0.001f;
}

reach_result reach_shell_apply_window_state(reach_platform_window_port *window,
                                            reach_rect_f32 bounds, float opacity,
                                            reach_rect_f32 *last_bounds, float *last_opacity,
                                            int32_t *bounds_valid, int32_t *opacity_valid,
                                            int32_t *out_changed)
{
    REACH_ASSERT(window != nullptr);
    REACH_ASSERT(last_bounds != nullptr);
    REACH_ASSERT(last_opacity != nullptr);
    REACH_ASSERT(bounds_valid != nullptr);
    REACH_ASSERT(opacity_valid != nullptr);
    REACH_ASSERT(out_changed != nullptr);
    if (window == nullptr || last_bounds == nullptr || last_opacity == nullptr ||
        bounds_valid == nullptr || opacity_valid == nullptr || out_changed == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_changed = 0;
    if (window->ops.set_bounds != nullptr &&
        (!*bounds_valid || !reach_shell_rect_equal(*last_bounds, bounds)))
    {
        reach_result result = window->ops.set_bounds(window->window, bounds);
        if (result != REACH_OK)
        {
            return result;
        }
        *last_bounds = bounds;
        *bounds_valid = 1;
        *out_changed = 1;
    }

    if (window->ops.set_opacity != nullptr &&
        (!*opacity_valid || !reach_shell_opacity_equal(*last_opacity, opacity)))
    {
        reach_result result = window->ops.set_opacity(window->window, opacity);
        if (result != REACH_OK)
        {
            return result;
        }
        *last_opacity = opacity;
        *opacity_valid = 1;
        *out_changed = 1;
    }

    return REACH_OK;
}
