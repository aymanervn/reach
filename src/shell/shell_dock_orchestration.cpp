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
    if (shell == nullptr || item_index >= shell->dock_model.item_count)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (shell->dock_model.items[item_index].pinned)
    {
        size_t pinned_index = shell->dock_model.items[item_index].pinned_index;
        return reach_shell_open_pinned_app(shell, pinned_index, force_new_instance, 0);
    }

    const uint16_t *path = reach_shell_dock_item_path(shell, item_index);
    return reach_shell_open_app(shell, path, nullptr, nullptr, force_new_instance, 0);
}

reach_result reach_shell_execute_dock_item_action(reach_shell *shell, reach_dock_item_action action)
{
    if (shell == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (action.type == REACH_DOCK_ITEM_ACTION_FOCUS_WINDOW)
    {
        return reach_shell_focus_window(shell, action.window, 1);
    }

    if (action.type == REACH_DOCK_ITEM_ACTION_LAUNCH_PINNED)
    {
        return reach_shell_open_pinned_app(shell, action.pinned_index, 0, 0);
    }

    return REACH_OK;
}
