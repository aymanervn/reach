#include "reach/features/dock.h"

static void reach_dock_copy_utf16(uint16_t *dst, size_t dst_count, const uint16_t *src)
{
    if (dst == nullptr || dst_count == 0) {
        return;
    }
    if (src == nullptr) {
        dst[0] = 0;
        return;
    }

    size_t index = 0;
    while (index + 1 < dst_count && src[index] != 0) {
        dst[index] = src[index];
        ++index;
    }
    dst[index] = 0;
}

void reach_dock_icon_cache_init(reach_dock_icon_cache *cache)
{
    if (cache == nullptr) {
        return;
    }

    *cache = {};
}

void reach_dock_release_pinned_icons(reach_dock_icon_cache *cache, reach_icon_provider_port *icon_provider)
{
    if (cache == nullptr) {
        return;
    }

    for (size_t index = 0; index < cache->pinned_icon_count; ++index) {
        if (icon_provider != nullptr && icon_provider->ops.release != nullptr && cache->pinned_icons[index].id != 0) {
            (void)icon_provider->ops.release(icon_provider->provider, cache->pinned_icons[index]);
        }
        cache->pinned_icons[index] = {};
        cache->pinned_icon_initials[index] = 0;
    }
    cache->pinned_icon_count = 0;
}

reach_result reach_dock_load_pinned_icons(
    reach_dock_icon_cache *cache,
    reach_icon_provider_port *icon_provider,
    const reach_pinned_app_model *pinned_apps,
    size_t pinned_app_count,
    int32_t size_px)
{
    if (cache == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_dock_release_pinned_icons(cache, icon_provider);
    cache->pinned_icon_count = pinned_app_count > REACH_MAX_PINNED_APPS ? REACH_MAX_PINNED_APPS : pinned_app_count;
    for (size_t index = 0; pinned_apps != nullptr && index < cache->pinned_icon_count; ++index) {
        cache->pinned_icon_initials[index] = pinned_apps[index].title[0] != 0 ? pinned_apps[index].title[0] : '?';
        const uint16_t *icon_path = pinned_apps[index].icon_ref[0] != 0
            ? pinned_apps[index].icon_ref
            : pinned_apps[index].path;
        if (icon_path[0] == 0 || icon_provider == nullptr || icon_provider->ops.load == nullptr) {
            continue;
        }

        reach_icon_request request = {};
        request.size_px = size_px;
        reach_dock_copy_utf16(request.path, 260, icon_path);
        (void)icon_provider->ops.load(icon_provider->provider, &request, &cache->pinned_icons[index]);
    }

    return REACH_OK;
}

void reach_dock_release_open_window_icons(reach_dock_icon_cache *cache, reach_icon_provider_port *icon_provider, size_t open_window_count)
{
    if (cache == nullptr) {
        return;
    }

    size_t count = open_window_count > REACH_MAX_PINNED_APPS ? REACH_MAX_PINNED_APPS : open_window_count;
    for (size_t index = 0; index < count; ++index) {
        if (icon_provider != nullptr && icon_provider->ops.release != nullptr && cache->open_window_icons[index].id != 0) {
            (void)icon_provider->ops.release(icon_provider->provider, cache->open_window_icons[index]);
        }
        cache->open_window_icons[index] = {};
        cache->open_window_initials[index] = 0;
    }
}

reach_result reach_dock_load_open_window_icons(
    reach_dock_icon_cache *cache,
    reach_icon_provider_port *icon_provider,
    const reach_window_snapshot *open_windows,
    size_t open_window_count,
    int32_t size_px)
{
    if (cache == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    size_t count = open_window_count > REACH_MAX_PINNED_APPS ? REACH_MAX_PINNED_APPS : open_window_count;
    for (size_t index = 0; open_windows != nullptr && index < count; ++index) {
        cache->open_window_initials[index] = open_windows[index].title[0] != 0 ? open_windows[index].title[0] : '?';
        if (open_windows[index].path[0] == 0 || icon_provider == nullptr || icon_provider->ops.load == nullptr) {
            continue;
        }

        reach_icon_request request = {};
        request.size_px = size_px;
        reach_dock_copy_utf16(request.path, 260, open_windows[index].path);
        (void)icon_provider->ops.load(icon_provider->provider, &request, &cache->open_window_icons[index]);
    }

    return REACH_OK;
}

void reach_dock_release_all_icons(reach_dock_icon_cache *cache, reach_icon_provider_port *icon_provider, size_t open_window_count)
{
    reach_dock_release_pinned_icons(cache, icon_provider);
    reach_dock_release_open_window_icons(cache, icon_provider, open_window_count);
}

reach_icon_handle reach_dock_icon_for_item(
    const reach_dock_icon_cache *cache,
    const reach_dock_feature_model *model,
    size_t item_index,
    uint16_t *out_fallback_initial)
{
    if (out_fallback_initial != nullptr) {
        *out_fallback_initial = '?';
    }
    if (cache == nullptr || model == nullptr || item_index >= model->item_count) {
        return reach_icon_handle {};
    }

    const reach_dock_item_model *item = &model->items[item_index];
    if (item->pinned) {
        size_t pinned_index = item->pinned_index;
        if (out_fallback_initial != nullptr) {
            *out_fallback_initial = pinned_index < REACH_MAX_PINNED_APPS ? cache->pinned_icon_initials[pinned_index] : '?';
        }
        return pinned_index < cache->pinned_icon_count ? cache->pinned_icons[pinned_index] : reach_icon_handle {};
    }

    size_t open_index = item->open_index;
    if (out_fallback_initial != nullptr) {
        *out_fallback_initial = open_index < REACH_MAX_PINNED_APPS ? cache->open_window_initials[open_index] : '?';
    }
    return open_index < REACH_MAX_PINNED_APPS ? cache->open_window_icons[open_index] : reach_icon_handle {};
}
