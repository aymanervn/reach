#include "shell_internal.h"

#include <math.h>

static float reach_shell_clamp_float(float value, float min_value, float max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

void reach_shell_end_quick_settings_drag(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    shell->quick_settings_drag.active = 0;
    shell->quick_settings_drag.type = REACH_QUICK_SETTINGS_HIT_NONE;
    shell->quick_settings_drag.level_valid = 0;
    shell->quick_settings_drag.session_index = 0;
    shell->quick_settings_drag.session_instance_id[0] = 0;

    if (shell->quick_settings.window.ops.set_pointer_capture != nullptr)
    {
        (void)shell->quick_settings.window.ops.set_pointer_capture(
            shell->quick_settings.window.window, 0);
    }

    if (shell->quick_settings.window.ops.set_pointer_move_enabled != nullptr)
    {
        (void)shell->quick_settings.window.ops.set_pointer_move_enabled(
            shell->quick_settings.window.window, 0);
    }
}

reach_result reach_shell_begin_quick_settings_drag_if_hit(reach_shell *shell,
                                                          const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr)
    {
        return REACH_OK;
    }

    reach_quick_settings_hit_result hit =
        reach_quick_settings_hit_test(&shell->quick_settings_layout, &shell->quick_settings_model,
                                      (float)event->x - shell->quick_settings_bounds.x,
                                      (float)event->y - shell->quick_settings_bounds.y);

    reach_quick_settings_action action = reach_quick_settings_action_for_hit(hit);
    if (action.type == REACH_QUICK_SETTINGS_ACTION_NONE)
    {
        return REACH_OK;
    }

    shell->quick_settings_drag.active =
        action.type == REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME ||
        action.type == REACH_QUICK_SETTINGS_ACTION_SET_SESSION_VOLUME ||
        action.type == REACH_QUICK_SETTINGS_ACTION_SET_BRIGHTNESS;

    if (shell->quick_settings_drag.active &&
        shell->quick_settings.window.ops.set_pointer_move_enabled != nullptr)
    {
        (void)shell->quick_settings.window.ops.set_pointer_move_enabled(
            shell->quick_settings.window.window, 1);
    }
    if (shell->quick_settings_drag.active &&
        shell->quick_settings.window.ops.set_pointer_capture != nullptr)
    {
        (void)shell->quick_settings.window.ops.set_pointer_capture(
            shell->quick_settings.window.window, 1);
    }

    shell->quick_settings_drag.type = hit.type;
    shell->quick_settings_drag.last_level = action.volume_level;
    shell->quick_settings_drag.level_valid = shell->quick_settings_drag.active;
    shell->quick_settings_drag.session_index = hit.session_index;

    reach_copy_utf16(shell->quick_settings_drag.session_instance_id,
                     REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY, hit.session_instance_id);

    reach_shell_execute_quick_settings_action(shell, action);
    return REACH_OK;
}

reach_result reach_shell_update_quick_settings_drag(reach_shell *shell, const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr)
    {
        return REACH_OK;
    }

    reach_shell_request_update(shell);

    reach_rect_f32 track = shell->quick_settings_layout.main_slider_track;
    if (shell->quick_settings_drag.type == REACH_QUICK_SETTINGS_HIT_SESSION_SLIDER &&
        shell->quick_settings_drag.session_index <
            shell->quick_settings_layout.app_volume_row_count)
    {
        track =
            shell->quick_settings_layout.app_volume_rows[shell->quick_settings_drag.session_index]
                .slider_full_range_line;
    }
    else if (shell->quick_settings_drag.type == REACH_QUICK_SETTINGS_HIT_BRIGHTNESS_SLIDER)
    {
        track = shell->quick_settings_layout.brightness_slider_track;
    }

    if (track.width <= 0.0f)
    {
        return REACH_OK;
    }

    float local_x = (float)event->x - shell->quick_settings_bounds.x;
    float next_level = reach_shell_clamp_float((local_x - track.x) / track.width, 0.0f, 1.0f);

    if (shell->quick_settings_drag.level_valid &&
        fabsf(next_level - shell->quick_settings_drag.last_level) < 0.005f)
    {
        return REACH_OK;
    }

    shell->quick_settings_drag.last_level = next_level;
    shell->quick_settings_drag.level_valid = 1;

    reach_quick_settings_action action = {};
    if (shell->quick_settings_drag.type == REACH_QUICK_SETTINGS_HIT_SESSION_SLIDER)
    {
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_SESSION_VOLUME;
    }
    else if (shell->quick_settings_drag.type == REACH_QUICK_SETTINGS_HIT_BRIGHTNESS_SLIDER)
    {
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_BRIGHTNESS;
    }
    else
    {
        action.type = REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME;
    }

    action.volume_level = next_level;
    action.session_index = shell->quick_settings_drag.session_index;

    reach_copy_utf16(action.session_instance_id, REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY,
                     shell->quick_settings_drag.session_instance_id);

    reach_shell_execute_quick_settings_action(shell, action);
    return REACH_OK;
}
