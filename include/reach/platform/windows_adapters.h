#ifndef REACH_PLATFORM_WINDOWS_ADAPTERS_H
#define REACH_PLATFORM_WINDOWS_ADAPTERS_H

#include "reach/ports/app_launcher.h"
#include "reach/ports/app_update.h"
#include "reach/ports/audio_volume.h"
#include "reach/ports/bluetooth.h"
#include "reach/ports/config_store.h"
#include "reach/ports/image_loader.h"
#include "reach/ports/clock.h"
#include "reach/ports/input_language.h"
#include "reach/ports/screen_hotspot.h"
#include "reach/ports/foreground_watcher.h"
#include "reach/ports/explorer_service.h"
#include "reach/ports/icon_provider.h"
#include "reach/ports/input_source.h"
#include "reach/ports/media_controls.h"
#include "reach/ports/monitor.h"
#include "reach/ports/platform_window.h"
#include "reach/ports/popup_capture.h"
#include "reach/ports/power_session.h"
#include "reach/ports/render_backend.h"
#include "reach/ports/search_provider.h"
#include "reach/ports/settings_launcher.h"
#include "reach/ports/startup_apps.h"
#include "reach/ports/system_controls.h"
#include "reach/ports/system_stats.h"
#include "reach/ports/tray_provider.h"
#include "reach/ports/user_account.h"
#include "reach/ports/wallpaper_service.h"
#include "reach/ports/wifi.h"
#include "reach/ports/wallpaper_surface.h"
#include "reach/ports/window_manager.h"
#include "reach/ports/window_thumbnail.h"
#include "reach/ports/windows_update.h"
#include "reach/ports/clipboard.h"

#ifdef __cplusplus
extern "C"
{
#endif

    reach_result reach_windows_create_platform_window(reach_surface_role role,
                                                      reach_platform_window_port *out_port);
    reach_result reach_windows_create_d2d_render_backend(reach_platform_window *window,
                                                         reach_render_backend_port *out_port);
    reach_result reach_windows_create_dcomp_render_backend(reach_platform_window *window,
                                                           reach_render_backend_port *out_port);
    reach_result reach_windows_create_search_provider(reach_search_provider_port *out_port);
    reach_result reach_windows_create_settings_launcher(reach_settings_launcher_port *out_port);
    reach_result reach_windows_create_app_launcher(reach_app_launcher_port *out_port);
    reach_result reach_windows_create_tray_provider(reach_tray_provider_port *out_port);
    reach_result reach_windows_create_icon_provider(reach_icon_provider_port *out_port);
    reach_result reach_windows_create_input_source(reach_input_source_port *out_port);
    reach_result reach_windows_create_media_controls(reach_media_controls_port *out_port);
    reach_result reach_windows_create_windows_update(reach_windows_update_port *out_port);
    reach_result reach_windows_create_app_update(reach_app_update_port *out_port);
    reach_result reach_windows_create_clipboard_provider(reach_clipboard_port *out_port);
    reach_result reach_windows_create_config_store(const uint16_t *path,
                                                   reach_config_store_port *out_port);
    reach_result reach_windows_create_window_manager(reach_window_manager_port *out_port);
    reach_result reach_windows_create_explorer_service(reach_explorer_service_port *out_port);
    reach_result reach_windows_create_wallpaper_service(reach_wallpaper_service_port *out_port);
    reach_result reach_windows_create_wallpaper_surface(reach_wallpaper_surface_port *out_port);
    reach_result reach_windows_create_popup_capture(reach_popup_capture_port *out_port);
    reach_result reach_windows_create_power_session(reach_power_session_port *out_port);
    reach_result reach_windows_create_audio_volume(reach_audio_volume_port *out_port);
    reach_result reach_windows_create_system_controls(reach_system_controls_port *out_port);
    reach_result reach_windows_create_wifi(reach_wifi_port *out_port);
    reach_result reach_windows_create_bluetooth(reach_bluetooth_port *out_port);
    reach_result
    reach_windows_create_screen_hotspot_factory(reach_screen_hotspot_factory_port *out_port);
    reach_result reach_windows_create_image_loader(reach_image_loader_port *out_port);
    reach_result reach_windows_create_input_language(reach_input_language_port *out_port);
    reach_result reach_windows_create_system_stats(reach_system_stats_port *out_port);
    reach_result reach_windows_create_clock(reach_clock_port *out_port);
    reach_result reach_windows_create_window_thumbnails(reach_window_thumbnail_port *out_port);
    reach_result reach_windows_create_foreground_watcher(reach_foreground_watcher_port *out_port);
    reach_result reach_windows_create_monitor_list(reach_monitor_port *out_port);
    reach_result reach_windows_create_user_account(reach_user_account_port *out_port);
    reach_result reach_windows_create_explorer_desktop_compat_host(void);
    reach_window_id reach_windows_desktop_compat_window(void);
    void reach_windows_destroy_explorer_desktop_compat_host(void);
    void reach_windows_notify_desktop_environment_changed(void);
    void reach_windows_request_desktop_environment_sync(void);
    reach_result reach_windows_default_config_path(uint16_t *path, uint32_t path_count);
    reach_result reach_windows_notify_config_changed(void);

    reach_result reach_windows_create_startup_apps(reach_startup_apps_port *out_port);
    size_t reach_windows_collect_startup_apps(reach_app_launch_request *out_requests,
                                              size_t capacity);
    uintptr_t reach_windows_get_current_foreground(void);

#ifdef __cplusplus
}
#endif

#endif
