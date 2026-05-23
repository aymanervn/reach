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

    const float popup_width = 280.0f;
    const float popup_height = 112.0f;
    const float gap = 8.0f;
    reach_rect_f32 button = shell->layout.dock.quick_settings_button;
    float anchor_x = button.x + button.width * 0.5f;

    shell->quick_settings_bounds.width = popup_width;
    shell->quick_settings_bounds.height = popup_height;
    shell->quick_settings_bounds.x = anchor_x - popup_width * 0.5f;
    shell->quick_settings_bounds.y = shell->layout.dock.bounds.y - popup_height - gap;
    shell->quick_settings_notch_anchor_x = anchor_x;

    reach_rect_f32 surface_bounds = {};
    surface_bounds.width = popup_width;
    surface_bounds.height = popup_height;
    shell->quick_settings_content_bounds =
        reach_shell_quick_settings_content_bounds(surface_bounds);
    shell->quick_settings_layout =
        reach_quick_settings_layout_for_content_bounds(
            shell->quick_settings_content_bounds,
            shell->theme);
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

    if (next_open) {
        reach_shell_set_tray_popup_open(shell, 0);
        reach_shell_close_context_menu(shell);
        reach_shell_refresh_quick_settings_audio(shell);
        reach_shell_refresh_quick_settings_layout(shell);
        reach_shell_capture_quick_settings_input(shell);
        if (shell->quick_settings.window.ops.show != nullptr) {
            (void)shell->quick_settings.window.ops.show(shell->quick_settings.window.window);
        }
    } else {
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

    if (action.type == REACH_QUICK_SETTINGS_ACTION_EXPAND) {
        shell->quick_settings_model.expanded =
            shell->quick_settings_model.expanded ? 0 : 1;

        shell->quick_settings.dirty_flags = 1;
        shell->render_dirty = 1;
        return;
    }
}
