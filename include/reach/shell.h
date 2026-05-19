#ifndef REACH_SHELL_H
#define REACH_SHELL_H

#include <stdint.h>

#include "reach/ports/app_launcher.h"
#include "reach/core/ui_events.h"
#include "reach/ports/config_store.h"
#include "reach/ports/explorer_service.h"
#include "reach/ports/icon_provider.h"
#include "reach/ports/input_source.h"
#include "reach/ports/platform_window.h"
#include "reach/ports/render_backend.h"
#include "reach/ports/search_provider.h"
#include "reach/ports/tray_provider.h"
#include "reach/ports/wallpaper_service.h"
#include "reach/ports/wallpaper_surface.h"
#include "reach/ports/window_manager.h"
#include "reach/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_shell reach_shell;

typedef struct reach_shell_desc {
    const uint16_t *config_path;
} reach_shell_desc;

typedef struct reach_shell_dependencies {
    reach_platform_window_port launcher_window;
    reach_render_backend_port launcher_renderer;
    reach_platform_window_port dock_window;
    reach_render_backend_port dock_renderer;
    reach_platform_window_port tray_window;
    reach_render_backend_port tray_renderer;
    reach_platform_window_port switcher_window;
    reach_render_backend_port switcher_renderer;
    reach_input_source_port input_source;
    reach_window_manager_port window_manager;
    reach_config_store_port config_store;
    reach_tray_provider_port tray_provider;
    reach_search_provider_port search_provider;
    reach_app_launcher_port app_launcher;
    reach_icon_provider_port icon_provider;
    reach_explorer_service_port explorer_service;
    reach_wallpaper_service_port wallpaper_service;
    reach_wallpaper_surface_port wallpaper_surface;
} reach_shell_dependencies;

reach_result reach_shell_create_with_dependencies(const reach_shell_desc *desc, const reach_shell_dependencies *dependencies, reach_shell **out_shell);
reach_result reach_shell_create(const reach_shell_desc *desc, reach_shell **out_shell);
void reach_shell_destroy(reach_shell *shell);
reach_result reach_shell_start(reach_shell *shell);
reach_result reach_shell_stop(reach_shell *shell);
reach_result reach_shell_handle_event(reach_shell *shell, const reach_ui_event *event);
reach_result reach_shell_update(reach_shell *shell, double delta_seconds);
int32_t reach_shell_needs_frame(const reach_shell *shell);

#ifdef __cplusplus
}
#endif

#endif
