#include "shell_internal.h"

static void reach_shell_dispatch_surface_events(reach_surface_runtime *surface)
{
    if (surface == nullptr || surface->window.ops.dispatch_events == nullptr)
    {
        return;
    }
    if (surface->window.ops.has_pending_events != nullptr &&
        !surface->window.ops.has_pending_events(surface->window.window))
    {
        return;
    }
    (void)surface->window.ops.dispatch_events(surface->window.window);
}

static int32_t reach_shell_surface_has_pending_events(const reach_surface_runtime *surface)
{
    return surface != nullptr && surface->window.ops.has_pending_events != nullptr &&
           surface->window.ops.has_pending_events(surface->window.window);
}

int32_t reach_shell_has_pending_events(const reach_shell *shell)
{
    return shell != nullptr && (reach_shell_surface_has_pending_events(&shell->launcher) ||
                                reach_shell_surface_has_pending_events(&shell->dock) ||
                                reach_shell_surface_has_pending_events(&shell->tray) ||
                                reach_shell_surface_has_pending_events(&shell->switcher) ||
                                reach_shell_surface_has_pending_events(&shell->context_menu) ||
                                reach_shell_surface_has_pending_events(&shell->quick_settings) ||
                                reach_shell_surface_has_pending_events(&shell->settings));
}

reach_result reach_shell_dispatch_events(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_shell_dispatch_surface_events(&shell->launcher);
    reach_shell_dispatch_surface_events(&shell->dock);
    reach_shell_dispatch_surface_events(&shell->tray);
    reach_shell_dispatch_surface_events(&shell->switcher);
    reach_shell_dispatch_surface_events(&shell->context_menu);
    reach_shell_dispatch_surface_events(&shell->quick_settings);
    reach_shell_dispatch_surface_events(&shell->settings);
    shell->dirty.events_dispatched_this_cycle = 1;
    return REACH_OK;
}
