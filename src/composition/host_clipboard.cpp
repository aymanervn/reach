#include "host_internal.h"

void reach_host_release_clipboard_item(reach_host *host, const reach_clipboard_item *item)
{
    if (host == nullptr || item == nullptr || item->id == 0)
    {
        return;
    }
    if (item->thumbnail_id != 0 && host->clipboard_surface.renderer.ops.release_icon != nullptr)
    {
        host->clipboard_surface.renderer.ops.release_icon(host->clipboard_surface.renderer.backend,
                                                          item->thumbnail_id);
    }
    if (host->clipboard.ops.release != nullptr)
    {
        host->clipboard.ops.release(host->clipboard.provider, item->id);
    }
}

void reach_host_release_clipboard_items(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }
    for (size_t index = 0; index < reach_clipboard_item_count(host->clipboard_capsule); ++index)
    {
        reach_host_release_clipboard_item(host,
                                          reach_clipboard_item_at(host->clipboard_capsule, index));
    }
    reach_clipboard_reset_items(host->clipboard_capsule);
}

void reach_host_clear_clipboard(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < reach_clipboard_item_count(host->clipboard_capsule); ++index)
    {
        reach_host_release_clipboard_item(host,
                                          reach_clipboard_item_at(host->clipboard_capsule, index));
    }

    reach_clipboard_clear_all(host->clipboard_capsule);
    host->clipboard_surface.dirty_flags = 1;
    host->dirty.layout = 1;
    reach_host_request_update(host);
}

void reach_host_set_clipboard_open(reach_host *host, int32_t open)
{
    if (host == nullptr)
    {
        return;
    }
    int32_t next = open ? 1 : 0;
    if (!reach_clipboard_set_open(host->clipboard_capsule, next))
    {
        return;
    }
    if (next)
    {
        reach_host_surface_opening(host, REACH_SURFACE_ID_CLIPBOARD,
                               REACH_SURFACE_ORIGIN_NONE);
    }
    reach_host_surface_transition_set(host, &host->clipboard_transition, next);
    reach_host_sync_pointer_move_subscriptions(host);
    reach_host_sync_popup_mouse_hook(host);
    host->clipboard_surface.dirty_flags = 1;
    host->dirty.layout = 1;
}

void reach_host_toggle_clipboard(reach_host *host)
{
    if (host != nullptr)
    {
        reach_host_set_clipboard_open(host, !reach_clipboard_is_open(host->clipboard_capsule));
    }
}

void reach_host_process_clipboard_refresh(reach_host *host)
{
    if (host == nullptr || reach_clipboard_feature_take_refresh(host->clipboard_capsule) == 0 ||
        host->clipboard.ops.capture_current == nullptr)
    {
        return;
    }
    reach_clipboard_item item = {};
    if (host->clipboard.ops.capture_current(host->clipboard.provider, &item) != REACH_OK ||
        item.id == 0)
    {
        return;
    }

    reach_clipboard_insert_outcome outcome = {};
    reach_clipboard_insert_captured(host->clipboard_capsule, item, &outcome);
    if (outcome.release_rejected.id != 0)
    {
        reach_host_release_clipboard_item(host, &outcome.release_rejected);
    }
    if (outcome.release_evicted.id != 0)
    {
        reach_host_release_clipboard_item(host, &outcome.release_evicted);
    }
    if (outcome.accepted)
    {
        host->clipboard_surface.dirty_flags = 1;
        host->dirty.layout = 1;
        reach_host_request_update(host);
    }
}
