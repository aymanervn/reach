#include "shell_internal.h"
#include <math.h>

static int32_t reach_shell_popup_float_animation_active(const reach_float_animation *animation)
{
    return animation != nullptr && animation->elapsed_seconds < animation->duration_seconds;
}

static float reach_shell_round_f32(float value)
{
    return floorf(value + 0.5f);
}

static reach_rect_f32 reach_shell_snap_rect(reach_rect_f32 rect)
{
    rect.x = reach_shell_round_f32(rect.x);
    rect.y = reach_shell_round_f32(rect.y);
    rect.width = reach_shell_round_f32(rect.width);
    rect.height = reach_shell_round_f32(rect.height);
    return rect;
}

void reach_shell_start_popup_bounds_animation(reach_shell_popup_bounds_animation *animation,
                                              reach_rect_f32 current_bounds,
                                              reach_rect_f32 target_bounds, int32_t animate_width,
                                              int32_t animate_height, double duration_seconds)
{
    if (animation == nullptr)
    {
        return;
    }

    animation->animate_width = animate_width ? 1 : 0;
    animation->animate_height = animate_height ? 1 : 0;

    if (animation->animate_width)
    {
        reach_float_animation_start(&animation->width, current_bounds.width, target_bounds.width,
                                    duration_seconds);
    }
    else
    {
        reach_float_animation_start(&animation->width, target_bounds.width, target_bounds.width,
                                    0.0);
    }

    if (animation->animate_height)
    {
        reach_float_animation_start(&animation->height, current_bounds.height, target_bounds.height,
                                    duration_seconds);
    }
    else
    {
        reach_float_animation_start(&animation->height, target_bounds.height, target_bounds.height,
                                    0.0);
    }

    animation->active = animation->animate_width || animation->animate_height;
}

reach_rect_f32
reach_shell_apply_popup_bounds_animation(reach_shell_popup_bounds_animation *animation,
                                         reach_rect_f32 target_bounds, float anchor_x,
                                         float reference_y, float gap, double delta_seconds)
{
    reach_rect_f32 bounds = target_bounds;

    if (animation != nullptr && animation->active)
    {
        if (animation->animate_width)
        {
            reach_float_animation_update(&animation->width, delta_seconds);
            bounds.width = animation->width.value;
        }
        else
        {
            bounds.width = target_bounds.width;
        }

        if (animation->animate_height)
        {
            reach_float_animation_update(&animation->height, delta_seconds);
            bounds.height = animation->height.value;
        }
        else
        {
            bounds.height = target_bounds.height;
        }

        animation->active = (animation->animate_width &&
                             reach_shell_popup_float_animation_active(&animation->width)) ||
                            (animation->animate_height &&
                             reach_shell_popup_float_animation_active(&animation->height));
    }

    if (animation != nullptr && animation->animate_width)
    {
        bounds.x = anchor_x - bounds.width * 0.5f;
    }
    else
    {
        bounds.x = target_bounds.x;
    }

    bounds.y = reference_y - bounds.height - gap;
    return reach_shell_snap_rect(bounds);
}

int32_t
reach_shell_popup_bounds_animation_active(const reach_shell_popup_bounds_animation *animation)
{
    return animation != nullptr && animation->active;
}

reach_result reach_shell_render_popup_surface(reach_shell *shell, reach_surface_runtime *surface,
                                              reach_rect_f32 bounds, float notch_anchor_x,
                                              const reach_render_command_buffer *content_commands)
{
    if (shell == nullptr || surface == nullptr || surface->renderer.ops.begin_frame == nullptr)
    {
        return REACH_OK;
    }

    reach_render_command_buffer commands = {};

    reach_popup_background_input popup = {};
    popup.theme = shell->theme != nullptr ? shell->theme : reach_theme_default();
    popup.bounds = bounds;
    popup.notch_center_x = notch_anchor_x - bounds.x;
    popup.dpi_scale = reach_shell_layout_dpi_scale(shell);

    reach_result result = reach_popup_push_background(&popup, &commands);
    if (result != REACH_OK)
    {
        return result;
    }

    if (content_commands != nullptr)
    {
        for (size_t index = 0; index < content_commands->count; ++index)
        {
            result =
                reach_render_command_buffer_push(&commands, &content_commands->commands[index]);
            if (result != REACH_OK)
            {
                return result;
            }
        }
    }

    result = surface->renderer.ops.begin_frame(surface->renderer.backend);
    if (result != REACH_OK)
    {
        return result;
    }

    result = surface->renderer.ops.execute(surface->renderer.backend, &commands);
    if (result != REACH_OK)
    {
        return result;
    }

    return surface->renderer.ops.end_frame(surface->renderer.backend);
}
