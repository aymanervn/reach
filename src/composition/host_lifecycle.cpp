#include "host_internal.h"

#include <new>

static void reach_host_on_dock_reveal_edge(void *user, reach_screen_hotspot_event event)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr)
    {
        return;
    }
    if (event == REACH_SCREEN_HOTSPOT_ENTER)
    {
        reach_dock_begin_reveal_session(host->dock_capsule);
    }
    reach_host_request_bar_visibility_update(host);
}

static void reach_host_on_top_bar_reveal_edge(void *user, reach_screen_hotspot_event event)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr)
    {
        return;
    }
    if (event == REACH_SCREEN_HOTSPOT_ENTER)
    {
        reach_top_bar_begin_reveal_session(host->top_bar_capsule);
    }
    reach_host_request_bar_visibility_update(host);
}

static void reach_host_on_config_service_ready(void *user)
{

    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr)
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
    reach_host_notify_launcher_search_ready(static_cast<reach_host *>(user));
}

static void reach_host_on_icon_service_ready(void *user)
{

    reach_host_notify_launcher_search_ready(static_cast<reach_host *>(user));
}

static void reach_host_on_app_control_notify(void *user)
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

static void reach_host_on_clipboard_changed(void *user)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr || host->clipboard_surface.window.ops.post_event == nullptr)
    {
        return;
    }
    (void)host->clipboard_surface.window.ops.post_event(host->clipboard_surface.window.window,
                                                        REACH_UI_EVENT_CLIPBOARD_CHANGED);
}

static void reach_host_cleanup(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_host_set_tray_popup_open(host, 0);
    reach_host_set_quick_settings_open(host, 0);
    reach_host_stop_config_reload_worker(host);
    reach_host_stop_launcher_search_worker(host);
    reach_icon_service_stop(host->icon_service);
    reach_host_stop_app_control(host);
    reach_now_playing_service_stop(host->now_playing_service);
    if (host->clipboard.ops.stop != nullptr)
    {
        (void)host->clipboard.ops.stop(host->clipboard.provider);
    }
    if (host->system_controls.stop_watching != nullptr)
    {
        host->system_controls.stop_watching(host->system_controls.userdata);
    }
    if (host->audio_volume.stop_watching != nullptr)
    {
        host->audio_volume.stop_watching(host->audio_volume.userdata);
    }
    reach_host_close_context_menu(host);
    reach_host_sync_popup_mouse_hook(host);
    reach_host_release_tray_render_icons(host);
    reach_idle_watch_stop(host->idle_watch);
    reach_system_status_stop(host->system_status);
    reach_system_stats_stop(host->system_stats);
    reach_host_release_quick_settings_audio_render_icons(host);
    reach_host_release_clipboard_items(host);
    if (host->monitors.ops.destroy != nullptr)
    {
        host->monitors.ops.destroy(host->monitors.list);
    }
    if (host->launcher.window.ops.destroy != nullptr)
    {
        host->launcher.window.ops.destroy(host->launcher.window.window);
    }
    if (host->launcher.renderer.ops.destroy != nullptr)
    {
        host->launcher.renderer.ops.destroy(host->launcher.renderer.backend);
    }
    if (host->dock.window.ops.destroy != nullptr)
    {
        host->dock.window.ops.destroy(host->dock.window.window);
    }
    if (host->dock.renderer.ops.destroy != nullptr)
    {
        host->dock.renderer.ops.destroy(host->dock.renderer.backend);
    }
    if (host->top_bar.window.ops.destroy != nullptr)
    {
        host->top_bar.window.ops.destroy(host->top_bar.window.window);
    }
    if (host->top_bar.renderer.ops.destroy != nullptr)
    {
        host->top_bar.renderer.ops.destroy(host->top_bar.renderer.backend);
    }
    if (host->tray.window.ops.destroy != nullptr)
    {
        host->tray.window.ops.destroy(host->tray.window.window);
    }
    if (host->tray.renderer.ops.destroy != nullptr)
    {
        host->tray.renderer.ops.destroy(host->tray.renderer.backend);
    }
    if (host->switcher.window.ops.destroy != nullptr)
    {
        host->switcher.window.ops.destroy(host->switcher.window.window);
    }
    if (host->switcher.renderer.ops.destroy != nullptr)
    {
        host->switcher.renderer.ops.destroy(host->switcher.renderer.backend);
    }
    if (host->context_menu.window.ops.destroy != nullptr)
    {
        host->context_menu.window.ops.destroy(host->context_menu.window.window);
    }
    if (host->context_menu.renderer.ops.destroy != nullptr)
    {
        host->context_menu.renderer.ops.destroy(host->context_menu.renderer.backend);
    }
    if (host->quick_settings.window.ops.destroy != nullptr)
    {
        host->quick_settings.window.ops.destroy(host->quick_settings.window.window);
    }
    if (host->quick_settings.renderer.ops.destroy != nullptr)
    {
        host->quick_settings.renderer.ops.destroy(host->quick_settings.renderer.backend);
    }
    if (host->clipboard_surface.window.ops.destroy != nullptr)
    {
        host->clipboard_surface.window.ops.destroy(host->clipboard_surface.window.window);
    }
    if (host->clipboard_surface.renderer.ops.destroy != nullptr)
    {
        host->clipboard_surface.renderer.ops.destroy(host->clipboard_surface.renderer.backend);
    }
    if (host->stage.window.ops.destroy != nullptr)
    {
        host->stage.window.ops.destroy(host->stage.window.window);
    }
    if (host->stage.renderer.ops.destroy != nullptr)
    {
        host->stage.renderer.ops.destroy(host->stage.renderer.backend);
    }
    if (host->window_thumbnails.ops.destroy != nullptr)
    {
        host->window_thumbnails.ops.destroy(host->window_thumbnails.thumbnails);
    }
    if (host->stage_reveal_corner.ops.hide != nullptr)
    {
        host->stage_reveal_corner.ops.hide(host->stage_reveal_corner.hotspot);
    }
    if (host->stage_reveal_corner.ops.destroy != nullptr)
    {
        host->stage_reveal_corner.ops.destroy(host->stage_reveal_corner.hotspot);
    }
    if (host->dock_reveal_edge.ops.hide != nullptr)
    {
        host->dock_reveal_edge.ops.hide(host->dock_reveal_edge.hotspot);
    }
    if (host->dock_reveal_edge.ops.destroy != nullptr)
    {
        host->dock_reveal_edge.ops.destroy(host->dock_reveal_edge.hotspot);
    }
    if (host->top_bar_reveal_edge.ops.hide != nullptr)
    {
        host->top_bar_reveal_edge.ops.hide(host->top_bar_reveal_edge.hotspot);
    }
    if (host->top_bar_reveal_edge.ops.destroy != nullptr)
    {
        host->top_bar_reveal_edge.ops.destroy(host->top_bar_reveal_edge.hotspot);
    }
    if (host->image_loader.ops.release != nullptr && host->wallpaper_image_id != 0)
    {
        host->image_loader.ops.release(host->image_loader.loader, host->wallpaper_image_id);
    }
    host->wallpaper_image_id = 0;
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
    if (host->tray_provider.ops.destroy != nullptr)
    {
        host->tray_provider.ops.destroy(host->tray_provider.provider);
    }
    reach_launcher_attach_search(host->launcher_capsule, nullptr);
    reach_launcher_attach_icons(host->launcher_capsule, nullptr);
    reach_dock_attach_services(host->dock_capsule, nullptr, nullptr);
    reach_top_bar_attach_app_control(host->top_bar_capsule, nullptr);
    reach_top_bar_attach_services(host->top_bar_capsule, nullptr, nullptr, nullptr, nullptr, nullptr,
                                  nullptr);
    reach_switcher_attach_services(host->switcher_capsule, nullptr, nullptr);
    reach_quick_settings_attach_status(host->quick_settings_capsule, nullptr);
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
    if (host->settings_launcher.ops.destroy != nullptr)
    {
        host->settings_launcher.ops.destroy(host->settings_launcher.launcher);
    }
    reach_icon_service_destroy(host->icon_service);
    host->icon_service = nullptr;
    reach_wallpaper_destroy(host->wallpaper);
    host->wallpaper = nullptr;
    reach_switcher_destroy(host->switcher_capsule);
    host->switcher_capsule = nullptr;
    reach_stage_destroy(host->stage_capsule);
    host->stage_capsule = nullptr;
    reach_quick_settings_destroy(host->quick_settings_capsule);
    host->quick_settings_capsule = nullptr;
    reach_clipboard_feature_destroy(host->clipboard_capsule);
    host->clipboard_capsule = nullptr;
    reach_dock_destroy(host->dock_capsule);
    host->dock_capsule = nullptr;
    reach_top_bar_destroy(host->top_bar_capsule);
    host->top_bar_capsule = nullptr;
    reach_tray_destroy(host->tray_capsule);
    host->tray_capsule = nullptr;
    reach_context_menu_destroy(host->context_menu_capsule);
    host->context_menu_capsule = nullptr;
    reach_launcher_destroy(host->launcher_capsule);
    host->launcher_capsule = nullptr;
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
    reach_surface_runtime_init(&host->launcher);
    reach_surface_runtime_init(&host->dock);
    reach_surface_runtime_init(&host->top_bar);
    reach_surface_runtime_init(&host->tray);
    reach_surface_runtime_init(&host->switcher);
    reach_surface_runtime_init(&host->stage);
    reach_surface_runtime_init(&host->context_menu);
    reach_surface_runtime_init(&host->quick_settings);
    reach_surface_runtime_init(&host->clipboard_surface);
    host->dock_reveal_edge = {};
    host->top_bar_reveal_edge = {};
    host->image_loader = {};
    host->wallpaper_image_id = 0;
    host->wallpaper_image_path[0] = 0;
    host->stage_reveal_corner = {};
    host->window_thumbnails = {};
    host->stage_reveal = {};
    host->stage_thumbnails_registered = 0;
    host->dock_reveal = {};
    host->top_bar_reveal = {};
    host->pointer_move = {};
    host->input_source = {};
    host->window_manager = {};
    host->foreground_watcher = {};
    host->config_store = {};
    host->tray_provider = {};
    host->search_provider = {};
    host->app_launcher = {};
    host->settings_launcher = {};
    host->icon_service = nullptr;
    host->explorer_service = {};
    host->wallpaper_service = {};
    host->wallpaper_surface = {};
    host->wallpaper = nullptr;
    host->switcher_capsule = nullptr;
    host->stage_capsule = nullptr;
    host->quick_settings_capsule = nullptr;
    host->clipboard_capsule = nullptr;
    host->dock_capsule = nullptr;
    host->tray_capsule = nullptr;
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

    host->switcher_capsule = nullptr;
    if (reach_switcher_create(&host->switcher_capsule) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    host->stage_capsule = nullptr;
    if (reach_stage_create(&host->stage_capsule) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    host->quick_settings_capsule = nullptr;
    if (reach_quick_settings_create(&host->quick_settings_capsule) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    host->clipboard_capsule = nullptr;
    if (reach_clipboard_feature_create(&host->clipboard_capsule) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    host->dock_capsule = nullptr;
    if (reach_dock_create(&host->dock_capsule) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    host->top_bar_capsule = nullptr;
    if (reach_top_bar_create(&host->top_bar_capsule) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    host->tray_capsule = nullptr;
    if (reach_tray_create(&host->tray_capsule) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    host->context_menu_capsule = nullptr;
    if (reach_context_menu_create(&host->context_menu_capsule) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    host->launcher_capsule = nullptr;
    if (reach_launcher_create(&host->launcher_capsule) != REACH_OK)
    {
        result = REACH_ERROR;
    }

    reach_dock_model_defaults(&host->dock_config);
    reach_surface_runtime_init(&host->launcher);
    reach_surface_runtime_init(&host->dock);
    reach_surface_runtime_init(&host->top_bar);
    reach_surface_runtime_init(&host->tray);
    reach_surface_runtime_init(&host->switcher);
    reach_surface_runtime_init(&host->stage);
    reach_surface_runtime_init(&host->context_menu);
    reach_surface_runtime_init(&host->quick_settings);
    reach_surface_runtime_init(&host->clipboard_surface);
    reach_animation_manager_init(&host->animations, host->animation_tracks,
                                 REACH_HOST_ANIMATION_COUNT);
    reach_host_surface_transitions_init(host);
    reach_host_init_surface_descriptors(host);
    reach_host_init_layout(host);

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->capsule_ops != nullptr && desc->capsule_ops->reset != nullptr)
        {
            desc->capsule_ops->reset(desc->capsule);
        }
    }
    reach_clipboard_feature_clear_refresh(host->clipboard_capsule);

    host->system_controls = {};
    host->quick_settings_system_change_flags.store(0);
    host->audio_volume_changed.store(0);

    host->launcher.window = dependencies->launcher_window;
    host->launcher.renderer = dependencies->launcher_renderer;
    host->dock.window = dependencies->dock_window;
    host->dock.renderer = dependencies->dock_renderer;
    host->dock_reveal_edge = dependencies->dock_reveal_edge;
    host->top_bar.window = dependencies->top_bar_window;
    host->top_bar.renderer = dependencies->top_bar_renderer;
    host->top_bar_reveal_edge = dependencies->top_bar_reveal_edge;
    host->image_loader = dependencies->image_loader;
    host->tray.window = dependencies->tray_window;
    host->tray.renderer = dependencies->tray_renderer;
    host->switcher.window = dependencies->switcher_window;
    host->switcher.renderer = dependencies->switcher_renderer;
    host->stage.window = dependencies->stage_window;
    host->stage.renderer = dependencies->stage_renderer;
    host->stage_reveal_corner = dependencies->stage_reveal_corner;
    host->window_thumbnails = dependencies->window_thumbnails;
    host->context_menu.window = dependencies->context_menu_window;
    host->context_menu.renderer = dependencies->context_menu_renderer;
    host->quick_settings.window = dependencies->quick_settings_window;
    host->quick_settings.renderer = dependencies->quick_settings_renderer;
    host->clipboard_surface.window = dependencies->clipboard_window;
    host->clipboard_surface.renderer = dependencies->clipboard_renderer;
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
    host->tray_provider = dependencies->tray_provider;
    host->search_provider = dependencies->search_provider;
    host->search_service = nullptr;
    if (reach_search_service_create(host->search_provider, reach_host_on_search_service_ready, host,
                                    &host->search_service) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    host->app_launcher = dependencies->app_launcher;
    host->app_control = nullptr;
    if (reach_app_control_create(host->app_launcher, dependencies->explorer_service,
                                 host->window_manager, reach_host_on_app_control_notify, host,
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
                               dependencies->config_store, &host->wallpaper) != REACH_OK)
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
    if (reach_input_language_service_create(dependencies->input_language,
                                            &host->input_language) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    host->media_controls = dependencies->media_controls;
    host->now_playing_service = nullptr;
    if (reach_now_playing_service_create(host->media_controls, reach_host_on_now_playing_ready,
                                         host, &host->now_playing_service) != REACH_OK)
    {
        result = REACH_ERROR;
    }
    reach_launcher_attach_search(host->launcher_capsule, host->search_service);
    reach_launcher_attach_icons(host->launcher_capsule, host->icon_service);
    reach_dock_attach_services(host->dock_capsule, host->icon_service, host->window_tracking);
    reach_top_bar_attach_services(host->top_bar_capsule, host->now_playing_service,
                                  host->icon_service, host->window_tracking, host->system_stats,
                                  host->clock, host->input_language);
    reach_top_bar_attach_app_control(host->top_bar_capsule, host->app_control);
    reach_switcher_attach_services(host->switcher_capsule, host->icon_service,
                                   host->window_tracking);
    reach_quick_settings_attach_status(host->quick_settings_capsule, host->system_status);
    reach_top_bar_attach_status(host->top_bar_capsule, host->system_status);
    reach_system_status_refresh_system(host->system_status, 0);
    host->clipboard = dependencies->clipboard;
    host->theme = reach_theme_default();

    if (host->monitors.list == nullptr || host->clipboard.provider == nullptr ||
        host->clipboard_surface.window.window == nullptr)
    {
        result = REACH_INVALID_ARGUMENT;
    }

    if (result == REACH_OK && host->config_store.ops.load != nullptr)
    {
        (void)reach_pin_config_ensure_defaults(&host->config_store);
        reach_config_snapshot snapshot = {};
        if (host->config_store.ops.load(host->config_store.store, &snapshot) == REACH_OK)
        {
            if (snapshot.dock_height > 0.0f)
                host->dock_config.height = snapshot.dock_height;
            if (snapshot.dock_width > 0.0f)
                host->dock_config.width = snapshot.dock_width;
            host->dock_config.icon_size =
                reach_theme_icon_box_size(host->theme, host->dock_config.height);
            (void)reach_host_set_pinned_apps(host, snapshot.pinned_apps, snapshot.pinned_app_count);
            reach_host_seed_or_apply_wallpaper(host, &snapshot);
            if (snapshot.power_shutdown_minutes != 0 || snapshot.power_restart_minutes != 0)
            {
                snapshot.power_shutdown_minutes = 0;
                snapshot.power_restart_minutes = 0;
                if (host->config_store.ops.save != nullptr)
                {
                    (void)host->config_store.ops.save(host->config_store.store, &snapshot);
                }
            }
            reach_host_apply_power_config(host, &snapshot);
            host->high_refresh_rate = snapshot.high_refresh_rate ? 1 : 0;
            reach_host_apply_ui_font(host, snapshot.bundled_font);
            reach_host_apply_theme_mode(host, snapshot.light_theme);
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
    host->dock.dirty_flags = 1;
    host->launcher.dirty_flags = 1;
    host->switcher.dirty_flags = 1;
    host->quick_settings.dirty_flags = 1;
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

    if (host->dock.window.ops.set_event_callback != nullptr)
    {
        result = host->dock.window.ops.set_event_callback(host->dock.window.window,
                                                          reach_host_on_dock_window_event, host);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    if (host->top_bar.window.ops.set_event_callback != nullptr)
    {
        result = host->top_bar.window.ops.set_event_callback(
            host->top_bar.window.window, reach_host_on_top_bar_window_event, host);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    if (host->launcher.window.ops.set_event_callback != nullptr)
    {
        result = host->launcher.window.ops.set_event_callback(
            host->launcher.window.window, reach_host_on_launcher_window_event, host);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    if (host->tray.window.ops.set_event_callback != nullptr)
    {
        result = host->tray.window.ops.set_event_callback(host->tray.window.window,
                                                          reach_host_on_tray_window_event, host);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    if (host->switcher.window.ops.set_event_callback != nullptr)
    {
        result = host->switcher.window.ops.set_event_callback(
            host->switcher.window.window, reach_host_on_switcher_window_event, host);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    if (host->stage.window.ops.set_event_callback != nullptr)
    {
        result = host->stage.window.ops.set_event_callback(
            host->stage.window.window, reach_host_on_stage_window_event, host);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    if (host->context_menu.window.ops.set_event_callback != nullptr)
    {
        result = host->context_menu.window.ops.set_event_callback(
            host->context_menu.window.window, reach_host_on_context_menu_window_event, host);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    if (host->quick_settings.window.ops.set_event_callback != nullptr)
    {
        result = host->quick_settings.window.ops.set_event_callback(
            host->quick_settings.window.window, reach_host_on_quick_settings_window_event, host);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    if (host->clipboard_surface.window.ops.set_event_callback != nullptr)
    {
        result = host->clipboard_surface.window.ops.set_event_callback(
            host->clipboard_surface.window.window, reach_host_on_clipboard_window_event, host);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    if (host->clipboard.ops.start != nullptr)
    {
        result = host->clipboard.ops.start(host->clipboard.provider,
                                           reach_host_on_clipboard_changed, host);
        if (result != REACH_OK)
        {
            return result;
        }
        reach_clipboard_feature_request_refresh(host->clipboard_capsule);
    }
    reach_host_sync_pointer_move_subscriptions(host);
    if (host->stage_reveal_corner.ops.set_callback != nullptr)
    {
        result = host->stage_reveal_corner.ops.set_callback(host->stage_reveal_corner.hotspot,
                                                      reach_host_on_stage_reveal_corner, host);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    if (host->dock_reveal_edge.ops.set_callback != nullptr)
    {
        result = host->dock_reveal_edge.ops.set_callback(host->dock_reveal_edge.hotspot,
                                                         reach_host_on_dock_reveal_edge, host);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    if (host->top_bar_reveal_edge.ops.set_callback != nullptr)
    {
        result = host->top_bar_reveal_edge.ops.set_callback(
            host->top_bar_reveal_edge.hotspot, reach_host_on_top_bar_reveal_edge, host);
        if (result != REACH_OK)
        {
            return result;
        }
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
    host->dock.dirty_flags = 1;
    host->top_bar.dirty_flags = 1;
    host->launcher.dirty_flags = 1;
    host->tray.dirty_flags = 1;
    host->switcher.dirty_flags = 1;
    host->quick_settings.dirty_flags = 1;
    reach_context_menu_force_close(host->context_menu_capsule);
    reach_quick_settings_force_close(host->quick_settings_capsule);
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
    reach_switcher_force_close(host->switcher_capsule);
    reach_stage_force_close(host->stage_capsule);
    reach_context_menu_force_close(host->context_menu_capsule);
    reach_host_set_tray_popup_open(host, 0);
    reach_host_set_quick_settings_open(host, 0);
    reach_launcher_cancel_search(host->launcher_capsule);
    reach_host_stop_config_reload_worker(host);
    reach_host_stop_launcher_search_worker(host);
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
