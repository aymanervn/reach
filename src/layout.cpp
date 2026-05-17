#include "reach/layout.h"

reach_result reach_layout_compute_split(reach_rect_i32 work_area, reach_split_mode mode, reach_rect_i32 *out_rect)
{
    REACH_ASSERT(out_rect != nullptr);
    if (out_rect == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_rect = work_area;
    int32_t width = work_area.right - work_area.left;
    int32_t height = work_area.bottom - work_area.top;
    if (mode == REACH_SPLIT_LEFT) out_rect->right = work_area.left + width / 2;
    if (mode == REACH_SPLIT_RIGHT) out_rect->left = work_area.left + width / 2;
    if (mode == REACH_SPLIT_TOP) out_rect->bottom = work_area.top + height / 2;
    if (mode == REACH_SPLIT_BOTTOM) out_rect->top = work_area.top + height / 2;
    return REACH_OK;
}
