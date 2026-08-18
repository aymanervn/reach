#include "host_internal.h"

#define REACH_HOST_LAYER_DOCK_REVEAL_EDGE 120
#define REACH_HOST_LAYER_TOP_BAR_REVEAL_EDGE 140
#define REACH_HOST_LAYER_STAGE_REVEAL_CORNER 170

static_assert(REACH_HOST_SURFACE_COUNT + 3 <= REACH_LAYOUT_MAX_PARTICIPANTS,
              "layout participant capacity must cover every surface and hotspot");

static void reach_host_register_hotspot_participant(reach_host *host,
                                                    reach_screen_hotspot_port *hotspot,
                                                    int32_t layer,
                                                    reach_layout_participant *out_participant)
{
    reach_layout_participant participant = 0;
    if (reach_layout_register(&host->layout_manager, layer, &participant) != REACH_OK)
    {
        return;
    }
    host->layout_targets[participant].hotspot = hotspot;
    *out_participant = participant;
}

void reach_host_init_layout(reach_host *host)
{
    REACH_ASSERT(host != nullptr);
    if (host == nullptr)
    {
        return;
    }

    host->layout_manager = {};
    host->applied_layout_plan = {};
    host->has_applied_layout_plan = 0;
    for (size_t index = 0; index < REACH_LAYOUT_MAX_PARTICIPANTS; ++index)
    {
        host->layout_targets[index] = {};
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_surface_desc *desc = &host->surface_descs[index];
        reach_layout_participant participant = 0;
        if (reach_layout_register(&host->layout_manager, desc->layer, &participant) != REACH_OK)
        {
            continue;
        }
        host->layout_targets[participant].surface = desc->surface;
        host->surface_participants[index] = participant;
    }

    reach_host_register_hotspot_participant(host, &host->dock_reveal_edge,
                                            REACH_HOST_LAYER_DOCK_REVEAL_EDGE,
                                            &host->dock_reveal_edge_participant);
    reach_host_register_hotspot_participant(host, &host->top_bar_reveal_edge,
                                            REACH_HOST_LAYER_TOP_BAR_REVEAL_EDGE,
                                            &host->top_bar_reveal_edge_participant);
    reach_host_register_hotspot_participant(host, &host->stage_reveal_corner,
                                            REACH_HOST_LAYER_STAGE_REVEAL_CORNER,
                                            &host->stage_reveal_corner_participant);
}

static reach_window_id reach_host_layout_native_id(const reach_host_layout_target *target)
{
    if (target->surface != nullptr && target->surface->window.ops.native_id != nullptr)
    {
        return target->surface->window.ops.native_id(target->surface->window.window);
    }
    if (target->hotspot != nullptr && target->hotspot->ops.native_id != nullptr)
    {
        return target->hotspot->ops.native_id(target->hotspot->hotspot);
    }
    return 0;
}

static reach_result reach_host_layout_set_topmost(const reach_host_layout_target *target,
                                                  int32_t enabled)
{
    if (target->surface != nullptr && target->surface->window.ops.set_topmost != nullptr)
    {
        return target->surface->window.ops.set_topmost(target->surface->window.window, enabled);
    }
    if (target->hotspot != nullptr && target->hotspot->ops.set_topmost != nullptr)
    {
        return target->hotspot->ops.set_topmost(target->hotspot->hotspot, enabled);
    }
    return REACH_NOT_IMPLEMENTED;
}

static reach_result reach_host_layout_place_behind(const reach_host_layout_target *target,
                                                   reach_window_id above)
{
    if (target->surface != nullptr && target->surface->window.ops.place_behind != nullptr)
    {
        return target->surface->window.ops.place_behind(target->surface->window.window, above);
    }
    if (target->hotspot != nullptr && target->hotspot->ops.place_behind != nullptr)
    {
        return target->hotspot->ops.place_behind(target->hotspot->hotspot, above);
    }
    return REACH_NOT_IMPLEMENTED;
}

static int32_t reach_host_layout_was_banded(const reach_host *host,
                                            reach_layout_participant participant)
{
    if (!host->has_applied_layout_plan)
    {
        return 0;
    }
    for (size_t index = 0; index < host->applied_layout_plan.count; ++index)
    {
        const reach_layout_entry *entry = &host->applied_layout_plan.entries[index];
        if (entry->participant == participant)
        {
            return entry->layer > 0;
        }
    }
    return 0;
}

void reach_host_apply_layout(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_layout_plan plan = {};
    reach_layout_resolve(&host->layout_manager, &plan);
    if (host->has_applied_layout_plan &&
        reach_layout_plan_equal(&plan, &host->applied_layout_plan))
    {
        return;
    }

    reach_window_id above = 0;
    for (size_t index = 0; index < plan.count; ++index)
    {
        const reach_layout_entry *entry = &plan.entries[index];
        const reach_host_layout_target *target = &host->layout_targets[entry->participant];

        if (entry->layer == 0)
        {
            if (reach_host_layout_was_banded(host, entry->participant))
            {
                (void)reach_host_layout_set_topmost(target, 0);
            }
            continue;
        }
        if (!entry->visible)
        {
            continue;
        }

        reach_window_id native = reach_host_layout_native_id(target);
        if (native == 0)
        {
            continue;
        }

        if (above == 0)
        {
            (void)reach_host_layout_set_topmost(target, 1);
        }
        else
        {
            (void)reach_host_layout_place_behind(target, above);
        }
        above = native;
    }

    host->applied_layout_plan = plan;
    host->has_applied_layout_plan = 1;
}
