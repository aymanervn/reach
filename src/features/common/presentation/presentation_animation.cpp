#include "reach/features/common/presentation.h"

#include <math.h>

typedef struct reach_feature_presentation_shape
{
    float open_scale;
    float close_scale;
    float open_offset;
    float close_offset;
    reach_motion_preset fade_in;
    reach_motion_preset fade_out;
} reach_feature_presentation_shape;

static const reach_feature_presentation_shape REACH_FEATURE_PRESENTATION_SHAPES[] = {
    {0.94f, 0.97f, 6.0f, 4.0f, REACH_MOTION_PRESET_FADE_IN, REACH_MOTION_PRESET_FADE_OUT},
    {0.98f, 0.99f, 0.0f, 0.0f, REACH_MOTION_PRESET_MENU_FADE, REACH_MOTION_PRESET_MENU_FADE},
};

static const reach_feature_presentation_shape *
reach_feature_transition_shape(const reach_feature_transition *transition)
{
    return &REACH_FEATURE_PRESENTATION_SHAPES[transition->style == REACH_FEATURE_PRESENTATION_MENU
                                                  ? REACH_FEATURE_PRESENTATION_MENU
                                                  : REACH_FEATURE_PRESENTATION_POPUP];
}

static float reach_feature_transition_sign(const reach_feature_transition *transition)
{
    return transition->direction == REACH_FEATURE_TRANSITION_FROM_ABOVE ? -1.0f : 1.0f;
}

static void reach_feature_transition_apply_theme(reach_feature_transition *transition,
                                                 const reach_theme *theme)
{
    const reach_feature_presentation_shape *shape = reach_feature_transition_shape(transition);
    transition->spring = reach_theme_motion_preset(theme, REACH_MOTION_PRESET_SNAPPY);
    transition->fade_in = reach_theme_motion_preset(theme, shape->fade_in);
    transition->fade_out = reach_theme_motion_preset(theme, shape->fade_out);
}

static void reach_feature_transition_rest_at_origin(reach_feature_transition *transition)
{
    const reach_feature_presentation_shape *shape = reach_feature_transition_shape(transition);
    reach_motion_set(&transition->scale, shape->open_scale);
    reach_motion_set(&transition->offset,
                     shape->open_offset * reach_feature_transition_sign(transition));
    reach_motion_set(&transition->opacity, 0.0f);
}

void reach_feature_transition_init(reach_feature_transition *transition, int32_t style,
                                   int32_t direction)
{
    if (transition == nullptr)
        return;
    *transition = {};
    transition->style = style;
    transition->direction = direction;
    transition->dpi_scale = 1.0f;
    reach_feature_transition_apply_theme(transition, nullptr);
    reach_feature_transition_reset(transition);
}

void reach_feature_transition_reset(reach_feature_transition *transition)
{
    if (transition == nullptr)
        return;
    transition->visible = 0;
    transition->target_open = 0;
    transition->completion_seconds = 0.0;
    reach_feature_transition_rest_at_origin(transition);
}

void reach_feature_transition_configure(reach_feature_transition *transition,
                                        const reach_theme *theme, float dpi_scale,
                                        int32_t direction)
{
    if (transition == nullptr)
        return;
    reach_feature_transition_apply_theme(transition, theme);
    transition->dpi_scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    transition->direction = direction;
    if (!transition->visible)
        reach_feature_transition_rest_at_origin(transition);
}

void reach_feature_transition_set_anchor(reach_feature_transition *transition, float x, float y)
{
    if (transition == nullptr || !isfinite(x) || !isfinite(y))
        return;
    transition->anchor_x = x;
    transition->anchor_y = y;
    transition->has_anchor = 1;
}

void reach_feature_transition_set_trigger(reach_feature_transition *transition,
                                          reach_rect_f32 surface_bounds, float trigger_x)
{
    if (transition == nullptr)
        return;
    float width = fmaxf(0.0f, surface_bounds.width);
    reach_feature_transition_set_anchor(transition,
                                        fminf(fmaxf(trigger_x - surface_bounds.x, 0.0f), width),
                                        transition->direction == REACH_FEATURE_TRANSITION_FROM_ABOVE
                                            ? 0.0f
                                            : fmaxf(0.0f, surface_bounds.height));
}

int32_t reach_feature_transition_set_open(reach_feature_transition *transition, int32_t open)
{
    if (transition == nullptr)
        return 0;
    int32_t target = open ? 1 : 0;
    if (transition->target_open == target &&
        (target || !transition->visible || reach_feature_transition_active(transition)))
        return 0;
    transition->target_open = target;
    if (target && !transition->visible)
    {
        transition->visible = 1;
        reach_feature_transition_rest_at_origin(transition);
    }
    if (transition->visible)
    {
        const reach_feature_presentation_shape *shape = reach_feature_transition_shape(transition);
        double now_seconds = reach_motion_now_seconds();
        reach_motion_animate_spec_to_at(&transition->scale, target ? 1.0f : shape->close_scale,
                                        transition->spring, now_seconds);
        reach_motion_animate_spec_to_at(
            &transition->offset,
            target ? 0.0f : shape->close_offset * reach_feature_transition_sign(transition),
            transition->spring, now_seconds);
        reach_motion_animate_spec_to_at(&transition->opacity, target ? 1.0f : 0.0f,
                                        target ? transition->fade_in : transition->fade_out,
                                        now_seconds);
        transition->completion_seconds = reach_motion_deadline(&transition->opacity);
        if (transition->completion_seconds == 0.0)
            transition->completion_seconds = now_seconds;
    }
    return 1;
}

int32_t reach_feature_transition_tick(reach_feature_transition *transition, double delta_seconds)
{
    (void)delta_seconds;
    if (!reach_feature_transition_needs_update(transition))
        return 0;
    transition->completion_seconds = 0.0;
    if (!transition->target_open)
        transition->visible = 0;
    return 1;
}

int32_t reach_feature_transition_visible(const reach_feature_transition *transition)
{
    return transition != nullptr && transition->visible;
}

int32_t reach_feature_transition_active(const reach_feature_transition *transition)
{
    return transition != nullptr &&
           (reach_motion_active(&transition->scale) || reach_motion_active(&transition->offset) ||
            reach_motion_active(&transition->opacity));
}

int32_t reach_feature_transition_needs_update(const reach_feature_transition *transition)
{
    return transition != nullptr && transition->completion_seconds > 0.0 &&
           reach_motion_wait_ms(transition->completion_seconds) == 0;
}

double reach_feature_transition_exit_seconds(const reach_feature_transition *transition)
{
    return transition != nullptr && transition->fade_out.easing != REACH_EASING_SPRING
               ? transition->fade_out.duration_seconds
               : 0.0;
}

void reach_feature_transition_presentation(const reach_feature_transition *transition,
                                           reach_feature_surface_geometry *geometry)
{
    if (transition == nullptr || geometry == nullptr)
        return;
    const reach_feature_presentation_shape *shape = reach_feature_transition_shape(transition);
    geometry->deadline_seconds = transition->completion_seconds;
    geometry->wait_for_presentation = !transition->target_open;
    geometry->presentation.retained = 1;
    geometry->presentation.max_scale = 1.0f;
    float reach = fmaxf(shape->open_offset, shape->close_offset) * transition->dpi_scale *
                  reach_feature_transition_sign(transition);
    geometry->presentation.translation_bounds = {0.0f, fminf(reach, 0.0f), 0.0f, fabsf(reach)};
    geometry->presentation.group.opacity = transition->opacity;
    geometry->presentation.group.scale = transition->scale;
    geometry->presentation.group.y =
        reach_motion_affine(&transition->offset, transition->dpi_scale, 0.0f);
    reach_rect_f32 bounds = geometry->visible_bounds;
    float anchor_x = transition->has_anchor ? transition->anchor_x : bounds.width * 0.5f;
    float anchor_y = transition->has_anchor ? transition->anchor_y
                     : transition->direction == REACH_FEATURE_TRANSITION_FROM_ABOVE ? 0.0f
                                                                                    : bounds.height;
    reach_motion_set(&geometry->presentation.center_x, anchor_x);
    reach_motion_set(&geometry->presentation.center_y, anchor_y);
}
