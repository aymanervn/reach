#ifndef REACH_PLATFORM_WINDOWS_ADAPTERS_H
#define REACH_PLATFORM_WINDOWS_ADAPTERS_H

#include "reach/ports/app_launcher.h"
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

#ifdef __cplusplus
extern "C" {
#endif

reach_result reach_windows_create_platform_window(reach_surface_role role, reach_platform_window_port *out_port);
reach_result reach_windows_create_d2d_render_backend(void *native_window, reach_render_backend_port *out_port);
reach_result reach_windows_create_search_stub(reach_search_provider_port *out_port);
reach_result reach_windows_create_app_launcher(reach_app_launcher_port *out_port);
reach_result reach_windows_create_tray_provider(reach_tray_provider_port *out_port);
reach_result reach_windows_create_icon_provider(reach_icon_provider_port *out_port);
reach_result reach_windows_create_input_source(reach_input_source_port *out_port);
reach_result reach_windows_create_config_store(const uint16_t *path, reach_config_store_port *out_port);
reach_result reach_windows_create_window_manager(reach_window_manager_port *out_port);
reach_result reach_windows_create_explorer_service(reach_explorer_service_port *out_port);
reach_result reach_windows_create_wallpaper_service(reach_wallpaper_service_port *out_port);
reach_result reach_windows_create_wallpaper_surface(reach_wallpaper_surface_port *out_port);

#ifdef __cplusplus
}
#endif

#endif
