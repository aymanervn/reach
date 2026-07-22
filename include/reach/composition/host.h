#ifndef REACH_HOST_H
#define REACH_HOST_H

#include <stdint.h>

#include "reach/ports/app_launcher.h"
#include "reach/core/ui_events.h"
#include "reach/ports/config_store.h"
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
#include "reach/ports/system_controls.h"
#include "reach/ports/tray_provider.h"
#include "reach/ports/wallpaper_service.h"
#include "reach/ports/wallpaper_surface.h"
#include "reach/ports/window_manager.h"
#include "reach/ports/audio_volume.h"
#include "reach/ports/dock_reveal_edge.h"
#include "reach/ports/foreground_watcher.h"
#include "reach/ports/clipboard.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_host reach_host;

    typedef struct reach_host_desc
    {
        const uint16_t *config_path;
    } reach_host_desc;

    typedef struct reach_host_dependencies
    {
        reach_platform_window_port launcher_window;
        reach_render_backend_port launcher_renderer;
        reach_platform_window_port dock_window;
        reach_render_backend_port dock_renderer;
        reach_dock_reveal_edge_port dock_reveal_edge;
        reach_platform_window_port tray_window;
        reach_render_backend_port tray_renderer;
        reach_platform_window_port switcher_window;
        reach_render_backend_port switcher_renderer;
        reach_platform_window_port context_menu_window;
        reach_render_backend_port context_menu_renderer;
        reach_platform_window_port quick_settings_window;
        reach_render_backend_port quick_settings_renderer;
        reach_platform_window_port clipboard_window;
        reach_render_backend_port clipboard_renderer;
        reach_input_source_port input_source;
        reach_monitor_port monitors;
        reach_window_manager_port window_manager;
        reach_foreground_watcher_port foreground_watcher;
        reach_config_store_port config_store;
        reach_tray_provider_port tray_provider;
        reach_search_provider_port search_provider;
        reach_app_launcher_port app_launcher;
        reach_settings_launcher_port settings_launcher;
        reach_icon_provider_port icon_provider;
        reach_explorer_service_port explorer_service;
        reach_wallpaper_service_port wallpaper_service;
        reach_wallpaper_surface_port wallpaper_surface;
        reach_popup_capture_port popup_capture;
        reach_power_session_port power_session;
        reach_audio_volume_port audio_volume;
        reach_system_controls_port system_controls;
        reach_media_controls_port media_controls;
        reach_clipboard_port clipboard;
    } reach_host_dependencies;

    reach_result reach_host_create_with_dependencies(const reach_host_desc *desc,
                                                     const reach_host_dependencies *dependencies,
                                                     reach_host **out_shell);
    void reach_host_set_initial_foreground(reach_host *host, uintptr_t window);
    void reach_host_destroy(reach_host *host);
    reach_result reach_host_start(reach_host *host);
    reach_result reach_host_stop(reach_host *host);
    reach_result reach_host_handle_event(reach_host *host, const reach_ui_event *event);
    int32_t reach_host_has_pending_events(const reach_host *host);
    reach_result reach_host_dispatch_events(reach_host *host);
    reach_result reach_host_update(reach_host *host, double delta_seconds);
    int32_t reach_host_needs_frame(const reach_host *host);

#ifdef __cplusplus
}
#endif

#endif
