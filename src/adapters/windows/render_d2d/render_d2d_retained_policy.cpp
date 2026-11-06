#include "render_d2d_retained_policy.h"

#include <math.h>

#include <algorithm>

static const uint32_t REACH_D2D_SURFACE_GROWTH_STEP = 64;
static const uint32_t REACH_D2D_SURFACE_MAX_EXTENT = 16384;

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

static uint32_t reach_d2d_grown_extent(uint32_t need, uint32_t capacity, uint32_t limit)
{
    if (need <= capacity)
        return need;
    uint64_t grown = std::max<uint64_t>(need, (uint64_t)capacity * 5 / 4);
    grown = (grown + REACH_D2D_SURFACE_GROWTH_STEP - 1) / REACH_D2D_SURFACE_GROWTH_STEP *
            REACH_D2D_SURFACE_GROWTH_STEP;
    uint64_t ceiling = std::min<uint64_t>(REACH_D2D_SURFACE_MAX_EXTENT, std::max(need, limit));
    return (uint32_t)std::max<uint64_t>(need, std::min<uint64_t>(grown, ceiling));
}

reach_d2d_surface_size reach_d2d_surface_capacity(reach_d2d_surface_size capacity,
                                                  reach_d2d_surface_size need, uint64_t budget_left,
                                                  reach_d2d_surface_size growth_limit)
{
    if (capacity.width == 0 || capacity.height == 0 || need.width == 0 || need.height == 0)
        return need;
    uint64_t need_area = (uint64_t)need.width * need.height;
    if (need.width <= capacity.width && need.height <= capacity.height)
        return reach_d2d_planned_surface(capacity, need);
    reach_d2d_surface_size grown = {
        reach_d2d_grown_extent(need.width, capacity.width, growth_limit.width),
        reach_d2d_grown_extent(need.height, capacity.height, growth_limit.height)};
    uint64_t grown_area = (uint64_t)grown.width * grown.height;
    if (grown_area > need_area * 2 || grown_area * 4 > budget_left)
        return need;
    return grown;
}

reach_d2d_surface_size reach_d2d_planned_surface(reach_d2d_surface_size capacity,
                                                 reach_d2d_surface_size need)
{
    if (capacity.width == 0 || capacity.height == 0 || need.width == 0 || need.height == 0)
        return need;
    if (need.width <= capacity.width && need.height <= capacity.height)
        return (uint64_t)need.width * need.height * 2 >= (uint64_t)capacity.width * capacity.height
                   ? capacity
                   : need;
    return need;
}

uint64_t reach_d2d_planned_layer_bytes(float width, float height, bool has_paint, bool has_shadow,
                                       reach_d2d_surface_size capacity)
{
    uint64_t need_width = reach_d2d_surface_extent(width);
    uint64_t need_height = reach_d2d_surface_extent(height);
    uint64_t shadow = has_shadow ? need_width * need_height * 4 : 0;
    if (!has_paint)
        return shadow;
    reach_d2d_surface_size kept =
        reach_d2d_planned_surface(capacity, {(uint32_t)need_width, (uint32_t)need_height});
    return (uint64_t)kept.width * kept.height * 4 + shadow;
}

bool reach_d2d_solid_fill(const reach_render_command *commands, size_t count, reach_rect_f32 bounds,
                          reach_color *out_color)
{
    if (commands == nullptr || out_color == nullptr || count != 1)
        return false;
    const reach_render_command &fill = commands[0];
    if (!isfinite(bounds.x) || !isfinite(bounds.y) || !isfinite(bounds.width) ||
        !isfinite(bounds.height) || fill.type != REACH_RENDER_COMMAND_RECT || fill.radius != 0 ||
        fill.stroke_width != 0 || fill.corner_mask != 0 || fill.rect_fit_to_clip ||
        fill.has_scissor || fill.has_clip_rect)
        return false;
    float width = std::max(1.0f, ceilf(bounds.width));
    float height = std::max(1.0f, ceilf(bounds.height));
    if (fill.rect.x != bounds.x || fill.rect.y != bounds.y || fill.rect.width != width ||
        fill.rect.height != height || !isfinite(fill.color.r) || !isfinite(fill.color.g) ||
        !isfinite(fill.color.b) || !isfinite(fill.color.a))
        return false;
    *out_color = fill.color;
    return true;
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
