#include "host_internal.h"

void reach_host_close_context_menu(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }
    int32_t was_open = reach_context_menu_is_open(host->context_menu_capsule);
    reach_context_menu_reset(host->context_menu_capsule);
    reach_host_surface_transition_set(host, &host->context_menu_transition, 0);
    host->context_menu.dirty_flags = 1;
    reach_host_sync_pointer_move_subscriptions(host);
    if (was_open)
    {
        reach_host_request_dock_visibility_update(host);
    }
    reach_host_sync_popup_mouse_hook(host);
}

static reach_result reach_host_launch_context_menu_item(reach_host *host, const uint16_t *path,
                                                        size_t pinned_index, int32_t run_as_admin)
{
    if (host == nullptr || path == nullptr || path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const uint16_t *arguments = nullptr;
    if (pinned_index < host->pinned_app_count)
    {
        arguments = host->pinned_apps[pinned_index].arguments;
    }

    return reach_host_launch_app(host, path, arguments, 1, run_as_admin, 0);
}

static int32_t reach_host_dock_item_command_allowed(reach_host *host, size_t item_index,
                                                    uint32_t command)
{
    uint32_t commands[REACH_CONTEXT_MENU_MAX_ITEMS] = {};
    size_t count = reach_dock_build_item_context_commands(host->dock_capsule, item_index, commands,
                                                          REACH_CONTEXT_MENU_MAX_ITEMS);
    for (size_t index = 0; index < count; ++index)
    {
        if (commands[index] == command)
        {
            return 1;
        }
    }
    return 0;
}

reach_result reach_host_execute_context_command(reach_host *host, uint32_t command)
{
    if (host == nullptr)
    {
        return REACH_OK;
    }

    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_LOCK)
    {
        reach_host_close_context_menu(host);
        return host->power_session.ops.lock != nullptr
                   ? host->power_session.ops.lock(host->power_session.session)
                   : REACH_ERROR;
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_SLEEP)
    {
        reach_host_close_context_menu(host);
        return host->power_session.ops.sleep != nullptr
                   ? host->power_session.ops.sleep(host->power_session.session)
                   : REACH_ERROR;
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_RESTART)
    {
        reach_host_close_context_menu(host);
        return host->power_session.ops.restart != nullptr
                   ? host->power_session.ops.restart(host->power_session.session)
                   : REACH_ERROR;
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_SHUTDOWN)
    {
        reach_host_close_context_menu(host);
        return host->power_session.ops.shutdown != nullptr
                   ? host->power_session.ops.shutdown(host->power_session.session)
                   : REACH_ERROR;
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_SIGN_OUT)
    {
        reach_host_close_context_menu(host);
        return host->power_session.ops.sign_out != nullptr
                   ? host->power_session.ops.sign_out(host->power_session.session)
                   : REACH_ERROR;
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_SETTINGS)
    {
        reach_host_close_context_menu(host);
        if (host->settings_launcher.ops.resolve == nullptr)
        {
            return REACH_ERROR;
        }
        reach_app_launch_request settings_request = {};
        reach_result resolve_result = host->settings_launcher.ops.resolve(
            host->settings_launcher.launcher, settings_request.path, 260,
            settings_request.arguments, 260);
        if (resolve_result != REACH_OK)
        {
            return resolve_result;
        }
        return reach_host_schedule_app_launch(host, &settings_request);
    }

    if (reach_context_menu_state_ptr(host->context_menu_capsule)->target_index >=
        reach_dock_item_count(host->dock_capsule))
    {
        return REACH_OK;
    }
    size_t item_index = reach_context_menu_state_ptr(host->context_menu_capsule)->target_index;
    uint16_t item_path[260] = {};
    const uint16_t *path = reach_host_dock_item_path(host, item_index);
    if (path != nullptr)
    {
        (void)reach_copy_utf16(item_path, 260, path);
    }
    size_t pinned_index = reach_dock_item_at(host->dock_capsule, item_index)->pinned_index;
    uint32_t pin_id =
        pinned_index < host->pinned_app_count ? host->pinned_apps[pinned_index].id : 0;
    uintptr_t window_id = reach_dock_item_at(host->dock_capsule, item_index)->window;
    int32_t command_allowed = reach_host_dock_item_command_allowed(host, item_index, command);
    reach_host_close_context_menu(host);

    if (!command_allowed)
    {
        return REACH_OK;
    }

    if (command == REACH_CONTEXT_MENU_COMMAND_OPEN_NEW)
    {
        return reach_host_launch_context_menu_item(host, item_path, pinned_index, 0);
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_OPEN_AS_ADMIN)
    {
        return reach_host_launch_context_menu_item(host, item_path, pinned_index, 1);
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_UNPIN)
    {
        return pin_id != 0 ? reach_host_schedule_unpin_id(host, pin_id) : REACH_ERROR;
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_PIN)
    {
        reach_pinned_app_model app = {};

        size_t open_index = reach_dock_item_at(host->dock_capsule, item_index)->open_index;
        if (open_index < reach_host_open_window_count(host))
        {
            const reach_window_snapshot *window = &reach_host_open_windows(host)[open_index];

            if (host->window_manager.ops.pin_app_for_window != nullptr &&
                host->window_manager.ops.pin_app_for_window(host->window_manager.manager,
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
                (void)reach_copy_utf16(app.icon_ref, 260,
                                       window->icon_ref[0] != 0 ? window->icon_ref : window->path);
                (void)reach_copy_utf16(app.app_user_model_id, 260, window->app_user_model_id);
            }
        }
        else if (item_path[0] != 0)
        {
            (void)reach_copy_utf16(app.path, 260, item_path);
            (void)reach_copy_utf16(app.icon_ref, 260, item_path);
        }

        return app.path[0] != 0 ? reach_host_schedule_pin_app(host, &app) : REACH_ERROR;
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_CLOSE)
    {
        return reach_host_schedule_window_control(host, REACH_WINDOW_CONTROL_CLOSE, window_id);
    }

    return REACH_OK;
}

static reach_rect_f32 reach_host_primary_monitor_bounds(reach_host *host, reach_rect_f32 fallback)
{
    const reach_monitor_info *primary_monitor =
        host->monitors.list != nullptr && host->monitors.ops.primary != nullptr
            ? host->monitors.ops.primary(host->monitors.list)
            : nullptr;
    if (primary_monitor == nullptr)
    {
        return fallback;
    }
    reach_rect_f32 monitor = {};
    monitor.x = (float)primary_monitor->bounds.left;
    monitor.y = (float)primary_monitor->bounds.top;
    monitor.width = (float)(primary_monitor->bounds.right - primary_monitor->bounds.left);
    monitor.height = (float)(primary_monitor->bounds.bottom - primary_monitor->bounds.top);
    return monitor;
}

void reach_host_reanchor_context_menu(reach_host *host)
{
    if (host == nullptr || !host->has_layout)
    {
        return;
    }
    const reach_context_menu_state *state =
        reach_context_menu_state_ptr(host->context_menu_capsule);
    if (!state->open || !state->anchored)
    {
        return;
    }

    reach_context_menu_open_context ctx = {};
    ctx.dpi_scale = reach_host_layout_dpi_scale(host);
    ctx.anchored = 1;
    ctx.dock_top_y = host->layout.dock.bounds.y;
    ctx.monitor = reach_host_primary_monitor_bounds(host, host->layout.dock.bounds);
    if (state->power_open)
    {
        ctx.anchor_x = host->layout.dock.bounds.x + host->layout.dock.power_button.x +
                       host->layout.dock.power_button.width * 0.5f;
    }
    else if (state->target_index < host->layout.dock.app_slot_count)
    {
        reach_rect_f32 slot = reach_dock_rect_to_screen(
            &host->layout.dock, host->layout.dock.app_slots[state->target_index]);
        ctx.anchor_x = slot.x + slot.width * 0.5f;
    }
    else
    {

        return;
    }

    reach_rect_f32 before = state->bounds;
    reach_context_menu_reanchor(host->context_menu_capsule, &ctx);
    if (!reach_host_rect_equal(before,
                               reach_context_menu_state_ptr(host->context_menu_capsule)->bounds))
    {
        host->context_menu.dirty_flags = 1;
    }
}

reach_result reach_host_show_power_context_menu(reach_host *host)
{
    if (host == nullptr || !host->has_layout)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_host_close_other_popups(host, REACH_SURFACE_ID_CONTEXT_MENU);

    reach_context_menu_open_context ctx = {};
    ctx.dpi_scale = reach_host_layout_dpi_scale(host);
    ctx.anchor_x = host->layout.dock.bounds.x + host->layout.dock.power_button.x +
                   host->layout.dock.power_button.width * 0.5f;
    ctx.dock_top_y = host->layout.dock.bounds.y;
    ctx.anchored = 1;
    ctx.monitor = reach_host_primary_monitor_bounds(host, host->layout.dock.bounds);

    reach_context_menu_open_power(host->context_menu_capsule, &ctx);
    reach_host_surface_transition_set(host, &host->context_menu_transition, 1);
    host->context_menu.dirty_flags = 1;
    reach_host_sync_pointer_move_subscriptions(host);
    reach_host_sync_popup_mouse_hook(host);
    return REACH_OK;
}

reach_result reach_host_show_dock_app_context_menu(reach_host *host, size_t item_index, int32_t x,
                                                   int32_t y)
{
    if (host == nullptr || item_index >= reach_dock_item_count(host->dock_capsule))
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_host_close_other_popups(host, REACH_SURFACE_ID_CONTEXT_MENU);

    uint32_t item_commands[REACH_CONTEXT_MENU_MAX_ITEMS] = {};
    size_t item_count = reach_dock_build_item_context_commands(
        host->dock_capsule, item_index, item_commands, REACH_CONTEXT_MENU_MAX_ITEMS);

    reach_context_menu_open_context ctx = {};
    ctx.dpi_scale = reach_host_layout_dpi_scale(host);
    ctx.item_commands = item_commands;
    ctx.item_count = item_count;
    ctx.pointer_x = (float)x;
    ctx.pointer_y = (float)y;
    if (host->has_layout && item_index < host->layout.dock.app_slot_count)
    {
        reach_rect_f32 slot =
            reach_dock_rect_to_screen(&host->layout.dock, host->layout.dock.app_slots[item_index]);
        ctx.anchored = 1;
        ctx.anchor_x = slot.x + slot.width * 0.5f;
        ctx.dock_top_y = host->layout.dock.bounds.y;
        ctx.monitor = reach_host_primary_monitor_bounds(host, host->layout.dock.bounds);
    }

    reach_context_menu_open_for_item(host->context_menu_capsule, item_index, &ctx);
    reach_host_surface_transition_set(host, &host->context_menu_transition, 1);
    host->context_menu.dirty_flags = 1;
    reach_host_sync_pointer_move_subscriptions(host);
    reach_host_sync_popup_mouse_hook(host);
    return REACH_OK;
}
