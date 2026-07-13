#include "reach/support/layout.h"

reach_result reach_layout_compute_split(reach_rect_i32 work_area, reach_split_mode mode,
                                        reach_rect_i32 *out_rect)
{
    REACH_ASSERT(out_rect != nullptr);
    if (out_rect == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    int32_t width = work_area.right - work_area.left;
    int32_t height = work_area.bottom - work_area.top;
    if (width <= 0 || height <= 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_rect = work_area;
    switch (mode)
    {
    case REACH_SPLIT_LEFT:
        out_rect->right = work_area.left + width / 2;
        break;
    case REACH_SPLIT_RIGHT:
        out_rect->left = work_area.left + width / 2;
        break;
    case REACH_SPLIT_TOP:
        out_rect->bottom = work_area.top + height / 2;
        break;
    case REACH_SPLIT_BOTTOM:
        out_rect->top = work_area.top + height / 2;
        break;
    case REACH_SPLIT_FULL:
        break;
    default:
        REACH_ASSERT(0);
        return REACH_INVALID_ARGUMENT;
    }
    return REACH_OK;
}

static int32_t reach_layout_edge_within(int32_t a, int32_t b, int32_t tolerance)
{
    int32_t delta = a - b;
    if (delta < 0)
    {
        delta = -delta;
    }
    return delta <= tolerance;
}

int32_t reach_layout_rect_matches_split(reach_rect_i32 work_area, reach_rect_i32 rect,
                                        reach_split_mode mode, int32_t tolerance)
{
    if (tolerance < 0)
    {
        return 0;
    }

    reach_rect_i32 split = {};
    if (reach_layout_compute_split(work_area, mode, &split) != REACH_OK)
    {
        return 0;
    }

    return reach_layout_edge_within(rect.left, split.left, tolerance) &&
           reach_layout_edge_within(rect.top, split.top, tolerance) &&
           reach_layout_edge_within(rect.right, split.right, tolerance) &&
           reach_layout_edge_within(rect.bottom, split.bottom, tolerance);
}
