#ifndef REACH_SERVICES_CONFIG_H
#define REACH_SERVICES_CONFIG_H

#include <stdint.h>

#include "reach/ports/config_store.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_config_service reach_config_service;

    typedef enum reach_config_service_event
    {
        REACH_CONFIG_SERVICE_SNAPSHOT_CHANGED = 1,
        REACH_CONFIG_SERVICE_PERSISTED = 2,
        REACH_CONFIG_SERVICE_PERSIST_FAILED = 3
    } reach_config_service_event;

    typedef struct reach_config_power_settings
    {
        int32_t screen_off_minutes;
        int32_t sleep_minutes;
        int32_t lock_minutes;
        int32_t shutdown_minutes;
        int32_t restart_minutes;
        int32_t sleep_wait_apps;
        int32_t shutdown_wait_apps;
        int32_t restart_wait_apps;
    } reach_config_power_settings;

    typedef struct reach_config_display_settings
    {
        int32_t high_refresh_rate;
        int32_t bundled_font;
        int32_t light_theme;
        reach_config_theme_preference windows_system_theme;
        reach_config_theme_preference windows_app_theme;
    } reach_config_display_settings;

    typedef void (*reach_config_service_notify)(void *user, reach_config_service_event event);

    reach_result reach_config_service_create(reach_config_store_port store,
                                             reach_config_service_notify notify, void *notify_user,
                                             reach_config_service **out_service);
    void reach_config_service_destroy(reach_config_service *service);
    void reach_config_service_stop(reach_config_service *service);
    reach_result reach_config_service_flush(reach_config_service *service);

    reach_result reach_config_service_snapshot(const reach_config_service *service,
                                               reach_config_snapshot *out_snapshot);
    int32_t reach_config_service_take_snapshot_update(reach_config_service *service,
                                                      reach_config_snapshot *out_snapshot);

    reach_result reach_config_service_reload(reach_config_service *service);
    reach_result reach_config_service_ensure_defaults(reach_config_service *service);
    reach_result reach_config_service_pin_path(reach_config_service *service, const uint16_t *path);
    reach_result reach_config_service_pin_app(reach_config_service *service,
                                              const reach_pinned_app_model *app);
    reach_result reach_config_service_unpin_id(reach_config_service *service, uint32_t pin_id);
    reach_result reach_config_service_unpin_path(reach_config_service *service,
                                                 const uint16_t *path);
    reach_result reach_config_service_move_pin(reach_config_service *service, uint32_t pin_id,
                                               size_t target_index);
    reach_result reach_config_service_set_power(reach_config_service *service,
                                                const reach_config_power_settings *settings);
    reach_result reach_config_service_set_display(reach_config_service *service,
                                                  const reach_config_display_settings *settings);
    reach_result reach_config_service_set_wallpapers(reach_config_service *service,
                                                     const uint16_t *wallpaper_path,
                                                     const uint16_t monitor_wallpaper_paths[][260],
                                                     size_t monitor_count);
    reach_result reach_config_service_set_monitor_wallpaper(reach_config_service *service,
                                                            size_t monitor_index,
                                                            const uint16_t *path);

    int32_t reach_config_service_dirty(const reach_config_service *service);

#ifdef __cplusplus
}
#endif

#endif
