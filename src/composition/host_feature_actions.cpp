#include "host_internal.h"

static void reach_host_close_surface(reach_host *host, const reach_feature_runtime *desc)
{
    reach_host_close_registered_surface(host, desc->definition->id,
                                        REACH_SURFACE_CLOSE_SUPERSEDED);
}

reach_result reach_host_apply_feature_action(reach_host *host, const reach_feature_runtime *desc,
                                             const reach_capsule_action *action)
{
    if (host == nullptr || desc == nullptr || action == nullptr)
    {
        return REACH_OK;
    }

    if ((action->flags & REACH_FEATURE_ACTION_FLAG_CLOSE_SELF_FIRST) != 0)
    {
        reach_host_close_surface(host, desc);
    }

    switch (action->kind)
    {
    case REACH_FEATURE_ACTION_CLOSE_SELF:
        reach_host_close_surface(host, desc);
        return REACH_OK;

    case REACH_FEATURE_ACTION_OPEN_PINNED_APP:
        return reach_host_open_pinned_app(host, action->index, 0, desc->definition->id, 0);

    case REACH_FEATURE_ACTION_OPEN_PINNED_APP_BY_ID:
        return reach_host_open_pinned_app_id(
            host, (uint32_t)action->id, 0, desc->definition->id,
            (action->flags & REACH_FEATURE_ACTION_FLAG_DEFER_UNTIL_CLOSED) != 0);

    case REACH_FEATURE_ACTION_MOVE_PIN:
        return reach_host_move_pin(host, (uint32_t)action->id, action->index);

    case REACH_FEATURE_ACTION_FOCUS_WINDOW:
        reach_host_close_surface(host, desc);
        return reach_host_focus_window(host, action->window, 0);

    case REACH_FEATURE_ACTION_TOGGLE_WINDOW_FOCUS:
        return reach_host_focus_window(host, action->window, 1);

    case REACH_FEATURE_ACTION_ACTIVATE_WINDOW:
    {
        reach_result activate_result =
            action->window != 0 ? reach_host_schedule_window_control(
                                      host, REACH_WINDOW_CONTROL_ACTIVATE, action->window)
                                : REACH_OK;
        reach_host_close_surface(host, desc);
        return activate_result;
    }

    case REACH_FEATURE_ACTION_CLOSE_WINDOW:
        return action->window != 0 ? reach_host_close_window(host, action->window) : REACH_OK;

    case REACH_FEATURE_ACTION_CLOSE_WINDOWS:
        return action->windows != nullptr && action->window_count > 0
                   ? reach_host_schedule_window_controls(host, REACH_WINDOW_CONTROL_CLOSE,
                                                         action->windows, action->window_count)
                   : REACH_OK;

    case REACH_FEATURE_ACTION_PIN_APP:
        return reach_host_pin_feature_target(host, &action->target, action->window);

    case REACH_FEATURE_ACTION_UNPIN_APP:
        return action->id != 0 ? reach_host_unpin_id(host, (uint32_t)action->id) : REACH_ERROR;

    case REACH_FEATURE_ACTION_MINIMIZE_ALL_WINDOWS:
        reach_host_close_surface(host, desc);
        return reach_host_schedule_minimize_open_windows(host);

    case REACH_FEATURE_ACTION_MEDIA_CONTROL:
        return reach_host_execute_media_action(host, (reach_now_playing_action)action->id);

    case REACH_FEATURE_ACTION_CYCLE_INPUT_LANGUAGE:
        return reach_host_cycle_input_language(host);

    case REACH_FEATURE_ACTION_OPEN_SETTINGS_APP:
        return reach_host_launch_settings_app(host);

    case REACH_FEATURE_ACTION_EXECUTE_MENU_COMMAND:
        return reach_host_execute_power_command(host, (uint32_t)action->id);

    case REACH_FEATURE_ACTION_OPEN_TARGET:
    {
        reach_result open_result = reach_host_open_feature_target(
            host, desc->definition->id, &action->target, action->flags);
        if (open_result == REACH_OK)
        {
            reach_host_close_transient_surfaces(host, 0);
        }
        return open_result;
    }

    case REACH_FEATURE_ACTION_REVEAL_TARGET:
    {
        if (action->target.kind != REACH_FEATURE_TARGET_APP || action->target.path == nullptr ||
            action->target.path[0] == 0 || !reach_app_control_reveal_available(host->app_control))
        {
            return REACH_OK;
        }
        reach_result reveal_result = reach_host_schedule_reveal_path(host, action->target.path);
        if (reveal_result == REACH_OK)
        {
            reach_host_close_surface(host, desc);
        }
        return reveal_result;
    }

    case REACH_FEATURE_ACTION_NONE:
    default:
        return REACH_OK;
    }
}
