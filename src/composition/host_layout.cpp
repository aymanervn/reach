#include "host_internal.h"

static void reach_host_apply_surface_activation(const reach_surface_desc *desc, int32_t active)
{
    reach_surface_runtime *surface = desc->surface;
    if ((desc->behavior_flags & REACH_SURFACE_BEHAVIOR_ACTIVATES) == 0)
    {
        return;
    }

    if (!active)
    {
        surface->activated = 0;
        return;
    }

    if (!surface->activated && surface->window.ops.show != nullptr)
    {
        (void)surface->window.ops.show(surface->window.window);
        surface->activated = 1;
    }
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
        if (visible)
        {
            if ((target->desc->behavior_flags & REACH_SURFACE_BEHAVIOR_ACTIVATES) != 0)
            {
                reach_host_apply_surface_activation(target->desc, 1);
            }
            else if (surface->window.ops.show != nullptr)
            {
                (void)surface->window.ops.show(surface->window.window);
            }
            return;
        }

        reach_host_apply_surface_activation(target->desc, 0);
        if (surface->window.ops.hide != nullptr)
        {
            (void)surface->window.ops.hide(surface->window.window);
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

    for (size_t index = 0; index < plan.count; ++index)
    {
        const reach_layout_entry *entry = &plan.entries[index];
        reach_host_layout_apply_visibility(&host->layout_targets[entry->participant],
                                           entry->visible);
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
