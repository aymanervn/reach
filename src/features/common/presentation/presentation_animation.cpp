#include "reach/features/common/presentation.h"

enum
{
    REACH_FEATURE_TRANSITION_Y = 0,
    REACH_FEATURE_TRANSITION_OPACITY = 1
};

static const float REACH_FEATURE_TRANSITION_OFFSET = 8.0f;

static float reach_feature_transition_origin(const reach_feature_transition *transition)
{
    return transition != nullptr &&
                   transition->direction == REACH_FEATURE_TRANSITION_FROM_ABOVE
               ? -REACH_FEATURE_TRANSITION_OFFSET
               : REACH_FEATURE_TRANSITION_OFFSET;
}

void reach_feature_transition_init(reach_feature_transition *transition, int32_t direction)
{
    if (transition == nullptr)
    {
        return;
    }
    *transition = {};
    reach_animation_manager_init(&transition->animations, transition->tracks, 2);
    transition->open_seconds = reach_theme_default()->surface_open_seconds;
    transition->close_seconds = reach_theme_default()->surface_close_seconds;
    transition->dpi_scale = 1.0f;
    transition->direction = direction;
    reach_feature_transition_reset(transition);
}

void reach_feature_transition_reset(reach_feature_transition *transition)
{
    if (transition == nullptr)
    {
        return;
    }
    transition->visible = 0;
    transition->target_open = 0;
    reach_animation_manager_set(&transition->animations, REACH_FEATURE_TRANSITION_Y,
                                reach_feature_transition_origin(transition));
    reach_animation_manager_set(&transition->animations, REACH_FEATURE_TRANSITION_OPACITY, 0.0f);
}

void reach_feature_transition_configure(reach_feature_transition *transition,
                                        const reach_theme *theme, float dpi_scale,
                                        int32_t direction)
{
    if (transition == nullptr)
    {
        return;
    }
    const reach_theme *resolved_theme = theme != nullptr ? theme : reach_theme_default();
    transition->open_seconds = resolved_theme->surface_open_seconds;
    transition->close_seconds = resolved_theme->surface_close_seconds;
    transition->dpi_scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    transition->direction = direction;
    if (!transition->visible)
    {
        reach_animation_manager_set(&transition->animations, REACH_FEATURE_TRANSITION_Y,
                                    reach_feature_transition_origin(transition));
    }
}

int32_t reach_feature_transition_set_open(reach_feature_transition *transition, int32_t open)
{
    if (transition == nullptr)
    {
        return 0;
    }
    int32_t target_open = open ? 1 : 0;
    if (transition->target_open == target_open &&
        (target_open || !transition->visible ||
         reach_animation_manager_any_active(&transition->animations)))
    {
        return 0;
    }

    transition->target_open = target_open;
    if (target_open)
    {
        if (!transition->visible)
        {
            transition->visible = 1;
            reach_animation_manager_set(&transition->animations, REACH_FEATURE_TRANSITION_Y,
                                        reach_feature_transition_origin(transition));
            reach_animation_manager_set(&transition->animations,
                                        REACH_FEATURE_TRANSITION_OPACITY, 0.0f);
        }
        reach_animation_manager_animate_to(&transition->animations, REACH_FEATURE_TRANSITION_Y,
                                           0.0f, transition->open_seconds,
                                           REACH_EASING_EASE_OUT);
        reach_animation_manager_animate_to(&transition->animations,
                                           REACH_FEATURE_TRANSITION_OPACITY, 1.0f,
                                           transition->open_seconds, REACH_EASING_EASE_OUT);
    }
    else if (transition->visible)
    {
        reach_animation_manager_animate_to(&transition->animations, REACH_FEATURE_TRANSITION_Y,
                                           reach_feature_transition_origin(transition),
                                           transition->close_seconds, REACH_EASING_EASE_IN);
        reach_animation_manager_animate_to(&transition->animations,
                                           REACH_FEATURE_TRANSITION_OPACITY, 0.0f,
                                           transition->close_seconds, REACH_EASING_EASE_IN);
    }
    return 1;
}

int32_t reach_feature_transition_tick(reach_feature_transition *transition, double delta_seconds)
{
    if (transition == nullptr)
    {
        return 0;
    }
    int32_t was_active = reach_animation_manager_any_active(&transition->animations);
    int32_t was_visible = transition->visible;
    reach_animation_manager_tick(&transition->animations, delta_seconds);
    int32_t active = reach_animation_manager_any_active(&transition->animations);
    if (!transition->target_open && transition->visible && !active &&
        reach_animation_manager_value(&transition->animations,
                                      REACH_FEATURE_TRANSITION_OPACITY) <= 0.001f)
    {
        transition->visible = 0;
    }
    return was_active || active || was_visible != transition->visible;
}

int32_t reach_feature_transition_visible(const reach_feature_transition *transition)
{
    return transition != nullptr && transition->visible;
}

int32_t reach_feature_transition_active(const reach_feature_transition *transition)
{
    return transition != nullptr &&
           reach_animation_manager_any_active(&transition->animations);
}

void reach_feature_transition_presentation(const reach_feature_transition *transition,
                                           reach_feature_surface_geometry *geometry)
{
    if (transition == nullptr || geometry == nullptr)
    {
        return;
    }
    geometry->presentation.managed = 1;
    geometry->presentation.opacity = reach_animation_manager_value(
        &transition->animations, REACH_FEATURE_TRANSITION_OPACITY);
    geometry->presentation.y_offset =
        reach_animation_manager_value(&transition->animations, REACH_FEATURE_TRANSITION_Y) *
        transition->dpi_scale;
    geometry->presentation.scale = 1.0f;
    geometry->presentation.max_scale = 1.0f;
}
