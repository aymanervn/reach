#include "reach/shell/shell.h"

#include "reach/platform/windows_adapters.h"

reach_result reach_shell_create(const reach_shell_desc *desc, reach_shell **out_shell)
{
    reach_shell_dependencies dependencies = {};
    reach_result result = reach_windows_create_wallpaper_surface(&dependencies.wallpaper_surface);
    if (result == REACH_OK) {
        result = reach_windows_create_platform_window(REACH_SURFACE_LAUNCHER, &dependencies.launcher_window);
    }
    if (result == REACH_OK) {
        void *native_window = dependencies.launcher_window.ops.native_handle(dependencies.launcher_window.window);
        result = reach_windows_create_d2d_render_backend(native_window, &dependencies.launcher_renderer);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_platform_window(REACH_SURFACE_DOCK, &dependencies.dock_window);
    }
    if (result == REACH_OK) {
        void *native_window = dependencies.dock_window.ops.native_handle(dependencies.dock_window.window);
        result = reach_windows_create_dcomp_render_backend(native_window, &dependencies.dock_renderer);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_platform_window(REACH_SURFACE_TRAY_MENU, &dependencies.tray_window);
    }
    if (result == REACH_OK) {
        void *native_window = dependencies.tray_window.ops.native_handle(dependencies.tray_window.window);
        result = reach_windows_create_dcomp_render_backend(native_window, &dependencies.tray_renderer);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_platform_window(REACH_SURFACE_SWITCHER, &dependencies.switcher_window);
    }
    if (result == REACH_OK) {
        void *native_window = dependencies.switcher_window.ops.native_handle(dependencies.switcher_window.window);
        result = reach_windows_create_dcomp_render_backend(native_window, &dependencies.switcher_renderer);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_platform_window(REACH_SURFACE_CONTEXT_MENU, &dependencies.context_menu_window);
    }
    if (result == REACH_OK) {
        void *native_window = dependencies.context_menu_window.ops.native_handle(dependencies.context_menu_window.window);
        result = reach_windows_create_dcomp_render_backend(native_window, &dependencies.context_menu_renderer);
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
    if (result == REACH_OK) {
        result = reach_windows_create_icon_provider(&dependencies.icon_provider);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_explorer_service(&dependencies.explorer_service);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_wallpaper_service(&dependencies.wallpaper_service);
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
    }
    return result;
}
