#include "reach/features/dock.h"

void reach_dock_icon_cache_init(reach_dock_icon_cache *cache)
{
    if (cache == nullptr)
    {
        return;
    }

    *cache = {};
}

void reach_dock_clear_pinned_icons(reach_dock_icon_cache *cache)
{
    if (cache == nullptr)
    {
        return;
    }

    size_t count = cache->pinned_icon_count;
    if (count > REACH_MAX_PINNED_APPS)
    {
        count = REACH_MAX_PINNED_APPS;
    }

    for (size_t index = 0; index < count; ++index)
    {
        cache->pinned_icons[index] = {};
        cache->pinned_icon_pin_ids[index] = 0;
        cache->pinned_icon_initials[index] = 0;
    }

    cache->pinned_icon_count = 0;
}

void reach_dock_clear_open_window_icons(reach_dock_icon_cache *cache, size_t open_window_count)
{
    if (cache == nullptr)
    {
        return;
    }

    size_t count = open_window_count;
    if (count > REACH_MAX_PINNED_APPS)
    {
        count = REACH_MAX_PINNED_APPS;
    }

    for (size_t index = 0; index < count; ++index)
    {
        cache->open_window_icons[index] = {};
        cache->open_window_initials[index] = 0;
    }
}

void reach_dock_clear_all_icons(reach_dock_icon_cache *cache, size_t open_window_count)
{
    reach_dock_clear_pinned_icons(cache);
    reach_dock_clear_open_window_icons(cache, open_window_count);
}

reach_icon_handle reach_dock_icon_for_item(const reach_dock_icon_cache *cache,
                                           const reach_dock_feature_model *model, size_t item_index,
                                           uint16_t *out_fallback_initial)
{
    if (out_fallback_initial != nullptr)
    {
        *out_fallback_initial = '?';
    }

    if (cache == nullptr || model == nullptr || item_index >= model->item_count)
    {
        return reach_icon_handle{};
    }

    const reach_dock_item_model *item = &model->items[item_index];

    if (item->pinned)
    {
        size_t pinned_index = item->pinned_index;
        if (out_fallback_initial != nullptr)
        {
            *out_fallback_initial = pinned_index < REACH_MAX_PINNED_APPS
                                        ? cache->pinned_icon_initials[pinned_index]
                                        : '?';
        }

        return pinned_index < cache->pinned_icon_count ? cache->pinned_icons[pinned_index]
                                                       : reach_icon_handle{};
    }

    size_t open_index = item->open_index;
    if (out_fallback_initial != nullptr)
    {
        *out_fallback_initial =
            open_index < REACH_MAX_PINNED_APPS ? cache->open_window_initials[open_index] : '?';
    }

    return open_index < REACH_MAX_PINNED_APPS ? cache->open_window_icons[open_index]
                                              : reach_icon_handle{};
}
