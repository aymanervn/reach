#include "host_internal.h"

static void reach_host_release_render_icon_from_surface(reach_surface_runtime *surface,
                                                        uint64_t icon_id)
{
    if (surface == nullptr || icon_id == 0 || surface->renderer.ops.release_icon == nullptr)
    {
        return;
    }

    surface->renderer.ops.release_icon(surface->renderer.backend, icon_id);
}

void reach_host_release_render_icon(reach_host *host, uint64_t icon_id)
{
    if (host == nullptr || icon_id == 0)
    {
        return;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_host_release_render_icon_from_surface(host->feature_runtimes[index].surface, icon_id);
    }
}

static const double REACH_HOST_ICON_TRIM_SECONDS = 60.0;

void reach_host_notify_icons_retained(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }
    reach_feature_notification notification = {};
    notification.kind = REACH_FEATURE_NOTIFICATION_ICONS_RETAIN;
    notification.icon_size_px = reach_host_icon_size_px(host);
    notification.present = 1;
    reach_host_notify_registered_features(host, &notification);
}

void reach_host_drain_icon_evictions(reach_host *host)
{
    if (host == nullptr || host->icon_service == nullptr)
    {
        return;
    }

    reach_host_notify_icons_retained(host);
    reach_icon_service_trim(host->icon_service, REACH_HOST_ICON_TRIM_SECONDS);
    for (;;)
    {
        uint64_t icon_ids[32] = {};
        size_t count = reach_icon_service_take_evicted(host->icon_service, icon_ids, 32);
        if (count == 0)
        {
            break;
        }
        for (size_t index = 0; index < count; ++index)
        {
            reach_host_release_render_icon(host, icon_ids[index]);
        }
    }
}

static void
reach_host_release_feature_render_resource(reach_host *host, reach_feature_runtime *runtime,
                                           const reach_feature_render_resource *resource)
{
    if (host == nullptr || runtime == nullptr || resource == nullptr)
    {
        return;
    }
    reach_host_release_render_icon(host, resource->render_icon_id);
    const reach_feature_render_resource_ops *resources =
        runtime->definition->control_ops->render_resources;
    if (resources->release_source != nullptr)
    {
        resources->release_source(runtime->capsule, resource);
    }
}

void reach_host_drain_registered_render_resources(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_feature_runtime *runtime = &host->feature_runtimes[index];
        const reach_feature_control_ops *control = runtime->definition->control_ops;
        if (control == nullptr || control->render_resources == nullptr ||
            control->render_resources->take_retired == nullptr)
        {
            continue;
        }
        for (;;)
        {
            reach_feature_render_resource retired[64] = {};
            size_t count = control->render_resources->take_retired(runtime->capsule, retired, 64);
            for (size_t resource = 0; resource < count; ++resource)
            {
                reach_host_release_feature_render_resource(host, runtime, &retired[resource]);
            }
            if (count < 64)
            {
                break;
            }
        }
    }
}

void reach_host_release_registered_render_resources(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_host_drain_registered_render_resources(host);
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_feature_runtime *runtime = &host->feature_runtimes[index];
        const reach_feature_control_ops *control = runtime->definition->control_ops;
        if (control == nullptr || control->render_resources == nullptr)
        {
            continue;
        }
        const reach_feature_render_resource_ops *resources = control->render_resources;
        size_t count =
            resources->active_count != nullptr ? resources->active_count(runtime->capsule) : 0;
        for (size_t resource_index = 0; resource_index < count; ++resource_index)
        {
            reach_feature_render_resource resource = {};
            if (resources->active_at != nullptr &&
                resources->active_at(runtime->capsule, resource_index, &resource))
            {
                reach_host_release_feature_render_resource(host, runtime, &resource);
            }
        }
        if (resources->clear_active != nullptr)
        {
            resources->clear_active(runtime->capsule);
        }
    }
}

void reach_host_drain_now_playing_retired_covers(reach_host *host)
{
    if (host == nullptr || host->now_playing_service == nullptr)
    {
        return;
    }
    uint64_t cover_image_id = 0;
    while (reach_now_playing_service_take_retired_cover(host->now_playing_service, &cover_image_id))
    {
        reach_host_release_render_icon(host, cover_image_id);
        reach_now_playing_service_release_cover(host->now_playing_service, cover_image_id);
    }
}
