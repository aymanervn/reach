#include "render_d2d_retained_policy.h"

#include <math.h>

#include <algorithm>

static const uint64_t REACH_D2D_UNGROUPED_RUN_BASE = UINT64_C(0x8000000000000000);
static const uint64_t REACH_D2D_UNGROUPED_RUN_MASK = UINT64_C(0x3fffffffffffffff);

static uint64_t reach_d2d_splitmix64(uint64_t value)
{
    value += UINT64_C(0x9e3779b97f4a7c15);
    value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31);
}

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

uint64_t reach_d2d_ungrouped_run_id(uint64_t anchor, uint64_t ordinal)
{
    return REACH_D2D_UNGROUPED_RUN_BASE |
           ((reach_d2d_splitmix64(anchor) + ordinal) & REACH_D2D_UNGROUPED_RUN_MASK);
}

void reach_d2d_retained_runs(const reach_render_command_buffer *buffer,
                             std::vector<reach_d2d_retained_run> *out)
{
    out->clear();
    uint64_t anchor = 0;
    uint64_t ordinal = 0;
    for (size_t index = 0; index < buffer->count;)
    {
        const reach_render_command &first = buffer->commands[index];
        size_t end = index + 1;
        if (first.group_id != 0 || first.type != REACH_RENDER_COMMAND_TEXTBOX)
        {
            while (
                end < buffer->count && buffer->commands[end].group_id == first.group_id &&
                (first.group_id != 0 || buffer->commands[end].type != REACH_RENDER_COMMAND_TEXTBOX))
                ++end;
        }
        uint64_t id = first.group_id;
        if (id != 0)
        {
            anchor = id;
            ordinal = 0;
        }
        else
        {
            id = reach_d2d_ungrouped_run_id(anchor, ordinal++);
            while (std::any_of(out->begin(), out->end(),
                               [&](const reach_d2d_retained_run &run) { return run.id == id; }))
                id = REACH_D2D_UNGROUPED_RUN_BASE | ((id + 1) & REACH_D2D_UNGROUPED_RUN_MASK);
        }
        out->push_back({index, end, id});
        index = end;
    }
}
