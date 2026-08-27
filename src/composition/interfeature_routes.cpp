/* The only composition code allowed to know which feature action affects another feature.
   Features expose neutral outbound slots describing what they did; the routes below decide
   what that reaches. Everything else in composition stays generic. */

#include "host_internal.h"

static void reach_host_route_dock_item_context_menu(void *user, size_t item_index, int32_t x,
                                                    int32_t y)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr)
    {
        return;
    }

    /* The Dock repaints around the opening menu so the pressed item keeps its feedback while
       the context menu takes over pointer ownership. */
    (void)reach_host_redraw_registered_surface(host, REACH_SURFACE_ID_DOCK);
    (void)reach_host_show_dock_app_context_menu(host, item_index, x, y);
    if (reach_dock_retain_context_feedback(host->dock_capsule))
    {
        host->dock.dirty_flags = 1;
    }
    (void)reach_host_redraw_registered_surface(host, REACH_SURFACE_ID_DOCK);
}

static void reach_host_route_dock_item_hovered(void *user, size_t item_index)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host != nullptr)
    {
        reach_host_dock_item_hovered(host, item_index);
    }
}

static void reach_host_route_top_bar_power_activated(void *user)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host != nullptr)
    {
        (void)reach_host_show_power_context_menu(host);
    }
}

void reach_host_bind_interfeature_routes(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_dock_routes dock = {};
    dock.user = host;
    dock.item_context_menu = reach_host_route_dock_item_context_menu;
    dock.item_hovered = reach_host_route_dock_item_hovered;
    reach_dock_set_routes(host->dock_capsule, &dock);

    reach_top_bar_routes top_bar = {};
    top_bar.user = host;
    top_bar.power_activated = reach_host_route_top_bar_power_activated;
    reach_top_bar_set_routes(host->top_bar_capsule, &top_bar);
}

void reach_host_clear_interfeature_routes(reach_host *host)
{
    if (host != nullptr)
    {
        reach_dock_set_routes(host->dock_capsule, nullptr);
        reach_top_bar_set_routes(host->top_bar_capsule, nullptr);
    }
}
