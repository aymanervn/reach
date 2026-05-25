#include "shell_internal.h"

static float reach_shell_quick_settings_clamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static reach_rect_f32 reach_shell_quick_settings_content_bounds(
    reach_rect_f32 surface_bounds
)
{
    reach_rect_f32 bounds = surface_bounds;

    const float horizontal_padding = 8.0f;
    const float top_padding = 8.0f;
    const float bottom_padding = 12.0f;

    bounds.x += horizontal_padding;
    bounds.y += top_padding;
    bounds.width -= horizontal_padding * 2.0f;
    bounds.height -= top_padding + bottom_padding + reach_popup_notch_height();

    if (bounds.width < 0.0f) {
        bounds.width = 0.0f;
    }
    if (bounds.height < 0.0f) {
        bounds.height = 0.0f;
    }

    return bounds;
}

static void reach_shell_quick_settings_target_size(
    reach_shell *shell,
    float *out_width,
    float *out_height)
{
    const float surface_vertical_padding =
        8.0f + 12.0f + reach_popup_notch_height();
    float content_height =
        reach_quick_settings_content_height_for_model(
            shell != nullptr ? &shell->quick_settings_model : nullptr);

    if (out_width != nullptr) {
        *out_width = 280.0f;
    }
    if (out_height != nullptr) {
        *out_height = content_height + surface_vertical_padding;
    }
}

static reach_rect_f32 reach_shell_quick_settings_target_bounds(reach_shell *shell)
{
    reach_rect_f32 bounds = {};
    if (shell == nullptr || !shell->has_layout) {
        return bounds;
    }

    float width = 280.0f;
    float height = 140.0f;
    reach_shell_quick_settings_target_size(shell, &width, &height);

    const float gap = 8.0f;
    reach_rect_f32 button = shell->layout.dock.quick_settings_button;
    float anchor_x = button.x + button.width * 0.5f;

    bounds.width = width;
    bounds.height = height;
    bounds.x = anchor_x - width * 0.5f;
    bounds.y = shell->layout.dock.bounds.y - height - gap;
    return bounds;
}

static int32_t reach_shell_quick_settings_height_changed(
    float a,
    float b)
{
    float diff = a - b;
    if (diff < 0.0f) {
        diff = -diff;
    }
    return diff > 0.5f;
}

static void reach_shell_relayout_quick_settings(
    reach_shell *shell,
    int32_t animate_height)
{
    if (shell == nullptr || !shell->has_layout) {
        return;
    }

    reach_rect_f32 old_target = shell->quick_settings_target_bounds;
    reach_rect_f32 current_bounds = shell->quick_settings_bounds;
    reach_rect_f32 new_target = reach_shell_quick_settings_target_bounds(shell);

    shell->quick_settings_target_bounds = new_target;

    if (animate_height &&
        reach_shell_quick_settings_height_changed(old_target.height, new_target.height)) {
        reach_shell_start_popup_bounds_animation(
            &shell->quick_settings_bounds_animation,
            current_bounds,
            new_target,
            0,
            1,
            0.16);
    } else if (!reach_shell_popup_bounds_animation_active(
        &shell->quick_settings_bounds_animation)) {
        shell->quick_settings_bounds = new_target;
    }

    reach_shell_refresh_quick_settings_layout(shell);
}

static void reach_shell_capture_quick_settings_input(reach_shell *shell)
{
    if (shell == nullptr ||
        shell->popup_capture.begin_capture == nullptr ||
        shell->quick_settings.window.ops.native_handle == nullptr) {
        return;
    }

    void *native_window = shell->quick_settings.window.ops.native_handle(
        shell->quick_settings.window.window);
    if (native_window != nullptr) {
        (void)shell->popup_capture.begin_capture(
            shell->popup_capture.userdata,
            native_window);
    }
}

static void reach_shell_release_quick_settings_input(reach_shell *shell)
{
    if (shell == nullptr ||
        shell->popup_capture.end_capture == nullptr ||
        shell->quick_settings.window.ops.native_handle == nullptr) {
        return;
    }

    void *native_window = shell->quick_settings.window.ops.native_handle(
        shell->quick_settings.window.window);
    if (native_window != nullptr) {
        shell->popup_capture.end_capture(
            shell->popup_capture.userdata,
            native_window);
    }
}

void reach_shell_refresh_quick_settings_layout(reach_shell *shell)
{
    if (shell == nullptr || !shell->has_layout) {
        return;
    }

    reach_rect_f32 button = shell->layout.dock.quick_settings_button;
    float anchor_x = button.x + button.width * 0.5f;

    shell->quick_settings_target_bounds =
        reach_shell_quick_settings_target_bounds(shell);
    if (!reach_shell_popup_bounds_animation_active(
        &shell->quick_settings_bounds_animation)) {
        shell->quick_settings_bounds = shell->quick_settings_target_bounds;
    }
    shell->quick_settings_notch_anchor_x = anchor_x;

    reach_rect_f32 surface_bounds = {};
    surface_bounds.width = shell->quick_settings_bounds.width;
    surface_bounds.height = shell->quick_settings_bounds.height;
    shell->quick_settings_content_bounds =
        reach_shell_quick_settings_content_bounds(surface_bounds);
    shell->quick_settings_layout =
        reach_quick_settings_layout_for_content_bounds(
            shell->quick_settings_content_bounds,
            shell->theme,
            &shell->quick_settings_model);
}

void reach_shell_refresh_quick_settings_audio(
    reach_shell *shell
)
{
    if (shell == nullptr) {
        return;
    }

    reach_audio_volume_state state = {};
    if (shell->audio_volume.get_state != nullptr &&
        shell->audio_volume.get_state(shell->audio_volume.userdata, &state) == REACH_OK) {
        state.level = reach_shell_quick_settings_clamp01(state.level);
        state.muted = state.muted ? 1 : 0;
        shell->quick_settings_audio_state = state;
        reach_quick_settings_model_set_main_volume(
            &shell->quick_settings_model,
            state.level,
            state.muted);
    }

    reach_audio_volume_session_list sessions = {};
    if (shell->audio_volume.list_sessions != nullptr &&
        shell->audio_volume.list_sessions(
            shell->audio_volume.userdata,
            &sessions) == REACH_OK) {
        shell->quick_settings_audio_sessions = sessions;
        reach_quick_settings_model_set_sessions(
            &shell->quick_settings_model,
            &sessions);
    } else {
        shell->quick_settings_audio_sessions = {};
        reach_quick_settings_model_set_sessions(
            &shell->quick_settings_model,
            nullptr);
    }

    reach_audio_output_device_list output_devices = {};
    if (shell->audio_volume.list_output_devices != nullptr &&
        shell->audio_volume.list_output_devices(
            shell->audio_volume.userdata,
            &output_devices) == REACH_OK) {
        shell->quick_settings_output_devices = output_devices;
        reach_quick_settings_model_set_output_devices(
            &shell->quick_settings_model,
            &output_devices);
    } else {
        shell->quick_settings_output_devices = {};
        reach_quick_settings_model_set_output_devices(
            &shell->quick_settings_model,
            nullptr);
    }
}

void reach_shell_refresh_quick_settings_system(
    reach_shell *shell
)
{
    if (shell == nullptr) {
        return;
    }

    reach_network_state network = {};
    if (shell->system_controls.get_network_state == nullptr ||
        shell->system_controls.get_network_state(
            shell->system_controls.userdata,
            &network) != REACH_OK) {
        network.kind = REACH_NETWORK_KIND_NONE;
        network.connected = 0;
    }

    reach_bluetooth_state bluetooth = {};
    if (shell->system_controls.get_bluetooth_state == nullptr ||
        shell->system_controls.get_bluetooth_state(
            shell->system_controls.userdata,
            &bluetooth) != REACH_OK) {
        bluetooth.available = 0;
        bluetooth.enabled = 0;
    }

    reach_power_state power = {};
    if (shell->system_controls.get_power_state == nullptr ||
        shell->system_controls.get_power_state(
            shell->system_controls.userdata,
            &power) != REACH_OK) {
        power = {};
    }

    reach_brightness_state brightness = {};
    if (shell->system_controls.get_brightness_state == nullptr ||
        shell->system_controls.get_brightness_state(
            shell->system_controls.userdata,
            &brightness) != REACH_OK) {
        brightness = {};
    }

    reach_quick_settings_model_set_system_states(
        &shell->quick_settings_model,
        &network,
        &bluetooth,
        &power,
        &brightness);
}

void reach_shell_on_system_controls_changed(
    void *user,
    uint32_t change_flags
)
{
    reach_shell *shell = static_cast<reach_shell *>(user);
    if (shell == nullptr || change_flags == 0) {
        return;
    }

    shell->quick_settings_system_change_flags.fetch_or(change_flags);
}

void reach_shell_process_quick_settings_system_changes(
    reach_shell *shell
)
{
    if (shell == nullptr) {
        return;
    }

    uint32_t change_flags = shell->quick_settings_system_change_flags.exchange(0);
    if (change_flags == 0 || !shell->quick_settings_open) {
        return;
    }

    reach_power_state previous_power = shell->quick_settings_model.power;
    reach_brightness_state previous_brightness = shell->quick_settings_model.brightness;
    int32_t bluetooth_pending = shell->quick_settings_model.bluetooth_pending;

    reach_shell_refresh_quick_settings_system(shell);
    if ((change_flags & REACH_SYSTEM_CONTROLS_CHANGE_BLUETOOTH) != 0 &&
        bluetooth_pending) {
        reach_quick_settings_model_set_bluetooth_pending(
            &shell->quick_settings_model,
            0,
            0);
    }

    int32_t layout_changed =
        previous_power.has_battery != shell->quick_settings_model.power.has_battery ||
        previous_brightness.available != shell->quick_settings_model.brightness.available;
    if (layout_changed) {
        reach_shell_relayout_quick_settings(shell, 1);
    }

    shell->quick_settings.dirty_flags = 1;
    shell->render_dirty = 1;
}

void reach_shell_update_quick_settings_animation(
    reach_shell *shell,
    double delta_seconds)
{
    if (shell == nullptr || !shell->quick_settings_open || !shell->has_layout) {
        return;
    }

    if (reach_shell_popup_bounds_animation_active(
        &shell->quick_settings_bounds_animation)) {
        const float gap = 8.0f;
        float anchor_x = shell->layout.dock.quick_settings_button.x +
            shell->layout.dock.quick_settings_button.width * 0.5f;
        shell->quick_settings_bounds =
            reach_shell_apply_popup_bounds_animation(
                &shell->quick_settings_bounds_animation,
                shell->quick_settings_target_bounds,
                anchor_x,
                shell->layout.dock.bounds.y,
                gap,
                delta_seconds);
        reach_shell_refresh_quick_settings_layout(shell);
        shell->quick_settings.dirty_flags = 1;
        shell->render_dirty = 1;
    }
}

void reach_shell_set_quick_settings_open(
    reach_shell *shell,
    int32_t open
)
{
    if (shell == nullptr) {
        return;
    }

    int32_t next_open = open ? 1 : 0;
    if (shell->quick_settings_open == next_open) {
        return;
    }

    shell->quick_settings_open = next_open;
    shell->quick_settings_dragging_volume = 0;
    shell->quick_settings_drag_type = REACH_QUICK_SETTINGS_HIT_NONE;
    if (shell->quick_settings.window.ops.set_pointer_move_enabled != nullptr) {
        (void)shell->quick_settings.window.ops.set_pointer_move_enabled(
            shell->quick_settings.window.window,
            0);
    }

    if (next_open) {
        reach_shell_set_tray_popup_open(shell, 0);
        reach_shell_close_context_menu(shell);
        reach_quick_settings_model_set_bluetooth_pending(
            &shell->quick_settings_model,
            0,
            0);
        reach_shell_refresh_quick_settings_system(shell);
        reach_shell_refresh_quick_settings_audio(shell);
        shell->quick_settings_bounds_animation = {};
        reach_shell_relayout_quick_settings(shell, 0);
        reach_shell_capture_quick_settings_input(shell);
        if (shell->quick_settings.window.ops.show != nullptr) {
            (void)shell->quick_settings.window.ops.show(shell->quick_settings.window.window);
        }
    } else {
        reach_quick_settings_model_set_bluetooth_pending(
            &shell->quick_settings_model,
            0,
            0);
        reach_shell_release_quick_settings_input(shell);
        if (shell->quick_settings.window.ops.hide != nullptr) {
            (void)shell->quick_settings.window.ops.hide(shell->quick_settings.window.window);
        }
    }

    reach_shell_sync_popup_mouse_hook(shell);

    shell->quick_settings.dirty_flags = 1;
    shell->render_dirty = 1;
}

void reach_shell_toggle_quick_settings(
    reach_shell *shell
)
{
    if (shell == nullptr) {
        return;
    }

    reach_shell_set_quick_settings_open(
        shell,
        shell->quick_settings_open ? 0 : 1);
}

void reach_shell_execute_quick_settings_action(
    reach_shell *shell,
    reach_quick_settings_action action
)
{
    if (shell == nullptr) {
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME) {
        float level = reach_shell_quick_settings_clamp01(action.volume_level);

        shell->quick_settings_audio_state.level = level;
        reach_quick_settings_model_set_main_volume(
            &shell->quick_settings_model,
            level,
            shell->quick_settings_audio_state.muted);

        if (shell->audio_volume.set_level != nullptr) {
            (void)shell->audio_volume.set_level(shell->audio_volume.userdata, level);
        }

        shell->quick_settings.dirty_flags = 1;
        shell->render_dirty = 1;
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_SET_SESSION_VOLUME) {
        float level = reach_shell_quick_settings_clamp01(action.volume_level);

        if (action.session_index < shell->quick_settings_model.sessions.count) {
            reach_audio_volume_session *session =
                &shell->quick_settings_model.sessions.sessions[action.session_index];
            session->level = level;
            if (shell->audio_volume.set_session_level != nullptr) {
                (void)shell->audio_volume.set_session_level(
                    shell->audio_volume.userdata,
                    session->session_instance_id,
                    level);
            }
        } else if (shell->audio_volume.set_session_level != nullptr) {
            (void)shell->audio_volume.set_session_level(
                shell->audio_volume.userdata,
                action.session_instance_id,
                level);
        }

        shell->quick_settings.dirty_flags = 1;
        shell->render_dirty = 1;
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_SET_SESSION_MUTED) {
        if (shell->audio_volume.set_session_muted != nullptr) {
            (void)shell->audio_volume.set_session_muted(
                shell->audio_volume.userdata,
                action.session_instance_id,
                action.muted);
        }
        reach_shell_refresh_quick_settings_audio(shell);
        shell->quick_settings.dirty_flags = 1;
        shell->render_dirty = 1;
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_SET_BRIGHTNESS) {
        if (shell->system_controls.set_brightness_level != nullptr) {
            (void)shell->system_controls.set_brightness_level(
                shell->system_controls.userdata,
                reach_shell_quick_settings_clamp01(action.volume_level));
        }
        reach_shell_refresh_quick_settings_system(shell);
        shell->quick_settings.dirty_flags = 1;
        shell->render_dirty = 1;
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_NETWORK_TILE) {
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_TOGGLE_BLUETOOTH) {
        if (shell->quick_settings_model.bluetooth_pending) {
            return;
        }

        reach_bluetooth_state bluetooth = {};
        if (shell->system_controls.get_bluetooth_state == nullptr ||
            shell->system_controls.get_bluetooth_state(
                shell->system_controls.userdata,
                &bluetooth) != REACH_OK ||
            !bluetooth.available) {
            reach_shell_refresh_quick_settings_system(shell);
            shell->quick_settings.dirty_flags = 1;
            shell->render_dirty = 1;
            return;
        }

        int32_t target_enabled = bluetooth.enabled ? 0 : 1;
        if (shell->system_controls.request_bluetooth_enabled != nullptr) {
            reach_quick_settings_model_set_bluetooth_pending(
                &shell->quick_settings_model,
                1,
                target_enabled);
            shell->quick_settings.dirty_flags = 1;
            shell->render_dirty = 1;
            if (shell->system_controls.request_bluetooth_enabled(
                shell->system_controls.userdata,
                target_enabled) != REACH_OK) {
                reach_quick_settings_model_set_bluetooth_pending(
                    &shell->quick_settings_model,
                    0,
                    0);
                reach_shell_refresh_quick_settings_system(shell);
            }
            shell->quick_settings.dirty_flags = 1;
            shell->render_dirty = 1;
            return;
        }

        if (shell->system_controls.set_bluetooth_enabled != nullptr) {
            reach_quick_settings_model_set_bluetooth_pending(
                &shell->quick_settings_model,
                1,
                target_enabled);
            shell->quick_settings.dirty_flags = 1;
            shell->render_dirty = 1;
            (void)shell->system_controls.set_bluetooth_enabled(
                shell->system_controls.userdata,
                target_enabled);
        }
        reach_shell_refresh_quick_settings_system(shell);
        reach_quick_settings_model_set_bluetooth_pending(
            &shell->quick_settings_model,
            0,
            0);
        shell->quick_settings.dirty_flags = 1;
        shell->render_dirty = 1;
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_TOGGLE_BATTERY_SAVER) {
        reach_shell_refresh_quick_settings_system(shell);
        shell->quick_settings.dirty_flags = 1;
        shell->render_dirty = 1;
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_OPEN_PROJECT) {
        if (shell->system_controls.open_project_menu != nullptr) {
            (void)shell->system_controls.open_project_menu(
                shell->system_controls.userdata);
        }
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_TOGGLE_OUTPUT_DEVICES) {
        shell->quick_settings_model.output_devices_expanded =
            shell->quick_settings_model.output_devices_expanded ? 0 : 1;
        if (shell->quick_settings_model.output_devices_expanded) {
            shell->quick_settings_model.expanded = 0;
        }

        reach_shell_refresh_quick_settings_system(shell);
        reach_shell_refresh_quick_settings_audio(shell);
        reach_shell_relayout_quick_settings(shell, 1);

        shell->quick_settings.dirty_flags = 1;
        shell->render_dirty = 1;
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_SET_OUTPUT_DEVICE) {
        int32_t changed = 0;
        if (shell->audio_volume.set_default_output_device != nullptr) {
            changed =
                shell->audio_volume.set_default_output_device(
                    shell->audio_volume.userdata,
                    action.output_device_id) == REACH_OK;
        }

        if (changed) {
            shell->quick_settings_model.output_devices_expanded = 0;
        }
        reach_shell_refresh_quick_settings_system(shell);
        reach_shell_refresh_quick_settings_audio(shell);
        if (changed) {
            reach_shell_relayout_quick_settings(shell, 1);
        }

        shell->quick_settings.dirty_flags = 1;
        shell->render_dirty = 1;
        return;
    }

    if (action.type == REACH_QUICK_SETTINGS_ACTION_EXPAND) {
        shell->quick_settings_model.expanded =
            shell->quick_settings_model.expanded ? 0 : 1;
        if (shell->quick_settings_model.expanded) {
            shell->quick_settings_model.output_devices_expanded = 0;
        }

        reach_shell_refresh_quick_settings_system(shell);
        reach_shell_refresh_quick_settings_audio(shell);
        reach_shell_relayout_quick_settings(shell, 1);

        shell->quick_settings.dirty_flags = 1;
        shell->render_dirty = 1;
        return;
    }
}
