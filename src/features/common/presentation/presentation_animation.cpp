#include "reach/features/common/presentation.h"

static const float REACH_FEATURE_TRANSITION_OFFSET = 8.0f;

static float reach_feature_transition_origin(const reach_feature_transition *transition)
{
    return transition != nullptr && transition->direction == REACH_FEATURE_TRANSITION_FROM_ABOVE
               ? -REACH_FEATURE_TRANSITION_OFFSET
               : REACH_FEATURE_TRANSITION_OFFSET;
}

void reach_feature_transition_init(reach_feature_transition *transition, int32_t direction)
{
    if (transition == nullptr)
        return;
    *transition = {};
    transition->open_seconds = reach_theme_default()->surface_open_seconds;
    transition->close_seconds = reach_theme_default()->surface_close_seconds;
    transition->dpi_scale = 1.0f;
    transition->direction = direction;
    reach_feature_transition_reset(transition);
}

void reach_feature_transition_reset(reach_feature_transition *transition)
{
    if (transition == nullptr)
        return;
    transition->visible = 0;
    transition->target_open = 0;
    transition->completion_seconds = 0.0;
    reach_motion_set(&transition->y, reach_feature_transition_origin(transition));
    reach_motion_set(&transition->opacity, 0.0f);
}

void reach_feature_transition_configure(reach_feature_transition *transition,
                                        const reach_theme *theme, float dpi_scale,
                                        int32_t direction)
{
    if (transition == nullptr)
        return;
    const reach_theme *actual = theme != nullptr ? theme : reach_theme_default();
    transition->open_seconds = actual->surface_open_seconds;
    transition->close_seconds = actual->surface_close_seconds;
    transition->dpi_scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    transition->direction = direction;
    if (!transition->visible)
    {
        reach_motion_set(&transition->y, reach_feature_transition_origin(transition));
    }
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
        reach_motion_set(&transition->y, reach_feature_transition_origin(transition));
        reach_motion_set(&transition->opacity, 0.0f);
    }
    if (transition->visible)
    {
        double seconds = target ? transition->open_seconds : transition->close_seconds;
        reach_easing easing = target ? REACH_EASING_EASE_OUT : REACH_EASING_EASE_IN;
        reach_motion_animate_to(&transition->y,
                                target ? 0.0f : reach_feature_transition_origin(transition),
                                seconds, easing);
        reach_motion_animate_to(&transition->opacity, target ? 1.0f : 0.0f, seconds, easing);
        transition->completion_seconds = reach_motion_deadline(&transition->opacity);
        double y_deadline = reach_motion_deadline(&transition->y);
        if (y_deadline > transition->completion_seconds)
            transition->completion_seconds = y_deadline;
        if (transition->completion_seconds == 0.0)
            transition->completion_seconds = reach_motion_now_seconds();
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
           (reach_motion_active(&transition->y) || reach_motion_active(&transition->opacity));
}

int32_t reach_feature_transition_needs_update(const reach_feature_transition *transition)
{
    return transition != nullptr && transition->completion_seconds > 0.0 &&
           reach_motion_wait_ms(transition->completion_seconds) == 0;
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
    float origin = reach_feature_transition_origin(transition) * transition->dpi_scale;
    geometry->presentation.translation_bounds = {0.0f, origin < 0.0f ? origin : 0.0f, 0.0f,
                                                 origin < 0.0f ? -origin : origin};
    geometry->presentation.group.opacity = transition->opacity;
    geometry->presentation.group.y = transition->y;
    geometry->presentation.group.y.from *= transition->dpi_scale;
    geometry->presentation.group.y.to *= transition->dpi_scale;
}
