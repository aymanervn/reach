#ifndef REACH_WINDOWS_ADAPTERS_INTERNAL_H
#define REACH_WINDOWS_ADAPTERS_INTERNAL_H

#include "reach/ports/app_launcher.h"
#include "reach/ports/audio_volume.h"
#include "reach/ports/config_store.h"
#include "reach/ports/dock_reveal_edge.h"
#include "reach/ports/explorer_service.h"
#include "reach/ports/hotkeys.h"
#include "reach/ports/icon_provider.h"
#include "reach/ports/input_source.h"
#include "reach/ports/monitor.h"
#include "reach/ports/platform_window.h"
#include "reach/ports/popup_capture.h"
#include "reach/ports/power_session.h"
#include "reach/ports/render_backend.h"
#include "reach/ports/search_provider.h"
#include "reach/ports/system_controls.h"
#include "reach/ports/tray_provider.h"
#include "reach/ports/wallpaper_service.h"
#include "reach/ports/wallpaper_surface.h"
#include "reach/ports/window_manager.h"
#include "reach/support/util.h"

#define REACH_WM_WALLPAPER_CHANGED 0x8014
#define REACH_WM_LAUNCHER_SEARCH_READY 0x8015
#define REACH_WM_CONFIG_CHANGED 0x8016

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_shell_registration_status {
    uint16_t current_shell[260];
    uint16_t previous_shell[260];
    int32_t reach_is_shell;
} reach_shell_registration_status;

reach_result reach_windows_create_platform_window(reach_surface_role role, reach_platform_window_port *out_port);
void *reach_windows_platform_window_native_handle(reach_platform_window *window);
reach_result reach_windows_create_d2d_render_backend(reach_platform_window *window, reach_render_backend_port *out_port);
reach_result reach_windows_create_dcomp_render_backend(reach_platform_window *window, reach_render_backend_port *out_port);
reach_result reach_windows_create_search_provider(reach_search_provider_port *out_port);
reach_result reach_windows_create_app_launcher(reach_app_launcher_port *out_port);
reach_result reach_windows_create_tray_provider(reach_tray_provider_port *out_port);
reach_result reach_windows_create_icon_provider(reach_icon_provider_port *out_port);
reach_result reach_windows_create_input_source(reach_input_source_port *out_port);
reach_result reach_windows_create_config_store(const uint16_t *path, reach_config_store_port *out_port);
reach_result reach_windows_create_window_manager(reach_window_manager_port *out_port);
reach_result reach_windows_create_explorer_service(reach_explorer_service_port *out_port);
reach_result reach_windows_create_wallpaper_service(reach_wallpaper_service_port *out_port);
reach_result reach_windows_create_wallpaper_surface(reach_wallpaper_surface_port *out_port);
reach_result reach_windows_create_popup_capture(reach_popup_capture_port *out_port);
reach_result reach_windows_create_power_session(reach_power_session_port *out_port);
reach_result reach_windows_create_audio_volume(reach_audio_volume_port *out_port);
reach_result reach_windows_create_system_controls(reach_system_controls_port *out_port);
reach_result reach_windows_create_dock_reveal_edge(reach_dock_reveal_edge_port *out_port);
reach_result reach_windows_create_hotkeys(reach_hotkeys_port *out_port);
reach_result reach_windows_create_monitor_list(reach_monitor_port *out_port);
reach_result reach_windows_default_config_path(uint16_t *path, uint32_t path_count);
reach_result reach_windows_launch_startup_apps(void);
reach_result reach_windows_shell_install_current_user(const uint16_t *exe_path);
reach_result reach_windows_shell_install_command_current_user(const uint16_t *identity_exe_path, const uint16_t *shell_command);
reach_result reach_windows_shell_restore_current_user(void);
reach_result reach_windows_shell_query_current_user(const uint16_t *exe_path, reach_shell_registration_status *out_status);
reach_result reach_windows_shell_launch_explorer(void);

#ifdef __cplusplus
}
#endif

#endif
