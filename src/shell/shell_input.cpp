#include "shell_internal.h"

static void reach_shell_mark_dirty_for_event(reach_shell *shell, const reach_ui_event *event)
{
    REACH_ASSERT(shell != nullptr);
    REACH_ASSERT(event != nullptr);
    if (shell == nullptr || event == nullptr)
    {
        return;
    }

    switch (event->type)
    {
    case REACH_UI_EVENT_WINDOWS_KEY:
    case REACH_UI_EVENT_ESCAPE:
    case REACH_UI_EVENT_TEXT:
    case REACH_UI_EVENT_BACKSPACE:
    case REACH_UI_EVENT_DELETE:
    case REACH_UI_EVENT_ENTER:
    case REACH_UI_EVENT_ARROW_UP:
    case REACH_UI_EVENT_ARROW_DOWN:
    case REACH_UI_EVENT_ARROW_LEFT:
    case REACH_UI_EVENT_ARROW_RIGHT:
    case REACH_UI_EVENT_HOME:
    case REACH_UI_EVENT_END:
        shell->dirty.layout = 1;
        shell->launcher.dirty_flags = 1;
        break;

    case REACH_UI_EVENT_DOCK_APP_CLICK:
    case REACH_UI_EVENT_TRAY_BUTTON_CLICK:
    case REACH_UI_EVENT_POINTER_UP:
    case REACH_UI_EVENT_POINTER_MOVE:
    case REACH_UI_EVENT_POINTER_LEAVE:
    case REACH_UI_EVENT_POINTER_MIDDLE:
    case REACH_UI_EVENT_POINTER_DOWN:
        break;

    case REACH_UI_EVENT_NONE:
    default:
        break;
    }
}

static int32_t reach_shell_game_mode_allows_event(reach_ui_event_type type)
{
    return type == REACH_UI_EVENT_CONFIG_CHANGED || type == REACH_UI_EVENT_DISPLAY_CHANGED ||
           type == REACH_UI_EVENT_WINDOW_STATE_CHANGED || type == REACH_UI_EVENT_WALLPAPER_CHANGED;
}

static int32_t reach_rect_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return (float)x >= rect.x && (float)x <= rect.x + rect.width && (float)y >= rect.y &&
           (float)y <= rect.y + rect.height;
}

static reach_result reach_shell_handle_pointer_up(reach_shell *shell, const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->has_layout)
    {
        return REACH_OK;
    }

    if (shell->context_menu_state.open)
    {
        reach_context_menu_hit_result context_hit = reach_context_menu_hit_test_items(
            shell->context_menu_state.item_slots, shell->context_menu_state.item_count, event->x,
            event->y);

        reach_context_menu_action context_action =
            reach_context_menu_action_for_hit(shell->context_menu_state.item_commands,
                                              shell->context_menu_state.item_count, context_hit);

        if (context_action.command != 0)
        {
            return reach_shell_execute_context_command(shell, context_action.command);
        }

        reach_shell_close_context_menu(shell);
        return REACH_OK;
    }

    if (shell->dock.window.ops.set_pointer_move_enabled != nullptr)
    {
        (void)shell->dock.window.ops.set_pointer_move_enabled(shell->dock.window.window, 0);
    }

    if (shell->dock_drag.active)
    {
        reach_result drag_result = reach_shell_end_dock_drag(shell);
        if (drag_result != REACH_OK)
        {
            return drag_result;
        }

        if (shell->pressed_dock_index == REACH_MAX_PINNED_APPS)
        {
            return REACH_OK;
        }
    }
    else
    {
        reach_shell_release_dock_item(shell);
    }

    if (shell->quick_settings_drag.active)
    {
        reach_shell_end_quick_settings_drag(shell);
        return REACH_OK;
    }

    if (shell->ui.launcher.open)
    {
        reach_launcher_hit_result launcher_hit =
            reach_launcher_hit_test(&shell->ui, &shell->layout.launcher, event->x, event->y);

        reach_launcher_action launcher_action =
            reach_launcher_action_for_hit(&shell->ui, launcher_hit);

        int32_t launcher_pressed_match = shell->pressed_launcher_hit_type == launcher_hit.type &&
                                         shell->pressed_launcher_index == launcher_hit.index;

        shell->pressed_launcher_hit_type = REACH_LAUNCHER_HIT_NONE;
        shell->pressed_launcher_index = REACH_MAX_PINNED_APPS;

        if (launcher_action.type == REACH_LAUNCHER_ACTION_LAUNCH_PINNED && launcher_pressed_match)
        {
            reach_ui_event routed = {};
            routed.type = REACH_UI_EVENT_DOCK_APP_CLICK;
            routed.id = launcher_action.pin_id;
            return reach_shell_handle_event(shell, &routed);
        }

        if (launcher_action.type == REACH_LAUNCHER_ACTION_OPEN_RESULT && launcher_pressed_match)
        {
            reach_shell_clear_launcher_restore_window(shell);
            (void)reach_shell_open_launcher_result(shell);
            reach_shell_close_launcher(shell);
            return REACH_OK;
        }
    }

    if (shell->tray_state.popup_open && shell->tray_provider.ops.activate != nullptr)
    {
        reach_tray_hit_result tray_hit = reach_tray_hit_test_popup(
            &shell->tray_state.model, shell->tray.last_bounds, event->x, event->y);

        reach_tray_feature_action tray_action = reach_tray_action_for_hit(
            &shell->tray_state.model, tray_hit, REACH_TRAY_ACTION_LEFT_CLICK);

        if (tray_action.type != REACH_TRAY_FEATURE_ACTION_NONE)
        {
            return reach_shell_execute_tray_action(shell, tray_action);
        }
    }

    reach_dock_hit_result dock_hit = reach_dock_hit_test(&shell->layout.dock, event->x, event->y);

    if (dock_hit.type == REACH_DOCK_HIT_TRAY_BUTTON)
    {
        reach_shell_toggle_tray_popup(shell);

        if (!shell->tray_state.popup_open && shell->tray.window.ops.hide != nullptr)
        {
            return shell->tray.window.ops.hide(shell->tray.window.window);
        }

        return REACH_OK;
    }

    if (dock_hit.type == REACH_DOCK_HIT_QUICK_SETTINGS_BUTTON)
    {
        reach_shell_toggle_quick_settings(shell);
        return REACH_OK;
    }

    if (dock_hit.type == REACH_DOCK_HIT_POWER_BUTTON)
    {
        if (shell->suppress_power_button_release)
        {
            shell->suppress_power_button_release = 0;
            return REACH_OK;
        }

        if (shell->context_menu_state.open && shell->context_menu_state.power_open)
        {
            reach_shell_close_context_menu(shell);
            return REACH_OK;
        }

        return reach_shell_show_power_context_menu(shell);
    }

    if (shell->tray_state.popup_open &&
        !reach_rect_contains(shell->tray.last_bounds, event->x, event->y))
    {
        reach_shell_set_tray_popup_open(shell, 0);

        if (shell->tray.window.ops.hide != nullptr)
        {
            (void)shell->tray.window.ops.hide(shell->tray.window.window);
        }
    }

    if (dock_hit.type == REACH_DOCK_HIT_ITEM && shell->pressed_dock_index == dock_hit.index)
    {
        shell->pressed_dock_index = REACH_MAX_PINNED_APPS;

        reach_dock_item_action action =
            reach_dock_item_action_for_index(&shell->dock_model, dock_hit.index);

        reach_shell_release_dock_item(shell);
        shell->dock.dirty_flags = 1;
        reach_shell_schedule_dock_reveal_recheck(shell);
        reach_shell_request_update(shell);

        return reach_shell_execute_dock_item_action(shell, action);
    }

    shell->pressed_dock_index = REACH_MAX_PINNED_APPS;
    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_down(reach_shell *shell, const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->has_layout)
    {
        return REACH_OK;
    }

    reach_shell_clear_sticky_dock_feedback(shell);

    if (shell->quick_settings_open)
    {
        reach_dock_hit_result dock_hit =
            reach_dock_hit_test(&shell->layout.dock, event->x, event->y);

        if (dock_hit.type == REACH_DOCK_HIT_QUICK_SETTINGS_BUTTON)
        {
            reach_shell_press_quick_settings_button(shell);
            return REACH_OK;
        }

        if (reach_rect_contains(shell->quick_settings_bounds, event->x, event->y))
        {
            return reach_shell_begin_quick_settings_drag_if_hit(shell, event);
        }

        reach_shell_set_quick_settings_open(shell, 0);
        return REACH_OK;
    }

    if (shell->context_menu_state.open)
    {
        reach_dock_hit_result dock_hit =
            reach_dock_hit_test(&shell->layout.dock, event->x, event->y);

        if (shell->context_menu_state.power_open && dock_hit.type == REACH_DOCK_HIT_POWER_BUTTON)
        {
            reach_shell_close_context_menu(shell);
            reach_shell_clear_sticky_dock_feedback(shell);
            shell->suppress_power_button_release = 1;
            return REACH_OK;
        }

        if (!reach_rect_contains(shell->context_menu_state.bounds, event->x, event->y))
        {
            reach_shell_close_context_menu(shell);
        }
        return REACH_OK;
    }

    reach_dock_hit_result dock_hit = reach_dock_hit_test(&shell->layout.dock, event->x, event->y);

    if (shell->ui.launcher.open)
    {
        reach_launcher_hit_result launcher_hit =
            reach_launcher_hit_test(&shell->ui, &shell->layout.launcher, event->x, event->y);

        if (launcher_hit.type == REACH_LAUNCHER_HIT_NONE &&
            !reach_rect_contains(shell->layout.launcher.bounds, event->x, event->y))
        {
            if (dock_hit.type != REACH_DOCK_HIT_NONE)
            {
                reach_shell_keep_dock_revealed(shell);
                reach_shell_close_launcher_without_focus_restore(shell);
            }
            else
            {
                reach_shell_close_launcher(shell);
                return REACH_OK;
            }
        }
        else
        {
            if (launcher_hit.type == REACH_LAUNCHER_HIT_SEARCH_RESULT &&
                launcher_hit.index < shell->ui.launcher.result_count)
            {
                shell->ui.launcher.selected_result_index = launcher_hit.index;
                shell->launcher.dirty_flags = 1;
            }

            shell->pressed_launcher_hit_type = launcher_hit.type;
            shell->pressed_launcher_index = launcher_hit.index;
            return REACH_OK;
        }
    }

    if (shell->suppress_power_button_release && dock_hit.type != REACH_DOCK_HIT_POWER_BUTTON)
    {
        shell->suppress_power_button_release = 0;
    }

    if (dock_hit.type == REACH_DOCK_HIT_TRAY_BUTTON)
    {
        reach_shell_press_tray_button(shell);
        return REACH_OK;
    }

    if (dock_hit.type == REACH_DOCK_HIT_QUICK_SETTINGS_BUTTON)
    {
        reach_shell_press_quick_settings_button(shell);
        return REACH_OK;
    }

    if (dock_hit.type == REACH_DOCK_HIT_POWER_BUTTON)
    {
        reach_shell_press_power_button(shell);
        return REACH_OK;
    }

    if (shell->tray_state.popup_open)
    {
        reach_tray_hit_result tray_hit = reach_tray_hit_test_popup(
            &shell->tray_state.model, shell->tray.last_bounds, event->x, event->y);

        if (tray_hit.type == REACH_TRAY_HIT_ITEM)
        {
            reach_shell_press_tray_item(shell, tray_hit.index);
            return REACH_OK;
        }

        if (tray_hit.type == REACH_TRAY_HIT_NONE)
        {
            return REACH_OK;
        }
    }

    if (dock_hit.type == REACH_DOCK_HIT_ITEM)
    {
        size_t index = dock_hit.index;
        shell->pressed_dock_index = index;

        reach_shell_press_dock_item(shell, index);
        reach_shell_begin_dock_drag(shell, index, event);

        return REACH_OK;
    }

    reach_shell_release_tray_item(shell);
    shell->pressed_dock_index = REACH_MAX_PINNED_APPS;
    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_move(reach_shell *shell, const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->has_layout)
    {
        return REACH_OK;
    }

    if (shell->context_menu_state.open)
    {
        reach_context_menu_hit_result context_hit = reach_context_menu_hit_test_items(
            shell->context_menu_state.item_slots, shell->context_menu_state.item_count, event->x,
            event->y);

        size_t hovered_context = context_hit.hit ? context_hit.index : REACH_MAX_PINNED_APPS;

        if (shell->context_menu_state.hovered_index != hovered_context)
        {
            shell->context_menu_state.hovered_index = hovered_context;
            shell->context_menu.dirty_flags = 1;
        }

        return REACH_OK;
    }

    if (shell->quick_settings_drag.active)
    {
        return reach_shell_update_quick_settings_drag(shell, event);
    }

    if (shell->dock_drag.active)
    {
        return reach_shell_update_dock_drag(shell, event);
    }

    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_middle(reach_shell *shell,
                                                      const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->has_layout)
    {
        return REACH_OK;
    }

    reach_shell_release_dock_item(shell);
    reach_shell_release_tray_item(shell);

    reach_dock_hit_result dock_hit = reach_dock_hit_test(&shell->layout.dock, event->x, event->y);

    if (dock_hit.type == REACH_DOCK_HIT_ITEM)
    {
        return reach_shell_launch_dock_item(shell, dock_hit.index, 1);
    }

    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_leave(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return REACH_OK;
    }

    if (shell->ui.dock.auto_hide && (!shell->dock_reveal.target_hidden ||
                                     shell->dock_reveal.active || shell->dock_animation.animating))
    {
        shell->dock_reveal.check_dirty = 1;
        reach_shell_request_update(shell);
    }

    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_context(reach_shell *shell,
                                                       const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->has_layout)
    {
        return REACH_OK;
    }

    reach_shell_clear_sticky_dock_feedback(shell);

    if (shell->context_menu_state.open)
    {
        reach_shell_close_context_menu(shell);
    }

    if (shell->quick_settings_open)
    {
        reach_shell_set_quick_settings_open(shell, 0);
    }

    if (shell->tray_state.popup_open && shell->tray_provider.ops.activate != nullptr)
    {
        reach_tray_hit_result tray_hit = reach_tray_hit_test_popup(
            &shell->tray_state.model, shell->tray.last_bounds, event->x, event->y);

        reach_tray_feature_action tray_action = reach_tray_action_for_hit(
            &shell->tray_state.model, tray_hit, REACH_TRAY_ACTION_RIGHT_CLICK);

        if (tray_action.type != REACH_TRAY_FEATURE_ACTION_NONE)
        {
            reach_shell_press_tray_item(shell, tray_action.item_index);
            return reach_shell_execute_tray_action(shell, tray_action);
        }
    }

    reach_dock_hit_result dock_hit = reach_dock_hit_test(&shell->layout.dock, event->x, event->y);

    if (dock_hit.type == REACH_DOCK_HIT_ITEM)
    {
        size_t index = dock_hit.index;

        shell->feedback.dock_pressed = 1;
        shell->feedback.dock_sticky = 0;

        reach_shell_set_dock_click_feedback_immediate(shell, index, 0.50f);
        (void)reach_shell_render_dock_surface(shell, &shell->layout.dock);

        reach_result result =
            reach_shell_show_dock_app_context_menu(shell, index, event->x, event->y);

        reach_shell_stick_dock_item(shell);
        (void)reach_shell_render_dock_surface(shell, &shell->layout.dock);

        return result;
    }

    return REACH_OK;
}

void reach_shell_on_window_event(void *user, const reach_ui_event *event)
{
    reach_shell *shell = static_cast<reach_shell *>(user);
    if (shell != nullptr && event != nullptr)
    {
        (void)reach_shell_handle_event(shell, event);
    }
}

reach_result reach_shell_handle_event(reach_shell *shell, const reach_ui_event *event)
{
    REACH_ASSERT(shell != nullptr);
    REACH_ASSERT(event != nullptr);
    if (shell == nullptr || event == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_ui_intent intent = {};

    if (reach_shell_game_mode_enabled(shell) && !reach_shell_game_mode_allows_event(event->type))
    {
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_ESCAPE && shell->quick_settings_open)
    {
        reach_shell_set_quick_settings_open(shell, 0);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_ESCAPE && shell->context_menu_state.open)
    {
        reach_shell_close_context_menu(shell);
        return REACH_OK;
    }

    int32_t launcher_was_open = shell->ui.launcher.open;

    if (event->type == REACH_UI_EVENT_POINTER_UP)
    {
        return reach_shell_handle_pointer_up(shell, event);
    }

    if (event->type == REACH_UI_EVENT_POINTER_DOWN)
    {
        return reach_shell_handle_pointer_down(shell, event);
    }

    if (event->type == REACH_UI_EVENT_POINTER_MOVE)
    {
        return reach_shell_handle_pointer_move(shell, event);
    }

    if (event->type == REACH_UI_EVENT_POINTER_MIDDLE)
    {
        return reach_shell_handle_pointer_middle(shell, event);
    }

    if (event->type == REACH_UI_EVENT_POINTER_LEAVE)
    {
        return reach_shell_handle_pointer_leave(shell);
    }

    if (event->type == REACH_UI_EVENT_POINTER_CONTEXT)
    {
        return reach_shell_handle_pointer_context(shell, event);
    }

    if (event->type == REACH_UI_EVENT_WALLPAPER_CHANGED)
    {
        reach_shell_reload_wallpaper(shell, 1);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_CONFIG_CHANGED)
    {
        if (reach_shell_apply_config_reload_result(shell))
        {
            return REACH_OK;
        }
        return reach_shell_schedule_config_reload(shell);
    }

    if (event->type == REACH_UI_EVENT_LAUNCHER_SEARCH_READY)
    {
        reach_shell_apply_launcher_search_results(shell);
        reach_shell_apply_launcher_result_icon_results(shell);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_DISPLAY_CHANGED)
    {
        shell->dirty.monitors = 1;
        shell->dirty.layout = 1;
        shell->dock.dirty_flags = 1;
        shell->launcher.dirty_flags = 1;
        shell->tray.dirty_flags = 1;
        shell->switcher.dirty_flags = 1;
        shell->context_menu.dirty_flags = 1;
        shell->quick_settings.dirty_flags = 1;
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_WINDOW_STATE_CHANGED)
    {
        if (shell->window_manager.ops.refresh != nullptr)
        {
            (void)shell->window_manager.ops.refresh(shell->window_manager.manager);
        }

        int32_t open_windows_changed = 0;
        (void)reach_shell_refresh_open_windows(shell, &open_windows_changed);

        uintptr_t foreground_window =
            shell->window_manager.ops.foreground != nullptr
                ? shell->window_manager.ops.foreground(shell->window_manager.manager)
                : 0;
        int32_t foreground_changed = shell->foreground_window != foreground_window;
        reach_shell_note_foreground_window(shell, foreground_window);

        if (open_windows_changed || foreground_changed)
        {
            reach_shell_refresh_switcher_windows(shell);
            shell->dock.dirty_flags = 1;
            shell->switcher.dirty_flags = 1;
        }
        if (foreground_changed && foreground_window != 0 && shell->ui.launcher.open)
        {
            reach_shell_clear_launcher_restore_window(shell);
            reach_shell_close_launcher(shell);
        }
        (void)reach_shell_update_game_mode(shell);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_WINDOWS_D_MINIMIZE_ALL)
    {
        if (shell->window_manager.ops.refresh != nullptr)
        {
            (void)shell->window_manager.ops.refresh(shell->window_manager.manager);
            (void)reach_shell_refresh_open_windows(shell, nullptr);
        }

        uintptr_t windows[REACH_MAX_PINNED_APPS] = {};
        size_t window_count = 0;
        for (size_t index = 0; index < shell->open_window_count && window_count < REACH_MAX_PINNED_APPS;
             ++index)
        {
            if (shell->open_windows[index].id != 0 && !shell->open_windows[index].minimized)
            {
                windows[window_count++] = shell->open_windows[index].id;
            }
        }

        if (window_count > 0)
        {
            (void)reach_shell_schedule_minimize_windows(shell, windows, window_count);
        }
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_ALT_TAB_BEGIN || event->type == REACH_UI_EVENT_ALT_TAB_NEXT ||
        event->type == REACH_UI_EVENT_ALT_TAB_PREVIOUS ||
        event->type == REACH_UI_EVENT_ALT_TAB_COMMIT ||
        event->type == REACH_UI_EVENT_ALT_TAB_CANCEL)
    {
        return reach_shell_handle_switcher_event(shell, event);
    }

    if (event->type == REACH_UI_EVENT_WINDOWS_KEY && !shell->ui.launcher.open)
    {
        reach_shell_remember_launcher_restore_window(shell);
    }

    reach_result result = reach_ui_handle_event(&shell->ui, event, &intent);
    if (result != REACH_OK)
    {
        return result;
    }

    reach_shell_mark_dirty_for_event(shell, event);

    if (launcher_was_open != shell->ui.launcher.open)
    {
        if (!shell->ui.launcher.open)
        {
            reach_shell_cancel_launcher_search(shell);
            reach_shell_release_launcher_result_icons(shell);
            (void)reach_ui_state_clear_launcher_results(&shell->ui);
        }

        reach_shell_sync_popup_mouse_hook(shell);
    }

    if (intent.type == REACH_UI_INTENT_OPEN_TRAY_MENU)
    {
        reach_shell_toggle_tray_popup(shell);
    }
    else if (intent.type == REACH_UI_INTENT_LAUNCH_APP)
    {
        int32_t launcher_launch = shell->ui.launcher.open;
        if (launcher_launch)
        {
            reach_shell_clear_launcher_restore_window(shell);
        }

        for (size_t index = 0; index < shell->ui.pinned_app_count; ++index)
        {
            if (shell->ui.pinned_apps[index].id == intent.id &&
                shell->app_launcher.ops.launch != nullptr)
            {
                reach_app_launch_request request = {};

                reach_copy_utf16(request.path, 260, shell->ui.pinned_apps[index].path);

                reach_copy_utf16(request.arguments, 260, shell->ui.pinned_apps[index].arguments);

                (void)shell->app_launcher.ops.launch(shell->app_launcher.launcher, &request);
            }
        }

        if (launcher_launch)
        {
            reach_shell_close_launcher_without_focus_restore(shell);
        }
    }
    else if (intent.type == REACH_UI_INTENT_RUN_SEARCH)
    {
        (void)reach_shell_schedule_launcher_search(shell);
    }
    else if (intent.type == REACH_UI_INTENT_OPEN_LAUNCHER_RESULT)
    {
        reach_shell_clear_launcher_restore_window(shell);
        (void)reach_shell_open_launcher_result(shell);
        reach_shell_close_launcher(shell);
    }

    if (shell->launcher.window.ops.show != nullptr && shell->launcher.window.ops.hide != nullptr)
    {
        if (shell->ui.launcher.open)
        {
            reach_result show_result =
                shell->launcher.window.ops.show(shell->launcher.window.window);

            if (show_result == REACH_OK)
            {
                reach_shell_raise_launcher(shell);
            }

            return show_result;
        }

        reach_result hide_result = shell->launcher.window.ops.hide(shell->launcher.window.window);
        reach_shell_restore_launcher_focus(shell);
        return hide_result;
    }

    return REACH_OK;
}
