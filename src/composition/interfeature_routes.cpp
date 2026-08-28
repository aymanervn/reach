#include "host_internal.h"

static void reach_host_route_dock_item_context_menu(void *user, size_t item_index, int32_t x,
                                                    int32_t y)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr)
    {
        return;
    }

    (void)reach_host_redraw_registered_surface(host, REACH_SURFACE_ID_DOCK);
    (void)reach_host_show_dock_app_context_menu(host, item_index, x, y);
    if (reach_dock_retain_context_feedback(
            reach_host_feature_capsule<reach_dock>(host, REACH_SURFACE_ID_DOCK)))
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

static void reach_host_route_dock_trigger_activated(void *user, size_t trigger)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host != nullptr && trigger == REACH_DOCK_TRIGGER_PRIMARY)
    {
        reach_host_toggle_stage(host);
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

static void reach_host_route_top_bar_quick_settings_activated(void *user)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host != nullptr)
    {
        reach_host_toggle_quick_settings(host);
    }
}

static void reach_host_route_top_bar_battery_activated(void *user)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host != nullptr)
    {
        reach_host_toggle_battery(host);
    }
}

static void reach_host_route_top_bar_tray_overflow_activated(void *user)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host != nullptr)
    {
        reach_host_toggle_tray_popup(host);
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
    dock.trigger_activated = reach_host_route_dock_trigger_activated;
    reach_dock_set_routes(reach_host_feature_capsule<reach_dock>(host, REACH_SURFACE_ID_DOCK),
                          &dock);

    reach_top_bar_routes top_bar = {};
    top_bar.user = host;
    top_bar.power_activated = reach_host_route_top_bar_power_activated;
    top_bar.quick_settings_activated = reach_host_route_top_bar_quick_settings_activated;
    top_bar.battery_activated = reach_host_route_top_bar_battery_activated;
    top_bar.tray_overflow_activated = reach_host_route_top_bar_tray_overflow_activated;
    reach_top_bar_set_routes(
        reach_host_feature_capsule<reach_top_bar>(host, REACH_SURFACE_ID_TOP_BAR), &top_bar);
}

void reach_host_clear_interfeature_routes(reach_host *host)
{
    if (host != nullptr)
    {
        reach_dock_set_routes(reach_host_feature_capsule<reach_dock>(host, REACH_SURFACE_ID_DOCK),
                              nullptr);
        reach_top_bar_set_routes(
            reach_host_feature_capsule<reach_top_bar>(host, REACH_SURFACE_ID_TOP_BAR), nullptr);
    }
}
