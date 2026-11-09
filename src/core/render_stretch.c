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
    if (motion->has_mapping == REACH_MOTION_MAPPING_RATIO ||
        motion->has_mapping == REACH_MOTION_MAPPING_QUADRATIC)
        return reach_motion_affine(motion, 1 / band, -caps / band);
    reach_motion result = *motion;
    if (result.has_mapping)
    {
        result.mapping_left_slope /= band;
        result.mapping_right_slope /= band;
        result.mapping_value = (motion->mapping_value - caps) / band;
        for (int32_t at = 0; at < result.mapping_extra_count; ++at)
            result.mapping_extra_deltas[at] /= band;
    }
    else
    {
        result.from = (motion->from - caps) / band;
        result.to = (motion->to - caps) / band;
        result.spring_velocity = motion->spring_velocity / band;
    }
    if (result.has_limits)
    {
        result.minimum = (motion->minimum - caps) / band;
        result.maximum = (motion->maximum - caps) / band;
    }
    float minimum = 0.0f, maximum = 0.0f;
    if (reach_motion_range(&result, &minimum, &maximum) == REACH_OK && minimum == maximum)
        result = reach_render_stretch_motion(
            motion, 0.0f, reach_motion_value_at(&result, result.started_seconds));
    return result;
}

static int32_t reach_render_stretch_axis_valid(float size, float near_inset, float far_inset,
                                               const reach_motion *motion, float anchor)
{
    float minimum = 0.0f, maximum = 0.0f;
    return isfinite(size) && size > 0.0f && size <= 16384.0f && roundf(size) == size &&
           isfinite(near_inset) && near_inset >= 0.0f && roundf(near_inset) == near_inset &&
           isfinite(far_inset) && far_inset >= 0.0f && roundf(far_inset) == far_inset &&
           near_inset + far_inset < size && motion->generation != 0 &&
           reach_motion_range(motion, &minimum, &maximum) == REACH_OK &&
           minimum > near_inset + far_inset && isfinite(anchor) && anchor >= 0.0f && anchor <= 1.0f;
}

size_t reach_render_stretch_patch_count(const reach_render_stretch *stretch)
{
    return stretch == 0 ? 0 : stretch->focal_x.generation != 0 ? 15 : 9;
}

static reach_result reach_render_stretch_focal_axis(const reach_render_group *source,
                                                    const reach_render_stretch *stretch,
                                                    size_t column, reach_render_group *patch)
{
    float fixed_width = stretch->focal_right - stretch->focal_left;
    float half = fixed_width * 0.5f;
    reach_motion leading = reach_motion_affine(&stretch->focal_x, 1, -half - stretch->left);
    reach_motion trailing = {0};
    float minimum = 0, maximum = 0;
    if (!isfinite(stretch->focal_left) || !isfinite(stretch->focal_right) ||
        roundf(stretch->focal_left) != stretch->focal_left ||
        roundf(stretch->focal_right) != stretch->focal_right ||
        stretch->focal_left <= stretch->left || fixed_width <= 0 ||
        stretch->focal_right >= source->bounds.width - stretch->right ||
        reach_motion_range(&leading, &minimum, &maximum) != REACH_OK || minimum <= 0 ||
        reach_motion_combine(&stretch->width, 1, &stretch->focal_x, -1, -half - stretch->right,
                             &trailing) != REACH_OK ||
        reach_motion_range(&trailing, &minimum, &maximum) != REACH_OK || minimum <= 0)
        return REACH_INVALID_ARGUMENT;
    float starts[] = {0,
                      stretch->left,
                      stretch->focal_left,
                      stretch->focal_right,
                      source->bounds.width - stretch->right,
                      source->bounds.width};
    reach_motion start = {0};
    if (column < 2)
        reach_motion_set(&start, starts[column]);
    else if (column < 4)
        start = reach_motion_affine(&stretch->focal_x, 1, column == 2 ? -half : half);
    else
        start = reach_motion_affine(&stretch->width, 1, -stretch->right);
    float source_offset = source->x.generation ? reach_motion_value_at(&source->x, 0) : 0;
    if (reach_motion_combine(&start, 1, &stretch->width, -stretch->anchor_x,
                             stretch->anchor_x * source->bounds.width - starts[column] +
                                 source_offset,
                             &patch->x) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    patch->bounds.x = source->bounds.x + starts[column];
    patch->bounds.width = starts[column + 1] - starts[column];
    if (column == 1 || column == 3)
        patch->scale =
            reach_render_stretch_scale(column == 1 ? &leading : &trailing, 0, patch->bounds.width);
    else
        patch->scale = reach_render_stretch_motion(&stretch->width, 0, 1);
    return REACH_OK;
}

reach_result reach_render_stretch_patch(const reach_render_group *source,
                                        const reach_render_stretch *stretch, size_t patch_index,
                                        reach_render_group *out_patch)
{
    float source_x[4], source_y[4], factor_x, factor_y;
    if (stretch == 0)
        return REACH_INVALID_ARGUMENT;
    size_t count = reach_render_stretch_patch_count(stretch), columns = count / 3;
    size_t column = patch_index % columns, row = patch_index / columns;
    if (source == 0 || stretch == 0 || out_patch == 0 || patch_index >= count || source->id == 0 ||
        source->id > UINT64_C(0x7fffffffffffffff) - (count - 1) || !isfinite(source->bounds.x) ||
        !isfinite(source->bounds.y) || roundf(source->bounds.x) != source->bounds.x ||
        roundf(source->bounds.y) != source->bounds.y ||
        !reach_render_stretch_axis_valid(source->bounds.width, stretch->left, stretch->right,
                                         &stretch->width, stretch->anchor_x) ||
        !reach_render_stretch_axis_valid(source->bounds.height, stretch->top, stretch->bottom,
                                         &stretch->height, stretch->anchor_y) ||
        !reach_render_stretch_fixed(&source->scale, 1.0f) ||
        !reach_render_stretch_fixed(&source->scale_y, 1.0f) || !isfinite(source->x.to) ||
        !isfinite(source->y.to) || source->x.duration_seconds > 0.0 ||
        source->y.duration_seconds > 0.0)
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
        (reach_rect_f32){source_x[0], source_y[row], 0, source_y[row + 1] - source_y[row]};
    factor_x = (column == columns - 1 ? 1.0f : 0.0f) - stretch->anchor_x;
    factor_y = (row == 2 ? 1.0f : 0.0f) - stretch->anchor_y;
    out_patch->x = reach_render_stretch_motion(
        &stretch->width, factor_x,
        -source->bounds.width * factor_x +
            (source->x.generation ? reach_motion_value_at(&source->x, 0.0) : 0.0f));
    out_patch->y = reach_render_stretch_motion(
        &stretch->height, factor_y,
        -source->bounds.height * factor_y +
            (source->y.generation ? reach_motion_value_at(&source->y, 0.0) : 0.0f));
    if (columns == 5)
    {
        if (reach_render_stretch_focal_axis(source, stretch, column, out_patch) != REACH_OK)
            return REACH_INVALID_ARGUMENT;
    }
    else if (column == 1)
        out_patch->scale = reach_render_stretch_scale(
            &stretch->width, stretch->left + stretch->right, source_x[2] - source_x[1]);
    else
        out_patch->scale = reach_render_stretch_motion(&stretch->width, 0.0f, 1.0f);
    if (columns == 3)
    {
        out_patch->bounds.x = source_x[column];
        out_patch->bounds.width = source_x[column + 1] - source_x[column];
    }
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
    float minimum = 0, maximum = 0;
    if (reach_motion_range(&stretch->width, &minimum, &maximum) != REACH_OK)
        return 0;
    int32_t width_changes = minimum != source->bounds.width || maximum != source->bounds.width;
    if (reach_motion_range(&stretch->height, &minimum, &maximum) != REACH_OK)
        return 0;
    int32_t height_changes = minimum != source->bounds.height || maximum != source->bounds.height;
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
    int32_t horizontal_motion = width_changes || stretch->focal_x.generation != 0;
    if (horizontal_motion && command->notch_width > 0 && command->notch_height > 0)
    {
        float center = command->notch_center_x - source->bounds.x;
        float half = command->notch_width * 0.5f + extent;
        if (!isfinite(center) || !isfinite(half))
            return 0;
        if (stretch->focal_x.generation != 0)
        {
            if (center - half < stretch->focal_left || center + half > stretch->focal_right ||
                center != (stretch->focal_left + stretch->focal_right) * 0.5f)
                return 0;
        }
        else if (center - half < 0 || center + half > source->bounds.width ||
                 (center + half > stretch->left &&
                  center - half < source->bounds.width - stretch->right))
            return 0;
    }
    if (horizontal_motion &&
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
    reach_render_group patches[15];
    size_t index, at, patch_count = 0, slots = reach_render_stretch_patch_count(stretch);
    if (buffer == 0 || source == 0 || stretch == 0 || commands == 0 || command_count == 0 ||
        command_count > REACH_MAX_RENDER_COMMANDS || buffer->current_group_id != 0 ||
        buffer->count > REACH_MAX_RENDER_COMMANDS || buffer->group_count > REACH_MAX_RENDER_GROUPS)
        return REACH_INVALID_ARGUMENT;
    for (index = 0; index < slots; ++index)
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
    for (index = 0; index < slots; ++index)
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
