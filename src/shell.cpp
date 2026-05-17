#include "reach/shell.h"

#include "reach/core/render_commands.h"
#include "reach/core/ui_events.h"
#include "reach/core/ui_layout.h"
#include "reach/dock.h"
#include "reach/hotkeys.h"
#include "reach/monitor.h"
#include "reach/platform/windows_adapters.h"
#include "reach/wm.h"

#include <windows.h>

#include <new>

struct reach_shell {
    reach_dock *dock;
    reach_hotkeys *hotkeys;
    reach_monitor_list *monitors;
    reach_wm *wm;
    reach_ui_state ui;
    reach_platform_window_port launcher_window;
    reach_render_backend_port launcher_renderer;
    reach_platform_window_port dock_window;
    reach_render_backend_port dock_renderer;
    reach_input_source_port input_source;
    reach_window_manager_port window_manager;
    reach_config_store_port config_store;
    reach_tray_provider_port tray_provider;
    reach_search_provider_port search_provider;
    reach_app_launcher_port app_launcher;
    reach_ui_layout layout;
    int32_t has_layout;
    int32_t running;
};

static void reach_shell_cleanup(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }

    reach_dock_destroy(shell->dock);
    reach_wm_destroy(shell->wm);
    reach_hotkeys_destroy(shell->hotkeys);
    reach_monitor_list_destroy(shell->monitors);
    if (shell->launcher_window.ops.destroy != nullptr) {
        shell->launcher_window.ops.destroy(shell->launcher_window.window);
    }
    if (shell->launcher_renderer.ops.destroy != nullptr) {
        shell->launcher_renderer.ops.destroy(shell->launcher_renderer.backend);
    }
    if (shell->dock_window.ops.destroy != nullptr) {
        shell->dock_window.ops.destroy(shell->dock_window.window);
    }
    if (shell->dock_renderer.ops.destroy != nullptr) {
        shell->dock_renderer.ops.destroy(shell->dock_renderer.backend);
    }
    if (shell->input_source.ops.destroy != nullptr) {
        shell->input_source.ops.destroy(shell->input_source.source);
    }
    if (shell->window_manager.ops.destroy != nullptr) {
        shell->window_manager.ops.destroy(shell->window_manager.manager);
    }
    if (shell->config_store.ops.destroy != nullptr) {
        shell->config_store.ops.destroy(shell->config_store.store);
    }
    if (shell->tray_provider.ops.destroy != nullptr) {
        shell->tray_provider.ops.destroy(shell->tray_provider.provider);
    }
    if (shell->search_provider.ops.destroy != nullptr) {
        shell->search_provider.ops.destroy(shell->search_provider.provider);
    }
    if (shell->app_launcher.ops.destroy != nullptr) {
        shell->app_launcher.ops.destroy(shell->app_launcher.launcher);
    }
    shell->dock = nullptr;
    shell->wm = nullptr;
    shell->hotkeys = nullptr;
    shell->monitors = nullptr;
    shell->launcher_window = {};
    shell->launcher_renderer = {};
    shell->dock_window = {};
    shell->dock_renderer = {};
    shell->input_source = {};
    shell->window_manager = {};
    shell->config_store = {};
    shell->tray_provider = {};
    shell->search_provider = {};
    shell->app_launcher = {};
}

static reach_result reach_shell_render_dock_surface(reach_shell *shell, const reach_dock_layout *layout)
{
    REACH_ASSERT(shell != nullptr);
    REACH_ASSERT(layout != nullptr);
    if (shell == nullptr || layout == nullptr || shell->dock_renderer.ops.begin_frame == nullptr) {
        return REACH_OK;
    }

    reach_render_command_buffer commands = {};
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = 0.0f;
    command.rect.y = 0.0f;
    command.rect.width = layout->bounds.width;
    command.rect.height = layout->bounds.height;
    command.color.r = 0.08f;
    command.color.g = 0.08f;
    command.color.b = 0.09f;
    command.color.a = 0.92f;
    command.radius = 14.0f;
    reach_render_command_buffer_push(&commands, &command);

    for (size_t index = 0; index < layout->app_slot_count; ++index) {
        command = {};
        command.type = REACH_RENDER_COMMAND_ICON;
        command.rect.x = layout->app_slots[index].x - layout->bounds.x;
        command.rect.y = layout->app_slots[index].y - layout->bounds.y;
        command.rect.width = layout->app_slots[index].width;
        command.rect.height = layout->app_slots[index].height;
        command.color.r = 0.18f;
        command.color.g = 0.20f;
        command.color.b = 0.24f;
        command.color.a = 1.0f;
        command.radius = 10.0f;
        reach_render_command_buffer_push(&commands, &command);
    }

    command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = layout->tray_button.x - layout->bounds.x;
    command.rect.y = layout->tray_button.y - layout->bounds.y;
    command.rect.width = layout->tray_button.width;
    command.rect.height = layout->tray_button.height;
    command.color.r = 0.22f;
    command.color.g = 0.24f;
    command.color.b = 0.28f;
    command.color.a = 1.0f;
    command.radius = 10.0f;
    reach_render_command_buffer_push(&commands, &command);

    if (shell->dock_renderer.ops.begin_frame(shell->dock_renderer.backend) != REACH_OK) {
        return REACH_ERROR;
    }
    (void)shell->dock_renderer.ops.execute(shell->dock_renderer.backend, &commands);
    return shell->dock_renderer.ops.end_frame(shell->dock_renderer.backend);
}

static reach_result reach_shell_render_launcher_surface(reach_shell *shell, const reach_launcher_layout *layout)
{
    REACH_ASSERT(shell != nullptr);
    REACH_ASSERT(layout != nullptr);
    if (shell == nullptr || layout == nullptr || shell->launcher_renderer.ops.begin_frame == nullptr) {
        return REACH_OK;
    }

    reach_render_command_buffer commands = {};
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = 0.0f;
    command.rect.y = 0.0f;
    command.rect.width = layout->bounds.width;
    command.rect.height = layout->bounds.height;
    command.color.r = 0.0f;
    command.color.g = 0.0f;
    command.color.b = 0.0f;
    command.color.a = 0.55f;
    reach_render_command_buffer_push(&commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = layout->search_box.x - layout->bounds.x;
    command.rect.y = layout->search_box.y - layout->bounds.y;
    command.rect.width = layout->search_box.width;
    command.rect.height = layout->search_box.height;
    command.color.r = 0.08f;
    command.color.g = 0.08f;
    command.color.b = 0.09f;
    command.color.a = 0.95f;
    command.radius = 10.0f;
    reach_render_command_buffer_push(&commands, &command);

    if (reach_ui_state_should_show_pinned_apps(&shell->ui)) {
        for (size_t index = 0; index < layout->pinned_app_slot_count; ++index) {
            command = {};
            command.type = REACH_RENDER_COMMAND_ICON;
            command.rect.x = layout->pinned_app_slots[index].x - layout->bounds.x;
            command.rect.y = layout->pinned_app_slots[index].y - layout->bounds.y;
            command.rect.width = layout->pinned_app_slots[index].width;
            command.rect.height = layout->pinned_app_slots[index].height;
            command.color.r = 0.18f;
            command.color.g = 0.20f;
            command.color.b = 0.24f;
            command.color.a = 1.0f;
            command.radius = 12.0f;
            reach_render_command_buffer_push(&commands, &command);
        }
    } else if (reach_ui_state_should_show_search_placeholder(&shell->ui)) {
        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect.x = layout->search_results.x - layout->bounds.x;
        command.rect.y = layout->search_results.y - layout->bounds.y;
        command.rect.width = layout->search_results.width;
        command.rect.height = layout->search_results.height;
        command.color.r = 0.10f;
        command.color.g = 0.10f;
        command.color.b = 0.11f;
        command.color.a = 0.96f;
        command.radius = 8.0f;
        reach_render_command_buffer_push(&commands, &command);

        command = {};
        command.type = REACH_RENDER_COMMAND_TEXT;
        command.rect.x = layout->search_results.x - layout->bounds.x + 16.0f;
        command.rect.y = layout->search_results.y - layout->bounds.y + 16.0f;
        command.rect.width = layout->search_results.width - 32.0f;
        command.rect.height = 32.0f;
        command.color.r = 1.0f;
        command.color.g = 1.0f;
        command.color.b = 1.0f;
        command.color.a = 0.72f;
        reach_copy_utf16(command.text, 260, (const uint16_t *)L"waiting for api implementation");
        reach_render_command_buffer_push(&commands, &command);
    }

    if (shell->launcher_renderer.ops.begin_frame(shell->launcher_renderer.backend) != REACH_OK) {
        return REACH_ERROR;
    }
    (void)shell->launcher_renderer.ops.execute(shell->launcher_renderer.backend, &commands);
    return shell->launcher_renderer.ops.end_frame(shell->launcher_renderer.backend);
}

static int32_t reach_rect_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return (float)x >= rect.x && (float)x <= rect.x + rect.width && (float)y >= rect.y && (float)y <= rect.y + rect.height;
}

static reach_result reach_shell_handle_pointer_up(reach_shell *shell, const reach_ui_event *event)
{
    if (!shell->has_layout) {
        return REACH_OK;
    }

    reach_ui_event routed = {};
    if (reach_rect_contains(shell->layout.dock.tray_button, event->x, event->y)) {
        routed.type = REACH_UI_EVENT_TRAY_BUTTON_CLICK;
        return reach_shell_handle_event(shell, &routed);
    }

    for (size_t index = 0; index < shell->layout.dock.app_slot_count; ++index) {
        if (reach_rect_contains(shell->layout.dock.app_slots[index], event->x, event->y)) {
            routed.type = REACH_UI_EVENT_DOCK_APP_CLICK;
            routed.id = shell->ui.pinned_apps[index].id;
            return reach_shell_handle_event(shell, &routed);
        }
    }

    return REACH_OK;
}

reach_result reach_shell_create(const reach_shell_desc *desc, reach_shell **out_shell)
{
    reach_shell_dependencies dependencies = {};
    reach_result result = reach_windows_create_platform_window(REACH_SURFACE_LAUNCHER, &dependencies.launcher_window);
    if (result == REACH_OK) {
        void *native_window = dependencies.launcher_window.ops.native_handle(dependencies.launcher_window.window);
        result = reach_windows_create_d2d_render_backend(native_window, &dependencies.launcher_renderer);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_platform_window(REACH_SURFACE_DOCK, &dependencies.dock_window);
    }
    if (result == REACH_OK) {
        void *native_window = dependencies.dock_window.ops.native_handle(dependencies.dock_window.window);
        result = reach_windows_create_d2d_render_backend(native_window, &dependencies.dock_renderer);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_search_stub(&dependencies.search_provider);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_app_launcher(&dependencies.app_launcher);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_input_source(&dependencies.input_source);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_window_manager(&dependencies.window_manager);
    }
    if (result == REACH_OK) {
        const uint16_t default_path[] = { 'r','e','a','c','h','.','i','n','i',0 };
        result = reach_windows_create_config_store(desc != nullptr && desc->config_path != nullptr ? desc->config_path : default_path, &dependencies.config_store);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_tray_provider(&dependencies.tray_provider);
    }
    if (result != REACH_OK) {
        if (dependencies.launcher_window.ops.destroy != nullptr) {
            dependencies.launcher_window.ops.destroy(dependencies.launcher_window.window);
        }
        if (dependencies.launcher_renderer.ops.destroy != nullptr) {
            dependencies.launcher_renderer.ops.destroy(dependencies.launcher_renderer.backend);
        }
        if (dependencies.dock_window.ops.destroy != nullptr) {
            dependencies.dock_window.ops.destroy(dependencies.dock_window.window);
        }
        if (dependencies.dock_renderer.ops.destroy != nullptr) {
            dependencies.dock_renderer.ops.destroy(dependencies.dock_renderer.backend);
        }
        if (dependencies.input_source.ops.destroy != nullptr) {
            dependencies.input_source.ops.destroy(dependencies.input_source.source);
        }
        if (dependencies.window_manager.ops.destroy != nullptr) {
            dependencies.window_manager.ops.destroy(dependencies.window_manager.manager);
        }
        if (dependencies.config_store.ops.destroy != nullptr) {
            dependencies.config_store.ops.destroy(dependencies.config_store.store);
        }
        if (dependencies.tray_provider.ops.destroy != nullptr) {
            dependencies.tray_provider.ops.destroy(dependencies.tray_provider.provider);
        }
        if (dependencies.search_provider.ops.destroy != nullptr) {
            dependencies.search_provider.ops.destroy(dependencies.search_provider.provider);
        }
        if (dependencies.app_launcher.ops.destroy != nullptr) {
            dependencies.app_launcher.ops.destroy(dependencies.app_launcher.launcher);
        }
        return result;
    }

    result = reach_shell_create_with_dependencies(desc, &dependencies, out_shell);
    if (result != REACH_OK) {
        if (dependencies.launcher_window.ops.destroy != nullptr) {
            dependencies.launcher_window.ops.destroy(dependencies.launcher_window.window);
        }
        if (dependencies.launcher_renderer.ops.destroy != nullptr) {
            dependencies.launcher_renderer.ops.destroy(dependencies.launcher_renderer.backend);
        }
        if (dependencies.dock_window.ops.destroy != nullptr) {
            dependencies.dock_window.ops.destroy(dependencies.dock_window.window);
        }
        if (dependencies.dock_renderer.ops.destroy != nullptr) {
            dependencies.dock_renderer.ops.destroy(dependencies.dock_renderer.backend);
        }
        if (dependencies.input_source.ops.destroy != nullptr) {
            dependencies.input_source.ops.destroy(dependencies.input_source.source);
        }
        if (dependencies.window_manager.ops.destroy != nullptr) {
            dependencies.window_manager.ops.destroy(dependencies.window_manager.manager);
        }
        if (dependencies.config_store.ops.destroy != nullptr) {
            dependencies.config_store.ops.destroy(dependencies.config_store.store);
        }
        if (dependencies.tray_provider.ops.destroy != nullptr) {
            dependencies.tray_provider.ops.destroy(dependencies.tray_provider.provider);
        }
        if (dependencies.search_provider.ops.destroy != nullptr) {
            dependencies.search_provider.ops.destroy(dependencies.search_provider.provider);
        }
        if (dependencies.app_launcher.ops.destroy != nullptr) {
            dependencies.app_launcher.ops.destroy(dependencies.app_launcher.launcher);
        }
    }
    return result;
}

reach_result reach_shell_create_with_dependencies(const reach_shell_desc *desc, const reach_shell_dependencies *dependencies, reach_shell **out_shell)
{
    (void)desc;
    REACH_ASSERT(dependencies != nullptr);
    if (out_shell == nullptr || dependencies == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_shell *shell = new (std::nothrow) reach_shell();
    if (shell == nullptr) {
        *out_shell = nullptr;
        return REACH_ERROR;
    }

    reach_ui_state_init(&shell->ui);

    reach_result result = reach_monitor_list_create(&shell->monitors);
    if (result == REACH_OK) {
        result = reach_wm_create(&shell->wm);
    }
    if (result == REACH_OK) {
        result = reach_hotkeys_create(&shell->hotkeys);
    }

    if (result == REACH_OK && dependencies->dock_window.window == nullptr) {
        reach_dock_config dock_config = {};
        dock_config.height_px = 56;
        dock_config.icon_size_px = 32;
        dock_config.auto_hide = 1;
        dock_config.animation_seconds = 0.16;

        reach_dock_desc dock_desc = {};
        dock_desc.config = &dock_config;
        result = reach_dock_create(&dock_desc, &shell->dock);
    }
    shell->launcher_window = dependencies->launcher_window;
    shell->launcher_renderer = dependencies->launcher_renderer;
    shell->dock_window = dependencies->dock_window;
    shell->dock_renderer = dependencies->dock_renderer;
    shell->input_source = dependencies->input_source;
    shell->window_manager = dependencies->window_manager;
    shell->config_store = dependencies->config_store;
    shell->tray_provider = dependencies->tray_provider;
    shell->search_provider = dependencies->search_provider;
    shell->app_launcher = dependencies->app_launcher;

    if (result == REACH_OK && shell->config_store.ops.load != nullptr) {
        reach_config_snapshot snapshot = {};
        if (shell->config_store.ops.load(shell->config_store.store, &snapshot) == REACH_OK) {
            if (snapshot.dock_height > 0.0f) shell->ui.dock.height = snapshot.dock_height;
            if (snapshot.dock_width > 0.0f) shell->ui.dock.width = snapshot.dock_width;
            if (snapshot.dock_icon_size > 0.0f) shell->ui.dock.icon_size = snapshot.dock_icon_size;
            (void)reach_ui_state_set_pinned_apps(&shell->ui, snapshot.pinned_apps, snapshot.pinned_app_count);
        }
    }

    if (result != REACH_OK) {
        reach_shell_cleanup(shell);
        delete shell;
        *out_shell = nullptr;
        return result;
    }

    *out_shell = shell;
    return REACH_OK;
}

void reach_shell_destroy(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }

    reach_shell_cleanup(shell);
    delete shell;
}

reach_result reach_shell_start(reach_shell *shell)
{
    if (shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = reach_wm_install_hooks(shell->wm);
    if (result != REACH_OK) {
        return result;
    }

    if (shell->dock_window.ops.show != nullptr) {
        result = shell->dock_window.ops.show(shell->dock_window.window);
        if (result != REACH_OK) {
            return result;
        }
    } else {
        result = reach_dock_show(shell->dock);
        if (result != REACH_OK) {
            return result;
        }
    }

    shell->running = 1;
    return REACH_OK;
}

reach_result reach_shell_stop(reach_shell *shell)
{
    if (shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    shell->running = 0;
    (void)reach_wm_uninstall_hooks(shell->wm);
    (void)reach_hotkeys_unregister_all(shell->hotkeys);
    if (shell->dock != nullptr) {
        (void)reach_dock_hide(shell->dock);
    }
    if (shell->dock_window.ops.hide != nullptr) {
        (void)shell->dock_window.ops.hide(shell->dock_window.window);
    }
    if (shell->launcher_window.ops.hide != nullptr) {
        (void)shell->launcher_window.ops.hide(shell->launcher_window.window);
    }
    return REACH_OK;
}

reach_result reach_shell_handle_event(reach_shell *shell, const reach_ui_event *event)
{
    REACH_ASSERT(shell != nullptr);
    REACH_ASSERT(event != nullptr);
    if (shell == nullptr || event == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_ui_intent intent = {};
    if (event->type == REACH_UI_EVENT_POINTER_UP) {
        return reach_shell_handle_pointer_up(shell, event);
    }

    reach_result result = reach_ui_handle_event(&shell->ui, event, &intent);
    if (result != REACH_OK) {
        return result;
    }

    if (intent.type == REACH_UI_INTENT_OPEN_TRAY_MENU) {
        if (shell->tray_provider.ops.open_menu != nullptr) {
            (void)shell->tray_provider.ops.open_menu(shell->tray_provider.provider, 0);
        }
    } else if (intent.type == REACH_UI_INTENT_LAUNCH_APP) {
        for (size_t index = 0; index < shell->ui.pinned_app_count; ++index) {
            if (shell->ui.pinned_apps[index].id == intent.id && shell->app_launcher.ops.launch != nullptr) {
                reach_app_launch_request request = {};
                reach_copy_utf16(request.path, 260, shell->ui.pinned_apps[index].path);
                (void)shell->app_launcher.ops.launch(shell->app_launcher.launcher, &request);
            }
        }
    } else if (intent.type == REACH_UI_INTENT_RUN_SEARCH && shell->search_provider.ops.query != nullptr) {
        (void)shell->search_provider.ops.query(shell->search_provider.provider, shell->ui.launcher.query);
    }

    if (shell->launcher_window.ops.show != nullptr && shell->launcher_window.ops.hide != nullptr) {
        return shell->ui.launcher.open
            ? shell->launcher_window.ops.show(shell->launcher_window.window)
            : shell->launcher_window.ops.hide(shell->launcher_window.window);
    }

    return REACH_OK;
}

reach_result reach_shell_update(reach_shell *shell, double delta_seconds)
{
    if (shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    if (shell->window_manager.ops.refresh != nullptr) {
        (void)shell->window_manager.ops.refresh(shell->window_manager.manager);
    } else {
        (void)reach_wm_update_z_order(shell->wm);
    }
    if (shell->launcher_window.ops.set_bounds != nullptr && shell->monitors != nullptr && reach_monitor_count(shell->monitors) > 0) {
        const reach_monitor_info *monitor = reach_monitor_get(shell->monitors, 0);
        reach_rect_f32 bounds = {};
        bounds.x = (float)monitor->bounds.left;
        bounds.y = (float)monitor->bounds.top;
        bounds.width = (float)(monitor->bounds.right - monitor->bounds.left);
        bounds.height = (float)(monitor->bounds.bottom - monitor->bounds.top);
        (void)shell->launcher_window.ops.set_bounds(shell->launcher_window.window, bounds);
        (void)shell->launcher_window.ops.set_opacity(shell->launcher_window.window, shell->ui.launcher.open ? 0.92f : 0.0f);
        if (shell->launcher_renderer.ops.begin_frame != nullptr) {
            reach_ui_layout_input input = {};
            input.monitor_bounds = bounds;
            input.work_area = bounds;
            input.dpi_scale = 1.0f;
            reach_ui_layout layout = {};
            reach_render_command_buffer commands = {};
            if (reach_ui_layout_compute(&shell->ui, &input, &layout) == REACH_OK &&
                reach_ui_build_render_commands(&shell->ui, &layout, &commands) == REACH_OK) {
                shell->layout = layout;
                shell->has_layout = 1;
                if (shell->ui.launcher.open) {
                    (void)reach_shell_render_launcher_surface(shell, &layout.launcher);
                }
            }
            if (shell->dock_window.ops.set_bounds != nullptr) {
                (void)shell->dock_window.ops.set_bounds(shell->dock_window.window, layout.dock.bounds);
                (void)shell->dock_window.ops.set_opacity(shell->dock_window.window, shell->ui.dock.visible ? 0.95f : 0.0f);
                (void)reach_shell_render_dock_surface(shell, &layout.dock);
            }
        }
    }
    if (shell->dock != nullptr) {
        return reach_dock_update(shell->dock, delta_seconds);
    }

    return REACH_OK;
}
