#include "reach/features/common/popup.h"

#include <math.h>
#include <memory>
#include <new>

float reach_popup_radius(const reach_theme *theme)
{
    return theme != nullptr ? theme->radius_large : reach_theme_default()->radius_large;
}

static float reach_popup_scale(float value, float dpi_scale)
{
    return value * (dpi_scale > 0.0f ? dpi_scale : 1.0f);
}

float reach_popup_radius_scaled(const reach_theme *theme, float dpi_scale)
{
    return reach_popup_scale(reach_popup_radius(theme), dpi_scale);
}

int32_t reach_popup_notch_side(int32_t direction)
{
    return direction == REACH_POPUP_DROP_DOWN ? REACH_NOTCH_SIDE_TOP : REACH_NOTCH_SIDE_BOTTOM;
}

static float reach_popup_clamp(float value, float min_value, float max_value)
{
    if (max_value < min_value)
    {
        return min_value;
    }
    if (value < min_value)
    {
        return min_value;
    }
    return value > max_value ? max_value : value;
}

reach_rect_f32 reach_popup_constrain_bounds(reach_rect_f32 bounds, reach_rect_f32 monitor,
                                            float margin)
{
    if (monitor.width <= 0.0f || monitor.height <= 0.0f)
    {
        return bounds;
    }

    float horizontal_margin = margin * 2.0f < monitor.width ? margin : 0.0f;
    float vertical_margin = margin * 2.0f < monitor.height ? margin : 0.0f;
    float available_width = monitor.width - horizontal_margin * 2.0f;
    float available_height = monitor.height - vertical_margin * 2.0f;
    if (bounds.width > available_width)
    {
        bounds.width = available_width;
    }
    if (bounds.height > available_height)
    {
        bounds.height = available_height;
    }
    if (bounds.width < 0.0f)
    {
        bounds.width = 0.0f;
    }
    if (bounds.height < 0.0f)
    {
        bounds.height = 0.0f;
    }
    bounds.x = reach_popup_clamp(bounds.x, monitor.x + horizontal_margin,
                                 monitor.x + monitor.width - bounds.width - horizontal_margin);
    bounds.y = reach_popup_clamp(bounds.y, monitor.y + vertical_margin,
                                 monitor.y + monitor.height - bounds.height - vertical_margin);
    return bounds;
}

reach_popup_placement reach_popup_place(const reach_popup_anchor *anchor, float width, float height,
                                        float margin)
{
    reach_popup_placement placement = {};
    if (anchor == nullptr)
    {
        return placement;
    }

    placement.bounds.width = width;
    placement.bounds.height = height;
    placement.bounds.x = anchor->button.x + anchor->button.width * 0.5f - width * 0.5f;
    placement.bounds.y = anchor->direction == REACH_POPUP_DROP_DOWN
                             ? anchor->bar_edge_y + margin
                             : anchor->bar_edge_y - height - margin;
    placement.bounds = reach_popup_constrain_bounds(placement.bounds, anchor->monitor, margin);
    placement.notch_anchor_x = anchor->button.x + anchor->button.width * 0.5f;
    placement.notch_side = reach_popup_notch_side(anchor->direction);
    return placement;
}

reach_result reach_popup_height_motion(const reach_popup_anchor *anchor, float width,
                                       const reach_motion *height, float margin,
                                       reach_rect_motion *out_bounds)
{
    if (anchor == nullptr || height == nullptr || out_bounds == nullptr || !isfinite(width) ||
        width <= 0 || !isfinite(margin) || margin < 0 || height->generation == 0 ||
        !isfinite(height->from) || !isfinite(height->to))
        return REACH_INVALID_ARGUMENT;
    reach_rect_motion result = {};
    result.height = *height;
    bool constrained = anchor->monitor.width > 0 && anchor->monitor.height > 0;
    float vertical_margin = margin * 2 < anchor->monitor.height ? margin : 0;
    float top = anchor->monitor.y + vertical_margin;
    float bottom = anchor->monitor.y + anchor->monitor.height - vertical_margin;
    if (constrained)
        result.height = reach_motion_clamp(height, 0, bottom - top);
    float first = reach_motion_value_at(&result.height, result.height.started_seconds);
    float last = reach_motion_value_at(&result.height, reach_motion_deadline(&result.height));
    if (fminf(first, last) <= 0)
        return REACH_INVALID_ARGUMENT;
    reach_popup_placement placement = reach_popup_place(anchor, width, fmaxf(first, last), margin);
    reach_motion_set(&result.x, placement.bounds.x);
    reach_motion_set(&result.width, placement.bounds.width);
    if (anchor->direction == REACH_POPUP_DROP_DOWN)
    {
        float anchor_y = anchor->bar_edge_y + margin;
        if (constrained)
        {
            result.y = reach_motion_affine(&result.height, -1, bottom);
            result.y = reach_motion_clamp(&result.y, top, fmaxf(top, anchor_y));
        }
        else
            reach_motion_set(&result.y, anchor_y);
    }
    else
    {
        float anchor_y = anchor->bar_edge_y - margin;
        result.y = reach_motion_affine(&result.height, -1,
                                       constrained ? fminf(anchor_y, bottom) : anchor_y);
        if (constrained)
            result.y = reach_motion_clamp(&result.y, top, bottom);
    }
    *out_bounds = result;
    return REACH_OK;
}

reach_result reach_popup_width_motion(const reach_popup_anchor *anchor, const reach_motion *width,
                                      float height, float margin, float dpi_scale,
                                      reach_rect_motion *out_bounds, reach_motion *out_notch_center)
{
    if (anchor == nullptr || width == nullptr || out_bounds == nullptr ||
        out_notch_center == nullptr || width->generation == 0 || width->has_mapping ||
        !isfinite(height) || height <= 0 || !isfinite(margin) || margin < 0 ||
        !isfinite(dpi_scale) || dpi_scale <= 0 || !isfinite(anchor->button.x) ||
        !isfinite(anchor->button.width) || !isfinite(anchor->monitor.x) ||
        !isfinite(anchor->monitor.y) || !isfinite(anchor->monitor.width) ||
        !isfinite(anchor->monitor.height) || !isfinite(anchor->bar_edge_y))
        return REACH_INVALID_ARGUMENT;
    reach_rect_motion result = {};
    result.width = *width;
    bool constrained = anchor->monitor.width > 0 && anchor->monitor.height > 0;
    float horizontal_margin = margin * 2 < anchor->monitor.width ? margin : 0;
    float left = anchor->monitor.x + horizontal_margin;
    float right = anchor->monitor.x + anchor->monitor.width - horizontal_margin;
    if (constrained)
        result.width = reach_motion_clamp(width, 0, right - left);
    float minimum = 0, maximum = 0;
    if (reach_motion_range(&result.width, &minimum, &maximum) != REACH_OK || minimum <= 0)
        return REACH_INVALID_ARGUMENT;
    reach_popup_placement narrow = reach_popup_place(anchor, minimum, height, margin);
    reach_popup_placement wide = reach_popup_place(anchor, maximum, height, margin);
    reach_motion raw_width = *width;
    raw_width.has_limits = 0;
    float owner = anchor->button.x + anchor->button.width * 0.5f;
    bool near_left = !constrained || owner <= (left + right) * 0.5f;
    if (near_left)
        result.x = reach_motion_affine(&raw_width, -0.5f, owner);
    else
    {
        float threshold = 2 * (right - owner);
        if (reach_motion_hinge(&raw_width, threshold, right - threshold, -0.5f, -1, &result.x) !=
            REACH_OK)
            return REACH_INVALID_ARGUMENT;
    }
    result.x = reach_motion_clamp(&result.x, fminf(narrow.bounds.x, wide.bounds.x),
                                  fmaxf(narrow.bounds.x, wide.bounds.x));
    reach_motion_set(&result.y, narrow.bounds.y);
    reach_motion_set(&result.height, narrow.bounds.height);
    float clearance = reach_popup_radius_scaled(reach_theme_default(), dpi_scale) +
                      reach_popup_notch_width_scaled(dpi_scale);
    float distance =
        constrained ? fmaxf(clearance, near_left ? owner - left : right - owner) : maximum;
    reach_motion notch = {};
    if (reach_motion_hinge(&raw_width, 2 * distance, distance, 0.5f, near_left ? 0 : 1, &notch) !=
        REACH_OK)
        return REACH_INVALID_ARGUMENT;
    float narrow_notch = reach_popup_clamp_notch_center_scaled(owner - narrow.bounds.x,
                                                               narrow.bounds.width, dpi_scale);
    float wide_notch =
        reach_popup_clamp_notch_center_scaled(owner - wide.bounds.x, wide.bounds.width, dpi_scale);
    notch = reach_motion_clamp(&notch, fminf(narrow_notch, wide_notch),
                               fmaxf(narrow_notch, wide_notch));
    *out_bounds = result;
    *out_notch_center = notch;
    return REACH_OK;
}

float reach_popup_notch_width(void)
{
    return 18.0f;
}

float reach_popup_notch_width_scaled(float dpi_scale)
{
    return reach_popup_scale(reach_popup_notch_width(), dpi_scale);
}

float reach_popup_notch_height(void)
{
    return 8.0f;
}

float reach_popup_notch_height_scaled(float dpi_scale)
{
    return reach_popup_scale(reach_popup_notch_height(), dpi_scale);
}

float reach_popup_clamp_notch_center_scaled(float notch_center_x, float width, float dpi_scale)
{
    float radius = reach_popup_radius_scaled(reach_theme_default(), dpi_scale);
    float notch_width = reach_popup_notch_width_scaled(dpi_scale);
    float min_center = radius + notch_width;
    float max_center = width - radius - notch_width;

    if (max_center < min_center)
    {
        return width * 0.5f;
    }
    if (notch_center_x < min_center)
    {
        return min_center;
    }
    if (notch_center_x > max_center)
    {
        return max_center;
    }
    return notch_center_x;
}

static reach_render_command reach_popup_shape(const reach_popup_background_input *input)
{
    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT;
    shape.rect.width = input->bounds.width;
    shape.rect.height = input->bounds.height;
    shape.radius = reach_popup_radius_scaled(input->theme, input->dpi_scale);
    shape.notch_center_x = reach_popup_clamp_notch_center_scaled(
        input->notch_center_x, input->bounds.width, input->dpi_scale);
    shape.notch_width = reach_popup_notch_width_scaled(input->dpi_scale);
    shape.notch_height = reach_popup_notch_height_scaled(input->dpi_scale);
    shape.notch_side = input->notch_side;
    return shape;
}

static int32_t reach_popup_render_input_valid(const reach_popup_background_input *input,
                                              const reach_render_command_buffer *out_commands)
{
    return input != nullptr && input->theme != nullptr && input->bounds.width > 0.0f &&
           input->bounds.height > 0.0f && out_commands != nullptr;
}

reach_result reach_popup_push_background(const reach_popup_background_input *input,
                                         reach_render_command_buffer *out_commands)
{
    if (!reach_popup_render_input_valid(input, out_commands))
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_render_command shape = reach_popup_shape(input);
    reach_color background = input->has_background_color_override ? input->background_color_override
                                                                  : input->theme->popup_background;
    return reach_render_push_bordered_background(
        out_commands, &shape, background, input->theme->popup_border,
        reach_theme_border_thickness(input->theme, input->dpi_scale), &input->theme->popup_shadow,
        input->dpi_scale);
}

reach_result reach_popup_push_height_background(const reach_popup_background_input *input,
                                                const reach_motion *height, uint64_t parent_id,
                                                uint64_t first_group_id,
                                                reach_render_command_buffer *out_commands)
{
    if (!reach_popup_render_input_valid(input, out_commands) || height == nullptr ||
        height->generation == 0)
        return REACH_INVALID_ARGUMENT;
    reach_shadow_pad pad = reach_theme_shadow_pad(&input->theme->popup_shadow, input->dpi_scale);
    pad.left = ceilf(pad.left);
    pad.right = ceilf(pad.right);
    pad.top = ceilf(pad.top);
    pad.bottom = ceilf(pad.bottom);
    float first = reach_motion_value_at(height, height->started_seconds);
    float last = reach_motion_value_at(height, reach_motion_deadline(height));
    auto commands = std::unique_ptr<reach_render_command_buffer>(new (std::nothrow)
                                                                     reach_render_command_buffer());
    if (!commands)
        return REACH_ERROR;
    reach_render_group source = {};
    source.id = first_group_id;
    source.parent_id = parent_id;
    source.bounds = {-pad.left, -pad.top, ceilf(input->bounds.width + pad.left + pad.right), 0};
    reach_popup_background_input shape = *input;
    float radius = reach_popup_radius_scaled(input->theme, input->dpi_scale);
    float extent = reach_theme_shadow_extent(&input->theme->popup_shadow, input->dpi_scale);
    float notch = reach_popup_notch_height_scaled(input->dpi_scale);
    reach_render_stretch stretch = {};
    stretch.top =
        ceilf(pad.top + radius + extent + (input->notch_side == REACH_NOTCH_SIDE_TOP ? notch : 0));
    stretch.bottom = ceilf(pad.bottom + radius + extent +
                           (input->notch_side == REACH_NOTCH_SIDE_BOTTOM ? notch : 0));
    source.bounds.height = stretch.top + stretch.bottom + 1;
    float source_body_height = source.bounds.height - pad.top - pad.bottom;
    if (first == last && first < source_body_height)
    {
        shape.bounds.height = last;
        source.bounds.height = ceilf(last + pad.top + pad.bottom);
        reach_result result = reach_popup_push_background(&shape, commands.get());
        if (result != REACH_OK)
            return result;
        result = reach_render_command_buffer_begin_group(out_commands, &source);
        if (result != REACH_OK)
            return result;
        for (size_t index = 0; index < commands->count && result == REACH_OK; ++index)
            result = reach_render_command_buffer_push(out_commands, &commands->commands[index]);
        reach_render_command_buffer_end_group(out_commands);
        return result;
    }
    shape.bounds.height = source.bounds.height - pad.top - pad.bottom;
    reach_result result = reach_popup_push_background(&shape, commands.get());
    if (result != REACH_OK)
        return result;
    reach_motion_set(&stretch.width, source.bounds.width);
    stretch.height = reach_motion_affine(height, 1, pad.top + pad.bottom);
    return reach_render_push_stretched_chrome(out_commands, &source, &stretch, commands->commands,
                                              commands->count);
}

static reach_result reach_popup_motion_shape(reach_render_command_buffer *out, uint64_t parent,
                                             uint64_t id, const reach_motion &width,
                                             const reach_motion &center, float height, float inset,
                                             float radius, float notch_width, float notch_height,
                                             int32_t side, reach_color color, float envelope_width)
{
    float minimum = 0, maximum = 0;
    if (reach_motion_range(&width, &minimum, &maximum) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    float body_height = height - notch_height - 2 * inset;
    float max_width = fmaxf(0, fmaxf(maximum, envelope_width) - 2 * inset);
    if (max_width <= 0 || body_height <= 0)
        return REACH_OK;
    auto shape_width = reach_motion_affine(&width, 1, -2 * inset);
    shape_width = reach_motion_clamp(&shape_width, 0, max_width);
    auto half = reach_motion_affine(&shape_width, .5f, 0);
    auto r = reach_motion_clamp(&half, 0, fminf(fmaxf(0, radius - inset), body_height * .5f));
    auto left_limit = reach_motion_affine(&r, 1, inset);
    reach_motion right_limit = {};
    if (reach_motion_combine(&shape_width, 1, &r, -1, inset, &right_limit) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    auto left = reach_motion_affine(&center, 1, -notch_width * .5f);
    auto right = reach_motion_affine(&center, 1, notch_width * .5f);
    if (reach_motion_max(&left, &left_limit, &left) != REACH_OK ||
        reach_motion_min(&right, &right_limit, &right) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    reach_motion span = {}, middle = {};
    if (reach_motion_combine(&right, 1, &left, -1, 0, &span) != REACH_OK ||
        reach_motion_combine(&right, .5f, &left, .5f, 0, &middle) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    span = reach_motion_clamp(&span, 0, notch_width);
    reach_render_group body = {};
    body.id = id;
    body.parent_id = parent;
    body.bounds = {inset, inset + (side == REACH_NOTCH_SIDE_TOP ? notch_height : 0), max_width,
                   body_height};
    reach_rect_motion body_motion = {};
    reach_motion_set(&body_motion.x, body.bounds.x);
    reach_motion_set(&body_motion.y, body.bounds.y);
    body_motion.width = shape_width;
    reach_motion_set(&body_motion.height, body_height);
    reach_result result =
        reach_render_push_rect_motion(out, &body, &body_motion, fmaxf(0, radius - inset), color);
    if (result != REACH_OK || notch_width <= 0 || notch_height <= 0)
        return result;
    float overlap = fminf(1, body_height);
    float factor = 1 + overlap / notch_height;
    float source_width = notch_width * factor;
    reach_render_group notch = {};
    notch.id = id + 2;
    notch.parent_id = parent;
    notch.bounds = {0,
                    side == REACH_NOTCH_SIDE_TOP ? inset : height - inset - notch_height - overlap,
                    source_width, notch_height + overlap};
    if (reach_motion_combine(&middle, 1, &span, -factor * .5f, 0, &notch.x) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    notch.scale = reach_motion_affine(&span, 1 / notch_width, 0);
    reach_motion_set(&notch.scale_y, 1);
    result = reach_render_command_buffer_begin_group(out, &notch);
    reach_render_command triangle = {};
    triangle.type = REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT;
    triangle.rect = notch.bounds;
    triangle.notch_center_x = source_width * .5f;
    triangle.notch_width = source_width;
    triangle.notch_height = notch_height + overlap;
    triangle.notch_side = side;
    triangle.color = color;
    if (result == REACH_OK)
        result = reach_render_command_buffer_push(out, &triangle);
    reach_render_command_buffer_end_group(out);
    return result;
}

reach_result reach_popup_push_width_background(const reach_popup_background_input *input,
                                               const reach_motion *width,
                                               const reach_motion *notch_center, uint64_t parent_id,
                                               uint64_t first_group_id,
                                               reach_render_command_buffer *out_commands)
{
    if (!reach_popup_render_input_valid(input, out_commands) || !width || !notch_center ||
        width->generation == 0 || notch_center->generation == 0 || first_group_id == 0 ||
        out_commands->current_group_id != 0 || !isfinite(input->dpi_scale) ||
        input->dpi_scale <= 0 || !isfinite(input->bounds.width) ||
        out_commands->count > REACH_MAX_RENDER_COMMANDS ||
        out_commands->group_count > REACH_MAX_RENDER_GROUPS ||
        first_group_id >= UINT64_C(0x8000000000000000) - 15)
        return REACH_INVALID_ARGUMENT;
    float minimum = 0, maximum = 0;
    if (reach_motion_range(width, &minimum, &maximum) != REACH_OK || minimum <= 0 ||
        !isfinite(input->bounds.height) || input->bounds.height <= 0)
        return REACH_INVALID_ARGUMENT;
    auto batch = std::unique_ptr<reach_render_command_buffer>(new (std::nothrow)
                                                                  reach_render_command_buffer());
    if (!batch)
        return REACH_ERROR;
    float radius = reach_popup_radius_scaled(input->theme, input->dpi_scale);
    float extent = reach_theme_shadow_extent(&input->theme->popup_shadow, input->dpi_scale);
    float notch_width = reach_popup_notch_width_scaled(input->dpi_scale);
    float notch_height = reach_popup_notch_height_scaled(input->dpi_scale);
    if (input->bounds.height <= notch_height)
        return REACH_INVALID_ARGUMENT;
    float clearance =
        reach_popup_radius_scaled(reach_theme_default(), input->dpi_scale) + notch_width;
    auto near_limit = reach_motion_affine(width, .5f, 0);
    near_limit = reach_motion_clamp(&near_limit, 0, clearance);
    reach_motion far_limit = {}, center = *notch_center;
    if (reach_motion_combine(width, 1, &near_limit, -1, 0, &far_limit) != REACH_OK ||
        reach_motion_max(&center, &near_limit, &center) != REACH_OK ||
        reach_motion_min(&center, &far_limit, &center) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    auto pad = reach_theme_shadow_pad(&input->theme->popup_shadow, input->dpi_scale);
    pad.left = ceilf(pad.left);
    pad.right = ceilf(pad.right);
    pad.top = ceilf(pad.top);
    pad.bottom = ceilf(pad.bottom);
    reach_render_group source = {};
    source.id = first_group_id;
    source.bounds = {-pad.left, -pad.top, 0, ceilf(input->bounds.height + pad.top + pad.bottom)};
    reach_render_stretch stretch = {};
    stretch.left = ceilf(pad.left + radius + extent);
    stretch.right = ceilf(pad.right + radius + extent);
    float focal_width = ceilf(notch_width + extent * 2);
    source.bounds.width = stretch.left + stretch.right + focal_width + 2;
    stretch.focal_left = stretch.left + 1;
    stretch.focal_right = stretch.focal_left + focal_width;
    stretch.focal_x = reach_motion_affine(&center, 1, pad.left);
    stretch.width = reach_motion_affine(width, 1, pad.left + pad.right);
    reach_motion_set(&stretch.height, source.bounds.height);
    reach_popup_background_input shape = *input;
    shape.bounds.width = source.bounds.width - pad.left - pad.right;
    shape.notch_center_x = (stretch.focal_left + stretch.focal_right) * .5f - pad.left;
    auto paint = std::unique_ptr<reach_render_command_buffer>(new (std::nothrow)
                                                                  reach_render_command_buffer());
    if (!paint)
        return REACH_ERROR;
    reach_result result = reach_popup_push_background(&shape, paint.get());
    if (result == REACH_OK)
        result = reach_render_push_stretched_chrome(batch.get(), &source, &stretch, paint->commands,
                                                    paint->count);
    if (result == REACH_INVALID_ARGUMENT)
    {
        reach_render_command_buffer_clear(batch.get());
        source.bounds.width = ceilf(fmaxf(maximum, input->bounds.width) + pad.left + pad.right);
        if (input->theme->popup_shadow.blur > 0 && input->theme->popup_shadow.color.a > 0)
        {
            source.has_shadow = 1;
            source.shadow = input->theme->popup_shadow;
            source.shadow.offset_x *= input->dpi_scale;
            source.shadow.offset_y *= input->dpi_scale;
            source.shadow.blur *= input->dpi_scale;
            result = reach_render_command_buffer_begin_group(batch.get(), &source);
            reach_render_command_buffer_end_group(batch.get());
            if (result == REACH_OK)
                result = reach_popup_motion_shape(batch.get(), source.id, first_group_id + 1,
                                                  *width, center, input->bounds.height, 0, radius,
                                                  notch_width, notch_height, input->notch_side,
                                                  {1, 1, 1, 1}, input->bounds.width);
        }
        else
            result = REACH_OK;
        float border = reach_theme_border_thickness(input->theme, input->dpi_scale);
        reach_color background = input->has_background_color_override
                                     ? input->background_color_override
                                     : input->theme->popup_background;
        background.a = 1;
        reach_color outer = border > 0 && input->theme->popup_border.a > 0
                                ? input->theme->popup_border
                                : background;
        outer.a = 1;
        if (result == REACH_OK)
            result = reach_popup_motion_shape(
                batch.get(), 0, first_group_id + 4, *width, center, input->bounds.height, 0, radius,
                notch_width, notch_height, input->notch_side, outer, input->bounds.width);
        if (result == REACH_OK && border > 0 && input->theme->popup_border.a > 0)
            result = reach_popup_motion_shape(batch.get(), 0, first_group_id + 7, *width, center,
                                              input->bounds.height, border, radius, notch_width,
                                              notch_height, input->notch_side, background,
                                              input->bounds.width);
    }
    if (result != REACH_OK)
        return result;
    uint64_t root_parent = parent_id;
    if (out_commands->has_scissor)
    {
        if (batch->group_count >= REACH_MAX_RENDER_GROUPS)
            return REACH_ERROR;
        reach_render_group clip = {};
        clip.id = first_group_id + 15;
        clip.parent_id = parent_id;
        clip.bounds = {-pad.left, -pad.top,
                       ceilf(fmaxf(maximum, input->bounds.width) + pad.left + pad.right),
                       ceilf(input->bounds.height + pad.top + pad.bottom)};
        clip.has_clip = 1;
        clip.clip = out_commands->scissor_rect;
        for (size_t index = batch->group_count; index > 0; --index)
            batch->groups[index] = batch->groups[index - 1];
        batch->groups[0] = clip;
        ++batch->group_count;
        root_parent = clip.id;
    }
    if (batch->group_count > REACH_MAX_RENDER_GROUPS - out_commands->group_count ||
        batch->count > REACH_MAX_RENDER_COMMANDS - out_commands->count)
        return REACH_ERROR;
    *paint = *out_commands;
    for (size_t index = 0; index < batch->group_count; ++index)
    {
        if (batch->groups[index].parent_id == 0 && (!out_commands->has_scissor || index != 0))
            batch->groups[index].parent_id = root_parent;
        paint->groups[paint->group_count++] = batch->groups[index];
    }
    for (size_t index = 0; index < batch->count; ++index)
        paint->commands[paint->count++] = batch->commands[index];
    result = reach_render_command_buffer_validate_groups(paint.get());
    if (result == REACH_OK)
        *out_commands = *paint;
    return result;
}
