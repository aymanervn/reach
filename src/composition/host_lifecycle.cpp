#include "host_internal.h"

#include <new>

static void reach_host_on_config_service_ready(void *user, reach_config_service_event event)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr)
    {
        return;
    }
    if (event == REACH_CONFIG_SERVICE_PERSIST_FAILED)
    {
        reach_log_error("Could not persist Reach configuration.");
        return;
    }
    if (event != REACH_CONFIG_SERVICE_SNAPSHOT_CHANGED)
    {
        return;
    }
    if (host->launcher.window.ops.post_event != nullptr)
    {
        (void)host->launcher.window.ops.post_event(host->launcher.window.window,
                                                   REACH_UI_EVENT_CONFIG_CHANGED);
    }
    else
    {
        reach_host_request_update(host);
    }
}

static void reach_host_on_foreground_changed(void *user)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr)
    {
        return;
    }
    if (host->dock.window.ops.post_event != nullptr)
    {
        (void)host->dock.window.ops.post_event(host->dock.window.window,
                                               REACH_UI_EVENT_FOREGROUND_CHANGED);
    }
    else
    {
        reach_host_request_update(host);
    }
}

static void reach_host_on_system_status_ready(void *user)
{
    reach_host_request_update(static_cast<reach_host *>(user));
}

static void reach_host_on_system_stats_ready(void *user)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr || host->top_bar.window.ops.post_event == nullptr)
    {
        return;
    }
    (void)host->top_bar.window.ops.post_event(host->top_bar.window.window,
                                              REACH_UI_EVENT_SYSTEM_STATS_CHANGED);
}

static void reach_host_on_search_service_ready(void *user)
{
    reach_host_post_feature_work_ready(static_cast<reach_host *>(user));
}

static void reach_host_on_icon_service_ready(void *user)
{
    reach_host_post_feature_work_ready(static_cast<reach_host *>(user));
}

static void reach_host_on_app_control_notify(void *user)
{
    reach_host_request_update(static_cast<reach_host *>(user));
}

static void reach_host_on_feature_update_requested(void *user)
{
    reach_host_request_update(static_cast<reach_host *>(user));
}

static void reach_host_on_now_playing_ready(void *user)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr || host->top_bar.window.ops.post_event == nullptr)
    {
        return;
    }
    (void)host->top_bar.window.ops.post_event(host->top_bar.window.window,
                                              REACH_UI_EVENT_NOW_PLAYING_CHANGED);
}

void reach_host_stop_search_service(reach_host *host)
{
    if (host != nullptr)
    {
        reach_search_service_stop(host->search_service);
    }
}

static void reach_host_cleanup(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_host_stop_registered_features(host);
    reach_host_stop_config_service(host);
    reach_host_stop_search_service(host);
    reach_icon_service_stop(host->icon_service);
    reach_host_stop_app_control(host);
    reach_now_playing_service_stop(host->now_playing_service);
    if (host->system_controls.stop_watching != nullptr)
    {
        host->system_controls.stop_watching(host->system_controls.userdata);
    }
    if (host->audio_volume.stop_watching != nullptr)
    {
        host->audio_volume.stop_watching(host->audio_volume.userdata);
    }
    reach_host_sync_popup_mouse_hook(host);
    reach_host_release_registered_render_resources(host);
    reach_idle_watch_stop(host->idle_watch);
    reach_system_status_stop(host->system_status);
    reach_system_stats_stop(host->system_stats);
    reach_host_clear_interfeature_routes(host);
    reach_host_attach_registered_features(host, nullptr);
    if (host->monitors.ops.destroy != nullptr)
    {
        host->monitors.ops.destroy(host->monitors.list);
    }
    reach_host_destroy_registered_surface_ports(host);
    if (host->window_thumbnails.ops.destroy != nullptr)
    {
        host->window_thumbnails.ops.destroy(host->window_thumbnails.thumbnails);
    }
    reach_host_destroy_edge_reveals(host);
    if (host->image_loader.ops.destroy != nullptr)
    {
        host->image_loader.ops.destroy(host->image_loader.loader);
    }
    if (host->input_source.ops.destroy != nullptr)
    {
        host->input_source.ops.destroy(host->input_source.source);
    }
    reach_window_tracking_destroy(host->window_tracking);
    host->window_tracking = nullptr;
    if (host->foreground_watcher.ops.destroy != nullptr)
    {
        host->foreground_watcher.ops.destroy(host->foreground_watcher.watcher);
    }
    if (host->window_manager.ops.destroy != nullptr)
    {
        host->window_manager.ops.destroy(host->window_manager.manager);
    }
    reach_config_service_destroy(host->config_service);
    host->config_service = nullptr;
    if (host->config_store.ops.destroy != nullptr)
    {
        host->config_store.ops.destroy(host->config_store.store);
    }
    reach_tray_service_destroy(host->tray_service);
    host->tray_service = nullptr;
    reach_search_service_destroy(host->search_service);
    host->search_service = nullptr;
    reach_system_status_destroy(host->system_status);
    host->system_status = nullptr;
    reach_system_stats_destroy(host->system_stats);
    host->system_stats = nullptr;
    reach_clock_destroy(host->clock);
    host->clock = nullptr;
    reach_input_language_service_destroy(host->input_language);
    host->input_language = nullptr;
    reach_now_playing_service_destroy(host->now_playing_service);
    host->now_playing_service = nullptr;
    if (host->search_provider.ops.destroy != nullptr)
    {
        host->search_provider.ops.destroy(host->search_provider.provider);
    }
    reach_app_control_destroy(host->app_control);
    host->app_control = nullptr;
    if (host->app_launcher.ops.destroy != nullptr)
    {
        host->app_launcher.ops.destroy(host->app_launcher.launcher);
    }
    if (host->terminal_launcher.ops.destroy != nullptr)
    {
        host->terminal_launcher.ops.destroy(host->terminal_launcher.launcher);
    }
    if (host->settings_launcher.ops.destroy != nullptr)
    {
        host->settings_launcher.ops.destroy(host->settings_launcher.launcher);
    }
    reach_icon_service_destroy(host->icon_service);
    host->icon_service = nullptr;
    reach_wallpaper_destroy(host->wallpaper);
    host->wallpaper = nullptr;
    reach_host_destroy_registered_features(host);
    if (host->explorer_service.ops.destroy != nullptr)
    {
        host->explorer_service.ops.destroy(host->explorer_service.service);
    }
    if (host->wallpaper_service.ops.destroy != nullptr)
    {
        host->wallpaper_service.ops.destroy(host->wallpaper_service.service);
    }
    if (host->wallpaper_surface.ops.destroy != nullptr)
    {
        host->wallpaper_surface.ops.destroy(host->wallpaper_surface.surface);
    }
    if (host->popup_capture.destroy != nullptr)
    {
        host->popup_capture.destroy(host->popup_capture.userdata);
    }
    reach_idle_watch_destroy(host->idle_watch);
    host->idle_watch = nullptr;
    if (host->power_session.ops.destroy != nullptr)
    {
        host->power_session.ops.destroy(host->power_session.session);
    }
    if (host->audio_volume.destroy != nullptr)
    {
        host->audio_volume.destroy(host->audio_volume.userdata);
    }
    if (host->system_controls.destroy != nullptr)
    {
        host->system_controls.destroy(host->system_controls.userdata);
    }
    if (host->media_controls.destroy != nullptr)
    {
        host->media_controls.destroy(host->media_controls.userdata);
    }
    if (host->clipboard.ops.destroy != nullptr)
    {
        host->clipboard.ops.destroy(host->clipboard.provider);
    }

    host->monitors = {};
    host->popup_capture = {};
    host->screen_hotspots = {};
    host->image_loader = {};
    host->window_thumbnails = {};
    host->pointer_move = {};
    host->input_source = {};
    host->window_manager = {};
    host->foreground_watcher = {};
    host->config_store = {};
    host->tray_service = nullptr;
    host->search_provider = {};
    host->app_launcher = {};
    host->terminal_launcher = {};
    host->settings_launcher = {};
    host->icon_service = nullptr;
    host->explorer_service = {};
    host->wallpaper_service = {};
    host->wallpaper_surface = {};
    host->wallpaper = nullptr;
    host->power_session = {};
    host->audio_volume = {};
    host->system_controls = {};
    host->media_controls = {};
    host->now_playing_service = nullptr;
    host->clipboard = {};
    host->quick_settings_system_change_flags.store(0);
    host->audio_volume_changed.store(0);
}

reach_result reach_host_create_with_dependencies(const reach_host_desc *desc,
                                                 const reach_host_dependencies *dependencies,
                                                 reach_host **out_shell)
{
    (void)desc;
    REACH_ASSERT(dependencies != nullptr);
    if (out_shell == nullptr || dependencies == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_host *host = new (std::nothrow) reach_host();
    if (host == nullptr)
    {
        *out_shell = nullptr;
        return REACH_ERROR;
    }

    reach_result result = REACH_OK;

    reach_animation_manager_init(&host->animations, host->animation_tracks,
                                 REACH_HOST_ANIMATION_COUNT);
    reach_host_surface_transitions_init(host);
    reach_host_init_feature_registry(host);
    reach_host_init_registered_surfaces(host);
    if (reach_host_create_registered_features(host) != REACH_OK)
    {
        result = REACH_ERROR;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_feature_runtime *desc = &host->feature_runtimes[index];
        if (desc->definition->capsule_ops != nullptr &&
            desc->definition->capsule_ops->reset != nullptr)
        {
            desc->definition->capsule_ops->reset(desc->capsule);
        }
    }
    host->system_controls = {};
    host->quick_settings_system_change_flags.store(0);
    host->audio_volume_changed.store(0);

    reach_host_bind_registered_surface_ports(host, dependencies);
    host->screen_hotspots = dependencies->screen_hotspots;
    host->image_loader = dependencies->image_loader;
    host->window_thumbnails = dependencies->window_thumbnails;
    if (result == REACH_OK)
    {
        result = reach_host_create_edge_reveals(host);
    }
    if (result == REACH_OK)
    {
        reach_host_init_layout(host);
    }
    host->input_source = dependencies->input_source;
    host->monitors = dependencies->monitors;
    host->window_manager = dependencies->window_manager;
    host->foreground_watcher = dependencies->foreground_watcher;
    host->window_tracking = nullptr;
    if (reach_window_tracking_create(host->window_manager, &host->window_tracking) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    host->config_store = dependencies->config_store;
    host->config_service = nullptr;
    if (reach_config_service_create(host->config_store, reach_host_on_config_service_ready, host,
                                    &host->config_service) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    else
    {
        (void)reach_config_service_ensure_defaults(host->config_service);
    }
    host->tray_service = nullptr;
    if (reach_tray_service_create(dependencies->tray_provider, &host->tray_service) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    host->search_provider = dependencies->search_provider;
    host->search_service = nullptr;
    if (reach_search_service_create(host->search_provider, reach_host_on_search_service_ready, host,
                                    &host->search_service) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    host->app_launcher = dependencies->app_launcher;
    host->terminal_launcher = dependencies->terminal_launcher;
    host->app_control = nullptr;
    if (reach_app_control_create(host->app_launcher, host->terminal_launcher,
                                 dependencies->explorer_service, host->window_manager,
                                 reach_host_on_app_control_notify, host,
                                 &host->app_control) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    host->settings_launcher = dependencies->settings_launcher;
    host->icon_service = nullptr;
    if (reach_icon_service_create(dependencies->icon_provider, &host->icon_service) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    reach_icon_service_set_notify(host->icon_service, reach_host_on_icon_service_ready, host);

    host->explorer_service = dependencies->explorer_service;
    host->wallpaper_service = dependencies->wallpaper_service;
    host->wallpaper_surface = dependencies->wallpaper_surface;
    host->wallpaper = nullptr;
    if (reach_wallpaper_create(dependencies->wallpaper_service, dependencies->wallpaper_surface,
                               &host->wallpaper) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    host->popup_capture = dependencies->popup_capture;
    host->power_session = dependencies->power_session;
    host->idle_watch = nullptr;
    if (reach_idle_watch_create(host->power_session, &host->idle_watch) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    host->audio_volume = dependencies->audio_volume;
    host->system_controls = dependencies->system_controls;
    host->system_status = nullptr;
    if (reach_system_status_create(host->audio_volume, host->system_controls,
                                   reach_host_on_system_status_ready, host,
                                   &host->system_status) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    host->system_stats = nullptr;
    if (reach_system_stats_create(dependencies->system_stats, host->system_controls,
                                  reach_host_on_system_stats_ready, host,
                                  &host->system_stats) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    reach_system_stats_set_enabled(host->system_stats, 1);
    host->clock = nullptr;
    if (reach_clock_create(dependencies->clock, &host->clock) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    host->input_language = nullptr;
    if (reach_input_language_service_create(dependencies->input_language, &host->input_language) !=
        REACH_OK)
    {
        result = REACH_ERROR;
    }
    host->media_controls = dependencies->media_controls;
    host->clipboard = dependencies->clipboard;
    host->now_playing_service = nullptr;
    if (reach_now_playing_service_create(host->media_controls, reach_host_on_now_playing_ready,
                                         host, &host->now_playing_service) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    uint16_t terminal_icon_ref[REACH_SEARCH_RESULT_PATH_CAPACITY] = {};
    if (host->terminal_launcher.ops.icon_ref != nullptr)
    {
        (void)host->terminal_launcher.ops.icon_ref(
            host->terminal_launcher.launcher, terminal_icon_ref, REACH_SEARCH_RESULT_PATH_CAPACITY);
    }
    reach_feature_dependencies feature_dependencies = {host->search_service,
                                                       host->icon_service,
                                                       host->window_tracking,
                                                       host->now_playing_service,
                                                       host->system_stats,
                                                       host->clock,
                                                       host->input_language,
                                                       host->tray_service,
                                                       host->app_control,
                                                       host->system_status,
                                                       &host->clipboard,
                                                       reach_host_on_feature_update_requested,
                                                       host,
                                                       terminal_icon_ref};
    reach_host_attach_registered_features(host, &feature_dependencies);
    reach_host_bind_interfeature_routes(host);
    reach_system_status_refresh_system(host->system_status, 0);
    host->theme = reach_theme_default();

    if (host->monitors.list == nullptr || host->clipboard.provider == nullptr ||
        host->clipboard_surface.window.window == nullptr)
    {
        result = REACH_INVALID_ARGUMENT;
    }

    if (result == REACH_OK)
    {
        reach_config_snapshot snapshot = {};
        if (reach_config_service_snapshot(host->config_service, &snapshot) == REACH_OK)
        {
            reach_host_notify_config_changed(host, &snapshot);
            (void)reach_host_set_pinned_apps(host, snapshot.pinned_apps, snapshot.pinned_app_count);
            reach_host_notify_pinned_apps_changed(host);
            reach_host_seed_or_apply_wallpaper(host, &snapshot);
            if (snapshot.power_shutdown_minutes != 0 || snapshot.power_restart_minutes != 0)
            {
                snapshot.power_shutdown_minutes = 0;
                snapshot.power_restart_minutes = 0;
                reach_config_power_settings power = {};
                power.screen_off_minutes = snapshot.power_screen_off_minutes;
                power.sleep_minutes = snapshot.power_sleep_minutes;
                power.lock_minutes = snapshot.power_lock_minutes;
                power.shutdown_minutes = snapshot.power_shutdown_minutes;
                power.restart_minutes = snapshot.power_restart_minutes;
                power.sleep_wait_apps = snapshot.power_sleep_wait_apps;
                power.shutdown_wait_apps = snapshot.power_shutdown_wait_apps;
                power.restart_wait_apps = snapshot.power_restart_wait_apps;
                (void)reach_config_service_set_power(host->config_service, &power);
            }
            reach_host_apply_power_config(host, &snapshot);
            reach_host_apply_display_config(host, &snapshot);
        }
    }
    if (result != REACH_OK)
    {
        reach_host_cleanup(host);
        delete host;
        *out_shell = nullptr;
        return result;
    }
    host->dirty.layout = 1;
    host->dirty.render = 1;
    host->dirty.monitors = 1;
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        host->feature_runtimes[index].surface->dirty_flags = 1;
    }
    *out_shell = host;
    return REACH_OK;
}

void reach_host_destroy(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_host_cleanup(host);
    delete host;
}

void reach_host_set_initial_foreground(reach_host *host, uintptr_t window)
{
    if (host == nullptr)
    {
        return;
    }

    reach_window_tracking_note_foreground(host->window_tracking, window);
}

reach_result reach_host_start(reach_host *host)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = REACH_OK;
    if (result == REACH_OK && host->window_manager.ops.start != nullptr)
    {
        result = host->window_manager.ops.start(host->window_manager.manager);
    }
    if (result != REACH_OK)
    {
        return result;
    }

    if (host->foreground_watcher.ops.start != nullptr)
    {
        (void)host->foreground_watcher.ops.start(host->foreground_watcher.watcher,
                                                 reach_host_on_foreground_changed, host);
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_feature_runtime *runtime = &host->feature_runtimes[index];
        reach_surface_runtime *surface = runtime->surface;
        if (surface == nullptr || surface->window.ops.set_event_callback == nullptr)
        {
            continue;
        }
        host->surface_event_bindings[index] = {host, runtime};
        result = surface->window.ops.set_event_callback(surface->window.window,
                                                        reach_host_on_registered_surface_event,
                                                        &host->surface_event_bindings[index]);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    result = reach_host_start_registered_features(host);
    if (result != REACH_OK)
    {
        return result;
    }
    reach_host_sync_pointer_move_subscriptions(host);
    result = reach_host_start_edge_reveals(host);
    if (result != REACH_OK)
    {
        return result;
    }
    if (host->system_controls.start_watching != nullptr)
    {
        (void)host->system_controls.start_watching(host->system_controls.userdata,
                                                   reach_host_on_system_controls_changed, host);
    }
    if (host->audio_volume.start_watching != nullptr)
    {
        (void)host->audio_volume.start_watching(host->audio_volume.userdata,
                                                reach_host_on_audio_volume_changed, host);
    }
    reach_system_status_refresh_audio(host->system_status);
    (void)reach_input_language_service_refresh(host->input_language,
                                               reach_host_foreground_window(host));
    result = reach_now_playing_service_start(host->now_playing_service);
    if (result != REACH_OK)
    {
        return result;
    }

    if (host->wallpaper_surface.ops.show != nullptr)
    {
        result = host->wallpaper_surface.ops.show(host->wallpaper_surface.surface);
        if (result != REACH_OK)
        {
            return result;
        }
    }

    uintptr_t startup_foreground = reach_host_foreground_window(host);
    if (startup_foreground != 0 && !reach_host_window_is_minimized(host, startup_foreground))
    {
        if (host->window_manager.ops.activate != nullptr)
        {
            (void)host->window_manager.ops.activate(host->window_manager.manager,
                                                    startup_foreground);
        }
    }

    host->running = 1;
    reach_runtime_policy_init(&host->runtime_policy);
    host->dirty.layout = 1;
    host->dirty.render = 1;
    host->dirty.monitors = 1;
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        host->feature_runtimes[index].surface->dirty_flags = 1;
    }
    return REACH_OK;
}

reach_result reach_host_stop(reach_host *host)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    host->running = 0;
    reach_runtime_policy_init(&host->runtime_policy);
    reach_host_stop_registered_features(host);
    reach_host_stop_config_service(host);
    reach_host_stop_search_service(host);
    reach_icon_service_stop(host->icon_service);
    reach_host_stop_app_control(host);
    reach_idle_watch_stop(host->idle_watch);
    reach_now_playing_service_stop(host->now_playing_service);
    if (host->system_controls.stop_watching != nullptr)
    {
        host->system_controls.stop_watching(host->system_controls.userdata);
    }
    if (host->audio_volume.stop_watching != nullptr)
    {
        host->audio_volume.stop_watching(host->audio_volume.userdata);
    }
    if (host->foreground_watcher.ops.stop != nullptr)
    {
        (void)host->foreground_watcher.ops.stop(host->foreground_watcher.watcher);
    }
    if (host->window_manager.ops.stop != nullptr)
    {
        (void)host->window_manager.ops.stop(host->window_manager.manager);
    }
    reach_host_hide_all_surfaces(host);
    if (host->wallpaper_surface.ops.hide != nullptr)
    {
        (void)host->wallpaper_surface.ops.hide(host->wallpaper_surface.surface);
    }
    return REACH_OK;
}
