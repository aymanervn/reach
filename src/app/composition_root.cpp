#include "reach/app/composition_root.h"

#include "reach/platform/windows_adapters.h"

#include <new>

static void reach_app_on_input(void *user, const reach_ui_event *event)
{
    reach_app *app = static_cast<reach_app *>(user);
    if (app != nullptr && app->shell != nullptr) {
        (void)reach_shell_handle_event(app->shell, event);
    }
}

reach_result reach_app_create(const reach_shell_desc *desc, reach_app **out_app)
{
    REACH_ASSERT(out_app != nullptr);
    if (out_app == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_app = nullptr;
    reach_app *app = new (std::nothrow) reach_app();
    if (app == nullptr) {
        return REACH_ERROR;
    }

    reach_shell_dependencies dependencies = {};
    reach_result result = reach_windows_create_tray_provider(&dependencies.tray_provider);

    if (result == REACH_OK) {
        result = reach_windows_create_wallpaper_surface(&dependencies.wallpaper_surface);
    }

    if (result == REACH_OK) {
        result = reach_windows_create_platform_window(REACH_SURFACE_LAUNCHER, &dependencies.launcher_window);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_dcomp_render_backend(dependencies.launcher_window.window, &dependencies.launcher_renderer);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_platform_window(REACH_SURFACE_DOCK, &dependencies.dock_window);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_dcomp_render_backend(dependencies.dock_window.window, &dependencies.dock_renderer);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_dock_reveal_edge(&dependencies.dock_reveal_edge);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_platform_window(REACH_SURFACE_TRAY_MENU, &dependencies.tray_window);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_dcomp_render_backend(dependencies.tray_window.window, &dependencies.tray_renderer);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_platform_window(REACH_SURFACE_SWITCHER, &dependencies.switcher_window);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_dcomp_render_backend(dependencies.switcher_window.window, &dependencies.switcher_renderer);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_platform_window(REACH_SURFACE_CONTEXT_MENU, &dependencies.context_menu_window);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_dcomp_render_backend(dependencies.context_menu_window.window, &dependencies.context_menu_renderer);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_platform_window(REACH_SURFACE_QUICK_SETTINGS, &dependencies.quick_settings_window);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_dcomp_render_backend(dependencies.quick_settings_window.window, &dependencies.quick_settings_renderer);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_search_provider(&dependencies.search_provider);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_app_launcher(&dependencies.app_launcher);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_input_source(&dependencies.input_source);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_hotkeys(&dependencies.hotkeys);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_monitor_list(&dependencies.monitors);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_window_manager(&dependencies.window_manager);
    }
    if (result == REACH_OK) {
        uint16_t default_path[260] = {};
        const uint16_t *config_path = desc != nullptr && desc->config_path != nullptr ? desc->config_path : default_path;
        if (desc == nullptr || desc->config_path == nullptr) {
            result = reach_windows_default_config_path(default_path, 260);
        }
        if (result == REACH_OK) {
            result = reach_windows_create_config_store(config_path, &dependencies.config_store);
        }
    }
    if (result == REACH_OK) {
        result = reach_windows_create_icon_provider(&dependencies.icon_provider);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_explorer_service(&dependencies.explorer_service);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_wallpaper_service(&dependencies.wallpaper_service);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_popup_capture(&dependencies.popup_capture);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_power_session(&dependencies.power_session);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_audio_volume(&dependencies.audio_volume);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_system_controls(&dependencies.system_controls);
    }
    if (result == REACH_OK) {
        result = reach_shell_create_with_dependencies(desc, &dependencies, &app->shell);
        if (result == REACH_OK) {
            dependencies.popup_capture = {};
            dependencies.power_session = {};
            dependencies.audio_volume = {};
            dependencies.system_controls = {};
            dependencies.quick_settings_window = {};
            dependencies.quick_settings_renderer = {};
        }
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
        if (dependencies.tray_window.ops.destroy != nullptr) {
            dependencies.tray_window.ops.destroy(dependencies.tray_window.window);
        }
        if (dependencies.tray_renderer.ops.destroy != nullptr) {
            dependencies.tray_renderer.ops.destroy(dependencies.tray_renderer.backend);
        }
        if (dependencies.switcher_window.ops.destroy != nullptr) {
            dependencies.switcher_window.ops.destroy(dependencies.switcher_window.window);
        }
        if (dependencies.switcher_renderer.ops.destroy != nullptr) {
            dependencies.switcher_renderer.ops.destroy(dependencies.switcher_renderer.backend);
        }
        if (dependencies.context_menu_window.ops.destroy != nullptr) {
            dependencies.context_menu_window.ops.destroy(dependencies.context_menu_window.window);
        }
        if (dependencies.context_menu_renderer.ops.destroy != nullptr) {
            dependencies.context_menu_renderer.ops.destroy(dependencies.context_menu_renderer.backend);
        }
        if (dependencies.quick_settings_window.ops.destroy != nullptr) {
            dependencies.quick_settings_window.ops.destroy(dependencies.quick_settings_window.window);
        }
        if (dependencies.quick_settings_renderer.ops.destroy != nullptr) {
            dependencies.quick_settings_renderer.ops.destroy(dependencies.quick_settings_renderer.backend);
        }
        if (dependencies.input_source.ops.destroy != nullptr) {
            dependencies.input_source.ops.destroy(dependencies.input_source.source);
        }
        if (dependencies.hotkeys.ops.destroy != nullptr) {
            dependencies.hotkeys.ops.destroy(dependencies.hotkeys.hotkeys);
        }
        if (dependencies.monitors.ops.destroy != nullptr) {
            dependencies.monitors.ops.destroy(dependencies.monitors.list);
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
        if (dependencies.icon_provider.ops.destroy != nullptr) {
            dependencies.icon_provider.ops.destroy(dependencies.icon_provider.provider);
        }
        if (dependencies.explorer_service.ops.destroy != nullptr) {
            dependencies.explorer_service.ops.destroy(dependencies.explorer_service.service);
        }
        if (dependencies.wallpaper_service.ops.destroy != nullptr) {
            dependencies.wallpaper_service.ops.destroy(dependencies.wallpaper_service.service);
        }
        if (dependencies.wallpaper_surface.ops.destroy != nullptr) {
            dependencies.wallpaper_surface.ops.destroy(dependencies.wallpaper_surface.surface);
        }
        if (dependencies.popup_capture.destroy != nullptr) {
            dependencies.popup_capture.destroy(dependencies.popup_capture.userdata);
        }
        if (dependencies.power_session.ops.destroy != nullptr) {
            dependencies.power_session.ops.destroy(dependencies.power_session.session);
        }
        if (dependencies.audio_volume.destroy != nullptr) {
            dependencies.audio_volume.destroy(dependencies.audio_volume.userdata);
        }
        if (dependencies.system_controls.destroy != nullptr) {
            dependencies.system_controls.destroy(dependencies.system_controls.userdata);
        }
        delete app;
        return result;
    }

    app->input_source = dependencies.input_source;
    *out_app = app;
    return REACH_OK;
}

reach_result reach_app_start(reach_app *app)
{
    REACH_ASSERT(app != nullptr);
    if (app == nullptr || app->shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = reach_shell_start(app->shell);
    if (result == REACH_OK) {
        (void)reach_windows_launch_startup_apps();
    }
    if (result == REACH_OK && app->input_source.ops.start != nullptr) {
        result = app->input_source.ops.start(app->input_source.source, reach_app_on_input, app);
    }
    return result;
}

reach_result reach_app_stop(reach_app *app)
{
    REACH_ASSERT(app != nullptr);
    if (app == nullptr || app->shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    if (app->input_source.ops.stop != nullptr) {
        (void)app->input_source.ops.stop(app->input_source.source);
    }
    return reach_shell_stop(app->shell);
}

reach_result reach_app_update(reach_app *app, double delta_seconds)
{
    REACH_ASSERT(app != nullptr);
    if (app == nullptr || app->shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    return reach_shell_update(app->shell, delta_seconds);
}

reach_result reach_app_dispatch_events(reach_app *app)
{
    REACH_ASSERT(app != nullptr);
    if (app == nullptr || app->shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    return reach_shell_dispatch_events(app->shell);
}

int32_t reach_app_has_pending_events(const reach_app *app)
{
    return app != nullptr && app->shell != nullptr && reach_shell_has_pending_events(app->shell);
}

int32_t reach_app_needs_frame(const reach_app *app)
{
    return app != nullptr && app->shell != nullptr && reach_shell_needs_frame(app->shell);
}

void reach_app_destroy(reach_app *app)
{
    if (app == nullptr) {
        return;
    }

    reach_shell_destroy(app->shell);
    delete app;
}
