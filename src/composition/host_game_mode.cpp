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
    reach_host_set_registered_surface_open(host, REACH_SURFACE_ID_CLIPBOARD, 0);

    reach_switcher_force_close(
        reach_host_feature_capsule<reach_switcher>(host, REACH_SURFACE_ID_SWITCHER));
    reach_animation_manager_init(&host->animations, host->animation_tracks,
                                 REACH_HOST_ANIMATION_COUNT);
    reach_host_surface_transitions_init(host);
    reach_dock_clear_item_x_animations(
        reach_host_feature_capsule<reach_dock>(host, REACH_SURFACE_ID_DOCK));

    reach_host_mark_all_surfaces_dirty(host);
    host->dirty.render = 1;
}

static void reach_host_disable_bar_pointer_observations(reach_host *host)
{
    if (host == nullptr || host->input_source.ops.set_pointer_region == nullptr)
    {
        return;
    }
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        if (host->feature_runtimes[index].definition->surface.bar_reveal.ops != nullptr)
        {
            (void)host->input_source.ops.set_pointer_region(host->input_source.source,
                                                            static_cast<uint32_t>(index), {}, 0);
        }
    }
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
    reach_layout_set_condition(&host->layout_manager, REACH_LAYOUT_CONDITION_GAME_MODE,
                               next_active);
    if (next_active)
    {
        host->top_bar_hidden = 1;
    }
    reach_system_stats_set_enabled(host->system_stats, !next_active);

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_feature_runtime *desc = &host->feature_runtimes[index];
        if (desc->definition->capsule_ops != nullptr &&
            desc->definition->capsule_ops->on_game_mode != nullptr)
        {
            desc->definition->capsule_ops->on_game_mode(desc->capsule, next_active);
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
        reach_host_mark_all_surfaces_dirty(host);
    }
    if (next_active)
    {
        reach_host_disable_bar_pointer_observations(host);
        reach_host_suspend_pointer_move_subscriptions(host);
    }
    else
    {
        reach_host_sync_pointer_move_subscriptions(host);
    }

    return REACH_OK;
}
