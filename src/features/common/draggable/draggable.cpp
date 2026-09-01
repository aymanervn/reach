#include "reach/features/common/draggable.h"

static void reach_draggable_result_init(reach_draggable_result *out)
{
    if (out != nullptr)
    {
        *out = {};
        out->target = REACH_DRAGGABLE_TARGET_NONE;
    }
}

void reach_draggable_init(reach_draggable *draggable)
{
    if (draggable == nullptr)
    {
        return;
    }
    *draggable = {};
    draggable->target = REACH_DRAGGABLE_TARGET_NONE;
}

void reach_draggable_begin(reach_draggable *draggable, uint64_t target, int32_t x, int32_t y)
{
    if (draggable == nullptr || draggable->tracking || target == REACH_DRAGGABLE_TARGET_NONE)
    {
        return;
    }
    draggable->target = target;
    draggable->start_x = x;
    draggable->start_y = y;
    draggable->tracking = 1;
    draggable->moved = 0;
}

void reach_draggable_update(reach_draggable *draggable, int32_t x, int32_t y,
                            int32_t threshold_squared, reach_draggable_result *out)
{
    reach_draggable_result_init(out);
    if (draggable == nullptr || !draggable->tracking)
    {
        return;
    }

    int32_t dx = x - draggable->start_x;
    int32_t dy = y - draggable->start_y;
    int32_t started = !draggable->moved && dx * dx + dy * dy >= threshold_squared;
    if (started)
    {
        draggable->moved = 1;
    }
    if (out != nullptr)
    {
        out->target = draggable->target;
        out->started = started;
        out->moved = draggable->moved;
    }
}

void reach_draggable_end(reach_draggable *draggable, reach_draggable_result *out)
{
    reach_draggable_result_init(out);
    if (draggable == nullptr || !draggable->tracking)
    {
        return;
    }
    if (out != nullptr)
    {
        out->target = draggable->target;
        out->moved = draggable->moved;
        out->ended = 1;
    }
    reach_draggable_init(draggable);
}

int32_t reach_draggable_tracking(const reach_draggable *draggable)
{
    return draggable != nullptr && draggable->tracking;
}

int32_t reach_draggable_moved(const reach_draggable *draggable)
{
    return draggable != nullptr && draggable->moved;
}

size_t reach_horizontal_reorder_target(const reach_rect_f32 *slots, size_t count,
                                       size_t current_index, float dragged_x,
                                       float neighbor_threshold_ratio)
{
    if (slots == nullptr || count == 0 || current_index >= count)
    {
        return SIZE_MAX;
    }

    size_t target = current_index;
    while (target > 0)
    {
        float threshold = slots[target - 1].x + slots[target - 1].width * neighbor_threshold_ratio;
        if (dragged_x > threshold)
        {
            break;
        }
        --target;
    }
    while (target + 1 < count)
    {
        float threshold = slots[target + 1].x - slots[target + 1].width * neighbor_threshold_ratio;
        if (dragged_x < threshold)
        {
            break;
        }
        ++target;
    }
    return target;
}
