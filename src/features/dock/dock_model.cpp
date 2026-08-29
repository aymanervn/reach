#include "reach/features/dock.h"

void reach_dock_feature_model_init(reach_dock_feature_model *model)
{
    if (model == nullptr)
    {
        return;
    }

    *model = {};
}

int32_t reach_dock_item_identity_equal(const reach_dock_item_model *item, const uint16_t *path,
                                       const uint16_t *app_user_model_id)
{
    return item != nullptr && reach_window_tracking_identity_equal(item->path,
                                                                   item->app_user_model_id, path,
                                                                   app_user_model_id);
}

uint32_t reach_dock_feature_model_item_pin_id(const reach_dock_feature_model *model, size_t index)
{
    if (model == nullptr || index >= model->item_count || !model->items[index].pinned)
    {
        return 0;
    }
    return model->items[index].pin_id;
}

uint32_t reach_dock_item_key_at(const reach_dock_feature_model *model, size_t index)
{
    return model != nullptr && index < model->item_count ? model->items[index].key : 0;
}

size_t reach_dock_feature_model_find_item_key(const reach_dock_feature_model *model, uint32_t key)
{
    if (model == nullptr || key == 0)
    {
        return REACH_MAX_DOCK_ITEMS;
    }
    for (size_t index = 0; index < model->item_count; ++index)
    {
        if (model->items[index].key == key)
        {
            return index;
        }
    }
    return REACH_MAX_DOCK_ITEMS;
}

size_t reach_dock_feature_model_find_order_key(const reach_dock_feature_model *model, uint32_t key)
{
    if (model == nullptr || key == 0)
    {
        return REACH_MAX_DOCK_ITEMS;
    }
    for (size_t index = 0; index < model->order_count; ++index)
    {
        if (model->order[index] == key)
        {
            return index;
        }
    }
    return REACH_MAX_DOCK_ITEMS;
}

void reach_dock_feature_model_move_order(reach_dock_feature_model *model, size_t source,
                                         size_t target)
{
    if (model == nullptr || source >= model->order_count || target >= model->order_count ||
        source == target)
    {
        return;
    }

    uint32_t key = model->order[source];
    if (source < target)
    {
        for (size_t index = source; index < target; ++index)
        {
            model->order[index] = model->order[index + 1];
        }
    }
    else
    {
        for (size_t index = source; index > target; --index)
        {
            model->order[index] = model->order[index - 1];
        }
    }
    model->order[target] = key;
}

static int32_t reach_dock_index_collected(const size_t *indices, size_t count, size_t index)
{
    for (size_t at = 0; at < count; ++at)
    {
        if (indices[at] == index)
        {
            return 1;
        }
    }
    return 0;
}

size_t reach_dock_collect_matching_windows(const reach_pinned_app_model *pinned_app,
                                           const reach_window_snapshot *windows,
                                           size_t window_count, const uintptr_t *focus_history,
                                           size_t focus_history_count,
                                           reach_dock_window_matches_pinned_fn matches,
                                           void *match_user, size_t *out_indices, size_t cap)
{
    if (pinned_app == nullptr || windows == nullptr || matches == nullptr ||
        out_indices == nullptr || cap == 0)
    {
        return 0;
    }

    size_t count = 0;
    for (size_t history_index = 0;
         focus_history != nullptr && history_index < focus_history_count && count < cap;
         ++history_index)
    {
        for (size_t index = 0; index < window_count; ++index)
        {
            if (windows[index].id != focus_history[history_index] ||
                reach_dock_index_collected(out_indices, count, index) ||
                !matches(match_user, pinned_app, &windows[index]))
            {
                continue;
            }
            out_indices[count++] = index;
            break;
        }
    }

    for (size_t index = 0; index < window_count && count < cap; ++index)
    {
        if (reach_dock_index_collected(out_indices, count, index) ||
            !matches(match_user, pinned_app, &windows[index]))
        {
            continue;
        }
        out_indices[count++] = index;
    }

    return count;
}

size_t reach_dock_feature_model_pinned_order_index(const reach_dock_feature_model *model,
                                                   uint32_t pin_id)
{
    if (model == nullptr || pin_id == 0)
    {
        return REACH_MAX_DOCK_ITEMS;
    }

    size_t pinned_index = 0;
    for (size_t index = 0; index < model->item_count; ++index)
    {
        if (!model->items[index].pinned)
        {
            continue;
        }
        if (model->items[index].pin_id == pin_id)
        {
            return pinned_index;
        }
        ++pinned_index;
    }
    return REACH_MAX_DOCK_ITEMS;
}
