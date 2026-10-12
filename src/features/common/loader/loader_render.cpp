#include "reach/features/common/loader_render.h"

#include <cmath>

static reach_rect_f32 reach_loader_intersection(reach_rect_f32 a, reach_rect_f32 b)
{
    float right = fminf(a.x + a.width, b.x + b.width);
    float bottom = fminf(a.y + a.height, b.y + b.height);
    a.x = fmaxf(a.x, b.x);
    a.y = fmaxf(a.y, b.y);
    a.width = fmaxf(0, right - a.x);
    a.height = fmaxf(0, bottom - a.y);
    return a;
}

static reach_result reach_loader_push_sampled(const reach_loader_model *model,
                                              reach_rect_f32 container, reach_color color,
                                              reach_render_command_buffer *out)
{
    reach_rect_f32 bar = reach_loader_bar_rect(model, container);
    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_CLIPPED_ROUNDED_RECT;
    shape.rect = bar;
    shape.clip_rect = container;
    shape.radius = fminf(container.height, bar.width) * 0.5f;
    shape.clip_radius = container.height * 0.5f;
    shape.color = color;
    out->requires_content_sampling = 1;
    return reach_render_command_buffer_push(out, &shape);
}

reach_result reach_loader_build_render_commands(const reach_loader_model *model,
                                                reach_rect_f32 container, reach_rect_f32 origin,
                                                reach_color bar_color, int32_t retained_supported,
                                                uint64_t id, reach_render_command_buffer *out)
{
    if (model == nullptr || out == nullptr || id == 0 || id > UINT64_C(0x7ffffffffffffff5))
        return REACH_INVALID_ARGUMENT;
    if (container.width <= 0 || container.height <= 0)
        return REACH_OK;
    container.x -= origin.x;
    container.y -= origin.y;
    int32_t clipped = out->has_scissor;
    reach_rect_f32 scissor = out->scissor_rect;
    if (clipped)
    {
        reach_rect_f32 visible = reach_loader_intersection(container, scissor);
        if (visible.width <= 0 || visible.height <= 0)
            return REACH_OK;
    }
    if (!retained_supported)
        return reach_loader_push_sampled(model, container, bar_color, out);
    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_RECT;
    shape.rect = container;
    shape.radius = container.height * 0.5f;
    shape.color = bar_color;
    reach_render_group source = {};
    source.id = id + 2;
    if (!reach_render_retained_command_bounds(&shape, &source.bounds))
        return REACH_INVALID_ARGUMENT;
    reach_render_stretch stretch = {};
    stretch.left = ceilf(container.x + shape.radius - source.bounds.x);
    stretch.right = ceilf(source.bounds.x + source.bounds.width -
                          (container.x + container.width - shape.radius));
    stretch.width =
        reach_motion_affine(&model->width, container.width, source.bounds.width - container.width);
    reach_motion_set(&stretch.height, source.bounds.height);
    reach_render_group probe = {};
    if (container.width * REACH_LOADER_MINIMUM_WIDTH_RATIO < container.height ||
        reach_render_stretch_patch(&source, &stretch, 4, &probe) != REACH_OK)
        return reach_loader_push_sampled(model, container, bar_color, out);
    uint64_t parent = out->current_group_id;
    reach_render_group viewport = {};
    viewport.id = id;
    viewport.parent_id = parent;
    viewport.bounds = source.bounds;
    viewport.has_clip = clipped;
    viewport.clip = scissor;
    out->current_group_id = 0;
    reach_result result = reach_render_command_buffer_begin_group(out, &viewport);
    reach_render_command_buffer_end_group(out);
    viewport.id = id + 1;
    viewport.parent_id = id;
    viewport.has_clip = 1;
    viewport.clip = container;
    viewport.clip_radius = shape.radius;
    viewport.x = reach_motion_affine(&model->x, container.width, 0);
    out->current_group_id = 0;
    if (result == REACH_OK)
        result = reach_render_command_buffer_begin_group(out, &viewport);
    reach_render_command_buffer_end_group(out);
    if (result == REACH_OK)
    {
        reach_render_command_buffer_clear_scissor(out);
        source.parent_id = id + 1;
        result = reach_render_push_stretched_chrome(out, &source, &stretch, &shape, 1);
    }
    out->current_group_id = parent;
    if (clipped)
        reach_render_command_buffer_set_scissor(out, scissor);
    return result;
}
