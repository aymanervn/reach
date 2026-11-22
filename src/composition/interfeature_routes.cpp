#include "host_internal.h"

#include "reach/features/battery.h"
#include "reach/features/context_menu.h"
#include "reach/features/dock.h"
#include "reach/features/quick_settings.h"
#include "reach/features/stage.h"
#include "reach/features/screenshot.h"
#include "reach/features/system_hud.h"
#include "reach/features/top_bar.h"

static reach_rect_f32 reach_host_menu_monitor(reach_host *host, reach_rect_f32 fallback)
{
    reach_rect_f32 monitor = {};
    return reach_host_primary_monitor_bounds(host, &monitor) ? monitor : fallback;
}

static reach_context_menu_open_context
reach_host_menu_open_context(reach_host *host, const reach_feature_runtime *menu,
                             const reach_menu_request *request)
{
    reach_context_menu_open_context ctx = {};
    ctx.theme = host->theme != nullptr ? host->theme : reach_theme_default();
    ctx.dpi_scale = reach_host_layout_dpi_scale(host);
    ctx.monitor = reach_host_menu_monitor(host, request->anchor_button);
    ctx.anchor_button = request->anchor_button;
    ctx.bar_edge_y = request->bar_edge_y;
    ctx.drop_direction = request->drop_direction;
    ctx.anchored = request->anchored;
    ctx.pointer_x = request->pointer_x;
    ctx.pointer_y = request->pointer_y;
    ctx.item_commands = request->commands;
    ctx.item_count = request->command_count;
    ctx.request = request;
    const auto &renderer = menu->surface->renderer;
    ctx.text_measure.context = renderer.backend;
    ctx.text_measure.measure = renderer.ops.measure_text;
    return ctx;
}

static void reach_host_watch_window_list_pointer(reach_host *host,
                                                 const reach_feature_runtime *runtime)
{
    auto *menu = static_cast<const reach_context_menu *>(runtime->capsule);
    reach_rect_union shape = {};
    if (!reach_context_menu_window_list_hold_shape(menu, &shape))
    {
        shape.count = 0;
    }
    reach_host_set_pointer_observation(host, runtime->definition->id, &shape);
}

static void reach_host_route_dock_item_context_menu(void *user, const reach_menu_request *request)
{
    auto *host = static_cast<reach_host *>(user);
    const auto dock_id = reach_host_find_surface<reach_dock>(host);
    const auto menu_id = reach_host_find_surface<reach_context_menu>(host);
    auto *dock = reach_host_surface_runtime(host, dock_id);
    auto *runtime = reach_host_surface_runtime(host, menu_id);
    if (request == nullptr || dock == nullptr || runtime == nullptr)
    {
        return;
    }

    auto *menu = static_cast<reach_context_menu *>(runtime->capsule);
    const reach_feature_layout_anchor owner = {dock_id, REACH_DOCK_CONTROL_ITEM,
                                               request->target_index, request->owner_identity};
    reach_feature_layout_anchor current = {};
    reach_popup_activation_mode mode = reach_context_menu_window_list_is_open(menu) &&
                                               reach_host_resolve_popup_owner(runtime, &current) &&
                                               reach_host_layout_anchor_equal(&current, &owner)
                                           ? REACH_POPUP_ACTIVATION_REPLACE
                                           : REACH_POPUP_ACTIVATION_TOGGLE;
    (void)reach_host_redraw_registered_surface(host, dock_id);
    if (reach_host_prepare_registered_popup(host, menu_id, &owner, mode) !=
        REACH_POPUP_ACTIVATION_PRESENT)
    {
        reach_host_watch_window_list_pointer(host, runtime);
        (void)reach_host_redraw_registered_surface(host, dock_id);
        return;
    }
    reach_context_menu_open_context ctx = reach_host_menu_open_context(host, runtime, request);
    reach_context_menu_open_for_item(menu, request->target_index, &ctx);
    runtime->popup_owner = ctx.anchored ? owner : reach_feature_layout_anchor{};
    reach_host_present_registered_popup(host, menu_id, owner.surface);
    reach_host_watch_window_list_pointer(host, runtime);
    if (reach_dock_retain_context_feedback(static_cast<reach_dock *>(dock->capsule)))
    {
        dock->surface->dirty_flags = 1;
    }
    (void)reach_host_redraw_registered_surface(host, dock_id);
}

static void reach_host_refresh_window_list(reach_host *host, reach_feature_runtime *runtime,
                                           const reach_menu_request *request,
                                           const reach_feature_layout_anchor &owner)
{
    reach_context_menu_refresh_window_list(static_cast<reach_context_menu *>(runtime->capsule),
                                           request);
    runtime->popup_owner = request->anchored ? owner : reach_feature_layout_anchor{};
    runtime->surface->dirty_flags = 1;
    reach_host_request_update(host);
}

static void reach_host_route_dock_item_hovered(void *user, const reach_menu_request *request)
{
    auto *host = static_cast<reach_host *>(user);
    const auto menu_id = reach_host_find_surface<reach_context_menu>(host);
    auto *runtime = reach_host_surface_runtime(host, menu_id);
    if (runtime == nullptr || request == nullptr)
    {
        return;
    }

    auto *menu = static_cast<reach_context_menu *>(runtime->capsule);
    const size_t open_target = reach_context_menu_window_list_target(menu);
    const reach_feature_layout_anchor owner = {reach_host_find_surface<reach_dock>(host),
                                               REACH_DOCK_CONTROL_ITEM, request->target_index,
                                               request->owner_identity};
    reach_feature_layout_anchor current = {};
    if (open_target != REACH_CONTEXT_MENU_NO_TARGET &&
        reach_host_resolve_popup_owner(runtime, &current) &&
        reach_host_layout_anchor_equal(&current, &owner))
    {
        reach_host_refresh_window_list(host, runtime, request, owner);
        reach_host_watch_window_list_pointer(host, runtime);
        return;
    }
    if (open_target == REACH_CONTEXT_MENU_NO_TARGET &&
        reach_host_any_surface_open(host, reach_surface_class_bit(REACH_SURFACE_CLASS_TRANSIENT) |
                                              reach_surface_class_bit(REACH_SURFACE_CLASS_POPUP) |
                                              reach_surface_class_bit(REACH_SURFACE_CLASS_OVERLAY)))
    {
        return;
    }
    if (reach_host_prepare_registered_popup(host, menu_id, &owner,
                                            REACH_POPUP_ACTIVATION_PASSIVE) !=
        REACH_POPUP_ACTIVATION_PRESENT)
    {
        return;
    }

    reach_context_menu_open_context ctx = reach_host_menu_open_context(host, runtime, request);
    reach_context_menu_open_window_list(menu, request->target_index, &ctx);
    runtime->popup_owner = ctx.anchored ? owner : reach_feature_layout_anchor{};
    reach_host_present_registered_popup(host, menu_id, owner.surface);
    reach_host_watch_window_list_pointer(host, runtime);
}

static void reach_host_route_dock_windows_changed(void *user, const reach_menu_request *request)
{
    auto *host = static_cast<reach_host *>(user);
    const auto menu_id = reach_host_find_surface<reach_context_menu>(host);
    auto *runtime = reach_host_surface_runtime(host, menu_id);
    if (runtime == nullptr || !reach_context_menu_window_list_is_open(
                                  static_cast<reach_context_menu *>(runtime->capsule)))
    {
        return;
    }
    if (request == nullptr || (request->window_count == 0 && request->label[0] == 0))
    {
        reach_host_close_registered_surface(host, menu_id, REACH_SURFACE_CLOSE_SUPERSEDED);
    }
    else
    {
        const reach_feature_layout_anchor owner = {reach_host_find_surface<reach_dock>(host),
                                                   REACH_DOCK_CONTROL_ITEM, request->target_index,
                                                   request->owner_identity};
        reach_host_refresh_window_list(host, runtime, request, owner);
    }
    reach_host_watch_window_list_pointer(host, runtime);
}

/* The window list must survive the pointer crossing the gap from its owning control. */
static void reach_host_route_pointer_region(void *user, uint32_t region_id,
                                            const reach_point_i32 *observed)
{
    auto *host = static_cast<reach_host *>(user);
    const auto menu_id = reach_host_find_surface<reach_context_menu>(host);
    auto *runtime = reach_host_surface_runtime(host, menu_id);
    if (runtime == nullptr || region_id != menu_id)
    {
        return;
    }

    auto *menu = static_cast<reach_context_menu *>(runtime->capsule);
    if (reach_context_menu_window_list_target(menu) == REACH_CONTEXT_MENU_NO_TARGET)
    {
        reach_host_watch_window_list_pointer(host, runtime);
        return;
    }

    reach_point_i32 pointer = observed != nullptr ? *observed : reach_point_i32{};
    if (observed == nullptr && !reach_host_get_pointer_position(host, &pointer))
    {
        return;
    }
    if (reach_context_menu_window_list_holds_pointer(menu, (float)pointer.x, (float)pointer.y))
    {
        return;
    }

    reach_host_close_registered_surface(host, menu_id, REACH_SURFACE_CLOSE_SUPERSEDED);
    reach_host_watch_window_list_pointer(host, runtime);
}

static void reach_host_route_pointer_moved(void *user, reach_point_i32 point)
{
    auto *host = static_cast<reach_host *>(user);
    const auto menu_id = reach_host_find_surface<reach_context_menu>(host);
    auto *runtime = reach_host_surface_runtime(host, menu_id);
    if (runtime == nullptr)
    {
        return;
    }
    auto *menu = static_cast<reach_context_menu *>(runtime->capsule);
    if (reach_context_menu_window_list_target(menu) == REACH_CONTEXT_MENU_NO_TARGET ||
        reach_context_menu_window_list_holds_pointer(menu, (float)point.x, (float)point.y))
    {
        return;
    }
    reach_host_close_registered_surface(host, menu_id, REACH_SURFACE_CLOSE_SUPERSEDED);
    reach_host_watch_window_list_pointer(host, runtime);
}

static void reach_host_route_dock_trigger_activated(void *user, size_t trigger)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host != nullptr && trigger == REACH_DOCK_TRIGGER_PRIMARY)
    {
        reach_host_toggle_registered_surface(host, reach_host_find_surface<reach_stage>(host));
    }
}

static void reach_host_route_top_bar_power_activated(void *user)
{
    auto *host = static_cast<reach_host *>(user);
    const auto menu_id = reach_host_find_surface<reach_context_menu>(host);
    auto *runtime = reach_host_surface_runtime(host, menu_id);
    if (runtime == nullptr || !host->has_layout)
    {
        return;
    }

    const auto top_bar_id = reach_host_find_surface<reach_top_bar>(host);
    reach_menu_request request = {};
    if (!reach_top_bar_build_power_menu_request(
            reach_host_feature_capsule<reach_top_bar>(host, top_bar_id), &request))
    {
        return;
    }

    const reach_feature_layout_anchor owner = {top_bar_id, REACH_TOP_BAR_CONTROL_POWER, 0, 0};
    if (reach_host_prepare_registered_popup(host, menu_id, &owner, REACH_POPUP_ACTIVATION_TOGGLE) !=
        REACH_POPUP_ACTIVATION_PRESENT)
    {
        reach_host_watch_window_list_pointer(host, runtime);
        return;
    }
    reach_context_menu_open_context ctx = reach_host_menu_open_context(host, runtime, &request);
    reach_context_menu_open_power(static_cast<reach_context_menu *>(runtime->capsule), &ctx);
    runtime->popup_owner = ctx.anchored ? owner : reach_feature_layout_anchor{};
    reach_host_present_registered_popup(host, menu_id, owner.surface);
    reach_host_watch_window_list_pointer(host, runtime);
}

static void reach_host_route_top_bar_quick_settings_activated(void *user)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host != nullptr)
    {
        reach_host_toggle_registered_popup(host,
                                           reach_host_find_surface<reach_quick_settings>(host));
    }
}

static void reach_host_route_top_bar_battery_activated(void *user)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host != nullptr)
    {
        reach_host_toggle_registered_popup(host, reach_host_find_surface<reach_battery>(host));
    }
}

static void reach_host_route_top_bar_tray_overflow_activated(void *user)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host != nullptr)
    {
        reach_host_toggle_registered_popup(host,
                                           reach_host_find_surface<reach_top_bar_tray_popup>(host));
    }
}

static void reach_host_route_battery_saver_pending_changed(void *user, int32_t pending,
                                                           int32_t pending_enabled)
{
    reach_host *host = static_cast<reach_host *>(user);
    auto *runtime = reach_host_surface_runtime(host, reach_host_find_surface<reach_top_bar>(host));
    if (runtime == nullptr)
    {
        return;
    }
    reach_top_bar_set_battery_saver_pending(static_cast<reach_top_bar *>(runtime->capsule), pending,
                                            pending_enabled);
    runtime->surface->dirty_flags = 1;
    host->dirty.render = 1;
}

static void reach_host_route_quick_settings_light_theme(void *user, int32_t light_theme)
{
    reach_host_set_light_theme(static_cast<reach_host *>(user), light_theme);
}

static void reach_host_route_screenshot_copied(void *user)
{
    reach_host *host = static_cast<reach_host *>(user);
    auto *runtime =
        reach_host_surface_runtime(host, reach_host_find_surface<reach_system_hud>(host));
    if (runtime == nullptr)
    {
        return;
    }
    int32_t was_open = reach_host_surface_is_open(runtime);
    reach_system_hud_show_screenshot_copied(static_cast<reach_system_hud *>(runtime->capsule));
    if (!was_open && reach_host_surface_is_open(runtime))
    {
        reach_host_apply_surface_open_change(host, runtime, 1);
    }
    reach_feature_tick_result result = {};
    result.redraw = 1;
    result.relayout = 1;
    result.request_update = 1;
    reach_host_apply_feature_tick_result(host, runtime, &result);
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
    dock.item_windows_changed = reach_host_route_dock_windows_changed;
    dock.trigger_activated = reach_host_route_dock_trigger_activated;
    reach_dock_set_routes(
        reach_host_feature_capsule<reach_dock>(host, reach_host_find_surface<reach_dock>(host)),
        &dock);

    reach_top_bar_routes top_bar = {};
    top_bar.user = host;
    top_bar.power_activated = reach_host_route_top_bar_power_activated;
    top_bar.quick_settings_activated = reach_host_route_top_bar_quick_settings_activated;
    top_bar.battery_activated = reach_host_route_top_bar_battery_activated;
    top_bar.tray_overflow_activated = reach_host_route_top_bar_tray_overflow_activated;
    reach_top_bar_set_routes(reach_host_feature_capsule<reach_top_bar>(
                                 host, reach_host_find_surface<reach_top_bar>(host)),
                             &top_bar);

    reach_battery_routes battery = {};
    battery.user = host;
    battery.saver_pending_changed = reach_host_route_battery_saver_pending_changed;
    reach_battery_set_routes(reach_host_feature_capsule<reach_battery>(
                                 host, reach_host_find_surface<reach_battery>(host)),
                             &battery);

    reach_quick_settings_routes quick_settings = {};
    quick_settings.user = host;
    quick_settings.set_light_theme = reach_host_route_quick_settings_light_theme;
    reach_quick_settings_set_routes(reach_host_feature_capsule<reach_quick_settings>(
                                        host, reach_host_find_surface<reach_quick_settings>(host)),
                                    &quick_settings);

    reach_screenshot_routes screenshot = {};
    screenshot.user = host;
    screenshot.copied = reach_host_route_screenshot_copied;
    reach_screenshot_set_routes(reach_host_feature_capsule<reach_screenshot>(
                                    host, reach_host_find_surface<reach_screenshot>(host)),
                                &screenshot);
}

void reach_host_clear_interfeature_routes(reach_host *host)
{
    if (host != nullptr)
    {
        host->pointer_moved_route = nullptr;
        host->pointer_region_route = nullptr;
        reach_dock_set_routes(
            reach_host_feature_capsule<reach_dock>(host, reach_host_find_surface<reach_dock>(host)),
            nullptr);
        reach_top_bar_set_routes(reach_host_feature_capsule<reach_top_bar>(
                                     host, reach_host_find_surface<reach_top_bar>(host)),
                                 nullptr);
        reach_quick_settings_set_routes(
            reach_host_feature_capsule<reach_quick_settings>(
                host, reach_host_find_surface<reach_quick_settings>(host)),
            nullptr);
        reach_battery_set_routes(reach_host_feature_capsule<reach_battery>(
                                     host, reach_host_find_surface<reach_battery>(host)),
                                 nullptr);
        reach_screenshot_set_routes(reach_host_feature_capsule<reach_screenshot>(
                                        host, reach_host_find_surface<reach_screenshot>(host)),
                                    nullptr);
    }
}
