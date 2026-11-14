#include "reach/features/common/presentation.h"

#include <math.h>

static void reach_feature_transition_apply_theme(reach_feature_transition *transition,
                                                 const reach_theme *theme)
{
    transition->fade_in = reach_theme_motion_preset(theme, REACH_MOTION_PRESET_CARD_FADE_IN);
    transition->fade_out = reach_theme_motion_preset(theme, REACH_MOTION_PRESET_CARD_FADE_OUT);
}

void reach_feature_transition_init(reach_feature_transition *transition, int32_t direction)
{
    if (transition == nullptr)
        return;
    *transition = {};
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
    reach_motion_set(&transition->opacity, 0.0f);
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
        reach_motion_set(&transition->opacity, 0.0f);
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
        reach_motion_set(&transition->opacity, 0.0f);
    }
    if (transition->visible)
    {
        double now_seconds = reach_motion_now_seconds();
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
    return transition != nullptr && reach_motion_active(&transition->opacity);
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
    geometry->deadline_seconds = transition->completion_seconds;
    geometry->wait_for_presentation = !transition->target_open;
    geometry->presentation.retained = 1;
    geometry->presentation.max_scale = 1.0f;
    geometry->presentation.translation_bounds = {};
    geometry->presentation.group.opacity = transition->opacity;
    reach_motion_set(&geometry->presentation.group.scale, 1.0f);
    reach_motion_set(&geometry->presentation.group.y, 0.0f);
    reach_rect_f32 bounds = geometry->visible_bounds;
    float anchor_x = transition->has_anchor ? transition->anchor_x : bounds.width * 0.5f;
    float anchor_y = transition->has_anchor ? transition->anchor_y
                     : transition->direction == REACH_FEATURE_TRANSITION_FROM_ABOVE ? 0.0f
                                                                                    : bounds.height;
    reach_motion_set(&geometry->presentation.center_x, anchor_x);
    reach_motion_set(&geometry->presentation.center_y, anchor_y);
}
