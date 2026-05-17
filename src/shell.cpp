#include "reach/shell.h"

#include "reach/applist.h"
#include "reach/config.h"
#include "reach/core/render_commands.h"
#include "reach/core/ui_events.h"
#include "reach/core/ui_layout.h"
#include "reach/dock.h"
#include "reach/hotkeys.h"
#include "reach/monitor.h"
#include "reach/platform/windows_adapters.h"
#include "reach/search.h"
#include "reach/wm.h"

#include <windows.h>

#include <new>

struct reach_shell {
    reach_config *config;
    reach_applist *apps;
    reach_dock *dock;
    reach_hotkeys *hotkeys;
    reach_monitor_list *monitors;
    reach_search *search;
    reach_wm *wm;
    reach_ui_state ui;
    reach_platform_window_port launcher_window;
    reach_search_provider_port search_provider;
    reach_app_launcher_port app_launcher;
    int32_t running;
};

static void reach_shell_cleanup(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }

    reach_dock_destroy(shell->dock);
    reach_wm_destroy(shell->wm);
    reach_hotkeys_destroy(shell->hotkeys);
    reach_monitor_list_destroy(shell->monitors);
    reach_search_destroy(shell->search);
    reach_applist_destroy(shell->apps);
    reach_config_destroy(shell->config);
    if (shell->launcher_window.ops.destroy != nullptr) {
        shell->launcher_window.ops.destroy(shell->launcher_window.window);
    }
    if (shell->search_provider.ops.destroy != nullptr) {
        shell->search_provider.ops.destroy(shell->search_provider.provider);
    }
    if (shell->app_launcher.ops.destroy != nullptr) {
        shell->app_launcher.ops.destroy(shell->app_launcher.launcher);
    }
    shell->dock = nullptr;
    shell->wm = nullptr;
    shell->hotkeys = nullptr;
    shell->monitors = nullptr;
    shell->search = nullptr;
    shell->apps = nullptr;
    shell->config = nullptr;
    shell->launcher_window = {};
    shell->search_provider = {};
    shell->app_launcher = {};
}

reach_result reach_shell_create(const reach_shell_desc *desc, reach_shell **out_shell)
{
    (void)desc;
    if (out_shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_shell *shell = new (std::nothrow) reach_shell();
    if (shell == nullptr) {
        *out_shell = nullptr;
        return REACH_ERROR;
    }

    reach_ui_state_init(&shell->ui);

    reach_dock_config dock_config = {};
    dock_config.height_px = 56;
    dock_config.icon_size_px = 32;
    dock_config.auto_hide = 1;
    dock_config.animation_seconds = 0.16;

    reach_dock_desc dock_desc = {};
    dock_desc.config = &dock_config;

    reach_result result = reach_monitor_list_create(&shell->monitors);
    if (result == REACH_OK) {
        result = reach_wm_create(&shell->wm);
    }
    if (result == REACH_OK) {
        result = reach_hotkeys_create(&shell->hotkeys);
    }
    if (result == REACH_OK) {
        result = reach_dock_create(&dock_desc, &shell->dock);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_platform_window(REACH_SURFACE_LAUNCHER, &shell->launcher_window);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_search_stub(&shell->search_provider);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_app_launcher(&shell->app_launcher);
    }

    if (result != REACH_OK) {
        reach_shell_cleanup(shell);
        delete shell;
        *out_shell = nullptr;
        return result;
    }

    *out_shell = shell;
    return REACH_OK;
}

void reach_shell_destroy(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }

    reach_shell_cleanup(shell);
    delete shell;
}

reach_result reach_shell_start(reach_shell *shell)
{
    if (shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = reach_wm_install_hooks(shell->wm);
    if (result != REACH_OK) {
        return result;
    }

    result = reach_dock_show(shell->dock);
    if (result != REACH_OK) {
        return result;
    }

    shell->running = 1;
    return REACH_OK;
}

reach_result reach_shell_stop(reach_shell *shell)
{
    if (shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    shell->running = 0;
    (void)reach_wm_uninstall_hooks(shell->wm);
    (void)reach_hotkeys_unregister_all(shell->hotkeys);
    (void)reach_dock_hide(shell->dock);
    if (shell->launcher_window.ops.hide != nullptr) {
        (void)shell->launcher_window.ops.hide(shell->launcher_window.window);
    }
    return REACH_OK;
}

reach_result reach_shell_update(reach_shell *shell, double delta_seconds)
{
    if (shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    (void)reach_wm_update_z_order(shell->wm);
    if (shell->launcher_window.ops.set_bounds != nullptr && shell->monitors != nullptr && reach_monitor_count(shell->monitors) > 0) {
        const reach_monitor_info *monitor = reach_monitor_get(shell->monitors, 0);
        reach_rect_f32 bounds = {};
        bounds.x = (float)monitor->bounds.left;
        bounds.y = (float)monitor->bounds.top;
        bounds.width = (float)(monitor->bounds.right - monitor->bounds.left);
        bounds.height = (float)(monitor->bounds.bottom - monitor->bounds.top);
        (void)shell->launcher_window.ops.set_bounds(shell->launcher_window.window, bounds);
        (void)shell->launcher_window.ops.set_opacity(shell->launcher_window.window, shell->ui.launcher.open ? 0.92f : 0.0f);
    }
    return reach_dock_update(shell->dock, delta_seconds);
}
