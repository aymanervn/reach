#include "reach/features/common/ui_controls.h"

#include "reach/support/util.h"

#include <math.h>

static void reach_ui_push_rect(reach_render_command_buffer *commands, reach_rect_f32 rect,
                               float radius, reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = rect;
    command.radius = radius;
    command.color = color;
    (void)reach_render_command_buffer_push(commands, &command);
}

reach_result reach_ui_feedback_begin(reach_render_command_buffer *commands, uint64_t id,
                                     reach_rect_f32 bounds, const reach_motion *opacity,
                                     const reach_render_group *placement)
{
    if (commands == nullptr || opacity == nullptr)
        return REACH_INVALID_ARGUMENT;
    reach_render_group group = placement != nullptr ? *placement : reach_render_group{};
    group.id = id;
    if (placement == nullptr)
    {
        group.bounds = bounds;
        reach_motion_set(&group.scale, 1.0f);
    }
    group.has_tint = 0;
    group.opacity = *opacity;
    if (group.opacity.generation == 0)
        reach_motion_set(&group.opacity, reach_motion_value(opacity));
    return reach_render_command_buffer_begin_group(commands, &group);
}

reach_result reach_ui_feedback_render(reach_render_command_buffer *commands, uint64_t id,
                                      reach_rect_f32 bounds, float radius, reach_color color,
                                      const reach_motion *opacity,
                                      const reach_render_group *placement)
{
    reach_result result = reach_ui_feedback_begin(commands, id, bounds, opacity, placement);
    if (result != REACH_OK)
        return result;
    reach_render_command overlay = {};
    overlay.type = REACH_RENDER_COMMAND_RECT;
    overlay.rect = bounds;
    overlay.radius = radius;
    overlay.color = color;
    result = reach_render_command_buffer_push(commands, &overlay);
    reach_render_command_buffer_end_group(commands);
    return result;
}

static reach_render_command reach_ui_text_command(reach_rect_f32 rect, const uint16_t *text,
                                                  float size, int32_t weight, int32_t alignment,
                                                  reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = rect;
    command.text_size = size;
    command.text_weight = weight;
    command.text_alignment = alignment;
    command.text_ellipsis = 1;
    command.color = color;
    reach_copy_utf16(command.text, 260, text);
    return command;
}

static void reach_ui_push_text(reach_render_command_buffer *commands, reach_rect_f32 rect,
                               const uint16_t *text, float size, int32_t weight, int32_t alignment,
                               reach_color color)
{
    reach_render_command command =
        reach_ui_text_command(rect, text, size, weight, alignment, color);
    (void)reach_render_command_buffer_push(commands, &command);
}

void reach_ui_color_feedback_apply(reach_render_group *group, reach_color from, reach_color to,
                                   const reach_motion *progress)
{
    group->has_tint = 1;
    float source[] = {from.r, from.g, from.b, from.a};
    float target[] = {to.r, to.g, to.b, to.a};
    for (size_t index = 0; index < REACH_RENDER_COLOR_CHANNEL_COUNT; ++index)
        group->tint[index] =
            reach_motion_affine(progress, target[index] - source[index], source[index]);
}

static reach_render_group reach_ui_color_group(uint64_t id, reach_rect_f32 bounds, reach_color from,
                                               reach_color to, const reach_motion *progress)
{
    reach_render_group group = {};
    group.id = id;
    group.bounds = bounds;
    reach_ui_color_feedback_apply(&group, from, to, progress);
    reach_motion_set(&group.opacity, 1.0f);
    reach_motion_set(&group.scale, 1.0f);
    return group;
}

reach_result reach_ui_motion_background_render(reach_render_command_buffer *commands,
                                               const reach_rect_motion *bounds, float radius,
                                               float border, reach_color background,
                                               reach_color outline, const reach_shadow *shadow,
                                               float dpi_scale, uint64_t first_id,
                                               uint64_t parent_id)
{
    if (commands == nullptr || bounds == nullptr || commands->current_group_id != 0 ||
        first_id == 0 || first_id >= UINT64_C(0x8000000000000000) - 7 || !isfinite(dpi_scale) ||
        dpi_scale <= 0 || !isfinite(border) || border < 0 || !isfinite(radius) || radius < 0)
        return REACH_INVALID_ARGUMENT;
    float left = 0, right = 0, top = 0, bottom = 0, minimum = 0, width = 0, height = 0;
    if (reach_motion_range(&bounds->x, &left, &right) != REACH_OK ||
        reach_motion_range(&bounds->y, &top, &bottom) != REACH_OK ||
        reach_motion_range(&bounds->width, &minimum, &width) != REACH_OK || minimum < 0 ||
        reach_motion_range(&bounds->height, &minimum, &height) != REACH_OK || minimum < 0)
        return REACH_INVALID_ARGUMENT;
    if (width <= 0 || height <= 0)
        return REACH_OK;
    if (commands->count > REACH_MAX_RENDER_COMMANDS - 3 ||
        commands->group_count > REACH_MAX_RENDER_GROUPS - 8)
        return REACH_ERROR;
    for (size_t index = 0; index < commands->group_count; ++index)
        if (commands->groups[index].id >= first_id && commands->groups[index].id < first_id + 8)
            return REACH_INVALID_ARGUMENT;
    size_t first_group = commands->group_count, first_command = commands->count;
    int32_t scissored = commands->has_scissor;
    bool casts = shadow != nullptr && shadow->blur > 0 && shadow->color.a > 0;
    if (casts && reach_render_command_buffer_group_shadow(commands, parent_id) != nullptr)
        return REACH_INVALID_ARGUMENT;
    reach_shadow_pad pad = {};
    if (casts)
        pad = reach_theme_shadow_pad(shadow, dpi_scale);
    reach_rect_f32 capture = {floorf(left - pad.left), floorf(top - pad.top), 0, 0};
    capture.width = ceilf(right + width + pad.right) - capture.x;
    capture.height = ceilf(bottom + height + pad.bottom) - capture.y;
    reach_result result = REACH_OK;
    if (scissored)
    {
        reach_render_group clip = {};
        clip.id = first_id + 7;
        clip.parent_id = parent_id;
        clip.bounds = capture;
        clip.has_clip = 1;
        clip.clip = commands->scissor_rect;
        result = reach_render_command_buffer_begin_group(commands, &clip);
        reach_render_command_buffer_end_group(commands);
        parent_id = clip.id;
        commands->has_scissor = 0;
    }
    bool fixed = left == right && top == bottom;
    float width_minimum = 0, height_minimum = 0, ignored = 0;
    fixed = fixed && reach_motion_range(&bounds->width, &width_minimum, &ignored) == REACH_OK &&
            reach_motion_range(&bounds->height, &height_minimum, &ignored) == REACH_OK &&
            width_minimum == width && height_minimum == height;
    if (result == REACH_OK && fixed)
    {
        reach_render_group background_source = {};
        background_source.id = first_id;
        background_source.parent_id = parent_id;
        background_source.bounds = capture;
        result = reach_render_command_buffer_begin_group(commands, &background_source);
        reach_render_command shape = {};
        shape.type = REACH_RENDER_COMMAND_RECT;
        shape.rect = reach_rect_motion_value_at(bounds, 0);
        shape.radius = radius;
        if (result == REACH_OK)
            result = reach_render_push_bordered_background(commands, &shape, background, outline,
                                                           border, shadow, dpi_scale);
        reach_render_command_buffer_end_group(commands);
        commands->has_scissor = scissored;
        if (result != REACH_OK)
        {
            commands->group_count = first_group;
            commands->count = first_command;
        }
        return result;
    }
    reach_render_group source = {};
    source.bounds = {0, 0, ceilf(width), ceilf(height)};
    if (result == REACH_OK && casts)
    {
        reach_render_group caster = {};
        caster.id = first_id;
        caster.parent_id = parent_id;
        caster.bounds = capture;
        caster.has_shadow = 1;
        caster.shadow = *shadow;
        caster.shadow.offset_x *= dpi_scale;
        caster.shadow.offset_y *= dpi_scale;
        caster.shadow.blur *= dpi_scale;
        result = reach_render_command_buffer_begin_group(commands, &caster);
        reach_render_command_buffer_end_group(commands);
        source.id = first_id + 1;
        source.parent_id = caster.id;
        if (result == REACH_OK)
            result = reach_render_push_rect_motion(commands, &source, bounds, radius, {1, 1, 1, 1});
    }
    reach_rect_motion paint = *bounds;
    auto aligned = reach_render_pixel_aligned_rect(reach_rect_motion_value_at(bounds, 0));
    auto align_axis = [](reach_motion &position, reach_motion &extent, float near, float size)
    {
        float near_minimum = 0, near_maximum = 0, size_minimum = 0, size_maximum = 0;
        if (reach_motion_range(&position, &near_minimum, &near_maximum) == REACH_OK &&
            reach_motion_range(&extent, &size_minimum, &size_maximum) == REACH_OK &&
            near_minimum == near_maximum && size_minimum == size_maximum)
        {
            reach_motion_set(&position, near);
            reach_motion_set(&extent, size);
        }
    };
    align_axis(paint.x, paint.width, aligned.x, aligned.width);
    align_axis(paint.y, paint.height, aligned.y, aligned.height);
    float paint_width = 0, paint_height = 0;
    (void)reach_motion_range(&paint.width, &minimum, &paint_width);
    (void)reach_motion_range(&paint.height, &minimum, &paint_height);
    source.bounds.width = fmaxf(1, ceilf(paint_width));
    source.bounds.height = fmaxf(1, ceilf(paint_height));
    bool bordered = border > 0 && outline.a > 0;
    background.a = outline.a = 1;
    source.id = first_id + 3;
    source.parent_id = parent_id;
    if (result == REACH_OK)
        result = reach_render_push_rect_motion(commands, &source, &paint, radius,
                                               bordered ? outline : background);
    if (result == REACH_OK && bordered && paint_width > border * 2 && paint_height > border * 2)
    {
        reach_rect_motion inner = paint;
        inner.x = reach_motion_affine(&inner.x, 1, border);
        inner.y = reach_motion_affine(&inner.y, 1, border);
        inner.width = reach_motion_affine(&inner.width, 1, -border * 2);
        inner.width = reach_motion_clamp(&inner.width, 0, paint_width - border * 2);
        inner.height = reach_motion_affine(&inner.height, 1, -border * 2);
        inner.height = reach_motion_clamp(&inner.height, 0, paint_height - border * 2);
        source.id = first_id + 5;
        source.bounds.width = ceilf(paint_width - border * 2);
        source.bounds.height = ceilf(paint_height - border * 2);
        result = reach_render_push_rect_motion(commands, &source, &inner, fmaxf(0, radius - border),
                                               background);
    }
    commands->has_scissor = scissored;
    if (result != REACH_OK)
    {
        commands->group_count = first_group;
        commands->count = first_command;
    }
    return result;
}

int32_t reach_ui_color_feedback_begin(reach_render_command_buffer *commands, uint64_t id,
                                      reach_rect_f32 bounds, reach_color from, reach_color to,
                                      const reach_motion *pressed)
{
    if (pressed == nullptr)
        return 0;
    reach_render_group group = reach_ui_color_group(id, bounds, from, to, pressed);
    return reach_render_command_buffer_begin_group(commands, &group) == REACH_OK;
}

reach_result reach_ui_bordered_color_feedback_render(reach_render_command_buffer *commands,
                                                     uint64_t first_id, reach_rect_f32 bounds,
                                                     float radius, float border, reach_color from,
                                                     reach_color to, reach_color outline,
                                                     const reach_motion *progress)
{
    if (commands == nullptr || progress == nullptr || first_id == 0 ||
        first_id > UINT64_C(0x7ffffffffffffffb))
        return REACH_INVALID_ARGUMENT;
    if (bounds.width <= 0 || bounds.height <= 0)
        return REACH_OK;
    uint64_t parent_id = commands->current_group_id;
    reach_motion hover = reach_motion_clamp(progress, 0, 1);
    from.a = to.a = 1;
    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_RECT;
    shape.rect = bounds;
    shape.radius = radius;
    reach_render_command shapes[2];
    size_t count =
        reach_render_bordered_background_shapes(&shape, {1, 1, 1, 1}, {1, 1, 1, 1}, border, shapes);
    for (size_t index = 0; index < count; ++index)
    {
        reach_rect_f32 source = {};
        if (!reach_render_retained_command_bounds(&shapes[index], &source))
            return REACH_INVALID_ARGUMENT;
        reach_render_group group = reach_ui_color_group(first_id + index, source, from, to, &hover);
        group.parent_id = parent_id;
        if (index == 0 && border > 0)
        {
            group.tint_rgb_target = outline;
            reach_motion weight = reach_motion_affine(&hover, outline.a, 0);
            group.tint_rgb_mix = reach_motion_clamp(&weight, 0, 1);
        }
        reach_result result = reach_render_command_buffer_begin_group(commands, &group);
        if (result != REACH_OK)
            return result;
        result = reach_render_command_buffer_push(commands, &shapes[index]);
        reach_render_command_buffer_end_group(commands);
        commands->current_group_id = parent_id;
        if (result != REACH_OK)
            return result;
    }
    return REACH_OK;
}

void reach_ui_button_render(reach_render_command_buffer *commands, reach_rect_f32 bounds,
                            const uint16_t *label, const reach_ui_button_style *style,
                            int32_t enabled, uint64_t feedback_id, const reach_motion *pressed)
{
    if (commands == nullptr || style == nullptr)
    {
        return;
    }
    reach_color background = enabled ? style->background : style->disabled_background;
    const float darken = style->pressed_darken > 0.0f ? style->pressed_darken : 1.0f;
    reach_color target = style->has_pressed_colors
                             ? style->pressed_background
                             : reach_color{background.r * darken, background.g * darken,
                                           background.b * darken, background.a};
    const reach_motion *feedback = enabled || style->has_pressed_colors ? pressed : nullptr;
    int32_t retained =
        reach_ui_color_feedback_begin(commands, feedback_id, bounds, background, target, feedback);
    reach_color white = {1.0f, 1.0f, 1.0f, 1.0f};
    reach_ui_push_rect(commands, bounds, style->radius, retained ? white : background);
    if (retained)
        reach_render_command_buffer_end_group(commands);
    reach_color text = enabled ? style->text : style->disabled_text;
    retained = style->has_pressed_colors &&
               reach_ui_color_feedback_begin(commands, feedback_id + 1, bounds, text,
                                             style->pressed_text, feedback);
    reach_ui_push_text(commands, bounds, label, style->text_size, style->text_weight,
                       REACH_TEXT_ALIGNMENT_CENTER, retained ? white : text);
    if (retained)
        reach_render_command_buffer_end_group(commands);
}

void reach_ui_selection_item_backdrop_render(reach_render_command_buffer *commands,
                                             reach_rect_f32 bounds,
                                             const reach_ui_selection_item_style *style,
                                             float selection)
{
    if (commands == nullptr || style == nullptr)
    {
        return;
    }
    float radius = bounds.height * 0.5f;
    if (selection <= 0.0f || style->border_width <= 0.0f)
    {
        reach_ui_push_rect(commands, bounds, radius, style->background);
        return;
    }

    reach_color background =
        reach_theme_color_mix(style->background, style->accent, 0.22f * selection);
    reach_color border = reach_theme_color_mix(background, style->accent, 0.85f * selection);
    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_RECT;
    shape.rect = bounds;
    shape.radius = radius;
    (void)reach_render_push_bordered_background(commands, &shape, background, border,
                                                style->border_width, nullptr, 1.0f);
}

void reach_ui_selection_item_render(reach_render_command_buffer *commands, reach_rect_f32 bounds,
                                    const uint16_t *label,
                                    const reach_ui_selection_item_style *style, float selection)
{
    if (commands == nullptr || style == nullptr)
    {
        return;
    }
    reach_ui_selection_item_backdrop_render(commands, bounds, style, selection);
    reach_ui_push_text(commands, bounds, label, style->text_size, style->text_weight,
                       REACH_TEXT_ALIGNMENT_CENTER,
                       reach_theme_color_mix(style->text, style->accent, selection));
}

reach_rect_f32 reach_ui_segmented_control_item_bounds(reach_rect_f32 bounds, size_t item_count,
                                                      size_t index)
{
    if (item_count == 0 || index >= item_count || bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return {};
    }
    float item_width = bounds.width / (float)item_count;
    return {bounds.x + item_width * (float)index, bounds.y, item_width, bounds.height};
}

int32_t reach_ui_segmented_control_index_at(reach_rect_f32 bounds, size_t item_count, float x,
                                            float y)
{
    if (item_count == 0 || bounds.width <= 0.0f || bounds.height <= 0.0f || x < bounds.x ||
        x >= bounds.x + bounds.width || y < bounds.y || y >= bounds.y + bounds.height)
    {
        return -1;
    }
    size_t index = (size_t)((x - bounds.x) * (float)item_count / bounds.width);
    return index < item_count ? (int32_t)index : -1;
}

reach_result reach_ui_segmented_control_motion_render(reach_render_command_buffer *commands,
                                                      reach_rect_f32 bounds,
                                                      const uint16_t *const *labels,
                                                      size_t item_count,
                                                      const reach_ui_segmented_control_style *style,
                                                      uint64_t id, const reach_motion *position)
{
    if (commands == nullptr || labels == nullptr || item_count == 0 || style == nullptr ||
        position == nullptr || id == 0 || id > UINT64_C(0x7fffffffffffffff) - item_count ||
        bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return REACH_INVALID_ARGUMENT;
    }
    uint64_t parent = commands->current_group_id;
    reach_motion progress = reach_motion_clamp(position, 0, (float)(item_count - 1));
    float radius = bounds.height * 0.5f;
    reach_ui_push_rect(commands, bounds, radius, style->track);
    reach_rect_f32 selected = reach_ui_segmented_control_item_bounds(bounds, item_count, 0);
    float maximum_inset = fminf(selected.width, selected.height) * 0.5f;
    float inset = style->selection_inset;
    if (inset < 0.0f)
    {
        inset = 0.0f;
    }
    if (inset > maximum_inset)
    {
        inset = maximum_inset;
    }
    selected.x += inset;
    selected.y += inset;
    selected.width -= inset * 2.0f;
    selected.height -= inset * 2.0f;
    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_RECT;
    shape.rect = selected;
    shape.radius = selected.height * 0.5f;
    shape.color = style->selection;
    reach_rect_f32 source = {};
    if (!reach_render_retained_command_bounds(&shape, &source))
        return REACH_INVALID_ARGUMENT;
    reach_render_group selection = {};
    selection.id = id;
    selection.parent_id = parent;
    selection.bounds = source;
    selection.x = reach_motion_affine(&progress, bounds.width / (float)item_count, 0);
    reach_result result = reach_render_command_buffer_begin_group(commands, &selection);
    if (result != REACH_OK)
        return result;
    result = reach_render_command_buffer_push(commands, &shape);
    reach_render_command_buffer_end_group(commands);
    commands->current_group_id = parent;
    if (result != REACH_OK)
        return result;
    for (size_t index = 0; index < item_count; ++index)
    {
        reach_motion weight = {};
        if (index == 0 || index + 1 == item_count)
        {
            weight = index == 0 ? reach_motion_affine(position, -1, 1)
                                : reach_motion_affine(position, 1, 2 - (float)item_count);
            weight = reach_motion_clamp(&weight, 0, 1);
        }
        else
        {
            result = reach_motion_peak(position, (float)index, 1, &weight);
            if (result != REACH_OK)
                return result;
        }
        reach_rect_f32 item = reach_ui_segmented_control_item_bounds(bounds, item_count, index);
        reach_render_command text =
            reach_ui_text_command(item, labels[index], style->text_size, style->text_weight,
                                  REACH_TEXT_ALIGNMENT_CENTER, {1, 1, 1, 1});
        if (!reach_render_retained_command_bounds(&text, &source))
            return REACH_INVALID_ARGUMENT;
        reach_render_group group = reach_ui_color_group(id + 1 + index, source, style->text,
                                                        style->selected_text, &weight);
        group.parent_id = parent;
        result = reach_render_command_buffer_begin_group(commands, &group);
        if (result != REACH_OK)
            return result;
        result = reach_render_command_buffer_push(commands, &text);
        reach_render_command_buffer_end_group(commands);
        commands->current_group_id = parent;
        if (result != REACH_OK)
            return result;
    }
    return REACH_OK;
}

void reach_ui_toggle_render(reach_render_command_buffer *commands, reach_rect_f32 bounds,
                            const reach_ui_toggle_style *style, float position)
{
    if (commands == nullptr || style == nullptr)
    {
        return;
    }
    if (position < 0.0f)
    {
        position = 0.0f;
    }
    if (position > 1.0f)
    {
        position = 1.0f;
    }
    float radius = bounds.height * 0.5f;
    reach_ui_push_rect(commands, bounds, radius,
                       reach_theme_color_mix(style->track_off, style->track_on, position));
    float inset = bounds.height * 0.15f;
    float knob = bounds.height - inset * 2.0f;
    float knob_off_x = bounds.x + inset;
    float knob_on_x = bounds.x + bounds.width - inset - knob;
    float knob_x = knob_off_x + (knob_on_x - knob_off_x) * position;
    reach_ui_push_rect(commands, {knob_x, bounds.y + inset, knob, knob}, knob * 0.5f, style->knob);
}

reach_result reach_ui_toggle_motion_render(reach_render_command_buffer *commands,
                                           reach_rect_f32 bounds,
                                           const reach_ui_toggle_style *style, uint64_t id,
                                           const reach_motion *position)
{
    if (commands == nullptr || style == nullptr || position == nullptr || id == 0 ||
        id > UINT64_MAX - 2)
        return REACH_INVALID_ARGUMENT;
    float inset = bounds.height * 0.15f;
    float diameter = bounds.height - inset * 2.0f;
    reach_rect_f32 knob_bounds = {bounds.x + inset, bounds.y + inset, diameter, diameter};
    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_RECT;
    shape.rect = bounds;
    reach_rect_f32 track_source = {}, knob_source = {};
    if (!reach_render_retained_command_bounds(&shape, &track_source))
        return REACH_INVALID_ARGUMENT;
    shape.rect = knob_bounds;
    if (!reach_render_retained_command_bounds(&shape, &knob_source))
        return REACH_INVALID_ARGUMENT;
    uint64_t parent = commands->current_group_id;
    int32_t clipped = commands->has_scissor;
    reach_rect_f32 clip = commands->scissor_rect;
    reach_motion progress = reach_motion_clamp(position, 0, 1);
    reach_render_group viewport = {};
    viewport.id = id;
    viewport.parent_id = parent;
    viewport.bounds = bounds;
    viewport.has_clip = clipped;
    viewport.clip = clip;
    commands->current_group_id = 0;
    reach_result result = reach_render_command_buffer_begin_group(commands, &viewport);
    reach_render_command_buffer_end_group(commands);
    if (result != REACH_OK)
    {
        commands->current_group_id = parent;
        return result;
    }
    reach_render_command_buffer_clear_scissor(commands);
    reach_render_group track =
        reach_ui_color_group(id + 1, track_source, style->track_off, style->track_on, &progress);
    track.parent_id = id;
    result = reach_render_command_buffer_begin_group(commands, &track);
    if (result == REACH_OK)
    {
        reach_ui_push_rect(commands, bounds, bounds.height * 0.5f, {1, 1, 1, 1});
        reach_render_command_buffer_end_group(commands);
        reach_render_group knob = {};
        knob.id = id + 2;
        knob.parent_id = id;
        knob.bounds = knob_source;
        knob.x = reach_motion_affine(&progress, bounds.width - inset * 2.0f - diameter, 0);
        result = reach_render_command_buffer_begin_group(commands, &knob);
        if (result == REACH_OK)
        {
            reach_ui_push_rect(commands, knob_bounds, diameter * 0.5f, style->knob);
            reach_render_command_buffer_end_group(commands);
        }
    }
    commands->current_group_id = parent;
    if (clipped)
        reach_render_command_buffer_set_scissor(commands, clip);
    return result;
}

void reach_ui_textbox_render(reach_render_command_buffer *commands, reach_rect_f32 bounds,
                             const reach_ui_selection_item_style *style, float selection,
                             const reach_ui_textbox_state *state)
{
    if (commands == nullptr || style == nullptr || state == nullptr)
    {
        return;
    }
    reach_rect_f32 box = bounds;
    if (state->suffix != nullptr && state->suffix_width > 0.0f &&
        state->suffix_width < bounds.width)
    {
        box.width -= state->suffix_width;
        reach_rect_f32 suffix_rect = {bounds.x + box.width, bounds.y, state->suffix_width,
                                      bounds.height};
        reach_ui_push_text(commands, suffix_rect, state->suffix, style->text_size,
                           style->text_weight, REACH_TEXT_ALIGNMENT_LEADING, state->suffix_color);
    }

    reach_color transparent = {};
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXTBOX;
    command.rect = box;
    command.color = transparent;
    command.radius = 0.0f;
    command.text_size = style->text_size;
    command.text_weight = style->text_weight;
    command.text_alignment = state->text_alignment;
    command.text_color = reach_theme_color_mix(state->text_color, style->accent, selection);
    command.placeholder_color = state->placeholder_color;
    command.selection_color = state->selection_color;
    command.caret_index = state->caret_index;
    command.caret_visible = state->caret_visible;
    command.selection_start = state->selection_start;
    command.selection_end = state->selection_end;
    reach_copy_utf16(command.text, 260, state->text);
    reach_copy_utf16(command.placeholder, 128, state->placeholder);
    auto overlaps = [&](reach_rect_f32 viewport)
    {
        return box.x < viewport.x + viewport.width && box.x + box.width > viewport.x &&
               box.y < viewport.y + viewport.height && box.y + box.height > viewport.y;
    };
    bool retained_caret = state->caret != nullptr && state->caret->deadline_seconds > 0 &&
                          box.width > 0 && box.height > 0 &&
                          (state->caret_viewport.width <= 0 || overlaps(state->caret_viewport)) &&
                          (!commands->has_scissor || overlaps(commands->scissor_rect));
    reach_motion opacity = reach_blink_opacity(state->caret);
    (void)reach_render_push_textbox(commands, &command, state->caret_id,
                                    retained_caret ? &opacity : nullptr);
}
