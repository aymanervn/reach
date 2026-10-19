#include "reach/core/render_commands.h"

#include <math.h>

static int32_t reach_render_rect_equal(reach_rect_f32 first, reach_rect_f32 second)
{
    return first.x == second.x && first.y == second.y && first.width == second.width &&
           first.height == second.height;
}

int32_t reach_render_group_equal(const reach_render_group *first, const reach_render_group *second)
{
    size_t index;
    if (first == second)
        return 1;
    if (first == 0 || second == 0 || first->id != second->id ||
        first->parent_id != second->parent_id ||
        !reach_render_rect_equal(first->bounds, second->bounds) ||
        first->has_clip != second->has_clip ||
        !reach_render_rect_equal(first->clip, second->clip) ||
        first->clip_radius != second->clip_radius ||
        !reach_motion_equal(&first->clip_radius_motion, &second->clip_radius_motion) ||
        first->clip_uniform_radius != second->clip_uniform_radius ||
        first->has_tint != second->has_tint || first->has_shadow != second->has_shadow ||
        (first->has_shadow && (first->shadow.offset_x != second->shadow.offset_x ||
                               first->shadow.offset_y != second->shadow.offset_y ||
                               first->shadow.blur != second->shadow.blur ||
                               first->shadow.color.r != second->shadow.color.r ||
                               first->shadow.color.g != second->shadow.color.g ||
                               first->shadow.color.b != second->shadow.color.b ||
                               first->shadow.color.a != second->shadow.color.a)) ||
        !reach_motion_equal(&first->tint_rgb_mix, &second->tint_rgb_mix) ||
        first->tint_rgb_target.r != second->tint_rgb_target.r ||
        first->tint_rgb_target.g != second->tint_rgb_target.g ||
        first->tint_rgb_target.b != second->tint_rgb_target.b ||
        !reach_motion_equal(&first->x, &second->x) || !reach_motion_equal(&first->y, &second->y) ||
        !reach_motion_equal(&first->opacity, &second->opacity) ||
        !reach_motion_equal(&first->scale, &second->scale) ||
        !reach_motion_equal(&first->scale_y, &second->scale_y))
        return 0;
    for (index = 0; index < REACH_RENDER_CLIP_EDGE_COUNT; ++index)
        if (!reach_motion_equal(&first->clip_insets[index], &second->clip_insets[index]))
            return 0;
    for (index = 0; index < REACH_RENDER_COLOR_CHANNEL_COUNT; ++index)
        if (!reach_motion_equal(&first->tint[index], &second->tint[index]))
            return 0;
    return 1;
}

static int32_t reach_render_group_motion_active(const reach_render_group *group)
{
    size_t edge;
    if (group->has_tint)
        for (edge = 0; edge < REACH_RENDER_COLOR_CHANNEL_COUNT; ++edge)
            if (reach_motion_active(&group->tint[edge]))
                return 1;
    if (group->has_tint && reach_motion_active(&group->tint_rgb_mix))
        return 1;
    if (group->has_clip && reach_motion_active(&group->clip_radius_motion))
        return 1;
    if (group->has_clip)
        for (edge = 0; edge < REACH_RENDER_CLIP_EDGE_COUNT; ++edge)
            if (reach_motion_active(&group->clip_insets[edge]))
                return 1;
    return reach_motion_active(&group->x) || reach_motion_active(&group->y) ||
           reach_motion_active(&group->scale) || reach_motion_active(&group->scale_y) ||
           reach_motion_active(&group->opacity);
}

reach_motion reach_render_group_y_scale(const reach_render_group *group)
{
    reach_motion empty = {0};
    return group != 0 ? (group->scale_y.generation != 0 ? group->scale_y : group->scale) : empty;
}

reach_point_f32 reach_render_group_scale_at(const reach_render_group *group, double now_seconds)
{
    reach_point_f32 scale = {1.0f, 1.0f};
    if (group != 0)
    {
        reach_motion y = reach_render_group_y_scale(group);
        if (group->scale.generation != 0)
            scale.x = reach_motion_value_at(&group->scale, now_seconds);
        if (y.generation != 0)
            scale.y = reach_motion_value_at(&y, now_seconds);
    }
    return scale;
}

reach_color reach_render_group_tint_at(const reach_render_group *group, double now_seconds)
{
    reach_color color = {1.0f, 1.0f, 1.0f, 1.0f};
    if (group != 0 && group->has_tint)
    {
        color.r = reach_motion_value_at(&group->tint[REACH_RENDER_COLOR_RED], now_seconds);
        color.g = reach_motion_value_at(&group->tint[REACH_RENDER_COLOR_GREEN], now_seconds);
        color.b = reach_motion_value_at(&group->tint[REACH_RENDER_COLOR_BLUE], now_seconds);
        color.a = reach_motion_value_at(&group->tint[REACH_RENDER_COLOR_ALPHA], now_seconds);
        if (group->tint_rgb_mix.generation != 0)
        {
            float weight = reach_motion_value_at(&group->tint_rgb_mix, now_seconds);
            color.r += (group->tint_rgb_target.r - color.r) * weight;
            color.g += (group->tint_rgb_target.g - color.g) * weight;
            color.b += (group->tint_rgb_target.b - color.b) * weight;
        }
    }
    return color;
}

reach_rect_f32 reach_render_group_clip_at(const reach_render_group *group, double now_seconds)
{
    reach_rect_f32 clip = {0};
    float left, top, right, bottom;
    if (group == 0 || !group->has_clip)
        return clip;
    left = reach_motion_value_at(&group->clip_insets[REACH_RENDER_CLIP_LEFT], now_seconds);
    top = reach_motion_value_at(&group->clip_insets[REACH_RENDER_CLIP_TOP], now_seconds);
    right = reach_motion_value_at(&group->clip_insets[REACH_RENDER_CLIP_RIGHT], now_seconds);
    bottom = reach_motion_value_at(&group->clip_insets[REACH_RENDER_CLIP_BOTTOM], now_seconds);
    clip = group->clip;
    clip.x += left;
    clip.y += top;
    clip.width = fmaxf(0.0f, clip.width - left - right);
    clip.height = fmaxf(0.0f, clip.height - top - bottom);
    return clip;
}

reach_result reach_render_group_clip_motion(const reach_render_group *group,
                                            reach_rect_motion *out_clip)
{
    if (group == 0 || out_clip == 0 || !group->has_clip || !isfinite(group->clip.x) ||
        !isfinite(group->clip.y) || !isfinite(group->clip.width) || !isfinite(group->clip.height) ||
        group->clip.width < 0 || group->clip.height < 0)
        return REACH_INVALID_ARGUMENT;
    reach_rect_motion clip = {0};
    clip.x = reach_motion_affine(&group->clip_insets[REACH_RENDER_CLIP_LEFT], 1, group->clip.x);
    clip.y = reach_motion_affine(&group->clip_insets[REACH_RENDER_CLIP_TOP], 1, group->clip.y);
    if (reach_motion_combine(&group->clip_insets[REACH_RENDER_CLIP_LEFT], -1,
                             &group->clip_insets[REACH_RENDER_CLIP_RIGHT], -1, group->clip.width,
                             &clip.width) != REACH_OK ||
        reach_motion_combine(&group->clip_insets[REACH_RENDER_CLIP_TOP], -1,
                             &group->clip_insets[REACH_RENDER_CLIP_BOTTOM], -1, group->clip.height,
                             &clip.height) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    float minimum = 0, maximum = 0;
    if (reach_motion_range(&clip.width, &minimum, &maximum) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    clip.width = reach_motion_clamp(&clip.width, 0, fmaxf(0, maximum));
    if (reach_motion_range(&clip.height, &minimum, &maximum) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    clip.height = reach_motion_clamp(&clip.height, 0, fmaxf(0, maximum));
    *out_clip = clip;
    return REACH_OK;
}

int32_t reach_render_group_has_rounded_clip(const reach_render_group *group)
{
    if (group == 0 || !group->has_clip)
        return 0;
    if (group->clip_radius_motion.generation == 0)
        return group->clip_radius > 0;
    float minimum = 0, maximum = 0;
    return reach_motion_range(&group->clip_radius_motion, &minimum, &maximum) != REACH_OK ||
           maximum > 0;
}

reach_point_f32 reach_render_group_clip_radii_at(const reach_render_group *group,
                                                 double now_seconds)
{
    reach_point_f32 radii = {0};
    if (group == 0 || !group->has_clip)
        return radii;
    reach_rect_f32 clip = reach_render_group_clip_at(group, now_seconds);
    float radius = group->clip_radius_motion.generation != 0
                       ? reach_motion_value_at(&group->clip_radius_motion, now_seconds)
                       : group->clip_radius;
    radius = fmaxf(0, radius);
    radii.x = fminf(radius, clip.width * .5f);
    radii.y = fminf(radius, clip.height * .5f);
    if (group->clip_uniform_radius)
        radii.x = radii.y = fminf(radii.x, radii.y);
    return radii;
}

reach_result reach_render_group_clip_radii_motion(const reach_render_group *group,
                                                  reach_motion *out_x, reach_motion *out_y)
{
    reach_rect_motion clip = {0};
    if (group == 0 || out_x == 0 || out_y == 0 || !isfinite(group->clip_radius) ||
        group->clip_radius < 0 ||
        (group->clip_uniform_radius != 0 && group->clip_uniform_radius != 1) ||
        reach_render_group_clip_motion(group, &clip) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    reach_motion half_x = reach_motion_affine(&clip.width, 0.5f, 0);
    reach_motion half_y = reach_motion_affine(&clip.height, 0.5f, 0);
    reach_motion x = reach_motion_clamp(&half_x, 0, group->clip_radius);
    reach_motion y = reach_motion_clamp(&half_y, 0, group->clip_radius);
    float radius_maximum = group->clip_radius;
    if (group->clip_radius_motion.generation != 0)
    {
        float minimum = 0, maximum = 0;
        if (reach_motion_range(&group->clip_radius_motion, &minimum, &maximum) != REACH_OK ||
            minimum < 0 || reach_motion_min(&half_x, &group->clip_radius_motion, &x) != REACH_OK ||
            reach_motion_min(&half_y, &group->clip_radius_motion, &y) != REACH_OK)
            return REACH_INVALID_ARGUMENT;
        radius_maximum = maximum;
    }
    if (group->clip_uniform_radius)
    {
        reach_motion radius = {0};
        if (reach_motion_min(&x, &y, &radius) != REACH_OK)
            return REACH_INVALID_ARGUMENT;
        x = y = radius;
    }
    *out_x = reach_motion_clamp(&x, 0, radius_maximum);
    *out_y = reach_motion_clamp(&y, 0, radius_maximum);
    return REACH_OK;
}

reach_point_f32 reach_render_presentation_center_at(const reach_render_command_buffer *buffer,
                                                    double now_seconds)
{
    reach_point_f32 point = {0};
    if (buffer != 0)
    {
        point.x = buffer->presentation_center_x.generation
                      ? reach_motion_value_at(&buffer->presentation_center_x, now_seconds)
                      : buffer->presentation.bounds.x + buffer->presentation.bounds.width * .5f;
        point.y = buffer->presentation_center_y.generation
                      ? reach_motion_value_at(&buffer->presentation_center_y, now_seconds)
                      : buffer->presentation.bounds.y + buffer->presentation.bounds.height * .5f;
    }
    return point;
}

int32_t reach_render_command_buffer_motion_active(const reach_render_command_buffer *buffer)
{
    size_t index;
    if (buffer == 0)
        return 0;
    if (buffer->has_presentation && (reach_render_group_motion_active(&buffer->presentation) ||
                                     reach_motion_active(&buffer->presentation_center_x) ||
                                     reach_motion_active(&buffer->presentation_center_y)))
        return 1;
    for (index = 0; index < buffer->group_count; ++index)
        if (reach_render_group_motion_active(&buffer->groups[index]))
            return 1;
    return 0;
}

const reach_render_group *
reach_render_command_buffer_group(const reach_render_command_buffer *buffer, uint64_t id)
{
    size_t index;
    if (buffer == 0 || id == 0 || buffer->group_count > REACH_MAX_RENDER_GROUPS)
        return 0;
    for (index = 0; index < buffer->group_count; ++index)
        if (buffer->groups[index].id == id)
            return &buffer->groups[index];
    return 0;
}

const reach_render_group *
reach_render_command_buffer_group_shadow(const reach_render_command_buffer *buffer, uint64_t id)
{
    const reach_render_group *group = reach_render_command_buffer_group(buffer, id);
    size_t depth = 0;
    while (group != 0 && depth++ < buffer->group_count)
    {
        if (group->has_shadow)
            return group;
        group = reach_render_command_buffer_group(buffer, group->parent_id);
    }
    return 0;
}

static int32_t reach_render_shadow_valid(const reach_render_group *group, int32_t root)
{
    if (group->has_shadow != 0 && group->has_shadow != 1)
        return 0;
    if (!group->has_shadow)
        return 1;
    return !root && !group->has_tint && isfinite(group->bounds.x) && isfinite(group->bounds.y) &&
           isfinite(group->bounds.width) && isfinite(group->bounds.height) &&
           group->bounds.width > 0 && group->bounds.height > 0 &&
           isfinite(group->shadow.offset_x) && isfinite(group->shadow.offset_y) &&
           isfinite(group->shadow.blur) && group->shadow.blur >= 0 &&
           isfinite(group->shadow.color.r) && isfinite(group->shadow.color.g) &&
           isfinite(group->shadow.color.b) && isfinite(group->shadow.color.a) &&
           group->shadow.color.r >= 0 && group->shadow.color.r <= 1 && group->shadow.color.g >= 0 &&
           group->shadow.color.g <= 1 && group->shadow.color.b >= 0 && group->shadow.color.b <= 1 &&
           group->shadow.color.a >= 0 && group->shadow.color.a <= 1;
}

static int32_t reach_render_repetition_valid(const reach_render_group *group, int32_t root)
{
    size_t index;
    if (!reach_render_shadow_valid(group, root))
        return 0;
    if (group->tint_rgb_mix.generation != 0 &&
        (root || !group->has_tint || !isfinite(group->tint_rgb_target.r) ||
         !isfinite(group->tint_rgb_target.g) || !isfinite(group->tint_rgb_target.b) ||
         group->tint_rgb_target.r < 0 || group->tint_rgb_target.r > 1 ||
         group->tint_rgb_target.g < 0 || group->tint_rgb_target.g > 1 ||
         group->tint_rgb_target.b < 0 || group->tint_rgb_target.b > 1 ||
         !isfinite(group->tint_rgb_mix.from) || !isfinite(group->tint_rgb_mix.to) ||
         !group->tint_rgb_mix.has_limits || !isfinite(group->tint_rgb_mix.minimum) ||
         !isfinite(group->tint_rgb_mix.maximum) ||
         group->tint_rgb_mix.minimum > group->tint_rgb_mix.maximum ||
         group->tint_rgb_mix.minimum < 0 || group->tint_rgb_mix.maximum > 1))
        return 0;
    if ((group->clip_uniform_radius != 0 && group->clip_uniform_radius != 1) ||
        (group->clip_uniform_radius && root) || !isfinite(group->clip_radius) ||
        group->clip_radius < 0 || (root && group->clip_radius > 0))
        return 0;
    if (group->clip_radius_motion.generation != 0)
    {
        float minimum = 0, maximum = 0;
        if (root || !group->has_clip || !isfinite(group->clip_radius_motion.started_seconds) ||
            !isfinite(group->clip_radius_motion.duration_seconds) ||
            group->clip_radius_motion.duration_seconds < 0 ||
            reach_motion_range(&group->clip_radius_motion, &minimum, &maximum) != REACH_OK ||
            minimum < 0)
            return 0;
    }
    if (reach_render_group_has_rounded_clip(group))
    {
        reach_motion radius_x = {0}, radius_y = {0};
        if (reach_render_group_clip_radii_motion(group, &radius_x, &radius_y) != REACH_OK)
            return 0;
    }
    const reach_motion *motions[] = {&group->x,
                                     &group->y,
                                     &group->opacity,
                                     &group->scale,
                                     &group->scale_y,
                                     &group->clip_insets[0],
                                     &group->clip_insets[1],
                                     &group->clip_insets[2],
                                     &group->clip_insets[3],
                                     &group->tint[0],
                                     &group->tint[1],
                                     &group->tint[2],
                                     &group->tint[3],
                                     &group->tint_rgb_mix,
                                     &group->clip_radius_motion};
    for (index = 0; index < sizeof(motions) / sizeof(motions[0]); ++index)
    {
        const reach_motion *motion = motions[index];
        if (motion->has_mapping != 0 && motion->has_mapping != 1 &&
            motion->has_mapping != REACH_MOTION_MAPPING_STEPS &&
            motion->has_mapping != REACH_MOTION_MAPPING_RATIO &&
            motion->has_mapping != REACH_MOTION_MAPPING_QUADRATIC)
            return 0;
        if (motion->has_mapping || motion->mapping_extra_count != 0)
        {
            float minimum = 0, maximum = 0;
            if (reach_motion_range(motion, &minimum, &maximum) != REACH_OK)
                return 0;
        }
        if (motion->repeating && (root || motion->repeating != 1 || motion->generation == 0 ||
                                  !isfinite(motion->started_seconds) || !isfinite(motion->from) ||
                                  !isfinite(motion->to) || reach_motion_cycle_seconds(motion) <= 0))
            return 0;
    }
    return 1;
}

static int32_t reach_render_presentation_center_valid(const reach_render_command_buffer *buffer)
{
    const reach_motion *motions[] = {&buffer->presentation_center_x,
                                     &buffer->presentation_center_y};
    for (size_t index = 0; index < 2; ++index)
    {
        const reach_motion *motion = motions[index];
        if (motion->generation == 0)
            continue;
        float minimum = 0, maximum = 0;
        if (!buffer->has_presentation || motion->repeating || !isfinite(motion->started_seconds) ||
            !isfinite(motion->duration_seconds) || motion->duration_seconds < 0 ||
            reach_motion_range(motion, &minimum, &maximum) != REACH_OK)
            return 0;
    }
    return 1;
}

static int32_t reach_render_width_text_valid(const reach_render_command *command,
                                             const reach_render_group *viewport)
{
    if (command == 0 || viewport == 0 || command->type != REACH_RENDER_COMMAND_TEXT ||
        !viewport->has_clip || reach_render_group_has_rounded_clip(viewport) ||
        viewport->has_tint || viewport->has_shadow ||
        !reach_render_rect_equal(viewport->clip, command->rect) ||
        !reach_render_rect_equal(viewport->bounds, command->rect) || !isfinite(command->rect.x) ||
        !isfinite(command->rect.y) || !isfinite(command->rect.width) ||
        !isfinite(command->rect.height) || command->rect.width <= 0 || command->rect.height <= 0)
        return 0;
    for (size_t side = 0; side < 4; ++side)
        if (side != REACH_RENDER_CLIP_RIGHT)
        {
            float minimum = 0, maximum = 0;
            if (reach_motion_range(&viewport->clip_insets[side], &minimum, &maximum) != REACH_OK ||
                minimum != 0 || maximum != 0)
                return 0;
        }
    reach_rect_motion clip = {0};
    float minimum = 0, maximum = 0;
    return reach_render_group_clip_motion(viewport, &clip) == REACH_OK &&
           reach_motion_range(&clip.width, &minimum, &maximum) == REACH_OK &&
           maximum <= command->rect.width;
}

static int32_t reach_render_fitted_rect_valid(const reach_render_command *command,
                                              const reach_render_group *source)
{
    return source != 0 &&
           (command->type == REACH_RENDER_COMMAND_RECT ||
            (command->type == REACH_RENDER_COMMAND_ICON &&
             source->clip_radius_motion.generation == 0 && command->icon_id != 0 &&
             command->icon_fade_start == 0)) &&
           command->radius == 0 && command->stroke_width == 0 && !command->has_clip_rect &&
           !command->has_scissor && source->has_clip && source->clip_uniform_radius &&
           !source->has_shadow && !source->x.generation && !source->y.generation &&
           !source->scale.generation && !source->scale_y.generation &&
           reach_render_rect_equal(command->rect, source->bounds) &&
           reach_render_rect_equal(source->clip, source->bounds);
}

static int32_t reach_render_textbox_color_owner_valid(const reach_render_group *group)
{
    return group != 0 && group->has_tint && group->parent_id == 0 && !group->has_clip &&
           !group->has_shadow && !group->x.generation && !group->y.generation &&
           !group->scale.generation && !group->scale_y.generation && !group->opacity.generation;
}

reach_result reach_render_command_buffer_validate_groups(const reach_render_command_buffer *buffer)
{
    size_t index, other, active_count = 0;
    size_t active[REACH_MAX_RENDER_GROUPS], path[REACH_MAX_RENDER_GROUPS];
    int32_t closed[REACH_MAX_RENDER_GROUPS] = {0};
    int32_t drawn[REACH_MAX_RENDER_GROUPS] = {0};
    int32_t descendants_drawn[REACH_MAX_RENDER_GROUPS] = {0};
    int32_t has_shadow = 0;
    if (buffer == 0 || buffer->group_count > REACH_MAX_RENDER_GROUPS ||
        buffer->count > REACH_MAX_RENDER_COMMANDS ||
        !reach_render_presentation_center_valid(buffer) ||
        (buffer->has_presentation && (buffer->presentation.parent_id != 0 ||
                                      !reach_render_repetition_valid(&buffer->presentation, 1))))
        return REACH_INVALID_ARGUMENT;
    for (index = 0; index < buffer->group_count; ++index)
    {
        const reach_render_group *group = &buffer->groups[index];
        if (group->id == 0 || (group->id & UINT64_C(0x8000000000000000)) != 0 ||
            !reach_render_repetition_valid(group, 0))
            return REACH_INVALID_ARGUMENT;
        for (other = 0; other < index; ++other)
            if (buffer->groups[other].id == group->id)
                return REACH_INVALID_ARGUMENT;
        has_shadow |= group->has_shadow;
        if (group->has_shadow && reach_render_command_buffer_group_shadow(buffer, group->parent_id))
            return REACH_INVALID_ARGUMENT;
        if (group->parent_id != 0)
        {
            const reach_render_group *parent =
                reach_render_command_buffer_group(buffer, group->parent_id);
            if (parent == 0 || (size_t)(parent - buffer->groups) >= index || parent->has_tint ||
                !isfinite(parent->bounds.x) || !isfinite(parent->bounds.y) ||
                !isfinite(parent->bounds.width) || !isfinite(parent->bounds.height) ||
                parent->bounds.width <= 0.0f || parent->bounds.height <= 0.0f)
                return REACH_INVALID_ARGUMENT;
        }
    }
    for (index = 0; index < buffer->count; ++index)
    {
        const reach_render_command *command = &buffer->commands[index];
        if (command->rect_fit_to_clip != 0 && command->rect_fit_to_clip != 1)
            return REACH_INVALID_ARGUMENT;
        if (command->rect_fit_to_clip)
        {
            const reach_render_group *source =
                reach_render_command_buffer_group(buffer, command->group_id);
            if (!reach_render_fitted_rect_valid(command, source))
                return REACH_INVALID_ARGUMENT;
            for (other = 0; other < buffer->count; ++other)
                if (other != index && buffer->commands[other].group_id == source->id)
                    return REACH_INVALID_ARGUMENT;
            for (other = 0; other < buffer->group_count; ++other)
                if (buffer->groups[other].parent_id == source->id)
                    return REACH_INVALID_ARGUMENT;
        }
        if (command->text_fit_to_clip < 0 ||
            command->text_fit_to_clip > REACH_RENDER_TEXT_FIT_VERTICAL_OVERFLOW)
            return REACH_INVALID_ARGUMENT;
        if (command->text_fit_to_clip)
        {
            const reach_render_group *viewport =
                reach_render_command_buffer_group(buffer, command->group_id);
            if (!reach_render_width_text_valid(command, viewport))
                return REACH_INVALID_ARGUMENT;
            for (other = 0; other < buffer->count; ++other)
                if (other != index && buffer->commands[other].group_id == command->group_id)
                    return REACH_INVALID_ARGUMENT;
        }
        const reach_render_group *shadow =
            has_shadow ? reach_render_command_buffer_group_shadow(buffer, command->group_id) : 0;
        if (shadow != 0 && (command->group_id == shadow->id ||
                            (command->type != REACH_RENDER_COMMAND_RECT &&
                             command->type != REACH_RENDER_COMMAND_TRIANGLE &&
                             command->type != REACH_RENDER_COMMAND_LINE &&
                             command->type != REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT &&
                             command->type != REACH_RENDER_COMMAND_CLIPPED_ROUNDED_RECT)))
            return REACH_INVALID_ARGUMENT;
        if (command->text_color_group_id != 0)
        {
            const reach_render_group *color =
                reach_render_command_buffer_group(buffer, command->text_color_group_id);
            const reach_render_group *body =
                reach_render_command_buffer_group(buffer, command->group_id);
            if (command->type != REACH_RENDER_COMMAND_TEXTBOX ||
                !reach_render_textbox_color_owner_valid(color) ||
                command->caret_group_id == color->id || (body != 0 && body->has_tint))
                return REACH_INVALID_ARGUMENT;
            for (other = 0; other < buffer->count; ++other)
                if (buffer->commands[other].group_id == color->id ||
                    buffer->commands[other].caret_group_id == color->id ||
                    (other != index && (buffer->commands[other].text_color_group_id == color->id ||
                                        (command->group_id != 0 &&
                                         buffer->commands[other].group_id == command->group_id))))
                    return REACH_INVALID_ARGUMENT;
        }
        if (command->caret_group_id == 0)
            continue;
        const reach_render_group *caret =
            reach_render_command_buffer_group(buffer, command->caret_group_id);
        if (command->type != REACH_RENDER_COMMAND_TEXTBOX || caret == 0 || caret->parent_id != 0 ||
            caret->has_clip || caret->has_tint || caret->has_shadow || caret->x.generation ||
            caret->y.generation || caret->scale.generation || caret->scale_y.generation ||
            caret->opacity.generation == 0)
            return REACH_INVALID_ARGUMENT;
        const reach_render_group *body =
            reach_render_command_buffer_group(buffer, command->group_id);
        if (body != 0 && body->has_tint)
            return REACH_INVALID_ARGUMENT;
        for (other = 0; other < buffer->count; ++other)
            if (buffer->commands[other].group_id == caret->id ||
                (other != index && (buffer->commands[other].caret_group_id == caret->id ||
                                    (command->group_id != 0 &&
                                     buffer->commands[other].group_id == command->group_id))))
                return REACH_INVALID_ARGUMENT;
    }
    for (index = 0; index < buffer->count;)
    {
        uint64_t id = buffer->commands[index].group_id;
        const reach_render_group *group = reach_render_command_buffer_group(buffer, id);
        size_t depth = 0, common = 0, end = index + 1;
        if (id != 0 && group == 0)
            return REACH_INVALID_ARGUMENT;
        while (group != 0)
        {
            path[depth++] = (size_t)(group - buffer->groups);
            group = reach_render_command_buffer_group(buffer, group->parent_id);
        }
        for (other = 0; other < depth / 2; ++other)
        {
            size_t swap = path[other];
            path[other] = path[depth - other - 1];
            path[depth - other - 1] = swap;
        }
        while (common < depth && common < active_count && path[common] == active[common])
            ++common;
        for (other = common; other < active_count; ++other)
            closed[active[other]] = 1;
        for (other = common; other < depth; ++other)
            if (closed[path[other]])
                return REACH_INVALID_ARGUMENT;
        if (depth != 0)
        {
            size_t leaf = path[depth - 1];
            if (drawn[leaf] || descendants_drawn[leaf])
                return REACH_INVALID_ARGUMENT;
            drawn[leaf] = 1;
            for (other = 0; other + 1 < depth; ++other)
                descendants_drawn[path[other]] = 1;
        }
        for (other = 0; other < depth; ++other)
            active[other] = path[other];
        active_count = depth;
        while (end < buffer->count && buffer->commands[end].group_id == id)
            ++end;
        index = end;
    }
    return REACH_OK;
}

reach_transform_f32
reach_render_command_buffer_group_transform_at(const reach_render_command_buffer *buffer,
                                               uint64_t id, double now_seconds)
{
    reach_transform_f32 result = {1.0f, 1.0f, 0.0f, 0.0f};
    const reach_render_group *group = reach_render_command_buffer_group(buffer, id);
    size_t depth = 0;
    while (group != 0 && depth++ < buffer->group_count)
    {
        reach_point_f32 scale = reach_render_group_scale_at(group, now_seconds);
        result.offset_x = result.offset_x * scale.x + group->bounds.x * (1.0f - scale.x) +
                          reach_motion_value_at(&group->x, now_seconds);
        result.offset_y = result.offset_y * scale.y + group->bounds.y * (1.0f - scale.y) +
                          reach_motion_value_at(&group->y, now_seconds);
        result.scale_x *= scale.x;
        result.scale_y *= scale.y;
        group = reach_render_command_buffer_group(buffer, group->parent_id);
    }
    return result;
}

float reach_render_command_buffer_group_opacity_at(const reach_render_command_buffer *buffer,
                                                   uint64_t id, double now_seconds)
{
    float opacity = 1.0f;
    const reach_render_group *group = reach_render_command_buffer_group(buffer, id);
    size_t depth = 0;
    while (group != 0 && depth++ < buffer->group_count)
    {
        if (group->opacity.generation != 0)
            opacity *= reach_motion_value_at(&group->opacity, now_seconds);
        group = reach_render_command_buffer_group(buffer, group->parent_id);
    }
    return opacity;
}

static int32_t reach_render_group_clips_at(const reach_render_command_buffer *buffer, uint64_t id,
                                           double now_seconds, reach_rect_f32 *out_clip,
                                           int32_t rectangular_only)
{
    int32_t has_clip = 0;
    size_t depth = 0;
    const reach_render_group *group = reach_render_command_buffer_group(buffer, id);
    if (out_clip == 0)
        return 0;
    while (group != 0 && depth++ < buffer->group_count)
    {
        if (group->has_clip && (!rectangular_only || !reach_render_group_has_rounded_clip(group)))
        {
            reach_rect_f32 clip = reach_render_group_clip_at(group, now_seconds);
            reach_transform_f32 transform = reach_render_command_buffer_group_transform_at(
                buffer, group->parent_id, now_seconds);
            float x = clip.x * transform.scale_x + transform.offset_x;
            float y = clip.y * transform.scale_y + transform.offset_y;
            float far_x = (clip.x + clip.width) * transform.scale_x + transform.offset_x;
            float far_y = (clip.y + clip.height) * transform.scale_y + transform.offset_y;
            clip.x = fminf(x, far_x);
            clip.y = fminf(y, far_y);
            clip.width = fabsf(far_x - x);
            clip.height = fabsf(far_y - y);
            if (has_clip)
            {
                float right = fminf(out_clip->x + out_clip->width, clip.x + clip.width);
                float bottom = fminf(out_clip->y + out_clip->height, clip.y + clip.height);
                clip.x = fmaxf(out_clip->x, clip.x);
                clip.y = fmaxf(out_clip->y, clip.y);
                clip.width = fmaxf(0.0f, right - clip.x);
                clip.height = fmaxf(0.0f, bottom - clip.y);
            }
            *out_clip = clip;
            has_clip = 1;
        }
        group = reach_render_command_buffer_group(buffer, group->parent_id);
    }
    return has_clip;
}

int32_t reach_render_command_buffer_group_clip_at(const reach_render_command_buffer *buffer,
                                                  uint64_t id, double now_seconds,
                                                  reach_rect_f32 *out_clip)
{
    return reach_render_group_clips_at(buffer, id, now_seconds, out_clip, 0);
}

int32_t
reach_render_command_buffer_group_rectangular_clip_at(const reach_render_command_buffer *buffer,
                                                      uint64_t id, double now_seconds,
                                                      reach_rect_f32 *out_clip)
{
    return reach_render_group_clips_at(buffer, id, now_seconds, out_clip, 1);
}

static void reach_copy_text(uint16_t *dst, size_t dst_count, const uint16_t *src)
{
    size_t index = 0;
    if (dst == 0 || dst_count == 0)
    {
        return;
    }
    if (src == 0)
    {
        dst[0] = 0;
        return;
    }
    while (index + 1 < dst_count && src[index] != 0)
    {
        dst[index] = src[index];
        ++index;
    }
    dst[index] = 0;
}

static reach_rect_f32 reach_dock_child_to_screen(const reach_ui_layout *layout, reach_rect_f32 rect)
{
    rect.x += layout->dock.bounds.x;
    rect.y += layout->dock.bounds.y;
    return rect;
}

void reach_render_command_buffer_clear(reach_render_command_buffer *buffer)
{
    if (buffer != 0)
    {
        reach_rect_f32 empty = {0.0f, 0.0f, 0.0f, 0.0f};
        buffer->count = 0;
        buffer->group_count = 0;
        buffer->current_group_id = 0;
        buffer->has_presentation = 0;
        buffer->requires_content_sampling = 0;
        buffer->presentation_center_x = (reach_motion){0};
        buffer->presentation_center_y = (reach_motion){0};
        buffer->has_scissor = 0;
        buffer->content_rect = empty;
        buffer->content_transform = (reach_transform_f32){1.0f, 1.0f, 0.0f, 0.0f};
    }
}

reach_result reach_render_command_buffer_check_group(const reach_render_command_buffer *buffer,
                                                     const reach_render_group *group)
{
    size_t index;
    if (buffer == 0 || group == 0 || group->id == 0 || !reach_render_repetition_valid(group, 0) ||
        (group->id & UINT64_C(0x8000000000000000)) != 0 ||
        buffer->group_count >= REACH_MAX_RENDER_GROUPS || buffer->current_group_id != 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    for (index = 0; index < buffer->group_count; ++index)
        if (buffer->groups[index].id == group->id)
            return REACH_INVALID_ARGUMENT;
    if (group->parent_id != 0)
    {
        const reach_render_group *parent =
            reach_render_command_buffer_group(buffer, group->parent_id);
        if (parent == 0 || parent->has_tint || !isfinite(parent->bounds.x) ||
            !isfinite(parent->bounds.y) || !isfinite(parent->bounds.width) ||
            !isfinite(parent->bounds.height) || parent->bounds.width <= 0.0f ||
            parent->bounds.height <= 0.0f)
            return REACH_INVALID_ARGUMENT;
    }
    return REACH_OK;
}

reach_result reach_render_command_buffer_begin_group(reach_render_command_buffer *buffer,
                                                     const reach_render_group *group)
{
    reach_result result = reach_render_command_buffer_check_group(buffer, group);
    if (result != REACH_OK)
        return result;
    buffer->groups[buffer->group_count++] = *group;
    buffer->current_group_id = group->id;
    return REACH_OK;
}

void reach_render_command_buffer_end_group(reach_render_command_buffer *buffer)
{
    if (buffer != 0)
    {
        buffer->current_group_id = 0;
    }
}

void reach_render_command_buffer_set_content_rect(reach_render_command_buffer *buffer,
                                                  reach_rect_f32 content_rect)
{
    if (buffer != 0)
    {
        buffer->content_rect = content_rect;
        buffer->content_transform =
            (reach_transform_f32){1.0f, 1.0f, content_rect.x, content_rect.y};
    }
}

void reach_render_command_buffer_set_content_transform(reach_render_command_buffer *buffer,
                                                       reach_rect_f32 content_rect,
                                                       reach_transform_f32 content_transform)
{
    if (buffer != 0)
    {
        buffer->content_rect = content_rect;
        buffer->content_transform = content_transform;
    }
}

void reach_render_command_buffer_set_scissor(reach_render_command_buffer *buffer,
                                             reach_rect_f32 scissor_rect)
{
    if (buffer != 0)
    {
        buffer->scissor_rect = scissor_rect;
        buffer->has_scissor = 1;
    }
}

void reach_render_command_buffer_clear_scissor(reach_render_command_buffer *buffer)
{
    if (buffer != 0)
    {
        buffer->has_scissor = 0;
    }
}

reach_result reach_render_push_shadow(reach_render_command_buffer *buffer,
                                      const reach_render_command *shape, const reach_shadow *shadow,
                                      float dpi_scale)
{
    float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    reach_render_command command;

    if (buffer == 0 || shape == 0 || shadow == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (shadow->color.a <= 0.0f || shadow->blur <= 0.0f)
    {
        return REACH_OK;
    }

    command = *shape;
    command.type = REACH_RENDER_COMMAND_SHADOW;
    command.color = shadow->color;
    command.stroke_width = 0.0f;
    command.blur_radius = shadow->blur * scale;
    command.shadow_offset_x = shadow->offset_x * scale;
    command.shadow_offset_y = shadow->offset_y * scale;
    return reach_render_command_buffer_push(buffer, &command);
}

reach_rect_f32 reach_render_pixel_aligned_rect(reach_rect_f32 rect)
{
    float right = roundf(rect.x + rect.width);
    float bottom = roundf(rect.y + rect.height);
    rect.x = roundf(rect.x);
    rect.y = roundf(rect.y);
    rect.width = right > rect.x ? right - rect.x : 0.0f;
    rect.height = bottom > rect.y ? bottom - rect.y : 0.0f;
    return rect;
}

static reach_render_command reach_render_inset_shape(const reach_render_command *shape, float inset)
{
    reach_render_command inner = *shape;
    inner.rect.x += inset;
    inner.rect.y += inset;
    inner.rect.width -= inset * 2.0f;
    inner.rect.height -= inset * 2.0f;
    inner.radius = inner.radius > inset ? inner.radius - inset : 0.0f;
    inner.stroke_width = 0.0f;
    return inner;
}

size_t reach_render_bordered_background_shapes(const reach_render_command *shape,
                                               reach_color background_color,
                                               reach_color border_color, float border_thickness,
                                               reach_render_command out_shapes[2])
{
    reach_render_command outer;
    reach_render_command inner;
    if (shape == 0 || out_shapes == 0 ||
        (shape->type != REACH_RENDER_COMMAND_RECT &&
         shape->type != REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT))
        return 0;
    outer = *shape;
    outer.rect = reach_render_pixel_aligned_rect(outer.rect);
    outer.stroke_width = 0;
    if (outer.rect.width <= 0 || outer.rect.height <= 0)
        return 0;
    background_color.a = 1;
    if (border_thickness <= 0 || border_color.a <= 0)
    {
        outer.color = background_color;
        out_shapes[0] = outer;
        return 1;
    }
    border_color.a = 1;
    outer.color = border_color;
    out_shapes[0] = outer;
    inner = reach_render_inset_shape(&outer, border_thickness);
    if (inner.rect.width <= 0 || inner.rect.height <= 0)
        return 1;
    inner.color = background_color;
    out_shapes[1] = inner;
    return 2;
}

reach_result reach_render_push_bordered_background(reach_render_command_buffer *buffer,
                                                   const reach_render_command *shape,
                                                   reach_color background_color,
                                                   reach_color border_color, float border_thickness,
                                                   const reach_shadow *shadow, float dpi_scale)
{
    reach_render_command shapes[2];
    size_t count;
    size_t index;
    reach_result result;
    if (buffer == 0 || shape == 0 ||
        (shape->type != REACH_RENDER_COMMAND_RECT &&
         shape->type != REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT))
        return REACH_INVALID_ARGUMENT;
    count = reach_render_bordered_background_shapes(shape, background_color, border_color,
                                                    border_thickness, shapes);
    if (count == 0)
        return REACH_OK;
    if (shadow != 0)
    {
        result = reach_render_push_shadow(buffer, &shapes[0], shadow, dpi_scale);
        if (result != REACH_OK)
            return result;
    }
    for (index = 0; index < count; ++index)
    {
        result = reach_render_command_buffer_push(buffer, &shapes[index]);
        if (result != REACH_OK)
            return result;
    }
    return REACH_OK;
}

reach_result reach_render_command_buffer_push(reach_render_command_buffer *buffer,
                                              const reach_render_command *command)
{
    if (buffer == 0 || command == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (buffer->count >= REACH_MAX_RENDER_COMMANDS)
    {
        return REACH_ERROR;
    }

    buffer->commands[buffer->count] = *command;
    if (buffer->current_group_id != 0)
    {
        buffer->commands[buffer->count].group_id = buffer->current_group_id;
    }
    if (buffer->has_scissor && !command->has_scissor)
    {
        buffer->commands[buffer->count].scissor_rect = buffer->scissor_rect;
        buffer->commands[buffer->count].has_scissor = 1;
    }
    buffer->count += 1;
    return REACH_OK;
}

reach_result reach_render_push_textbox(reach_render_command_buffer *buffer,
                                       const reach_render_command *command, uint64_t caret_id,
                                       const reach_motion *caret_opacity,
                                       const reach_render_group *text_color)
{
    if (buffer == 0 || command == 0 || command->type != REACH_RENDER_COMMAND_TEXTBOX ||
        command->caret_group_id != 0 || command->text_color_group_id != 0)
        return REACH_INVALID_ARGUMENT;
    int32_t has_caret = caret_id != 0 && caret_opacity != 0;
    if (text_color != 0 && (!reach_render_textbox_color_owner_valid(text_color) ||
                            (has_caret && caret_id == text_color->id)))
        return REACH_INVALID_ARGUMENT;
    if (buffer->count >= REACH_MAX_RENDER_COMMANDS ||
        buffer->group_count + (size_t)has_caret + (text_color != 0) > REACH_MAX_RENDER_GROUPS)
        return REACH_ERROR;
    reach_render_group caret = {0};
    caret.id = caret_id;
    caret.bounds = command->rect;
    if (has_caret)
        caret.opacity = *caret_opacity;
    uint64_t parent = buffer->current_group_id;
    buffer->current_group_id = 0;
    int32_t valid =
        (!has_caret || reach_render_command_buffer_check_group(buffer, &caret) == REACH_OK) &&
        (text_color == 0 ||
         reach_render_command_buffer_check_group(buffer, text_color) == REACH_OK);
    buffer->current_group_id = parent;
    if (!valid)
        return REACH_INVALID_ARGUMENT;
    size_t first_group = buffer->group_count;
    buffer->current_group_id = 0;
    reach_result result = REACH_OK;
    if (text_color != 0)
        result = reach_render_command_buffer_begin_group(buffer, text_color);
    reach_render_command_buffer_end_group(buffer);
    if (result == REACH_OK && has_caret)
        result = reach_render_command_buffer_begin_group(buffer, &caret);
    buffer->current_group_id = parent;
    reach_render_command textbox = *command;
    if (has_caret)
    {
        textbox.caret_group_id = caret_id;
        textbox.caret_visible = 1;
    }
    if (text_color != 0)
        textbox.text_color_group_id = text_color->id;
    if (result == REACH_OK)
        result = reach_render_command_buffer_push(buffer, &textbox);
    if (result != REACH_OK)
        buffer->group_count = first_group;
    return result;
}

reach_result reach_render_push_width_text(reach_render_command_buffer *buffer,
                                          const reach_render_command *command,
                                          const reach_render_group *viewport)
{
    if (buffer == 0 || buffer->current_group_id != 0 ||
        !reach_render_width_text_valid(command, viewport) ||
        reach_render_command_buffer_group_shadow(buffer, viewport->parent_id) != 0)
        return REACH_INVALID_ARGUMENT;
    if (buffer->count >= REACH_MAX_RENDER_COMMANDS)
        return REACH_ERROR;
    reach_result result = reach_render_command_buffer_begin_group(buffer, viewport);
    if (result != REACH_OK)
        return result;
    reach_render_command text = *command;
    text.text_fit_to_clip = command->text_fit_to_clip == REACH_RENDER_TEXT_FIT_VERTICAL_OVERFLOW
                                ? REACH_RENDER_TEXT_FIT_VERTICAL_OVERFLOW
                                : REACH_RENDER_TEXT_FIT_CLIPPED;
    result = reach_render_command_buffer_push(buffer, &text);
    reach_render_command_buffer_end_group(buffer);
    if (result != REACH_OK)
        --buffer->group_count;
    return result;
}

static reach_result reach_render_push_fitted_rect(reach_render_command_buffer *buffer,
                                                  const reach_render_group *source,
                                                  const reach_rect_motion *bounds, float radius,
                                                  const reach_render_command *command)
{
    if (buffer == 0 || source == 0 || bounds == 0 || buffer->current_group_id != 0 ||
        source->id == 0 || source->id >= UINT64_C(0x8000000000000000) - 1 || source->has_clip ||
        source->has_shadow || source->x.generation || source->y.generation ||
        source->scale.generation || source->scale_y.generation || !isfinite(radius) || radius < 0 ||
        !isfinite(source->bounds.x) || !isfinite(source->bounds.y) ||
        !isfinite(source->bounds.width) || !isfinite(source->bounds.height) ||
        source->bounds.width <= 0 || source->bounds.height <= 0 || source->bounds.width > 16384 ||
        source->bounds.height > 16384 || bounds->width.generation == 0 ||
        bounds->height.generation == 0 ||
        (source->has_tint && reach_render_command_buffer_group_shadow(buffer, source->parent_id)))
        return REACH_INVALID_ARGUMENT;
    float minimum = 0, maximum = 0;
    if (reach_motion_range(&bounds->width, &minimum, &maximum) != REACH_OK || minimum < 0 ||
        maximum > source->bounds.width ||
        reach_motion_range(&bounds->height, &minimum, &maximum) != REACH_OK || minimum < 0 ||
        maximum > source->bounds.height)
        return REACH_INVALID_ARGUMENT;
    if (buffer->count >= REACH_MAX_RENDER_COMMANDS ||
        buffer->group_count > REACH_MAX_RENDER_GROUPS - 2)
        return REACH_ERROR;
    for (size_t index = 0; index < buffer->group_count; ++index)
        if (buffer->groups[index].id == source->id || buffer->groups[index].id == source->id + 1)
            return REACH_INVALID_ARGUMENT;
    reach_render_group position = {0};
    position.id = source->id;
    position.parent_id = source->parent_id;
    position.bounds = source->bounds;
    position.x = reach_motion_affine(&bounds->x, 1, -source->bounds.x);
    position.y = reach_motion_affine(&bounds->y, 1, -source->bounds.y);
    position.has_clip = buffer->has_scissor;
    position.clip = buffer->scissor_rect;
    reach_result result = reach_render_command_buffer_check_group(buffer, &position);
    if (result != REACH_OK)
        return result;
    reach_render_group paint = *source;
    paint.id = source->id + 1;
    paint.parent_id = position.id;
    paint.has_clip = 1;
    paint.clip = source->bounds;
    paint.clip_radius = radius;
    paint.clip_uniform_radius = 1;
    paint.clip_insets[REACH_RENDER_CLIP_RIGHT] =
        reach_motion_affine(&bounds->width, -1, source->bounds.width);
    paint.clip_insets[REACH_RENDER_CLIP_BOTTOM] =
        reach_motion_affine(&bounds->height, -1, source->bounds.height);
    if (!reach_render_repetition_valid(&paint, 0))
        return REACH_INVALID_ARGUMENT;
    size_t first_group = buffer->group_count;
    result = reach_render_command_buffer_begin_group(buffer, &position);
    reach_render_command_buffer_end_group(buffer);
    if (result == REACH_OK)
        result = reach_render_command_buffer_begin_group(buffer, &paint);
    reach_render_command fill = *command;
    fill.rect_fit_to_clip = 1;
    fill.rect = source->bounds;
    int32_t scissored = buffer->has_scissor;
    buffer->has_scissor = 0;
    if (result == REACH_OK)
        result = reach_render_command_buffer_push(buffer, &fill);
    buffer->has_scissor = scissored;
    reach_render_command_buffer_end_group(buffer);
    if (result != REACH_OK)
        buffer->group_count = first_group;
    return result;
}

reach_result reach_render_push_rect_motion(reach_render_command_buffer *buffer,
                                           const reach_render_group *source,
                                           const reach_rect_motion *bounds, float radius,
                                           reach_color color)
{
    reach_render_command command = {0};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.color = color;
    return reach_render_push_fitted_rect(buffer, source, bounds, radius, &command);
}

reach_result reach_render_push_image_motion(reach_render_command_buffer *buffer,
                                            const reach_render_group *source,
                                            const reach_rect_motion *bounds, float radius,
                                            const reach_render_command *image)
{
    if (buffer == 0 || source == 0 ||
        reach_render_command_buffer_group_shadow(buffer, source->parent_id) != 0 || image == 0 ||
        source->clip_radius_motion.generation != 0 || image->type != REACH_RENDER_COMMAND_ICON ||
        image->icon_id == 0 || image->radius != 0 || image->icon_fade_start != 0 ||
        image->has_clip_rect || image->has_scissor || image->rect_fit_to_clip ||
        image->text_fit_to_clip || image->stroke_width != 0 || image->caret_group_id != 0 ||
        image->text_color_group_id != 0 ||
        (image->corner_mask != 0 && image->corner_mask != REACH_RENDER_CORNER_ALL))
        return REACH_INVALID_ARGUMENT;
    if (bounds == 0)
        return REACH_INVALID_ARGUMENT;
    reach_rect_motion aligned = *bounds;
    reach_motion *axes[] = {&aligned.x, &aligned.y, &aligned.width, &aligned.height};
    for (size_t index = 0; index < 4; ++index)
    {
        float minimum = 0, maximum = 0;
        if (reach_motion_range(axes[index], &minimum, &maximum) != REACH_OK)
            return REACH_INVALID_ARGUMENT;
        if (minimum == maximum)
        {
            float value = floorf(minimum + .5f);
            if (index >= 2 && minimum > 0)
                value = fmaxf(1, value);
            reach_motion_set(axes[index], value);
        }
    }
    if (!isfinite(source->bounds.x) || !isfinite(source->bounds.y))
        return REACH_INVALID_ARGUMENT;
    reach_render_group raster = *source;
    raster.bounds.x = floorf(raster.bounds.x);
    raster.bounds.y = floorf(raster.bounds.y);
    return reach_render_push_fitted_rect(buffer, &raster, &aligned, radius, image);
}

int32_t reach_render_retained_command_bounds(const reach_render_command *command,
                                             reach_rect_f32 *out_bounds)
{
    if (command == 0 || out_bounds == 0)
        return 0;
    if (!isfinite(command->rect.x) || !isfinite(command->rect.y) ||
        !isfinite(command->rect.width) || !isfinite(command->rect.height) ||
        command->rect.width < 0.0f || command->rect.height < 0.0f ||
        !isfinite(command->text_size) || !isfinite(command->stroke_width))
        return 0;
    float padding = command->type == REACH_RENDER_COMMAND_TEXT
                        ? command->text_size
                        : fmaxf(1.0f, command->stroke_width * 0.5f + 1.0f);
    if (command->type == REACH_RENDER_COMMAND_SHADOW)
    {
        reach_shadow shadow = {command->shadow_offset_x, command->shadow_offset_y,
                               command->blur_radius, command->color};
        padding = reach_theme_shadow_extent(&shadow, 1.0f);
    }
    if (!isfinite(padding) || padding < 0.0f)
        return 0;
    reach_rect_f32 bounds = {floorf(command->rect.x - padding), floorf(command->rect.y - padding),
                             0, 0};
    bounds.width = ceilf(command->rect.x + command->rect.width + padding) - bounds.x;
    bounds.height = ceilf(command->rect.y + command->rect.height + padding) - bounds.y;
    if (!isfinite(bounds.x) || !isfinite(bounds.y) || !isfinite(bounds.width) ||
        !isfinite(bounds.height) || bounds.width <= 0 || bounds.height <= 0 ||
        bounds.width > 16384 || bounds.height > 16384)
        return 0;
    *out_bounds = bounds;
    return 1;
}

reach_result reach_render_retain_commands(reach_render_command_buffer *buffer, size_t first_command,
                                          uint64_t first_group_id, uint64_t parent_id,
                                          const reach_motion *opacity, const reach_motion *y)
{
    if (buffer == 0 || opacity == 0 || y == 0 || first_command > buffer->count ||
        buffer->count > REACH_MAX_RENDER_COMMANDS || buffer->current_group_id != 0)
        return REACH_INVALID_ARGUMENT;
    size_t count = buffer->count - first_command;
    if (buffer->group_count > REACH_MAX_RENDER_GROUPS ||
        count > REACH_MAX_RENDER_GROUPS - buffer->group_count)
        return REACH_ERROR;
    if (first_group_id == 0 || first_group_id >= UINT64_C(0x8000000000000000) ||
        count > UINT64_C(0x8000000000000000) - first_group_id)
        return REACH_INVALID_ARGUMENT;
    for (size_t index = 0; index < count; ++index)
    {
        const reach_render_command *command = &buffer->commands[first_command + index];
        reach_render_group group = {0};
        group.id = first_group_id + index;
        group.parent_id = parent_id;
        if (command->group_id != 0 ||
            !reach_render_retained_command_bounds(command, &group.bounds) ||
            reach_render_command_buffer_check_group(buffer, &group) != REACH_OK)
            return REACH_INVALID_ARGUMENT;
    }
    for (size_t index = 0; index < count; ++index)
    {
        reach_render_command *command = &buffer->commands[first_command + index];
        reach_render_group group = {0};
        group.id = first_group_id + index;
        group.parent_id = parent_id;
        (void)reach_render_retained_command_bounds(command, &group.bounds);
        group.opacity = *opacity;
        group.y = *y;
        reach_result result = reach_render_command_buffer_begin_group(buffer, &group);
        if (result != REACH_OK)
            return result;
        command->group_id = group.id;
        reach_render_command_buffer_end_group(buffer);
    }
    return REACH_OK;
}
