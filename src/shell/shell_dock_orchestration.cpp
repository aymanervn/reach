#include "shell_internal.h"

const uint16_t *reach_shell_dock_item_path(const reach_shell *shell, size_t item_index)
{
    if (shell == nullptr || item_index >= shell->dock_model.item_count)
    {
        return nullptr;
    }
    if (shell->dock_model.items[item_index].pinned)
    {
        size_t pinned_index = shell->dock_model.items[item_index].pinned_index;
        return pinned_index < shell->ui.pinned_app_count ? shell->ui.pinned_apps[pinned_index].path
                                                         : nullptr;
    }

    size_t open_index = shell->dock_model.items[item_index].open_index;
    return open_index < shell->open_window_count ? shell->open_windows[open_index].path : nullptr;
}

reach_result reach_shell_launch_dock_item(reach_shell *shell, size_t item_index,
                                          int32_t force_new_instance)
{
    if (shell == nullptr || item_index >= shell->dock_model.item_count ||
        shell->app_launcher.ops.launch == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const uint16_t *path = reach_shell_dock_item_path(shell, item_index);
    if (path == nullptr || path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_app_launch_request request = {};
    reach_copy_utf16(request.path, 260, path);
    if (shell->dock_model.items[item_index].pinned &&
        shell->dock_model.items[item_index].pinned_index < shell->ui.pinned_app_count)
    {
        reach_copy_utf16(
            request.arguments, 260,
            shell->ui.pinned_apps[shell->dock_model.items[item_index].pinned_index].arguments);
    }
    request.force_new_instance = force_new_instance;
    return shell->app_launcher.ops.launch(shell->app_launcher.launcher, &request);
}

reach_result reach_shell_execute_dock_item_action(reach_shell *shell, reach_dock_item_action action)
{
    if (shell == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (action.type == REACH_DOCK_ITEM_ACTION_FOCUS_WINDOW)
    {
        uintptr_t window_id = action.window;
        if (window_id == 0)
        {
            return REACH_OK;
        }
        if (shell->window_manager.ops.refresh != nullptr)
        {
            (void)shell->window_manager.ops.refresh(shell->window_manager.manager);
            (void)reach_shell_refresh_open_windows(shell, nullptr);
        }
        uintptr_t foreground;
        int32_t minimized;
        reach_result result = REACH_OK;
        if (reach_shell_window_is_settings_window(shell, window_id))
        {
            foreground =
                reach_shell_foreground_is_settings_window(shell, window_id) ? window_id : 0;
            minimized = reach_shell_settings_window_minimized(shell);
        }
        else
        {
            foreground = shell->window_manager.ops.foreground != nullptr
                             ? shell->window_manager.ops.foreground(shell->window_manager.manager)
                             : 0;
            minimized = reach_shell_window_is_minimized(shell, window_id);
        }
        if (foreground == window_id && !minimized)
        {
            result = reach_shell_schedule_window_control(shell, REACH_SHELL_WINDOW_CONTROL_MINIMIZE,
                                                         window_id);
        }
        else
        {
            result = reach_shell_schedule_window_control(shell, REACH_SHELL_WINDOW_CONTROL_ACTIVATE,
                                                         window_id);
        }
        shell->dock.dirty_flags = 1;
        return result;
    }
    if (action.type == REACH_DOCK_ITEM_ACTION_LAUNCH_PINNED)
    {
        if (action.pinned_index >= shell->ui.pinned_app_count)
        {
            return REACH_OK;
        }
        reach_ui_event routed = {};
        routed.type = REACH_UI_EVENT_DOCK_APP_CLICK;
        routed.id = shell->ui.pinned_apps[action.pinned_index].id;
        return reach_shell_handle_event(shell, &routed);
    }
    return REACH_OK;
}
