#include "host_internal.h"

static void reach_host_close_surface(reach_host *host, const reach_feature_runtime *desc)
{
    if (desc->definition->dismiss != nullptr)
    {
        desc->definition->dismiss(host);
    }
    else if (desc->definition->force_close != nullptr)
    {
        desc->definition->force_close(host);
    }
}

reach_result reach_host_apply_feature_action(reach_host *host, const reach_feature_runtime *desc,
                                             const reach_capsule_pointer_result *result)
{
    if (host == nullptr || desc == nullptr || result == nullptr)
    {
        return REACH_OK;
    }

    const reach_capsule_action *action = &result->action;

    switch (action->kind)
    {
    case REACH_FEATURE_ACTION_CLOSE_SELF:
        reach_host_close_surface(host, desc);
        return REACH_OK;

    case REACH_FEATURE_ACTION_OPEN_PINNED_APP:
        return reach_host_open_pinned_app(host, action->index, 0, 0);

    case REACH_FEATURE_ACTION_OPEN_PINNED_APP_BY_ID:
        return reach_host_open_pinned_app_id(
            host, (uint32_t)action->id, 0,
            (action->flags & REACH_FEATURE_ACTION_FLAG_DEFER_UNTIL_CLOSED) != 0);

    case REACH_FEATURE_ACTION_LAUNCH_NEW_INSTANCE:
        return reach_host_launch_dock_item(host, action->index, 1);

    case REACH_FEATURE_ACTION_MOVE_PIN:
        return reach_host_move_pin(host, (uint32_t)action->id, action->index);

    case REACH_FEATURE_ACTION_REBUILD_ITEMS:
    {
        reach_dock_build_context build_ctx = reach_host_dock_build_context(host);
        reach_dock_rebuild_items(
            reach_host_feature_capsule<reach_dock>(host, REACH_SURFACE_ID_DOCK), &build_ctx,
            &host->layout.dock, &host->layout.dock);
        return REACH_OK;
    }

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
        return reach_host_execute_context_command(host, (uint32_t)action->id);

    case REACH_FEATURE_ACTION_OPEN_SEARCH_RESULT:
        return reach_host_open_launcher_result_and_close_transients(host);

    case REACH_FEATURE_ACTION_REVEAL_SEARCH_RESULT:
    {
        reach_result reveal_result = reach_host_reveal_launcher_result(host, action->index);
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
