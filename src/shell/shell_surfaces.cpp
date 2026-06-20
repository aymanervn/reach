#include "shell_internal.h"

#include <math.h>

static const float REACH_SHELL_TRANSITION_OFFSET = 8.0f;
static const double REACH_SHELL_TRANSITION_OPEN_SECONDS = 0.16;
static const double REACH_SHELL_TRANSITION_CLOSE_SECONDS = 0.12;

int32_t reach_shell_rect_equal(reach_rect_f32 a, reach_rect_f32 b)
{
    return fabsf(a.x - b.x) < 0.5f && fabsf(a.y - b.y) < 0.5f && fabsf(a.width - b.width) < 0.5f &&
           fabsf(a.height - b.height) < 0.5f;
}

int32_t reach_shell_opacity_equal(float a, float b)
{
    return fabsf(a - b) < 0.001f;
}

reach_result reach_shell_apply_window_state(reach_platform_window_port *window,
                                            reach_rect_f32 bounds, float opacity,
                                            reach_rect_f32 *last_bounds, float *last_opacity,
                                            int32_t *bounds_valid, int32_t *opacity_valid,
                                            int32_t *out_changed)
{
    REACH_ASSERT(window != nullptr);
    REACH_ASSERT(last_bounds != nullptr);
    REACH_ASSERT(last_opacity != nullptr);
    REACH_ASSERT(bounds_valid != nullptr);
    REACH_ASSERT(opacity_valid != nullptr);
    REACH_ASSERT(out_changed != nullptr);
    if (window == nullptr || last_bounds == nullptr || last_opacity == nullptr ||
        bounds_valid == nullptr || opacity_valid == nullptr || out_changed == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_changed = 0;
    if (window->ops.set_bounds != nullptr &&
        (!*bounds_valid || !reach_shell_rect_equal(*last_bounds, bounds)))
    {
        reach_result result = window->ops.set_bounds(window->window, bounds);
        if (result != REACH_OK)
        {
            return result;
        }
        *last_bounds = bounds;
        *bounds_valid = 1;
        *out_changed = 1;
    }

    if (window->ops.set_opacity != nullptr &&
        (!*opacity_valid || !reach_shell_opacity_equal(*last_opacity, opacity)))
    {
        reach_result result = window->ops.set_opacity(window->window, opacity);
        if (result != REACH_OK)
        {
            return result;
        }
        *last_opacity = opacity;
        *opacity_valid = 1;
        *out_changed = 1;
    }

    return REACH_OK;
}

void reach_shell_surface_transition_init(reach_shell *shell,
                                         reach_shell_surface_transition *transition, size_t y_track,
                                         size_t opacity_track)
{
    if (shell == nullptr || transition == nullptr)
    {
        return;
    }
    *transition = {};
    transition->y_track = y_track;
    transition->opacity_track = opacity_track;
    reach_animation_manager_set(&shell->animations, y_track, REACH_SHELL_TRANSITION_OFFSET);
    reach_animation_manager_set(&shell->animations, opacity_track, 0.0f);
}

void reach_shell_surface_transitions_init(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }
    reach_shell_surface_transition_init(shell, &shell->launcher_transition,
                                        REACH_SHELL_ANIMATION_LAUNCHER_TRANSITION_Y,
                                        REACH_SHELL_ANIMATION_LAUNCHER_TRANSITION_OPACITY);
    reach_shell_surface_transition_init(shell, &shell->tray_transition,
                                        REACH_SHELL_ANIMATION_TRAY_TRANSITION_Y,
                                        REACH_SHELL_ANIMATION_TRAY_TRANSITION_OPACITY);
    reach_shell_surface_transition_init(shell, &shell->quick_settings_transition,
                                        REACH_SHELL_ANIMATION_QUICK_SETTINGS_TRANSITION_Y,
                                        REACH_SHELL_ANIMATION_QUICK_SETTINGS_TRANSITION_OPACITY);
    reach_shell_surface_transition_init(shell, &shell->switcher_transition,
                                        REACH_SHELL_ANIMATION_SWITCHER_TRANSITION_Y,
                                        REACH_SHELL_ANIMATION_SWITCHER_TRANSITION_OPACITY);
    reach_shell_surface_transition_init(shell, &shell->context_menu_transition,
                                        REACH_SHELL_ANIMATION_CONTEXT_MENU_TRANSITION_Y,
                                        REACH_SHELL_ANIMATION_CONTEXT_MENU_TRANSITION_OPACITY);
    reach_shell_surface_transition_init(shell, &shell->clipboard_transition,
                                        REACH_SHELL_ANIMATION_CLIPBOARD_TRANSITION_Y,
                                        REACH_SHELL_ANIMATION_CLIPBOARD_TRANSITION_OPACITY);
}

void reach_shell_surface_transition_set(reach_shell *shell,
                                        reach_shell_surface_transition *transition, int32_t open)
{
    if (shell == nullptr || transition == nullptr)
    {
        return;
    }

    int32_t target_open = open ? 1 : 0;
    if (transition->target_open == target_open && (target_open || !transition->visible))
    {
        return;
    }

    transition->target_open = target_open;
    if (target_open)
    {
        if (!transition->visible)
        {
            transition->visible = 1;
            reach_animation_manager_set(&shell->animations, transition->y_track,
                                        REACH_SHELL_TRANSITION_OFFSET);
            reach_animation_manager_set(&shell->animations, transition->opacity_track, 0.0f);
        }
        reach_animation_manager_animate_to(&shell->animations, transition->y_track, 0.0f,
                                           REACH_SHELL_TRANSITION_OPEN_SECONDS,
                                           REACH_EASING_EASE_OUT);
        reach_animation_manager_animate_to(&shell->animations, transition->opacity_track, 1.0f,
                                           REACH_SHELL_TRANSITION_OPEN_SECONDS,
                                           REACH_EASING_EASE_OUT);
    }
    else if (transition->visible)
    {
        reach_animation_manager_animate_to(
            &shell->animations, transition->y_track, REACH_SHELL_TRANSITION_OFFSET,
            REACH_SHELL_TRANSITION_CLOSE_SECONDS, REACH_EASING_EASE_IN);
        reach_animation_manager_animate_to(&shell->animations, transition->opacity_track, 0.0f,
                                           REACH_SHELL_TRANSITION_CLOSE_SECONDS,
                                           REACH_EASING_EASE_IN);
    }
    reach_shell_request_update(shell);
}

reach_rect_f32
reach_shell_surface_transition_bounds(const reach_shell *shell,
                                      const reach_shell_surface_transition *transition,
                                      reach_rect_f32 target_bounds)
{
    if (shell != nullptr && transition != nullptr)
    {
        target_bounds.y += reach_animation_manager_value(&shell->animations, transition->y_track) *
                           reach_shell_layout_dpi_scale(shell);
    }
    return target_bounds;
}

float reach_shell_surface_transition_opacity(const reach_shell *shell,
                                             const reach_shell_surface_transition *transition)
{
    return shell != nullptr && transition != nullptr
               ? reach_animation_manager_value(&shell->animations, transition->opacity_track)
               : 0.0f;
}

int32_t reach_shell_surface_transition_visible(const reach_shell_surface_transition *transition)
{
    return transition != nullptr && transition->visible;
}

int32_t reach_shell_surface_transition_active(const reach_shell *shell,
                                              const reach_shell_surface_transition *transition)
{
    return shell != nullptr && transition != nullptr &&
           (reach_animation_manager_active(&shell->animations, transition->y_track) ||
            reach_animation_manager_active(&shell->animations, transition->opacity_track));
}

void reach_shell_surface_transition_finish(reach_shell *shell,
                                           reach_shell_surface_transition *transition)
{
    if (shell == nullptr || transition == nullptr || transition->target_open ||
        !transition->visible || reach_shell_surface_transition_active(shell, transition))
    {
        return;
    }

    transition->visible = 0;
    reach_animation_manager_set(&shell->animations, transition->y_track,
                                REACH_SHELL_TRANSITION_OFFSET);
    reach_animation_manager_set(&shell->animations, transition->opacity_track, 0.0f);
    reach_shell_request_update(shell);
}
