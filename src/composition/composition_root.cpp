#include "reach/composition/composition_root.h"

#include "host_internal.h"
#include "reach/adapters/windows/watchdog_retirement.h"
#include "reach/core/app_update.h"
#include "reach/platform/windows_adapters.h"
#include "reach/protocol/version.h"

#include <new>

static void reach_app_on_input(void *user, const reach_ui_event *event)
{
    reach_app *app = static_cast<reach_app *>(user);
    if (app != nullptr && app->host != nullptr)
    {
        (void)reach_host_handle_event(app->host, event);
    }
}

static reach_result reach_app_create_surface_ports(void *user, uint32_t window_flags,
                                                   reach_platform_window_port *window,
                                                   reach_render_backend_port *renderer)
{
    auto *app = static_cast<reach_app *>(user);
    const reach_result result = reach_windows_create_platform_window(window_flags, window);
    return result == REACH_OK ? reach_windows_create_dcomp_render_backend(
                                    app->render_device, window->window, window_flags, renderer)
                              : result;
}

reach_result reach_app_create(const reach_host_desc *desc, reach_app **out_app)
{
    REACH_ASSERT(out_app != nullptr);
    if (out_app == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_app = nullptr;
    reach_app *app = new (std::nothrow) reach_app();
    if (app == nullptr)
    {
        return REACH_ERROR;
    }

    reach_host_dependencies dependencies = {};
    dependencies.surface_factory_user = app;
    dependencies.create_surface_ports = reach_app_create_surface_ports;
    reach_result result = reach_windows_create_render_device(&app->render_device);
    reach_windows_render_device_port(app->render_device, &app->render_device_port);

    if (result == REACH_OK)
    {
        result = reach_windows_create_tray_provider(&dependencies.tray_provider);
    }

    if (result == REACH_OK)
    {
        result = reach_windows_create_screen_hotspot_factory(&dependencies.screen_hotspots);
    }

    if (result == REACH_OK)
    {
        result = reach_windows_create_explorer_desktop_compat_host();
    }

    if (result == REACH_OK)
    {
        result = reach_windows_create_wallpaper_surface(&dependencies.wallpaper_surface);
    }

    if (result == REACH_OK)
    {
        result = reach_windows_create_window_thumbnails(&dependencies.window_thumbnails);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_image_loader(&dependencies.image_loader);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_clock(&dependencies.clock);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_input_language(&dependencies.input_language);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_system_stats(&dependencies.system_stats);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_screen_capture(&dependencies.screen_capture);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_search_provider(&dependencies.search_provider);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_app_launcher(&dependencies.app_launcher);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_application_resolver(&dependencies.application_resolver);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_terminal_launcher(&dependencies.terminal_launcher);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_settings_launcher(&dependencies.settings_launcher);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_input_source(&dependencies.input_source);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_monitor_list(&dependencies.monitors);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_window_manager(&dependencies.window_manager);
    }
    if (result == REACH_OK)
    {
        uint16_t default_path[260] = {};
        const uint16_t *config_path =
            desc != nullptr && desc->config_path != nullptr ? desc->config_path : default_path;
        if (desc == nullptr || desc->config_path == nullptr)
        {
            result = reach_windows_default_config_path(default_path, 260);
        }
        if (result == REACH_OK)
        {
            result = reach_windows_create_config_store(config_path, &dependencies.config_store);
        }
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_icon_provider(&dependencies.icon_provider);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_explorer_service(&dependencies.explorer_service);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_wallpaper_service(&dependencies.wallpaper_service);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_popup_capture(&dependencies.popup_capture);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_power_session(&dependencies.power_session);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_audio_volume(&dependencies.audio_volume);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_system_controls(&dependencies.system_controls);
    }
    if (result == REACH_OK && reach_windows_create_wifi(&dependencies.wifi) != REACH_OK)
    {
        dependencies.wifi = {};
    }
    if (result == REACH_OK && reach_windows_create_bluetooth(&dependencies.bluetooth) != REACH_OK)
    {
        dependencies.bluetooth = {};
    }
    if (result == REACH_OK &&
        reach_windows_create_user_account(&dependencies.user_account) != REACH_OK)
    {
        dependencies.user_account = {};
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_media_controls(&dependencies.media_controls);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_clipboard_provider(&dependencies.clipboard);
    }
    if (result == REACH_OK)
    {
        result = reach_windows_create_foreground_watcher(&dependencies.foreground_watcher);
    }
    if (result == REACH_OK)
    {
        result = reach_host_create_with_dependencies(desc, &dependencies, &app->host);
        if (result == REACH_OK)
        {
            reach_host_set_initial_foreground(app->host, reach_windows_get_current_foreground());
        }
    }
    if (result != REACH_OK)
    {
        if (dependencies.clipboard.ops.destroy != nullptr)
        {
            dependencies.clipboard.ops.destroy(dependencies.clipboard.provider);
        }
        if (dependencies.screen_capture.ops.destroy != nullptr)
        {
            dependencies.screen_capture.ops.destroy(dependencies.screen_capture.capture);
        }
        if (dependencies.input_source.ops.destroy != nullptr)
        {
            dependencies.input_source.ops.destroy(dependencies.input_source.source);
        }
        if (dependencies.monitors.ops.destroy != nullptr)
        {
            dependencies.monitors.ops.destroy(dependencies.monitors.list);
        }
        if (dependencies.window_manager.ops.destroy != nullptr)
        {
            dependencies.window_manager.ops.destroy(dependencies.window_manager.manager);
        }
        if (dependencies.foreground_watcher.ops.destroy != nullptr)
        {
            dependencies.foreground_watcher.ops.destroy(dependencies.foreground_watcher.watcher);
        }
        if (dependencies.config_store.ops.destroy != nullptr)
        {
            dependencies.config_store.ops.destroy(dependencies.config_store.store);
        }
        if (dependencies.tray_provider.ops.destroy != nullptr)
        {
            dependencies.tray_provider.ops.destroy(dependencies.tray_provider.provider);
        }
        if (dependencies.search_provider.ops.destroy != nullptr)
        {
            dependencies.search_provider.ops.destroy(dependencies.search_provider.provider);
        }
        if (dependencies.app_launcher.ops.destroy != nullptr)
        {
            dependencies.app_launcher.ops.destroy(dependencies.app_launcher.launcher);
        }
        if (dependencies.terminal_launcher.ops.destroy != nullptr)
        {
            dependencies.terminal_launcher.ops.destroy(dependencies.terminal_launcher.launcher);
        }
        if (dependencies.settings_launcher.ops.destroy != nullptr)
        {
            dependencies.settings_launcher.ops.destroy(dependencies.settings_launcher.launcher);
        }
        if (dependencies.icon_provider.ops.destroy != nullptr)
        {
            dependencies.icon_provider.ops.destroy(dependencies.icon_provider.provider);
        }
        if (dependencies.explorer_service.ops.destroy != nullptr)
        {
            dependencies.explorer_service.ops.destroy(dependencies.explorer_service.service);
        }
        if (dependencies.wallpaper_service.ops.destroy != nullptr)
        {
            dependencies.wallpaper_service.ops.destroy(dependencies.wallpaper_service.service);
        }
        if (dependencies.wallpaper_surface.ops.destroy != nullptr)
        {
            dependencies.wallpaper_surface.ops.destroy(dependencies.wallpaper_surface.surface);
        }
        if (dependencies.popup_capture.destroy != nullptr)
        {
            dependencies.popup_capture.destroy(dependencies.popup_capture.userdata);
        }
        if (dependencies.power_session.ops.destroy != nullptr)
        {
            dependencies.power_session.ops.destroy(dependencies.power_session.session);
        }
        if (dependencies.audio_volume.destroy != nullptr)
        {
            dependencies.audio_volume.destroy(dependencies.audio_volume.userdata);
        }
        if (dependencies.system_controls.destroy != nullptr)
        {
            dependencies.system_controls.destroy(dependencies.system_controls.userdata);
        }
        if (dependencies.wifi.destroy != nullptr)
        {
            dependencies.wifi.destroy(dependencies.wifi.userdata);
        }
        if (dependencies.bluetooth.destroy != nullptr)
        {
            dependencies.bluetooth.destroy(dependencies.bluetooth.userdata);
        }
        if (dependencies.user_account.ops.destroy != nullptr)
        {
            dependencies.user_account.ops.destroy(dependencies.user_account.account);
        }
        if (dependencies.media_controls.destroy != nullptr)
        {
            dependencies.media_controls.destroy(dependencies.media_controls.userdata);
        }
        if (dependencies.window_thumbnails.ops.destroy != nullptr)
        {
            dependencies.window_thumbnails.ops.destroy(dependencies.window_thumbnails.thumbnails);
        }
        if (dependencies.image_loader.ops.destroy != nullptr)
        {
            dependencies.image_loader.ops.destroy(dependencies.image_loader.loader);
        }
        if (dependencies.application_resolver.ops.destroy != nullptr)
        {
            dependencies.application_resolver.ops.destroy(
                dependencies.application_resolver.resolver);
        }
        if (dependencies.clock.ops.destroy != nullptr)
        {
            dependencies.clock.ops.destroy(dependencies.clock.source);
        }
        if (dependencies.input_language.ops.destroy != nullptr)
        {
            dependencies.input_language.ops.destroy(dependencies.input_language.language);
        }
        if (dependencies.system_stats.ops.destroy != nullptr)
        {
            dependencies.system_stats.ops.destroy(dependencies.system_stats.source);
        }
        reach_windows_destroy_explorer_desktop_compat_host();
        reach_windows_destroy_render_device(app->render_device);
        delete app;
        return result;
    }

    *out_app = app;
    return REACH_OK;
}

// The version is stamped before the cleanup starts, so a declined prompt or a restart caused by
// the cleanup never runs it twice for the same version.
static void reach_app_run_install_cleanup_once(reach_app *app)
{
    char previous[32] = {};
    for (size_t index = 0; index + 1 < sizeof(previous); ++index)
    {
        uint16_t c = app->host->config_version_at_start[index];
        if (c == 0)
        {
            break;
        }
        previous[index] = c < 0x80 ? (char)c : '?';
    }
    if (reach_app_version_compare(previous, REACH_VERSION_STRING) >= 0)
    {
        return;
    }

    uint16_t current[32] = {};
    for (size_t index = 0; index + 1 < 32 && REACH_VERSION_STRING[index] != 0; ++index)
    {
        current[index] = (uint16_t)(unsigned char)REACH_VERSION_STRING[index];
    }
    (void)reach_config_service_set_version(app->host->config_service, current);
    reach_windows_start_install_cleanup();
}

reach_result reach_app_start(reach_app *app)
{
    REACH_ASSERT(app != nullptr);
    if (app == nullptr || app->host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (app->host->running)
    {
        return REACH_OK;
    }

    reach_result result = reach_host_start(app->host);
    if (result != REACH_OK)
    {
        return result;
    }

    const auto &input = app->host->input_source;
    if (input.ops.start != nullptr)
    {
        result = input.ops.start(input.source, reach_app_on_input, app);
        if (result != REACH_OK)
        {
            if (input.ops.stop != nullptr)
            {
                (void)input.ops.stop(input.source);
            }
            reach_host_stop_runtime(app->host, false);
            return result;
        }
    }

    reach_app_launch_request startup_requests[32] = {};
    size_t startup_count = reach_windows_collect_startup_apps(startup_requests, 32);
    for (size_t index = 0; index < startup_count; ++index)
    {
        (void)reach_host_schedule_app_launch(app->host, &startup_requests[index]);
    }

    reach_app_run_install_cleanup_once(app);
    return REACH_OK;
}

reach_result reach_app_stop(reach_app *app)
{
    REACH_ASSERT(app != nullptr);
    if (app == nullptr || app->host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (!app->host->running)
    {
        return REACH_OK;
    }

    const auto &input = app->host->input_source;
    if (input.ops.stop != nullptr)
    {
        (void)input.ops.stop(input.source);
    }
    return reach_host_stop(app->host);
}

reach_result reach_app_update(reach_app *app, double delta_seconds)
{
    REACH_ASSERT(app != nullptr);
    if (app == nullptr || app->host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    return reach_host_update(app->host, delta_seconds);
}

const reach_render_device_port *reach_app_render_device(const reach_app *app)
{
    return app != nullptr ? &app->render_device_port : nullptr;
}

reach_result reach_app_recover_render_device(reach_app *app)
{
    REACH_ASSERT(app != nullptr);
    if (app == nullptr || app->host == nullptr || app->render_device_port.ops.recover == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_render_device_port &port = app->render_device_port;
    uint64_t generation = port.ops.status(port.device).generation;
    reach_result result = port.ops.recover(port.device);
    if (result == REACH_OK && port.ops.status(port.device).generation != generation)
    {
        reach_host_render_device_recovered(app->host);
    }
    return result;
}

reach_result reach_app_dispatch_events(reach_app *app)
{
    REACH_ASSERT(app != nullptr);
    if (app == nullptr || app->host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    return reach_host_dispatch_events(app->host);
}

int32_t reach_app_has_pending_events(const reach_app *app)
{
    return app != nullptr && app->host != nullptr && reach_host_has_pending_events(app->host);
}

int32_t reach_app_needs_frame(const reach_app *app)
{
    return app != nullptr && app->host != nullptr && reach_host_needs_frame(app->host);
}

int32_t reach_app_frame_rate_hz(const reach_app *app)
{
    return app != nullptr && app->host != nullptr ? reach_host_frame_rate_hz(app->host) : 60;
}

uint32_t reach_app_idle_wait_ms(const reach_app *app)
{
    if (app == nullptr || app->host == nullptr)
    {
        return REACH_APP_WAIT_FOREVER;
    }

    uint32_t wait_ms = reach_host_idle_wait_ms(app->host);
    return wait_ms == REACH_CLOCK_WAIT_FOREVER ? REACH_APP_WAIT_FOREVER : wait_ms;
}

void reach_app_destroy(reach_app *app)
{
    if (app == nullptr)
    {
        return;
    }

    if (app->host != nullptr && app->host->running)
    {
        (void)reach_app_stop(app);
    }
    reach_host_destroy(app->host);
    reach_windows_destroy_explorer_desktop_compat_host();
    reach_windows_destroy_render_device(app->render_device);
    delete app;
}
