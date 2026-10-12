#include "reach/core/render_commands.h"

#include <math.h>

static reach_motion reach_render_stretch_motion(const reach_motion *motion, float factor,
                                                float offset)
{
    reach_motion result = {0};
    if (factor == 0.0f)
        reach_motion_set(&result, offset);
    else
        result = reach_motion_affine(motion, factor, offset);
    return result;
}

static int32_t reach_render_stretch_fixed(const reach_motion *motion, float expected)
{
    return motion->generation == 0 ||
           (motion->duration_seconds <= 0.0 && reach_motion_value_at(motion, 0.0) == expected);
}

static reach_motion reach_render_stretch_scale(const reach_motion *motion, float caps, float band)
{
    reach_motion result = *motion;
    result.from = (motion->from - caps) / band;
    result.to = (motion->to - caps) / band;
    if (result.has_limits)
    {
        result.minimum = (motion->minimum - caps) / band;
        result.maximum = (motion->maximum - caps) / band;
    }
    if (result.from == result.to)
        result = reach_render_stretch_motion(
            motion, 0.0f, reach_motion_value_at(&result, result.started_seconds));
    return result;
}

static int32_t reach_render_stretch_axis_valid(float size, float near_inset, float far_inset,
                                               const reach_motion *motion, float anchor)
{
    return isfinite(size) && size > 0.0f && size <= 16384.0f && roundf(size) == size &&
           isfinite(near_inset) && near_inset >= 0.0f && roundf(near_inset) == near_inset &&
           isfinite(far_inset) && far_inset >= 0.0f && roundf(far_inset) == far_inset &&
           near_inset + far_inset < size && motion->generation != 0 && isfinite(motion->from) &&
           isfinite(motion->to) &&
           (!motion->has_limits || (isfinite(motion->minimum) && isfinite(motion->maximum) &&
                                    motion->minimum <= motion->maximum)) &&
           fminf(motion->has_limits ? fminf(motion->maximum, fmaxf(motion->minimum, motion->from))
                                    : motion->from,
                 motion->has_limits ? fminf(motion->maximum, fmaxf(motion->minimum, motion->to))
                                    : motion->to) > near_inset + far_inset &&
           isfinite(anchor) && anchor >= 0.0f && anchor <= 1.0f;
}

reach_result reach_render_stretch_patch(const reach_render_group *source,
                                        const reach_render_stretch *stretch, size_t patch_index,
                                        reach_render_group *out_patch)
{
    float source_x[4], source_y[4], factor_x, factor_y;
    size_t column = patch_index % 3, row = patch_index / 3;
    if (source == 0 || stretch == 0 || out_patch == 0 || patch_index >= 9 || source->id == 0 ||
        source->id > UINT64_C(0x7ffffffffffffff7) || !isfinite(source->bounds.x) ||
        !isfinite(source->bounds.y) || roundf(source->bounds.x) != source->bounds.x ||
        roundf(source->bounds.y) != source->bounds.y ||
        !reach_render_stretch_axis_valid(source->bounds.width, stretch->left, stretch->right,
                                         &stretch->width, stretch->anchor_x) ||
        !reach_render_stretch_axis_valid(source->bounds.height, stretch->top, stretch->bottom,
                                         &stretch->height, stretch->anchor_y) ||
        !reach_render_stretch_fixed(&source->scale, 1.0f) ||
        !reach_render_stretch_fixed(&source->scale_y, 1.0f) || !isfinite(source->x.to) ||
        !isfinite(source->y.to) || source->x.duration_seconds > 0.0 ||
        source->y.duration_seconds > 0.0 || source->repeat_travel_seconds > 0.0f)
        return REACH_INVALID_ARGUMENT;
    source_x[0] = source->bounds.x;
    source_x[1] = source_x[0] + stretch->left;
    source_x[2] = source_x[0] + source->bounds.width - stretch->right;
    source_x[3] = source_x[0] + source->bounds.width;
    source_y[0] = source->bounds.y;
    source_y[1] = source_y[0] + stretch->top;
    source_y[2] = source_y[0] + source->bounds.height - stretch->bottom;
    source_y[3] = source_y[0] + source->bounds.height;
    *out_patch = *source;
    out_patch->id = source->id + patch_index;
    out_patch->bounds =
        (reach_rect_f32){source_x[column], source_y[row], source_x[column + 1] - source_x[column],
                         source_y[row + 1] - source_y[row]};
    factor_x = (column == 2 ? 1.0f : 0.0f) - stretch->anchor_x;
    factor_y = (row == 2 ? 1.0f : 0.0f) - stretch->anchor_y;
    out_patch->x = reach_render_stretch_motion(
        &stretch->width, factor_x,
        -source->bounds.width * factor_x +
            (source->x.generation ? reach_motion_value_at(&source->x, 0.0) : 0.0f));
    out_patch->y = reach_render_stretch_motion(
        &stretch->height, factor_y,
        -source->bounds.height * factor_y +
            (source->y.generation ? reach_motion_value_at(&source->y, 0.0) : 0.0f));
    if (column == 1)
        out_patch->scale = reach_render_stretch_scale(
            &stretch->width, stretch->left + stretch->right, out_patch->bounds.width);
    else
        out_patch->scale = reach_render_stretch_motion(&stretch->width, 0.0f, 1.0f);
    if (row == 1)
        out_patch->scale_y = reach_render_stretch_scale(
            &stretch->height, stretch->top + stretch->bottom, out_patch->bounds.height);
    else
        out_patch->scale_y = reach_render_stretch_motion(&stretch->height, 0.0f, 1.0f);
    return REACH_OK;
}

static reach_rect_f32 reach_render_stretch_intersection(reach_rect_f32 a, reach_rect_f32 b)
{
    float right = fminf(a.x + a.width, b.x + b.width);
    float bottom = fminf(a.y + a.height, b.y + b.height);
    a.x = fmaxf(a.x, b.x);
    a.y = fmaxf(a.y, b.y);
    a.width = fmaxf(0.0f, right - a.x);
    a.height = fmaxf(0.0f, bottom - a.y);
    return a;
}

static int32_t reach_render_stretch_command_valid(const reach_render_group *source,
                                                  const reach_render_stretch *stretch,
                                                  const reach_render_command *command)
{
    float extent = 0.0f, top_notch = 0.0f, bottom_notch = 0.0f;
    int32_t width_changes =
        reach_motion_value_at(&stretch->width, stretch->width.started_seconds) !=
            source->bounds.width ||
        reach_motion_value_at(&stretch->width, reach_motion_deadline(&stretch->width)) !=
            source->bounds.width;
    int32_t height_changes =
        reach_motion_value_at(&stretch->height, stretch->height.started_seconds) !=
            source->bounds.height ||
        reach_motion_value_at(&stretch->height, reach_motion_deadline(&stretch->height)) !=
            source->bounds.height;
    if (command->type != REACH_RENDER_COMMAND_RECT &&
        command->type != REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT &&
        command->type != REACH_RENDER_COMMAND_SHADOW)
        return 0;
    if (!isfinite(command->radius) || command->radius < 0.0f || !isfinite(command->rect.x) ||
        !isfinite(command->rect.y) || !isfinite(command->rect.width) ||
        !isfinite(command->rect.height) || command->rect.width <= 0.0f ||
        command->rect.height <= 0.0f)
        return 0;
    if (command->notch_width > 0.0f && command->notch_height > 0.0f)
    {
        if (width_changes)
            return 0;
        if (command->notch_side == REACH_NOTCH_SIDE_TOP)
            top_notch = command->notch_height;
        else
            bottom_notch = command->notch_height;
    }
    if (command->type == REACH_RENDER_COMMAND_SHADOW)
    {
        reach_shadow shadow = {command->shadow_offset_x, command->shadow_offset_y,
                               command->blur_radius, command->color};
        extent = reach_theme_shadow_extent(&shadow, 1.0f);
    }
    if (width_changes &&
        (stretch->left < command->rect.x - source->bounds.x + command->radius + extent ||
         stretch->right < source->bounds.x + source->bounds.width - command->rect.x -
                              command->rect.width + command->radius + extent))
        return 0;
    if (height_changes &&
        (stretch->top < command->rect.y - source->bounds.y + command->radius + extent + top_notch ||
         stretch->bottom < source->bounds.y + source->bounds.height - command->rect.y -
                               command->rect.height + command->radius + extent + bottom_notch))
        return 0;
    return 1;
}

reach_result reach_render_push_stretched_chrome(reach_render_command_buffer *buffer,
                                                const reach_render_group *source,
                                                const reach_render_stretch *stretch,
                                                const reach_render_command *commands,
                                                size_t command_count)
{
    reach_render_group patches[9];
    size_t index, at, patch_count = 0;
    if (buffer == 0 || commands == 0 || command_count == 0 ||
        command_count > REACH_MAX_RENDER_COMMANDS || buffer->current_group_id != 0 ||
        buffer->count > REACH_MAX_RENDER_COMMANDS || buffer->group_count > REACH_MAX_RENDER_GROUPS)
        return REACH_INVALID_ARGUMENT;
    for (index = 0; index < 9; ++index)
    {
        if (reach_render_stretch_patch(source, stretch, index, &patches[index]) != REACH_OK)
            return REACH_INVALID_ARGUMENT;
        if (patches[index].bounds.width <= 0.0f || patches[index].bounds.height <= 0.0f)
            continue;
        ++patch_count;
        if (reach_render_command_buffer_check_group(buffer, &patches[index]) != REACH_OK)
            return REACH_INVALID_ARGUMENT;
    }
    if (patch_count > REACH_MAX_RENDER_GROUPS - buffer->group_count ||
        command_count * patch_count > REACH_MAX_RENDER_COMMANDS - buffer->count)
        return REACH_ERROR;
    for (at = 0; at < command_count; ++at)
        if (!reach_render_stretch_command_valid(source, stretch, &commands[at]))
            return REACH_INVALID_ARGUMENT;
    for (index = 0; index < 9; ++index)
    {
        reach_render_group *patch = &patches[index];
        if (patch->bounds.width <= 0.0f || patch->bounds.height <= 0.0f)
            continue;
        (void)reach_render_command_buffer_begin_group(buffer, patch);
        for (at = 0; at < command_count; ++at)
        {
            reach_render_command command = commands[at];
            reach_rect_f32 crop = patch->bounds;
            if (command.has_scissor)
                crop = reach_render_stretch_intersection(crop, command.scissor_rect);
            if (buffer->has_scissor)
                crop = reach_render_stretch_intersection(crop, buffer->scissor_rect);
            command.has_scissor = 1;
            command.scissor_rect = crop;
            (void)reach_render_command_buffer_push(buffer, &command);
        }
        reach_render_command_buffer_end_group(buffer);
    }
    return REACH_OK;
}
