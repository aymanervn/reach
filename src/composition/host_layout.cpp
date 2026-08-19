#include "host_internal.h"

reach_layout_participant reach_host_hotspot_participant(const reach_host *host,
                                                        const reach_screen_hotspot_port *hotspot)
{
    if (host == nullptr || hotspot == nullptr)
    {
        return REACH_LAYOUT_MAX_PARTICIPANTS;
    }

    for (size_t index = 0; index < host->layout_manager.participant_count; ++index)
    {
        if (host->layout_targets[index].hotspot == hotspot)
        {
            return (reach_layout_participant)index;
        }
    }
    return REACH_LAYOUT_MAX_PARTICIPANTS;
}

static reach_window_id reach_host_layout_native_id(const reach_host_layout_target *target)
{
    if (target->desc != nullptr && target->desc->surface->window.ops.native_id != nullptr)
    {
        return target->desc->surface->window.ops.native_id(target->desc->surface->window.window);
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
    if (target->desc != nullptr && target->desc->surface->window.ops.set_topmost != nullptr)
    {
        return target->desc->surface->window.ops.set_topmost(target->desc->surface->window.window,
                                                             enabled);
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
    if (target->desc != nullptr && target->desc->surface->window.ops.place_behind != nullptr)
    {
        return target->desc->surface->window.ops.place_behind(target->desc->surface->window.window,
                                                              above);
    }
    if (target->hotspot != nullptr && target->hotspot->ops.place_behind != nullptr)
    {
        return target->hotspot->ops.place_behind(target->hotspot->hotspot, above);
    }
    return REACH_NOT_IMPLEMENTED;
}

static void reach_host_layout_apply_visibility(const reach_host_layout_target *target,
                                               int32_t visible)
{
    if (target->desc != nullptr)
    {
        reach_surface_runtime *surface = target->desc->surface;
        int32_t activates = (target->desc->behavior_flags & REACH_SURFACE_BEHAVIOR_ACTIVATES) != 0;

        if (!visible)
        {
            surface->activated = 0;
            if (surface->window.ops.hide != nullptr)
            {
                (void)surface->window.ops.hide(surface->window.window);
            }
            return;
        }

        if (activates && surface->activated)
        {
            return;
        }
        if (surface->window.ops.show != nullptr)
        {
            (void)surface->window.ops.show(surface->window.window);
            surface->activated = activates;
        }
        return;
    }

    if (target->hotspot == nullptr || target->hotspot->hotspot == nullptr)
    {
        return;
    }
    if (visible)
    {
        if (target->hotspot->ops.show != nullptr)
        {
            (void)target->hotspot->ops.show(target->hotspot->hotspot);
        }
    }
    else if (target->hotspot->ops.hide != nullptr)
    {
        (void)target->hotspot->ops.hide(target->hotspot->hotspot);
    }
}

static const reach_layout_entry *reach_host_layout_applied_entry(
    const reach_host *host, reach_layout_participant participant)
{
    if (!host->has_applied_layout_plan)
    {
        return nullptr;
    }
    for (size_t index = 0; index < host->applied_layout_plan.count; ++index)
    {
        const reach_layout_entry *entry = &host->applied_layout_plan.entries[index];
        if (entry->participant == participant)
        {
            return entry;
        }
    }
    return nullptr;
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

    for (size_t index = 0; index < plan.count; ++index)
    {
        const reach_layout_entry *entry = &plan.entries[index];
        const reach_layout_entry *applied =
            reach_host_layout_applied_entry(host, entry->participant);
        if (applied == nullptr || applied->visible != entry->visible)
        {
            reach_host_layout_apply_visibility(&host->layout_targets[entry->participant],
                                               entry->visible);
        }
    }

    reach_window_id above = 0;
    for (size_t index = 0; index < plan.count; ++index)
    {
        const reach_layout_entry *entry = &plan.entries[index];
        const reach_host_layout_target *target = &host->layout_targets[entry->participant];

        if (entry->layer == 0)
        {
            const reach_layout_entry *applied =
                reach_host_layout_applied_entry(host, entry->participant);
            if (applied != nullptr && applied->layer > 0)
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

void reach_host_hide_all_surfaces(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    for (reach_layout_participant participant = 0;
         participant < (reach_layout_participant)host->layout_manager.participant_count;
         ++participant)
    {
        reach_layout_set_visible(&host->layout_manager, participant, 0);
    }
    reach_host_apply_layout(host);
}
