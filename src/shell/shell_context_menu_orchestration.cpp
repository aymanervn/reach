#include "shell_internal.h"

void reach_shell_close_context_menu(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }
    shell->context_menu_state.open = 0;
    shell->context_menu_state.power_open = 0;
    shell->context_menu_state.target_index = REACH_MAX_PINNED_APPS;
    shell->context_menu_state.hovered_index = REACH_MAX_PINNED_APPS;
    shell->context_menu_state.item_count = 0;
    for (size_t index = 0; index < REACH_CONTEXT_MENU_MAX_ITEMS; ++index)
    {
        shell->context_menu_state.item_icon_ids[index] = 0;
    }
    shell->context_menu.dirty_flags = 1;
    if (shell->context_menu.window.ops.set_pointer_move_enabled != nullptr)
    {
        (void)shell->context_menu.window.ops.set_pointer_move_enabled(
            shell->context_menu.window.window, 0);
    }
    if (shell->context_menu.window.ops.hide != nullptr)
    {
        (void)shell->context_menu.window.ops.hide(shell->context_menu.window.window);
    }
    reach_shell_sync_popup_mouse_hook(shell);
}

reach_result reach_shell_execute_context_command(reach_shell *shell, uint32_t command)
{
    if (shell == nullptr)
    {
        return REACH_OK;
    }

    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_LOCK)
    {
        reach_shell_close_context_menu(shell);
        return shell->power_session.ops.lock != nullptr
                   ? shell->power_session.ops.lock(shell->power_session.session)
                   : REACH_ERROR;
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_SLEEP)
    {
        reach_shell_close_context_menu(shell);
        return shell->power_session.ops.sleep != nullptr
                   ? shell->power_session.ops.sleep(shell->power_session.session)
                   : REACH_ERROR;
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_RESTART)
    {
        reach_shell_close_context_menu(shell);
        return shell->power_session.ops.restart != nullptr
                   ? shell->power_session.ops.restart(shell->power_session.session)
                   : REACH_ERROR;
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_SHUTDOWN)
    {
        reach_shell_close_context_menu(shell);
        return shell->power_session.ops.shutdown != nullptr
                   ? shell->power_session.ops.shutdown(shell->power_session.session)
                   : REACH_ERROR;
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_SIGN_OUT)
    {
        reach_shell_close_context_menu(shell);
        return shell->power_session.ops.sign_out != nullptr
                   ? shell->power_session.ops.sign_out(shell->power_session.session)
                   : REACH_ERROR;
    }

    if (shell->context_menu_state.target_index >= shell->dock_model.item_count)
    {
        return REACH_OK;
    }
    size_t item_index = shell->context_menu_state.target_index;
    uint16_t item_path[260] = {};
    const uint16_t *path = reach_shell_dock_item_path(shell, item_index);
    if (path != nullptr)
    {
        (void)reach_copy_utf16(item_path, 260, path);
    }
    size_t pinned_index = shell->dock_model.items[item_index].pinned_index;
    uint32_t pin_id =
        pinned_index < shell->ui.pinned_app_count ? shell->ui.pinned_apps[pinned_index].id : 0;
    uintptr_t window_id = shell->dock_model.items[item_index].window;
    reach_shell_close_context_menu(shell);

    if (command == REACH_CONTEXT_MENU_COMMAND_OPEN_NEW)
    {
        if (item_path[0] == 0 || shell->app_launcher.ops.launch == nullptr)
        {
            return REACH_INVALID_ARGUMENT;
        }
        reach_app_launch_request request = {};
        reach_copy_utf16(request.path, 260, item_path);
        if (pinned_index < shell->ui.pinned_app_count)
        {
            reach_copy_utf16(request.arguments, 260, shell->ui.pinned_apps[pinned_index].arguments);
        }
        request.force_new_instance = 1;
        return shell->app_launcher.ops.launch(shell->app_launcher.launcher, &request);
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_UNPIN)
    {
        return pin_id != 0 ? reach_shell_schedule_unpin_id(shell, pin_id) : REACH_ERROR;
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_PIN)
    {
        reach_pinned_app_model app = {};

        size_t open_index = shell->dock_model.items[item_index].open_index;
        if (open_index < shell->open_window_count)
        {
            const reach_window_snapshot *window = &shell->open_windows[open_index];

            if (shell->window_manager.ops.pin_app_for_window != nullptr &&
                shell->window_manager.ops.pin_app_for_window(shell->window_manager.manager,
                                                             window->id, window, &app) == REACH_OK)
            {
            }
            else
            {
                if (window->title[0] != 0)
                {
                    (void)reach_copy_utf16(app.title, 128, window->title);
                }
                (void)reach_copy_utf16(app.path, 260, window->path);
                (void)reach_copy_utf16(app.icon_ref, 260, window->path);
                (void)reach_copy_utf16(app.app_user_model_id, 260, window->app_user_model_id);
            }
        }
        else if (item_path[0] != 0)
        {
            (void)reach_copy_utf16(app.path, 260, item_path);
            (void)reach_copy_utf16(app.icon_ref, 260, item_path);
        }

        return app.path[0] != 0 ? reach_shell_schedule_pin_app(shell, &app) : REACH_ERROR;
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_CLOSE && shell->window_manager.ops.close != nullptr)
    {
        return shell->window_manager.ops.close(shell->window_manager.manager, window_id);
    }

    return REACH_OK;
}

reach_result reach_shell_show_power_context_menu(reach_shell *shell)
{
    if (shell == nullptr || !shell->has_layout)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_shell_set_quick_settings_open(shell, 0);
    reach_shell_set_tray_popup_open(shell, 0);

    reach_context_menu_build_power_commands(shell->context_menu_state.item_commands,
                                            shell->context_menu_state.item_icon_ids,
                                            &shell->context_menu_state.item_count);

    float popup_width = 176.0f;
    float item_height = 34.0f;
    float padding = 8.0f;
    float notch_height = reach_popup_notch_height();
    float popup_body_height =
        padding * 2.0f + item_height * (float)shell->context_menu_state.item_count;
    float popup_height = popup_body_height + notch_height;
    float anchor_x =
        shell->layout.dock.power_button.x + shell->layout.dock.power_button.width * 0.5f;
    float popup_x = anchor_x - popup_width * 0.72f;
    float popup_y = shell->layout.dock.bounds.y - popup_height - 8.0f;

    reach_rect_f32 monitor = {};
    const reach_monitor_info *primary_monitor =
        shell->monitors.list != nullptr && shell->monitors.ops.primary != nullptr
            ? shell->monitors.ops.primary(shell->monitors.list)
            : nullptr;
    if (primary_monitor != nullptr)
    {
        monitor.x = (float)primary_monitor->bounds.left;
        monitor.y = (float)primary_monitor->bounds.top;
        monitor.width = (float)(primary_monitor->bounds.right - primary_monitor->bounds.left);
        monitor.height = (float)(primary_monitor->bounds.bottom - primary_monitor->bounds.top);
    }
    else
    {
        monitor = shell->layout.dock.bounds;
    }
    if (popup_x < monitor.x + 8.0f)
    {
        popup_x = monitor.x + 8.0f;
    }
    float max_x = monitor.x + monitor.width - popup_width - 8.0f;
    if (popup_x > max_x)
    {
        popup_x = max_x;
    }
    if (popup_y < monitor.y + 8.0f)
    {
        popup_y = monitor.y + 8.0f;
    }

    shell->context_menu_state.bounds = {popup_x, popup_y, popup_width, popup_height};
    for (size_t index = 0; index < shell->context_menu_state.item_count; ++index)
    {
        shell->context_menu_state.item_slots[index] = {
            popup_x + padding, popup_y + padding + item_height * (float)index,
            popup_width - padding * 2.0f, item_height};
    }
    shell->context_menu_state.target_index = REACH_MAX_PINNED_APPS;
    shell->context_menu_state.hovered_index = REACH_MAX_PINNED_APPS;
    shell->context_menu_state.power_open = 1;
    shell->context_menu_state.open = 1;
    shell->context_menu.dirty_flags = 1;
    if (shell->context_menu.window.ops.set_pointer_move_enabled != nullptr)
    {
        (void)shell->context_menu.window.ops.set_pointer_move_enabled(
            shell->context_menu.window.window, 1);
    }
    reach_shell_sync_popup_mouse_hook(shell);
    return REACH_OK;
}

reach_result reach_shell_show_dock_app_context_menu(reach_shell *shell, size_t item_index,
                                                    int32_t x, int32_t y)
{
    (void)x;
    (void)y;
    if (shell == nullptr || item_index >= shell->dock_model.item_count)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_shell_set_quick_settings_open(shell, 0);
    reach_shell_set_tray_popup_open(shell, 0);

    reach_context_menu_build_dock_item_commands(
        shell->dock_model.items[item_index].pinned,
        reach_shell_dock_item_path(shell, item_index) != nullptr,
        shell->dock_model.items[item_index].window != 0, shell->context_menu_state.item_commands,
        &shell->context_menu_state.item_count);
    for (size_t index = 0; index < REACH_CONTEXT_MENU_MAX_ITEMS; ++index)
    {
        shell->context_menu_state.item_icon_ids[index] = 0;
    }

    float popup_width = 208.0f;
    float item_height = 34.0f;
    float padding = 8.0f;
    float notch_height = reach_popup_notch_height();
    float anchor_ratio = 0.30f;
    float popup_body_height =
        padding * 2.0f + item_height * (float)shell->context_menu_state.item_count;
    float popup_height = popup_body_height + notch_height;
    float popup_x = (float)x - popup_width * anchor_ratio;
    float popup_y = (float)y - popup_height;
    if (shell->has_layout && item_index < shell->layout.dock.app_slot_count)
    {
        reach_rect_f32 slot = shell->layout.dock.app_slots[item_index];
        popup_x = slot.x + slot.width * 0.5f - popup_width * anchor_ratio;
        popup_y = shell->layout.dock.bounds.y - popup_height - 8.0f;
        reach_rect_f32 monitor = {};
        const reach_monitor_info *primary_monitor =
            shell->monitors.list != nullptr && shell->monitors.ops.primary != nullptr
                ? shell->monitors.ops.primary(shell->monitors.list)
                : nullptr;
        if (primary_monitor != nullptr)
        {
            monitor.x = (float)primary_monitor->bounds.left;
            monitor.y = (float)primary_monitor->bounds.top;
            monitor.width = (float)(primary_monitor->bounds.right - primary_monitor->bounds.left);
            monitor.height = (float)(primary_monitor->bounds.bottom - primary_monitor->bounds.top);
        }
        else
        {
            monitor = shell->layout.dock.bounds;
        }
        if (popup_x < monitor.x + 8.0f)
        {
            popup_x = monitor.x + 8.0f;
        }
        float max_x = monitor.x + monitor.width - popup_width - 8.0f;
        if (popup_x > max_x)
        {
            popup_x = max_x;
        }
        if (popup_y < monitor.y + 8.0f)
        {
            popup_y = monitor.y + 8.0f;
        }
    }

    shell->context_menu_state.bounds = {popup_x, popup_y, popup_width, popup_height};
    for (size_t index = 0; index < shell->context_menu_state.item_count; ++index)
    {
        shell->context_menu_state.item_slots[index] = {
            popup_x + padding, popup_y + padding + item_height * (float)index,
            popup_width - padding * 2.0f, item_height};
    }
    shell->context_menu_state.target_index = item_index;
    shell->context_menu_state.hovered_index = REACH_MAX_PINNED_APPS;
    shell->context_menu_state.power_open = 0;
    shell->context_menu_state.open = 1;
    shell->context_menu.dirty_flags = 1;
    if (shell->context_menu.window.ops.set_pointer_move_enabled != nullptr)
    {
        (void)shell->context_menu.window.ops.set_pointer_move_enabled(
            shell->context_menu.window.window, 1);
    }
    reach_shell_sync_popup_mouse_hook(shell);
    return REACH_OK;
}
