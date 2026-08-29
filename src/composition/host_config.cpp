#include "host_internal.h"

#include <memory>
#include <new>

reach_result reach_host_set_pinned_apps(reach_host *host, const reach_pinned_app_model *apps,
                                        size_t count)
{
    if (host == nullptr || (apps == nullptr && count != 0))
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (count > REACH_MAX_PINNED_APPS)
    {
        count = REACH_MAX_PINNED_APPS;
    }
    for (size_t index = 0; index < count; ++index)
    {
        host->pinned_apps[index] = apps[index];
    }
    host->pinned_app_count = count;
    return REACH_OK;
}

reach_result reach_host_request_config_reload(reach_host *host)
{
    return host != nullptr ? reach_config_service_reload(host->config_service)
                           : REACH_INVALID_ARGUMENT;
}

reach_result reach_host_pin_app(reach_host *host, const reach_pinned_app_model *app)
{
    return host != nullptr ? reach_config_service_pin_app(host->config_service, app)
                           : REACH_INVALID_ARGUMENT;
}

reach_result reach_host_unpin_id(reach_host *host, uint32_t pin_id)
{
    return host != nullptr ? reach_config_service_unpin_id(host->config_service, pin_id)
                           : REACH_INVALID_ARGUMENT;
}

reach_result reach_host_move_pin(reach_host *host, uint32_t pin_id, size_t target_index)
{
    return host != nullptr
               ? reach_config_service_move_pin(host->config_service, pin_id, target_index)
               : REACH_INVALID_ARGUMENT;
}

void reach_host_stop_config_service(reach_host *host)
{
    if (host != nullptr)
    {
        reach_config_service_stop(host->config_service);
    }
}

int32_t reach_host_apply_config_update(reach_host *host)
{
    if (host == nullptr)
    {
        return 0;
    }

    reach_config_snapshot snapshot = {};
    if (!reach_config_service_take_snapshot_update(host->config_service, &snapshot))
    {
        return 0;
    }
    (void)reach_host_apply_config_snapshot(host, &snapshot, 1, 1);
    return 1;
}

static reach_result reach_host_apply_pins_from_snapshot(reach_host *host,
                                                        const reach_config_snapshot *snapshot)
{
    if (host == nullptr || snapshot == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result =
        reach_host_set_pinned_apps(host, snapshot->pinned_apps, snapshot->pinned_app_count);
    if (result != REACH_OK)
    {
        return result;
    }

    reach_host_notify_pinned_apps_changed(host);
    host->dirty.layout = 1;
    host->dirty.render = 1;
    reach_host_mark_all_surfaces_dirty(host);
    reach_host_request_update(host);
    return result;
}

void reach_host_apply_power_config(reach_host *host, const reach_config_snapshot *snapshot)
{
    if (host == nullptr || snapshot == nullptr)
    {
        return;
    }
    reach_idle_watch_config config = {};
    config.timeout_minutes[REACH_IDLE_WATCH_ACTION_SCREEN_OFF] = snapshot->power_screen_off_minutes;
    config.wait_awake_apps[REACH_IDLE_WATCH_ACTION_SCREEN_OFF] = 1;
    config.timeout_minutes[REACH_IDLE_WATCH_ACTION_SLEEP] = snapshot->power_sleep_minutes;
    config.timeout_minutes[REACH_IDLE_WATCH_ACTION_LOCK] = snapshot->power_lock_minutes;
    config.timeout_minutes[REACH_IDLE_WATCH_ACTION_SHUTDOWN] = snapshot->power_shutdown_minutes;
    config.timeout_minutes[REACH_IDLE_WATCH_ACTION_RESTART] = snapshot->power_restart_minutes;
    config.wait_awake_apps[REACH_IDLE_WATCH_ACTION_SLEEP] = snapshot->power_sleep_wait_apps;
    config.wait_awake_apps[REACH_IDLE_WATCH_ACTION_SHUTDOWN] = snapshot->power_shutdown_wait_apps;
    config.wait_awake_apps[REACH_IDLE_WATCH_ACTION_RESTART] = snapshot->power_restart_wait_apps;
    reach_idle_watch_set_config(host->idle_watch, &config);
}

void reach_host_seed_or_apply_wallpaper(reach_host *host, reach_config_snapshot *snapshot)
{
    if (host == nullptr || snapshot == nullptr)
    {
        return;
    }
    if (reach_wallpaper_apply_snapshot(host->wallpaper, snapshot))
    {
        (void)reach_config_service_set_wallpapers(host->config_service, snapshot->wallpaper_path,
                                                  snapshot->monitor_wallpaper_paths,
                                                  REACH_MAX_WALLPAPER_MONITORS);
    }
}

static int32_t reach_host_pinned_apps_equal(const reach_pinned_app_model *a, size_t a_count,
                                            const reach_pinned_app_model *b, size_t b_count)
{
    if (a_count != b_count)
    {
        return 0;
    }

    for (size_t index = 0; index < a_count; ++index)
    {
        if (a[index].id != b[index].id || !reach_utf16_equal(a[index].path, b[index].path) ||
            !reach_utf16_equal(a[index].arguments, b[index].arguments) ||
            !reach_utf16_equal(a[index].icon_ref, b[index].icon_ref) ||
            !reach_utf16_equal(a[index].app_user_model_id, b[index].app_user_model_id))
        {
            return 0;
        }
    }

    return 1;
}

reach_result reach_host_apply_config_snapshot(reach_host *host,
                                              const reach_config_snapshot *snapshot,
                                              int32_t apply_pins, int32_t apply_wallpaper)
{
    if (host == nullptr || snapshot == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (apply_pins &&
        !reach_host_pinned_apps_equal(host->pinned_apps, host->pinned_app_count,
                                      snapshot->pinned_apps, snapshot->pinned_app_count))
    {
        reach_result pin_result = reach_host_apply_pins_from_snapshot(host, snapshot);
        if (pin_result != REACH_OK)
        {
            return pin_result;
        }
    }

    if (apply_wallpaper)
    {
        std::unique_ptr<reach_config_snapshot> writable_snapshot(
            new (std::nothrow) reach_config_snapshot(*snapshot));
        if (writable_snapshot == nullptr)
        {
            return REACH_ERROR;
        }
        reach_host_seed_or_apply_wallpaper(host, writable_snapshot.get());
    }
    reach_host_apply_power_config(host, snapshot);
    reach_host_apply_display_config(host, snapshot);
    if (snapshot->stage_animation_ms > 0)
    {
        reach_stage_set_animation_seconds(
            reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE),
            (float)snapshot->stage_animation_ms / 1000.0f);
    }
    return REACH_OK;
}

void reach_host_apply_ui_font(reach_host *host, int32_t bundled_font)
{
    if (host == nullptr)
    {
        return;
    }
    int32_t enabled = bundled_font ? 1 : 0;
    if (host->bundled_font == enabled)
    {
        return;
    }
    host->bundled_font = enabled;

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_surface_runtime *surface = host->feature_runtimes[index].surface;
        if (surface == nullptr || surface->renderer.ops.set_ui_font == nullptr)
        {
            continue;
        }
        surface->renderer.ops.set_ui_font(surface->renderer.backend, enabled);
        reach_surface_runtime_mark_dirty(surface, 1);
    }
    host->dirty.layout = 1;
    host->dirty.render = 1;
    reach_host_request_update(host);
}

void reach_host_apply_theme_mode(reach_host *host, int32_t light_theme)
{
    if (host == nullptr)
    {
        return;
    }
    const reach_theme *previous = host->theme;
    reach_host_set_theme_mode(host, light_theme ? REACH_THEME_MODE_LIGHT : REACH_THEME_MODE_DARK);
    if (host->theme == previous)
    {
        return;
    }
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_surface_runtime *surface = host->feature_runtimes[index].surface;
        if (surface != nullptr)
        {
            reach_surface_runtime_mark_dirty(surface, 1);
        }
    }
    reach_host_request_update(host);
}

static reach_theme_mode reach_host_resolve_system_theme(reach_config_theme_preference preference,
                                                        reach_theme_mode reach_mode)
{
    if (preference == REACH_CONFIG_THEME_LIGHT)
    {
        return REACH_THEME_MODE_LIGHT;
    }
    if (preference == REACH_CONFIG_THEME_DARK)
    {
        return REACH_THEME_MODE_DARK;
    }
    return reach_mode;
}

void reach_host_apply_display_config(reach_host *host, const reach_config_snapshot *snapshot)
{
    if (host == nullptr || snapshot == nullptr)
    {
        return;
    }

    host->high_refresh_rate = snapshot->high_refresh_rate ? 1 : 0;
    reach_host_apply_ui_font(host, snapshot->bundled_font);
    reach_host_apply_theme_mode(host, snapshot->light_theme);

    if (host->system_controls.set_theme_modes == nullptr)
    {
        return;
    }
    reach_theme_mode reach_mode = reach_host_theme_mode(host);
    reach_theme_mode system_mode =
        reach_host_resolve_system_theme(snapshot->windows_system_theme, reach_mode);
    reach_theme_mode app_mode =
        reach_host_resolve_system_theme(snapshot->windows_app_theme, reach_mode);
    (void)host->system_controls.set_theme_modes(host->system_controls.userdata, system_mode,
                                                app_mode);
}

void reach_host_reload_wallpaper(reach_host *host, int32_t force)
{
    if (host == nullptr)
    {
        return;
    }
    reach_config_snapshot snapshot = {};
    if (reach_config_service_snapshot(host->config_service, &snapshot) == REACH_OK)
    {
        reach_wallpaper_reload(host->wallpaper, &snapshot, force);
    }
}
