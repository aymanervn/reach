#include "host_internal.h"

#include "reach/features/battery.h"
#include "reach/features/context_menu.h"
#include "reach/features/dock.h"
#include "reach/features/top_bar.h"

static reach_rect_f32 reach_host_menu_monitor(reach_host *host, reach_rect_f32 fallback)
{
    reach_rect_f32 monitor = {};
    return reach_host_primary_monitor_bounds(host, &monitor) ? monitor : fallback;
}

static reach_context_menu_open_context
reach_host_menu_open_context(reach_host *host, const reach_menu_request *request)
{
    reach_context_menu_open_context ctx = {};
    ctx.theme = host->theme != nullptr ? host->theme : reach_theme_default();
    ctx.dpi_scale = reach_host_layout_dpi_scale(host);
    ctx.monitor = reach_host_menu_monitor(host, request->anchor_button);
    ctx.text_measure.context = host->surfaces[REACH_SURFACE_ID_CONTEXT_MENU].renderer.backend;
    ctx.text_measure.measure = host->surfaces[REACH_SURFACE_ID_CONTEXT_MENU].renderer.ops.measure_text;
    ctx.anchor_button = request->anchor_button;
    ctx.bar_edge_y = request->bar_edge_y;
    ctx.drop_direction = request->drop_direction;
    ctx.anchored = request->anchored;
    ctx.pointer_x = request->pointer_x;
    ctx.pointer_y = request->pointer_y;
    ctx.item_commands = request->commands;
    ctx.item_count = request->command_count;
    ctx.request = request;
    return ctx;
}

static void reach_host_route_dock_item_context_menu(void *user, const reach_menu_request *request)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr || request == nullptr)
    {
        return;
    }

    (void)reach_host_redraw_registered_surface(host, REACH_SURFACE_ID_DOCK);
    reach_host_surface_opening(host, REACH_SURFACE_ID_CONTEXT_MENU, REACH_SURFACE_ID_DOCK);
    reach_context_menu_open_context ctx = reach_host_menu_open_context(host, request);
    reach_context_menu_open_for_item(
        reach_host_feature_capsule<reach_context_menu>(host, REACH_SURFACE_ID_CONTEXT_MENU),
        request->target_index, &ctx);
    reach_host_present_registered_popup(host, REACH_SURFACE_ID_CONTEXT_MENU,
                                       request->drop_direction);
    if (reach_dock_retain_context_feedback(
            reach_host_feature_capsule<reach_dock>(host, REACH_SURFACE_ID_DOCK)))
    {
        host->surfaces[REACH_SURFACE_ID_DOCK].dirty_flags = 1;
    }
    (void)reach_host_redraw_registered_surface(host, REACH_SURFACE_ID_DOCK);
}

/* Reach only sees pointer moves over its own windows, so the window list also asks the platform
   to watch the band it lives in. Leaving that band over a foreign window is the case that has no
   other signal at all. */
static void reach_host_watch_window_list_pointer(reach_host *host)
{
    reach_context_menu *menu =
        reach_host_feature_capsule<reach_context_menu>(host, REACH_SURFACE_ID_CONTEXT_MENU);
    reach_rect_f32 bounds = {};
    int32_t watching = reach_context_menu_window_list_hover_bounds(menu, &bounds);
    reach_host_set_pointer_observation(host, REACH_SURFACE_ID_CONTEXT_MENU, bounds, watching);
}

static void reach_host_open_window_list(reach_host *host, const reach_menu_request *request)
{
    reach_host_surface_opening(host, REACH_SURFACE_ID_CONTEXT_MENU, REACH_SURFACE_ID_DOCK);

    reach_context_menu_window_entry entries[REACH_CONTEXT_MENU_MAX_ITEMS] = {};
    size_t entry_count = request->window_count < REACH_CONTEXT_MENU_MAX_ITEMS
                             ? request->window_count
                             : REACH_CONTEXT_MENU_MAX_ITEMS;
    for (size_t index = 0; index < entry_count; ++index)
    {
        entries[index].window = request->windows[index].window;
        entries[index].title = request->windows[index].title;
    }

    reach_context_menu_open_context ctx = reach_host_menu_open_context(host, request);
    ctx.window_entries = entries;
    ctx.window_entry_count = entry_count;
    reach_context_menu_open_window_list(
        reach_host_feature_capsule<reach_context_menu>(host, REACH_SURFACE_ID_CONTEXT_MENU),
        request->target_index, &ctx);
    reach_host_present_registered_popup(host, REACH_SURFACE_ID_CONTEXT_MENU,
                                        request->drop_direction);
    reach_host_watch_window_list_pointer(host);
}

static void reach_host_route_dock_item_hovered(void *user, const reach_menu_request *request)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr)
    {
        return;
    }

    reach_context_menu *menu =
        reach_host_feature_capsule<reach_context_menu>(host, REACH_SURFACE_ID_CONTEXT_MENU);
    size_t open_target = reach_context_menu_window_list_target(menu);
    if (request == nullptr)
    {
        return;
    }
    if (open_target == request->target_index)
    {
        return;
    }
    if (open_target == REACH_CONTEXT_MENU_NO_TARGET &&
        reach_host_any_surface_open(host,
                                    reach_surface_class_bit(REACH_SURFACE_CLASS_TRANSIENT) |
                                        reach_surface_class_bit(REACH_SURFACE_CLASS_POPUP) |
                                        reach_surface_class_bit(REACH_SURFACE_CLASS_OVERLAY)))
    {
        return;
    }

    reach_host_open_window_list(host, request);
}

/* The window list is the only surface that must survive the pointer crossing the gap between
   the control it hangs off and itself, so it decides when the pointer has truly left. */
static void reach_host_route_pointer_region(void *user, uint32_t region_id)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr || region_id != (uint32_t)REACH_SURFACE_ID_CONTEXT_MENU)
    {
        return;
    }

    reach_context_menu *menu =
        reach_host_feature_capsule<reach_context_menu>(host, REACH_SURFACE_ID_CONTEXT_MENU);
    if (reach_context_menu_window_list_target(menu) == REACH_CONTEXT_MENU_NO_TARGET)
    {
        reach_host_watch_window_list_pointer(host);
        return;
    }

    reach_point_i32 pointer = {};
    if (!reach_host_get_pointer_position(host, &pointer) ||
        reach_context_menu_window_list_holds_pointer(menu, (float)pointer.x, (float)pointer.y))
    {
        return;
    }

    reach_host_close_registered_surface(host, REACH_SURFACE_ID_CONTEXT_MENU,
                                        REACH_SURFACE_CLOSE_SUPERSEDED);
    reach_host_watch_window_list_pointer(host);
}

static void reach_host_route_pointer_moved(void *user, reach_point_i32 point)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr)
    {
        return;
    }
    reach_context_menu *menu =
        reach_host_feature_capsule<reach_context_menu>(host, REACH_SURFACE_ID_CONTEXT_MENU);
    if (reach_context_menu_window_list_target(menu) == REACH_CONTEXT_MENU_NO_TARGET ||
        reach_context_menu_window_list_holds_pointer(menu, (float)point.x, (float)point.y))
    {
        return;
    }
    reach_host_close_registered_surface(host, REACH_SURFACE_ID_CONTEXT_MENU,
                                        REACH_SURFACE_CLOSE_SUPERSEDED);
    reach_host_watch_window_list_pointer(host);
}

static void reach_host_route_dock_trigger_activated(void *user, size_t trigger)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host != nullptr && trigger == REACH_DOCK_TRIGGER_PRIMARY)
    {
        reach_host_toggle_registered_surface(host, REACH_SURFACE_ID_STAGE);
    }
}

static void reach_host_route_top_bar_power_activated(void *user)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr || !host->has_layout)
    {
        return;
    }

    reach_menu_request request = {};
    if (!reach_top_bar_build_power_menu_request(
            reach_host_feature_capsule<reach_top_bar>(host, REACH_SURFACE_ID_TOP_BAR), &request))
    {
        return;
    }

    reach_host_surface_opening(host, REACH_SURFACE_ID_CONTEXT_MENU, REACH_SURFACE_ID_DOCK);
    reach_context_menu_open_context ctx = reach_host_menu_open_context(host, &request);
    reach_context_menu_open_power(
        reach_host_feature_capsule<reach_context_menu>(host, REACH_SURFACE_ID_CONTEXT_MENU), &ctx);
    reach_host_present_registered_popup(host, REACH_SURFACE_ID_CONTEXT_MENU,
                                       request.drop_direction);
}

static void reach_host_route_top_bar_quick_settings_activated(void *user)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host != nullptr)
    {
        reach_host_toggle_registered_surface(host, REACH_SURFACE_ID_QUICK_SETTINGS);
    }
}

static void reach_host_route_top_bar_battery_activated(void *user)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host != nullptr)
    {
        reach_host_toggle_registered_surface(host, REACH_SURFACE_ID_BATTERY);
    }
}

static void reach_host_route_top_bar_tray_overflow_activated(void *user)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host != nullptr)
    {
        reach_host_toggle_registered_surface(host, REACH_SURFACE_ID_TRAY);
    }
}

static void reach_host_route_battery_saver_pending_changed(void *user, int32_t pending,
                                                           int32_t pending_enabled)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr)
    {
        return;
    }
    reach_top_bar_set_battery_saver_pending(
        reach_host_feature_capsule<reach_top_bar>(host, REACH_SURFACE_ID_TOP_BAR), pending,
        pending_enabled);
    host->surfaces[REACH_SURFACE_ID_TOP_BAR].dirty_flags = 1;
    host->dirty.render = 1;
}

void reach_host_bind_interfeature_routes(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    host->pointer_moved_route = reach_host_route_pointer_moved;
    host->pointer_region_route = reach_host_route_pointer_region;

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

    reach_battery_routes battery = {};
    battery.user = host;
    battery.saver_pending_changed = reach_host_route_battery_saver_pending_changed;
    reach_battery_set_routes(
        reach_host_feature_capsule<reach_battery>(host, REACH_SURFACE_ID_BATTERY), &battery);
}

void reach_host_clear_interfeature_routes(reach_host *host)
{
    if (host != nullptr)
    {
        host->pointer_moved_route = nullptr;
        host->pointer_region_route = nullptr;
        reach_dock_set_routes(reach_host_feature_capsule<reach_dock>(host, REACH_SURFACE_ID_DOCK),
                              nullptr);
        reach_top_bar_set_routes(
            reach_host_feature_capsule<reach_top_bar>(host, REACH_SURFACE_ID_TOP_BAR), nullptr);
        reach_battery_set_routes(
            reach_host_feature_capsule<reach_battery>(host, REACH_SURFACE_ID_BATTERY), nullptr);
    }
}
