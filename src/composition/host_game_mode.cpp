#include "host_internal.h"

static int32_t reach_host_detect_game_mode(const reach_host *host)
{
    if (host != nullptr && host->window_manager.ops.game_mode_active != nullptr)
    {
        return host->window_manager.ops.game_mode_active(host->window_manager.manager);
    }
    return 0;
}

static void reach_host_close_transient_ui_for_game_mode(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_ui_event cancel = {};
    cancel.type = REACH_UI_EVENT_POINTER_CANCEL;
    (void)reach_host_handle_event(host, &cancel);
    reach_host_close_transient_surfaces(host, 1);
    reach_host_set_clipboard_open(host, 0);

    reach_switcher_force_close(host->switcher_capsule);
    reach_animation_manager_init(&host->animations, host->animation_tracks,
                                 REACH_HOST_ANIMATION_COUNT);
    reach_host_surface_transitions_init(host);
    reach_dock_clear_item_x_animations(host->dock_capsule);
    host->dock_reveal.edge_visible = 0;
    host->dock_reveal.edge_bounds_valid = 0;
    if (host->dock_reveal_edge.ops.hide != nullptr)
    {
        (void)host->dock_reveal_edge.ops.hide(host->dock_reveal_edge.edge);
    }

    host->launcher.dirty_flags = 1;
    host->tray.dirty_flags = 1;
    host->switcher.dirty_flags = 1;
    host->context_menu.dirty_flags = 1;
    host->quick_settings.dirty_flags = 1;
    host->clipboard_surface.dirty_flags = 1;
    host->dock.dirty_flags = 1;
    host->dirty.render = 1;
}

int32_t reach_host_game_mode_enabled(const reach_host *host)
{
    return host != nullptr && reach_runtime_policy_game_mode_enabled(&host->runtime_policy);
}

reach_result reach_host_update_game_mode(reach_host *host)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    int32_t next_active = reach_host_detect_game_mode(host);
    int32_t current_active = reach_runtime_policy_game_mode_enabled(&host->runtime_policy);

    if (next_active == current_active)
    {
        return REACH_OK;
    }

    reach_runtime_policy_set_game_mode(&host->runtime_policy, next_active);

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->capsule_ops != nullptr && desc->capsule_ops->on_game_mode != nullptr)
        {
            desc->capsule_ops->on_game_mode(desc->capsule, next_active);
        }
    }

    if (next_active)
    {
        reach_host_close_transient_ui_for_game_mode(host);
    }
    else
    {
        host->dirty.layout = 1;
        host->dirty.render = 1;
        host->dock.dirty_flags = 1;
        host->launcher.dirty_flags = 1;
        host->tray.dirty_flags = 1;
        host->switcher.dirty_flags = 1;
        host->context_menu.dirty_flags = 1;
        host->quick_settings.dirty_flags = 1;
        host->clipboard_surface.dirty_flags = 1;
    }
    reach_host_sync_pointer_move_subscriptions(host);

    return REACH_OK;
}
