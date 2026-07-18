#include "host_internal.h"

static const double REACH_HOST_WINDOW_LIST_DWELL_SECONDS = 0.35;
static const double REACH_HOST_WINDOW_LIST_GRACE_SECONDS = 0.2;
static const float REACH_HOST_WINDOW_LIST_MARGIN = 12.0f;

static size_t reach_host_window_list_item_windows(reach_host *host, size_t item_index,
                                                  reach_dock_item_window *out, size_t cap)
{
    return reach_dock_collect_item_windows(host->dock_capsule, item_index, host->pinned_apps,
                                           host->pinned_app_count, out, cap);
}

static int32_t reach_host_window_list_blocked(reach_host *host)
{
    if (reach_host_game_mode_enabled(host))
    {
        return 1;
    }
    if (reach_context_menu_window_list_is_open(host->context_menu_capsule))
    {
        return 0;
    }
    return reach_host_any_surface_open(host,
                                       reach_surface_class_bit(REACH_SURFACE_CLASS_TRANSIENT) |
                                           reach_surface_class_bit(REACH_SURFACE_CLASS_POPUP) |
                                           reach_surface_class_bit(REACH_SURFACE_CLASS_OVERLAY));
}

reach_result reach_host_show_dock_window_list(reach_host *host, size_t item_index)
{
    if (host == nullptr || !host->has_layout ||
        item_index >= reach_dock_item_count(host->dock_capsule) ||
        item_index >= host->layout.dock.app_slot_count)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_dock_item_window item_windows[REACH_CONTEXT_MENU_MAX_ITEMS] = {};
    size_t window_count = reach_host_window_list_item_windows(host, item_index, item_windows,
                                                              REACH_CONTEXT_MENU_MAX_ITEMS);
    if (window_count < 2)
    {
        return REACH_OK;
    }

    reach_context_menu_window_entry entries[REACH_CONTEXT_MENU_MAX_ITEMS] = {};
    for (size_t index = 0; index < window_count; ++index)
    {
        entries[index].window = item_windows[index].window;
        entries[index].title = item_windows[index].title;
    }

    reach_host_close_other_popups(host, REACH_SURFACE_ID_CONTEXT_MENU);

    reach_rect_f32 slot =
        reach_dock_rect_to_screen(&host->layout.dock, host->layout.dock.app_slots[item_index]);

    reach_context_menu_open_context ctx = {};
    ctx.dpi_scale = reach_host_layout_dpi_scale(host);
    ctx.anchored = 1;
    ctx.anchor_x = slot.x + slot.width * 0.5f;
    ctx.dock_top_y = host->layout.dock.bounds.y;
    ctx.monitor = host->layout.dock.bounds;
    const reach_monitor_info *primary_monitor =
        host->monitors.list != nullptr && host->monitors.ops.primary != nullptr
            ? host->monitors.ops.primary(host->monitors.list)
            : nullptr;
    if (primary_monitor != nullptr)
    {
        ctx.monitor.x = (float)primary_monitor->bounds.left;
        ctx.monitor.y = (float)primary_monitor->bounds.top;
        ctx.monitor.width = (float)(primary_monitor->bounds.right - primary_monitor->bounds.left);
        ctx.monitor.height = (float)(primary_monitor->bounds.bottom - primary_monitor->bounds.top);
    }
    ctx.window_entries = entries;
    ctx.window_entry_count = window_count;

    reach_context_menu_open_window_list(host->context_menu_capsule, item_index, &ctx);
    reach_host_surface_transition_set(host, &host->context_menu_transition, 1);
    host->context_menu.dirty_flags = 1;
    host->window_list.open_item = item_index;
    host->window_list.grace_seconds = 0.0;
    host->window_list.dwell_active = 0;
    reach_host_sync_pointer_move_subscriptions(host);
    reach_host_sync_popup_mouse_hook(host);
    reach_host_request_dock_visibility_update(host);
    return REACH_OK;
}

void reach_host_dock_item_hovered(reach_host *host, size_t item_index)
{
    if (host == nullptr)
    {
        return;
    }
    reach_host_window_list_state *state = &host->window_list;

    if (item_index >= reach_dock_item_count(host->dock_capsule))
    {
        state->dwell_active = 0;
        return;
    }

    if (reach_context_menu_window_list_is_open(host->context_menu_capsule))
    {
        state->dwell_active = 0;
        if (item_index == state->open_item)
        {
            state->grace_seconds = 0.0;
            return;
        }
        reach_dock_item_window item_windows[REACH_CONTEXT_MENU_MAX_ITEMS] = {};
        if (reach_host_window_list_item_windows(host, item_index, item_windows,
                                                REACH_CONTEXT_MENU_MAX_ITEMS) >= 2)
        {
            (void)reach_host_show_dock_window_list(host, item_index);
        }
        return;
    }

    if (reach_host_window_list_blocked(host))
    {
        state->dwell_active = 0;
        return;
    }

    reach_dock_item_window item_windows[REACH_CONTEXT_MENU_MAX_ITEMS] = {};
    if (reach_host_window_list_item_windows(host, item_index, item_windows,
                                            REACH_CONTEXT_MENU_MAX_ITEMS) < 2)
    {
        state->dwell_active = 0;
        return;
    }

    state->dwell_active = 1;
    state->dwell_item = item_index;
    state->dwell_seconds = 0.0;
    reach_host_request_update(host);
}

static int32_t reach_host_window_list_pointer_inside(reach_host *host)
{
    reach_point_i32 pointer = {};
    if (host->input_source.ops.get_pointer_position == nullptr ||
        host->input_source.ops.get_pointer_position(host->input_source.source, &pointer) !=
            REACH_OK)
    {
        return 1;
    }

    const reach_context_menu_state *menu_state =
        reach_context_menu_state_ptr(host->context_menu_capsule);
    size_t open_item = host->window_list.open_item;
    if (open_item >= host->layout.dock.app_slot_count)
    {
        return 0;
    }
    reach_rect_f32 slot =
        reach_dock_rect_to_screen(&host->layout.dock, host->layout.dock.app_slots[open_item]);
    float margin = REACH_HOST_WINDOW_LIST_MARGIN * reach_host_layout_dpi_scale(host);
    return reach_context_menu_hover_region_contains(menu_state->bounds, slot,
                                                    host->layout.dock.bounds.y, margin,
                                                    (float)pointer.x, (float)pointer.y);
}

void reach_host_window_list_update(reach_host *host, double delta_seconds)
{
    if (host == nullptr)
    {
        return;
    }
    reach_host_window_list_state *state = &host->window_list;

    if (state->dwell_active)
    {
        if (reach_host_window_list_blocked(host) ||
            reach_context_menu_is_open(host->context_menu_capsule))
        {
            state->dwell_active = 0;
        }
        else
        {
            state->dwell_seconds += delta_seconds;
            if (state->dwell_seconds >= REACH_HOST_WINDOW_LIST_DWELL_SECONDS)
            {
                state->dwell_active = 0;
                (void)reach_host_show_dock_window_list(host, state->dwell_item);
            }
        }
    }

    if (!reach_context_menu_window_list_is_open(host->context_menu_capsule))
    {
        return;
    }

    if (!host->has_layout || reach_host_game_mode_enabled(host) ||
        state->open_item >= reach_dock_item_count(host->dock_capsule))
    {
        reach_host_close_context_menu(host);
        return;
    }

    if (reach_host_window_list_pointer_inside(host))
    {
        state->grace_seconds = 0.0;
        return;
    }

    state->grace_seconds += delta_seconds;
    if (state->grace_seconds >= REACH_HOST_WINDOW_LIST_GRACE_SECONDS)
    {
        reach_host_close_context_menu(host);
    }
}

int32_t reach_host_window_list_wants_frames(const reach_host *host)
{
    return host != nullptr &&
           (host->window_list.dwell_active ||
            reach_context_menu_window_list_is_open(host->context_menu_capsule));
}
