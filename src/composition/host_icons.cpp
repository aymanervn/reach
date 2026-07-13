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

    reach_host_release_render_icon_from_surface(&host->launcher, icon_id);
    reach_host_release_render_icon_from_surface(&host->dock, icon_id);
    reach_host_release_render_icon_from_surface(&host->tray, icon_id);
    reach_host_release_render_icon_from_surface(&host->switcher, icon_id);
    reach_host_release_render_icon_from_surface(&host->context_menu, icon_id);
    reach_host_release_render_icon_from_surface(&host->quick_settings, icon_id);
    reach_host_release_render_icon_from_surface(&host->clipboard_surface, icon_id);
}

static const double REACH_HOST_ICON_TRIM_SECONDS = 60.0;

void reach_host_drain_icon_evictions(reach_host *host)
{
    if (host == nullptr || host->icon_service == nullptr)
    {
        return;
    }

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

void reach_host_release_tray_render_icons(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    size_t count = reach_tray_item_count(host->tray_capsule);
    if (count > REACH_MAX_TRAY_ITEMS)
    {
        count = REACH_MAX_TRAY_ITEMS;
    }

    for (size_t index = 0; index < count; ++index)
    {
        uint64_t icon_id = reach_tray_item_icon_id(host->tray_capsule, index);
        if (icon_id != 0)
        {
            reach_host_release_render_icon(host, icon_id);
        }
    }
}

void reach_host_release_quick_settings_audio_render_icons(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    const reach_quick_settings_model *model =
        &reach_quick_settings_state_ptr(host->quick_settings_capsule)->model;
    size_t session_count = model->sessions.count;
    if (session_count > REACH_AUDIO_VOLUME_MAX_SESSIONS)
    {
        session_count = REACH_AUDIO_VOLUME_MAX_SESSIONS;
    }

    for (size_t index = 0; index < session_count; ++index)
    {
        uint64_t icon_id = model->sessions.sessions[index].icon_id;
        if (icon_id != 0)
        {
            reach_host_release_render_icon(host, icon_id);
        }
    }

    size_t device_count = model->output_devices.count;
    if (device_count > REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES)
    {
        device_count = REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES;
    }

    for (size_t index = 0; index < device_count; ++index)
    {
        uint64_t icon_id = model->output_devices.devices[index].icon_id;
        if (icon_id != 0)
        {
            reach_host_release_render_icon(host, icon_id);
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
    while (reach_now_playing_service_take_retired_cover(host->now_playing_service,
                                                        &cover_image_id))
    {
        reach_host_release_render_icon(host, cover_image_id);
        reach_now_playing_service_release_cover(host->now_playing_service, cover_image_id);
    }
}
