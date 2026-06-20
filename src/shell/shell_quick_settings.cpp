#include "shell_internal.h"

#include <math.h>

static float reach_shell_quick_settings_clamp01(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    if (value > 1.0f)
    {
        return 1.0f;
    }
    return value;
}

static reach_rect_f32 reach_shell_quick_settings_content_bounds(reach_rect_f32 surface_bounds,
                                                                float dpi_scale)
{
    reach_rect_f32 bounds = surface_bounds;

    float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    const float horizontal_padding = 8.0f * scale;
    const float top_padding = 8.0f * scale;
    const float bottom_padding = 12.0f * scale;

    bounds.x += horizontal_padding;
    bounds.y += top_padding;
    bounds.width -= horizontal_padding * 2.0f;
    bounds.height -= top_padding + bottom_padding + reach_popup_notch_height_scaled(scale);

    if (bounds.width < 0.0f)
    {
        bounds.width = 0.0f;
    }
    if (bounds.height < 0.0f)
    {
        bounds.height = 0.0f;
    }

    return bounds;
}

static void reach_shell_quick_settings_target_size(reach_shell *shell, float *out_width,
                                                   float *out_height)
{
    float scale = reach_shell_layout_dpi_scale(shell);
    const float surface_vertical_padding =
        8.0f * scale + 12.0f * scale + reach_popup_notch_height_scaled(scale);
    float content_height = reach_quick_settings_content_height_for_model_scaled(
        shell != nullptr ? &shell->quick_settings_model : nullptr, scale);

    if (out_width != nullptr)
    {
        *out_width = 280.0f * scale;
    }
    if (out_height != nullptr)
    {
        *out_height = content_height + surface_vertical_padding;
    }
}

static reach_rect_f32 reach_shell_quick_settings_target_bounds(reach_shell *shell)
{
    reach_rect_f32 bounds = {};
    if (shell == nullptr || !shell->has_layout)
    {
        return bounds;
    }

    float width = 280.0f;
    float height = 140.0f;
    reach_shell_quick_settings_target_size(shell, &width, &height);

    float scale = reach_shell_layout_dpi_scale(shell);
    const float gap = 8.0f * scale;
    reach_rect_f32 button = reach_shell_dock_rect_to_screen(
        &shell->layout.dock, shell->layout.dock.quick_settings_button);
    float anchor_x = button.x + button.width * 0.5f;

    bounds.width = width;
    bounds.height = height;
    bounds.x = anchor_x - width * 0.5f;
    bounds.y = shell->layout.dock.bounds.y - height - gap;
    return bounds;
}

static int32_t reach_shell_quick_settings_height_changed(float a, float b)
{
    float diff = a - b;
    if (diff < 0.0f)
    {
        diff = -diff;
    }
    return diff > 0.5f;
}

void reach_shell_relayout_quick_settings(reach_shell *shell, int32_t animate_height)
{
    if (shell == nullptr || !shell->has_layout)
    {
        return;
    }

    reach_rect_f32 old_target = shell->quick_settings_target_bounds;
    reach_rect_f32 current_bounds = shell->quick_settings_bounds;
    reach_rect_f32 new_target = reach_shell_quick_settings_target_bounds(shell);

    shell->quick_settings_target_bounds = new_target;

    if (animate_height &&
        reach_shell_quick_settings_height_changed(old_target.height, new_target.height))
    {
        reach_animation_manager_start(
            &shell->animations, REACH_SHELL_ANIMATION_QUICK_SETTINGS_HEIGHT, current_bounds.height,
            new_target.height, 0.16, REACH_EASING_EASE_IN_OUT);
    }
    else if (!reach_animation_manager_active(&shell->animations,
                                             REACH_SHELL_ANIMATION_QUICK_SETTINGS_HEIGHT))
    {
        shell->quick_settings_bounds = new_target;
    }

    reach_shell_refresh_quick_settings_layout(shell);
}

void reach_shell_refresh_quick_settings_layout(reach_shell *shell)
{
    if (shell == nullptr || !shell->has_layout)
    {
        return;
    }

    reach_rect_f32 button = reach_shell_dock_rect_to_screen(
        &shell->layout.dock, shell->layout.dock.quick_settings_button);
    float anchor_x = button.x + button.width * 0.5f;

    shell->quick_settings_target_bounds = reach_shell_quick_settings_target_bounds(shell);
    if (!reach_animation_manager_active(&shell->animations,
                                        REACH_SHELL_ANIMATION_QUICK_SETTINGS_HEIGHT))
    {
        shell->quick_settings_bounds = shell->quick_settings_target_bounds;
    }
    shell->quick_settings_notch_anchor_x = anchor_x;

    reach_rect_f32 surface_bounds = {};
    surface_bounds.width = shell->quick_settings_bounds.width;
    surface_bounds.height = shell->quick_settings_bounds.height;
    shell->quick_settings_content_bounds = reach_shell_quick_settings_content_bounds(
        surface_bounds, reach_shell_layout_dpi_scale(shell));
    shell->quick_settings_layout = reach_quick_settings_layout_for_content_bounds_scaled(
        shell->quick_settings_content_bounds, shell->theme, &shell->quick_settings_model,
        reach_shell_layout_dpi_scale(shell));
}

void reach_shell_update_quick_settings_animation(reach_shell *shell)
{
    if (shell == nullptr || !shell->quick_settings_open || !shell->has_layout)
    {
        return;
    }

    if (reach_animation_manager_active(&shell->animations,
                                       REACH_SHELL_ANIMATION_QUICK_SETTINGS_HEIGHT) ||
        reach_shell_quick_settings_height_changed(shell->quick_settings_bounds.height,
                                                  shell->quick_settings_target_bounds.height))
    {
        float scale = reach_shell_layout_dpi_scale(shell);
        const float gap = 8.0f * scale;
        reach_rect_f32 animated = shell->quick_settings_target_bounds;
        animated.height = reach_animation_manager_value(
            &shell->animations, REACH_SHELL_ANIMATION_QUICK_SETTINGS_HEIGHT);
        animated.y = floorf(shell->layout.dock.bounds.y - animated.height - gap + 0.5f);

        shell->quick_settings_bounds = animated;

        reach_shell_refresh_quick_settings_layout(shell);
        shell->quick_settings.dirty_flags = 1;
        shell->dirty.render = 1;
    }
}

void reach_shell_set_quick_settings_open(reach_shell *shell, int32_t open)
{
    if (shell == nullptr)
    {
        return;
    }

    int32_t next_open = open ? 1 : 0;
    if (shell->quick_settings_open == next_open)
    {
        return;
    }

    int32_t was_open = shell->quick_settings_open;
    if (next_open)
    {
        reach_shell_set_tray_popup_open(shell, 0);
        reach_shell_close_context_menu(shell);
    }

    shell->quick_settings_open = next_open;
    reach_shell_surface_transition_set(shell, &shell->quick_settings_transition, next_open);
    shell->quick_settings_drag.active = 0;
    shell->quick_settings_drag.type = REACH_QUICK_SETTINGS_HIT_NONE;
    shell->quick_settings_drag.level_valid = 0;
    reach_shell_sync_pointer_move_subscriptions(shell);

    if (next_open)
    {
        reach_quick_settings_model_set_bluetooth_pending(&shell->quick_settings_model, 0, 0);
        shell->quick_settings_bluetooth_pending = {};
        reach_shell_start_quick_settings_system_refresh(shell, 0);
        reach_shell_start_quick_settings_audio_refresh(shell);
        reach_animation_manager_reset(&shell->animations,
                                      REACH_SHELL_ANIMATION_QUICK_SETTINGS_HEIGHT);
        reach_shell_relayout_quick_settings(shell, 0);
    }
    else
    {
        reach_quick_settings_model_set_bluetooth_pending(&shell->quick_settings_model, 0, 0);
        shell->quick_settings_bluetooth_pending = {};
        if (was_open)
        {
            reach_shell_request_dock_visibility_update(shell);
        }
    }

    reach_shell_sync_popup_mouse_hook(shell);

    shell->quick_settings.dirty_flags = 1;
    shell->dirty.render = 1;
}

void reach_shell_toggle_quick_settings(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    reach_shell_set_quick_settings_open(shell, shell->quick_settings_open ? 0 : 1);
}

void reach_shell_execute_quick_settings_action(reach_shell *shell,
                                               reach_quick_settings_action action)
{
    if (shell == nullptr)
    {
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME)
    {
        float level = reach_shell_quick_settings_clamp01(action.volume_level);

        shell->quick_settings_audio_state.level = level;
        reach_quick_settings_model_set_main_volume(&shell->quick_settings_model, level,
                                                   shell->quick_settings_audio_state.muted);

        if (shell->audio_volume.set_level != nullptr)
        {
            (void)shell->audio_volume.set_level(shell->audio_volume.userdata, level);
        }

        shell->quick_settings.dirty_flags = 1;
        shell->dirty.render = 1;
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_SET_SESSION_VOLUME)
    {
        float level = reach_shell_quick_settings_clamp01(action.volume_level);

        if (action.session_index < shell->quick_settings_model.sessions.count)
        {
            reach_audio_volume_session *session =
                &shell->quick_settings_model.sessions.sessions[action.session_index];
            session->level = level;
            if (shell->audio_volume.set_session_level != nullptr)
            {
                (void)shell->audio_volume.set_session_level(shell->audio_volume.userdata,
                                                            session->session_instance_id, level);
            }
        }
        else if (shell->audio_volume.set_session_level != nullptr)
        {
            (void)shell->audio_volume.set_session_level(shell->audio_volume.userdata,
                                                        action.session_instance_id, level);
        }

        shell->quick_settings.dirty_flags = 1;
        shell->dirty.render = 1;
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_SET_SESSION_MUTED)
    {
        if (shell->audio_volume.set_session_muted != nullptr)
        {
            (void)shell->audio_volume.set_session_muted(shell->audio_volume.userdata,
                                                        action.session_instance_id, action.muted);
        }
        reach_shell_start_quick_settings_audio_refresh(shell);
        shell->quick_settings.dirty_flags = 1;
        shell->dirty.render = 1;
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_SET_BRIGHTNESS)
    {
        if (shell->system_controls.set_brightness_level != nullptr)
        {
            (void)shell->system_controls.set_brightness_level(
                shell->system_controls.userdata,
                reach_shell_quick_settings_clamp01(action.volume_level));
        }
        reach_shell_start_quick_settings_system_refresh(shell, 0);
        shell->quick_settings.dirty_flags = 1;
        shell->dirty.render = 1;
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_NETWORK_TILE)
    {
        reach_shell_set_quick_settings_open(shell, 0);
        if (shell->system_controls.open_system_quick_settings != nullptr)
        {
            (void)shell->system_controls.open_system_quick_settings(
                shell->system_controls.userdata);
        }
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_TOGGLE_BLUETOOTH)
    {
        if (shell->quick_settings_model.bluetooth_pending)
        {
            return;
        }

        if (!shell->quick_settings_model.bluetooth.available)
        {
            reach_shell_start_quick_settings_system_refresh(shell, 0);
            shell->quick_settings.dirty_flags = 1;
            shell->dirty.render = 1;
            return;
        }

        int32_t target_enabled = shell->quick_settings_model.bluetooth.enabled ? 0 : 1;
        if (shell->system_controls.request_bluetooth_enabled != nullptr)
        {
            reach_quick_settings_model_set_bluetooth_pending(&shell->quick_settings_model, 1,
                                                             target_enabled);
            shell->quick_settings_bluetooth_pending.active = 1;
            shell->quick_settings_bluetooth_pending.elapsed_seconds = 0.0;
            shell->quick_settings_bluetooth_pending.refresh_elapsed_seconds = 0.0;
            shell->quick_settings.dirty_flags = 1;
            shell->dirty.render = 1;
            if (shell->system_controls.request_bluetooth_enabled(shell->system_controls.userdata,
                                                                 target_enabled) != REACH_OK)
            {
                reach_quick_settings_model_set_bluetooth_pending(&shell->quick_settings_model, 0,
                                                                 0);
                shell->quick_settings_bluetooth_pending = {};
                reach_shell_start_quick_settings_system_refresh(shell, 0);
            }
            shell->quick_settings.dirty_flags = 1;
            shell->dirty.render = 1;
            return;
        }

        if (shell->system_controls.set_bluetooth_enabled != nullptr)
        {
            reach_quick_settings_model_set_bluetooth_pending(&shell->quick_settings_model, 1,
                                                             target_enabled);
            shell->quick_settings.dirty_flags = 1;
            shell->dirty.render = 1;
            (void)shell->system_controls.set_bluetooth_enabled(shell->system_controls.userdata,
                                                               target_enabled);
        }
        reach_shell_start_quick_settings_system_refresh(shell, 0);
        reach_quick_settings_model_set_bluetooth_pending(&shell->quick_settings_model, 0, 0);
        shell->quick_settings.dirty_flags = 1;
        shell->dirty.render = 1;
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_TOGGLE_BATTERY_SAVER)
    {
        reach_shell_start_quick_settings_system_refresh(shell, 0);
        shell->quick_settings.dirty_flags = 1;
        shell->dirty.render = 1;
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_OPEN_PROJECT)
    {
        reach_shell_set_quick_settings_open(shell, 0);
        if (shell->system_controls.open_project_menu != nullptr)
        {
            (void)shell->system_controls.open_project_menu(shell->system_controls.userdata);
        }
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_TOGGLE_OUTPUT_DEVICES)
    {
        shell->quick_settings_model.output_devices_expanded =
            shell->quick_settings_model.output_devices_expanded ? 0 : 1;
        if (shell->quick_settings_model.output_devices_expanded)
        {
            shell->quick_settings_model.expanded = 0;
        }

        reach_shell_start_quick_settings_system_refresh(shell, 0);
        reach_shell_start_quick_settings_audio_refresh(shell);
        reach_shell_relayout_quick_settings(shell, 1);

        shell->quick_settings.dirty_flags = 1;
        shell->dirty.render = 1;
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_SET_OUTPUT_DEVICE)
    {
        int32_t changed = 0;
        if (shell->audio_volume.set_default_output_device != nullptr)
        {
            changed = shell->audio_volume.set_default_output_device(
                          shell->audio_volume.userdata, action.output_device_id) == REACH_OK;
        }

        if (changed)
        {
            shell->quick_settings_model.output_devices_expanded = 0;
        }
        reach_shell_start_quick_settings_system_refresh(shell, 0);
        reach_shell_start_quick_settings_audio_refresh(shell);
        if (changed)
        {
            reach_shell_relayout_quick_settings(shell, 1);
        }

        shell->quick_settings.dirty_flags = 1;
        shell->dirty.render = 1;
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_EXPAND)
    {
        shell->quick_settings_model.expanded = shell->quick_settings_model.expanded ? 0 : 1;
        if (shell->quick_settings_model.expanded)
        {
            shell->quick_settings_model.output_devices_expanded = 0;
        }

        reach_shell_start_quick_settings_system_refresh(shell, 0);
        reach_shell_start_quick_settings_audio_refresh(shell);
        reach_shell_relayout_quick_settings(shell, 1);

        shell->quick_settings.dirty_flags = 1;
        shell->dirty.render = 1;
        return;
    }
}
