#include "host_internal.h"

static void reach_host_close_surface(reach_host *host, const reach_feature_runtime *desc)
{
    reach_host_close_registered_surface(host, desc->definition->id, REACH_SURFACE_CLOSE_SUPERSEDED);
}

static void reach_host_arm_close_handoff(reach_host *host, reach_feature_runtime *desc,
                                         const reach_capsule_action *action, uint64_t request_id)
{
    if ((action->flags & REACH_FEATURE_ACTION_FLAG_CLOSE_HANDOFF) == 0 || request_id == 0 ||
        desc->surface == nullptr)
    {
        return;
    }
    desc->surface->close_handoff_request_id = request_id;
    desc->surface->close_handoff_pending = 1;
    if (action->kind == REACH_FEATURE_ACTION_ACTIVATE_WINDOW && action->window != 0)
    {
        reach_host_set_native_overlay_front_source(host, desc, action->window);
    }
    if (desc->definition != nullptr && desc->definition->capsule_ops != nullptr &&
        desc->definition->capsule_ops->set_close_handoff_pending != nullptr)
    {
        reach_feature_tick_result tick = {};
        desc->definition->capsule_ops->set_close_handoff_pending(desc->capsule, 1, &tick);
        reach_host_apply_feature_tick_result(host, desc, &tick);
    }
}

reach_result reach_host_apply_feature_action(reach_host *host, reach_feature_runtime *desc,
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

    case REACH_FEATURE_ACTION_MOVE_PIN:
        return reach_host_move_pin(host, (uint32_t)action->id, action->index);

    case REACH_FEATURE_ACTION_FOCUS_WINDOW:
        reach_host_close_surface(host, desc);
        return reach_host_focus_window(host, action->window, 0);

    case REACH_FEATURE_ACTION_TOGGLE_WINDOW_FOCUS:
        return reach_host_focus_window(host, action->window, 1);

    case REACH_FEATURE_ACTION_ACTIVATE_WINDOW:
    {
        uint64_t request_id = 0;
        reach_result activate_result =
            action->window != 0
                ? reach_host_schedule_window_control(host, REACH_WINDOW_CONTROL_ACTIVATE,
                                                     action->window, &request_id)
                : REACH_OK;
        if (activate_result == REACH_OK)
        {
            reach_host_arm_close_handoff(host, desc, action, request_id);
        }
        if ((action->flags & REACH_FEATURE_ACTION_FLAG_CLOSE_SELF_FIRST) == 0)
        {
            reach_host_close_surface(host, desc);
        }
        return activate_result;
    }

    case REACH_FEATURE_ACTION_CLOSE_WINDOW:
        return action->window != 0 ? reach_host_close_window(host, action->window) : REACH_OK;

    case REACH_FEATURE_ACTION_CLOSE_WINDOWS:
        return action->windows != nullptr && action->window_count > 0
                   ? reach_host_schedule_window_controls(host, REACH_WINDOW_CONTROL_CLOSE,
                                                         action->windows, action->window_count,
                                                         nullptr)
                   : REACH_OK;

    case REACH_FEATURE_ACTION_PIN_APP:
        return reach_host_pin_feature_target(host, &action->target, action->window);

    case REACH_FEATURE_ACTION_UNPIN_APP:
        return action->id != 0 ? reach_host_unpin_id(host, (uint32_t)action->id) : REACH_ERROR;

    case REACH_FEATURE_ACTION_MINIMIZE_ALL_WINDOWS:
    {
        uint64_t request_id = 0;
        reach_result minimize_result = reach_host_schedule_minimize_open_windows(host, &request_id);
        if (minimize_result == REACH_OK)
        {
            reach_host_arm_close_handoff(host, desc, action, request_id);
        }
        if ((action->flags & REACH_FEATURE_ACTION_FLAG_CLOSE_SELF_FIRST) == 0)
        {
            reach_host_close_surface(host, desc);
        }
        return minimize_result;
    }

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
            host, desc->definition->id, &action->target, action->flags, (uint32_t)action->id);
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
