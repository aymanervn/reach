#include "shell_internal.h"

#include <new>

static void reach_shell_cleanup(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }

    reach_shell_set_tray_popup_open(shell, 0);
    reach_shell_set_quick_settings_open(shell, 0);
    reach_shell_stop_launcher_search_worker(shell);
    if (shell->system_controls.stop_watching != nullptr) {
        shell->system_controls.stop_watching(shell->system_controls.userdata);
    }
    reach_shell_close_context_menu(shell);
    reach_shell_sync_popup_mouse_hook(shell);
    reach_hotkeys_destroy(shell->hotkeys);
    reach_monitor_list_destroy(shell->monitors);
    if (shell->launcher.window.ops.destroy != nullptr) {
        shell->launcher.window.ops.destroy(shell->launcher.window.window);
    }
    if (shell->launcher.renderer.ops.destroy != nullptr) {
        shell->launcher.renderer.ops.destroy(shell->launcher.renderer.backend);
    }
    if (shell->dock.window.ops.destroy != nullptr) {
        shell->dock.window.ops.destroy(shell->dock.window.window);
    }
    if (shell->dock.renderer.ops.destroy != nullptr) {
        shell->dock.renderer.ops.destroy(shell->dock.renderer.backend);
    }
    if (shell->tray.window.ops.destroy != nullptr) {
        shell->tray.window.ops.destroy(shell->tray.window.window);
    }
    if (shell->tray.renderer.ops.destroy != nullptr) {
        shell->tray.renderer.ops.destroy(shell->tray.renderer.backend);
    }
    if (shell->switcher.window.ops.destroy != nullptr) {
        shell->switcher.window.ops.destroy(shell->switcher.window.window);
    }
    if (shell->switcher.renderer.ops.destroy != nullptr) {
        shell->switcher.renderer.ops.destroy(shell->switcher.renderer.backend);
    }
    if (shell->context_menu.window.ops.destroy != nullptr) {
        shell->context_menu.window.ops.destroy(shell->context_menu.window.window);
    }
    if (shell->context_menu.renderer.ops.destroy != nullptr) {
        shell->context_menu.renderer.ops.destroy(shell->context_menu.renderer.backend);
    }
    if (shell->quick_settings.window.ops.destroy != nullptr) {
        shell->quick_settings.window.ops.destroy(shell->quick_settings.window.window);
    }
    if (shell->quick_settings.renderer.ops.destroy != nullptr) {
        shell->quick_settings.renderer.ops.destroy(shell->quick_settings.renderer.backend);
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
    reach_dock_release_all_icons(&shell->dock_icons, &shell->icon_provider, shell->open_window_count);
    reach_shell_release_launcher_result_icons(shell);
    if (shell->app_launcher.ops.destroy != nullptr) {
        shell->app_launcher.ops.destroy(shell->app_launcher.launcher);
    }
    if (shell->icon_provider.ops.destroy != nullptr) {
        shell->icon_provider.ops.destroy(shell->icon_provider.provider);
    }
    if (shell->explorer_service.ops.destroy != nullptr) {
        shell->explorer_service.ops.destroy(shell->explorer_service.service);
    }
    if (shell->wallpaper_service.ops.destroy != nullptr) {
        shell->wallpaper_service.ops.destroy(shell->wallpaper_service.service);
    }
    if (shell->wallpaper_surface.ops.destroy != nullptr) {
        shell->wallpaper_surface.ops.destroy(shell->wallpaper_surface.surface);
    }
    if (shell->popup_capture.destroy != nullptr) {
        shell->popup_capture.destroy(shell->popup_capture.userdata);
    }
    if (shell->power_session.ops.destroy != nullptr) {
        shell->power_session.ops.destroy(shell->power_session.session);
    }
    if (shell->audio_volume.destroy != nullptr) {
        shell->audio_volume.destroy(shell->audio_volume.userdata);
    }
    if (shell->system_controls.destroy != nullptr) {
        shell->system_controls.destroy(shell->system_controls.userdata);
    }

    shell->hotkeys = nullptr;
    shell->monitors = nullptr;
    shell->popup_capture = {};
    reach_surface_runtime_init(&shell->launcher);
    reach_surface_runtime_init(&shell->dock);
    reach_surface_runtime_init(&shell->tray);
    reach_surface_runtime_init(&shell->switcher);
    reach_surface_runtime_init(&shell->context_menu);
    reach_surface_runtime_init(&shell->quick_settings);
    shell->input_source = {};
    shell->window_manager = {};
    shell->config_store = {};
    shell->tray_provider = {};
    shell->search_provider = {};
    shell->app_launcher = {};
    shell->icon_provider = {};
    shell->explorer_service = {};
    shell->wallpaper_service = {};
    shell->wallpaper_surface = {};
    shell->power_session = {};
    shell->audio_volume = {};
    shell->system_controls = {};
    shell->quick_settings_system_change_flags.store(0);
    reach_dock_icon_cache_init(&shell->dock_icons);
    reach_tray_model_init(&shell->tray_model);
    reach_quick_settings_model_init(&shell->quick_settings_model);
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
    reach_dock_feature_model_init(&shell->dock_model);
    reach_dock_icon_cache_init(&shell->dock_icons);
    reach_tray_model_init(&shell->tray_model);
    reach_quick_settings_model_init(&shell->quick_settings_model);
    reach_surface_runtime_init(&shell->launcher);
    reach_surface_runtime_init(&shell->dock);
    reach_surface_runtime_init(&shell->tray);
    reach_surface_runtime_init(&shell->switcher);
    reach_surface_runtime_init(&shell->context_menu);
    reach_surface_runtime_init(&shell->quick_settings);
    shell->dock_click_feedback_index = REACH_SHELL_DOCK_FEEDBACK_NONE;
    shell->dock_click_feedback_opacity = {};
    shell->tray_click_feedback_index = REACH_MAX_TRAY_ITEMS;
    shell->tray_click_feedback_opacity = {};
    shell->dock_drag_source_index = REACH_MAX_PINNED_APPS;
    shell->dock_drag_target_index = REACH_MAX_PINNED_APPS;
    shell->pressed_dock_index = REACH_MAX_PINNED_APPS;
    shell->pressed_launcher_hit_type = REACH_LAUNCHER_HIT_NONE;
    shell->pressed_launcher_index = REACH_MAX_PINNED_APPS;
    shell->context_menu_target_index = REACH_MAX_PINNED_APPS;
    shell->context_menu_hovered_index = REACH_MAX_PINNED_APPS;

    shell->quick_settings_open = 0;
    shell->quick_settings_dragging_volume = 0;
    shell->quick_settings_drag_type = REACH_QUICK_SETTINGS_HIT_NONE;
    shell->quick_settings_drag_session_index = 0;
    shell->quick_settings_drag_session_instance_id[0] = 0;
    shell->quick_settings_audio_state.level = 0.0f;
    shell->quick_settings_audio_state.muted = 0;
    shell->quick_settings_audio_sessions = {};
    shell->quick_settings_output_devices = {};
    shell->system_controls = {};
    shell->quick_settings_system_change_flags.store(0);
    shell->quick_settings_notch_anchor_x = 0.0f;
    shell->quick_settings_bounds = {};
    shell->quick_settings_target_bounds = {};
    shell->quick_settings_bounds_animation = {};
    shell->quick_settings_content_bounds = {};
    shell->quick_settings_layout = {};
    shell->launcher_search_notify = reach_shell_notify_launcher_search_ready;

    reach_result result = reach_monitor_list_create(&shell->monitors);
    if (result == REACH_OK) {
        result = reach_hotkeys_create(&shell->hotkeys);
    }

    shell->launcher.window = dependencies->launcher_window;
    shell->launcher.renderer = dependencies->launcher_renderer;
    shell->dock.window = dependencies->dock_window;
    shell->dock.renderer = dependencies->dock_renderer;
    shell->tray.window = dependencies->tray_window;
    shell->tray.renderer = dependencies->tray_renderer;
    shell->switcher.window = dependencies->switcher_window;
    shell->switcher.renderer = dependencies->switcher_renderer;
    shell->context_menu.window = dependencies->context_menu_window;
    shell->context_menu.renderer = dependencies->context_menu_renderer;
    shell->quick_settings.window = dependencies->quick_settings_window;
    shell->quick_settings.renderer = dependencies->quick_settings_renderer;
    shell->input_source = dependencies->input_source;
    shell->window_manager = dependencies->window_manager;
    shell->config_store = dependencies->config_store;
    shell->tray_provider = dependencies->tray_provider;
    shell->search_provider = dependencies->search_provider;
    shell->app_launcher = dependencies->app_launcher;
    shell->icon_provider = dependencies->icon_provider;
    shell->explorer_service = dependencies->explorer_service;
    shell->wallpaper_service = dependencies->wallpaper_service;
    shell->wallpaper_surface = dependencies->wallpaper_surface;
    shell->popup_capture = dependencies->popup_capture;
    shell->power_session = dependencies->power_session;
    shell->audio_volume = dependencies->audio_volume;
    shell->system_controls = dependencies->system_controls;
    shell->theme = reach_theme_default();

    if (result == REACH_OK && shell->config_store.ops.load != nullptr) {
        (void)reach_pin_config_ensure_defaults(&shell->config_store);
        reach_config_snapshot snapshot = {};
        if (shell->config_store.ops.load(shell->config_store.store, &snapshot) == REACH_OK) {
            if (snapshot.dock_height > 0.0f) shell->ui.dock.height = snapshot.dock_height;
            if (snapshot.dock_width > 0.0f) shell->ui.dock.width = snapshot.dock_width;
            shell->ui.dock.icon_size = reach_theme_icon_box_size(shell->theme, shell->ui.dock.height);
            (void)reach_ui_state_set_pinned_apps(&shell->ui, snapshot.pinned_apps, snapshot.pinned_app_count);
            reach_shell_seed_or_apply_wallpaper(shell, &snapshot);
        }
    }
    if (result == REACH_OK) {
        result = reach_shell_load_pinned_icons(shell);
    }

    if (result != REACH_OK) {
        reach_shell_cleanup(shell);
        delete shell;
        *out_shell = nullptr;
        return result;
    }

    shell->layout_dirty = 1;
    shell->render_dirty = 1;
    shell->dock.dirty_flags = 1;
    shell->launcher.dirty_flags = 1;
    shell->switcher.dirty_flags = 1;
    shell->quick_settings.dirty_flags = 1;
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

    reach_result result = REACH_OK;
    if (shell->window_manager.ops.start != nullptr) {
        result = shell->window_manager.ops.start(shell->window_manager.manager);
    }
    if (result != REACH_OK) {
        return result;
    }

    if (shell->dock.window.ops.set_event_callback != nullptr) {
        result = shell->dock.window.ops.set_event_callback(shell->dock.window.window, reach_shell_on_window_event, shell);
        if (result != REACH_OK) {
            return result;
        }
    }
    if (shell->launcher.window.ops.set_event_callback != nullptr) {
        result = shell->launcher.window.ops.set_event_callback(shell->launcher.window.window, reach_shell_on_window_event, shell);
        if (result != REACH_OK) {
            return result;
        }
    }
    if (shell->tray.window.ops.set_event_callback != nullptr) {
        result = shell->tray.window.ops.set_event_callback(shell->tray.window.window, reach_shell_on_window_event, shell);
        if (result != REACH_OK) {
            return result;
        }
    }
    if (shell->switcher.window.ops.set_event_callback != nullptr) {
        result = shell->switcher.window.ops.set_event_callback(shell->switcher.window.window, reach_shell_on_window_event, shell);
        if (result != REACH_OK) {
            return result;
        }
    }
    if (shell->context_menu.window.ops.set_event_callback != nullptr) {
        result = shell->context_menu.window.ops.set_event_callback(shell->context_menu.window.window, reach_shell_on_window_event, shell);
        if (result != REACH_OK) {
            return result;
        }
    }
    if (shell->quick_settings.window.ops.set_event_callback != nullptr) {
        result = shell->quick_settings.window.ops.set_event_callback(shell->quick_settings.window.window, reach_shell_on_window_event, shell);
        if (result != REACH_OK) {
            return result;
        }
    }
    if (shell->system_controls.start_watching != nullptr) {
        (void)shell->system_controls.start_watching(
            shell->system_controls.userdata,
            reach_shell_on_system_controls_changed,
            shell);
    }

    if (shell->dock.window.ops.show != nullptr) {
        if (shell->wallpaper_surface.ops.show != nullptr) {
            result = shell->wallpaper_surface.ops.show(shell->wallpaper_surface.surface);
            if (result != REACH_OK) {
                return result;
            }
        }
        result = shell->dock.window.ops.show(shell->dock.window.window);
        if (result != REACH_OK) {
            return result;
        }
    }

    shell->running = 1;
    shell->layout_dirty = 1;
    shell->render_dirty = 1;
    shell->dock.dirty_flags = 1;
    shell->launcher.dirty_flags = 1;
    shell->tray.dirty_flags = 1;
    shell->switcher.dirty_flags = 1;
    shell->quick_settings.dirty_flags = 1;
    shell->context_menu_open = 0;
    shell->quick_settings_open = 0;
    shell->quick_settings_dragging_volume = 0;
    shell->quick_settings_drag_type = REACH_QUICK_SETTINGS_HIT_NONE;
    return REACH_OK;
}

reach_result reach_shell_stop(reach_shell *shell)
{
    if (shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    shell->running = 0;
    shell->switcher_open = 0;
    shell->context_menu_open = 0;
    reach_shell_set_tray_popup_open(shell, 0);
    reach_shell_set_quick_settings_open(shell, 0);
    reach_shell_cancel_launcher_search(shell);
    reach_shell_stop_launcher_search_worker(shell);
    if (shell->system_controls.stop_watching != nullptr) {
        shell->system_controls.stop_watching(shell->system_controls.userdata);
    }
    if (shell->window_manager.ops.stop != nullptr) {
        (void)shell->window_manager.ops.stop(shell->window_manager.manager);
    }
    (void)reach_hotkeys_unregister_all(shell->hotkeys);
    if (shell->dock.window.ops.hide != nullptr) {
        (void)shell->dock.window.ops.hide(shell->dock.window.window);
    }
    if (shell->launcher.window.ops.hide != nullptr) {
        (void)shell->launcher.window.ops.hide(shell->launcher.window.window);
    }
    if (shell->tray.window.ops.hide != nullptr) {
        (void)shell->tray.window.ops.hide(shell->tray.window.window);
    }
    if (shell->switcher.window.ops.hide != nullptr) {
        (void)shell->switcher.window.ops.hide(shell->switcher.window.window);
    }
    if (shell->context_menu.window.ops.hide != nullptr) {
        (void)shell->context_menu.window.ops.hide(shell->context_menu.window.window);
    }
    if (shell->quick_settings.window.ops.hide != nullptr) {
        (void)shell->quick_settings.window.ops.hide(shell->quick_settings.window.window);
    }
    if (shell->wallpaper_surface.ops.hide != nullptr) {
        (void)shell->wallpaper_surface.ops.hide(shell->wallpaper_surface.surface);
    }
    return REACH_OK;
}
