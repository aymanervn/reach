#include "reach/shell.h"

#include "reach/applist.h"
#include "reach/config.h"
#include "reach/core/render_commands.h"
#include "reach/core/ui_events.h"
#include "reach/core/ui_layout.h"
#include "reach/dock.h"
#include "reach/hotkeys.h"
#include "reach/monitor.h"
#include "reach/platform/windows_adapters.h"
#include "reach/search.h"
#include "reach/wm.h"

#include <windows.h>

#include <new>

struct reach_shell {
    reach_config *config;
    reach_applist *apps;
    reach_dock *dock;
    reach_hotkeys *hotkeys;
    reach_monitor_list *monitors;
    reach_search *search;
    reach_wm *wm;
    reach_ui_state ui;
    reach_platform_window_port launcher_window;
    reach_render_backend_port launcher_renderer;
    reach_platform_window_port dock_window;
    reach_render_backend_port dock_renderer;
    reach_input_source_port input_source;
    reach_window_manager_port window_manager;
    reach_search_provider_port search_provider;
    reach_app_launcher_port app_launcher;
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
    reach_search_destroy(shell->search);
    reach_applist_destroy(shell->apps);
    reach_config_destroy(shell->config);
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
    shell->search = nullptr;
    shell->apps = nullptr;
    shell->config = nullptr;
    shell->launcher_window = {};
    shell->launcher_renderer = {};
    shell->dock_window = {};
    shell->dock_renderer = {};
    shell->input_source = {};
    shell->window_manager = {};
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

    if (shell->dock_renderer.ops.begin_frame(shell->dock_renderer.backend) != REACH_OK) {
        return REACH_ERROR;
    }
    (void)shell->dock_renderer.ops.execute(shell->dock_renderer.backend, &commands);
    return shell->dock_renderer.ops.end_frame(shell->dock_renderer.backend);
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
    shell->search_provider = dependencies->search_provider;
    shell->app_launcher = dependencies->app_launcher;

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
    reach_result result = reach_ui_handle_event(&shell->ui, event, &intent);
    if (result != REACH_OK) {
        return result;
    }

    if (intent.type == REACH_UI_INTENT_OPEN_TRAY_MENU) {
        // Execute through tray_provider when the tray adapter replaces the legacy dock tray menu.
    } else if (intent.type == REACH_UI_INTENT_LAUNCH_APP) {
        // Resolve app id through pinned app config, then execute app_launcher.
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
            reach_ui_build_render_commands(&shell->ui, &layout, &commands) == REACH_OK &&
                shell->launcher_renderer.ops.begin_frame(shell->launcher_renderer.backend) == REACH_OK) {
                (void)shell->launcher_renderer.ops.execute(shell->launcher_renderer.backend, &commands);
                (void)shell->launcher_renderer.ops.end_frame(shell->launcher_renderer.backend);
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
