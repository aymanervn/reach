#include "reach/features/dock.h"

static reach_dock_order_key reach_dock_order_key_for_item(const reach_dock_item_model *item)
{
    reach_dock_order_key key = {};
    if (item == nullptr)
    {
        return key;
    }

    key.pinned = item->pinned;
    key.pin_id = item->pin_id;
    key.window = item->window;
    return key;
}

static void reach_dock_set_order_key(reach_dock_feature_model *model, size_t index,
                                     const reach_dock_item_model *item)
{
    if (model == nullptr || item == nullptr || index >= REACH_MAX_PINNED_APPS)
    {
        return;
    }
    model->order[index] = reach_dock_order_key_for_item(item);
}

size_t reach_dock_find_pinned_for_window(const reach_pinned_app_model *pinned_apps,
                                         size_t pinned_app_count,
                                         const reach_window_snapshot *window,
                                         reach_dock_window_matches_pinned_fn window_matches_pinned,
                                         void *match_user)
{
    if (pinned_apps == nullptr || window == nullptr || window_matches_pinned == nullptr)
    {
        return REACH_MAX_PINNED_APPS;
    }

    for (size_t index = 0; index < pinned_app_count; ++index)
    {
        if (window_matches_pinned(match_user, &pinned_apps[index], window))
        {
            return index;
        }
    }

    return REACH_MAX_PINNED_APPS;
}

static int32_t reach_dock_pinned_running(const reach_pinned_app_model *pinned_apps,
                                         size_t pinned_app_count,
                                         const reach_window_snapshot *open_windows,
                                         size_t open_window_count, size_t pinned_index,
                                         reach_dock_window_matches_pinned_fn window_matches_pinned,
                                         void *match_user, uintptr_t *out_window)
{
    if (out_window != nullptr)
    {
        *out_window = 0;
    }
    if (pinned_apps == nullptr || open_windows == nullptr || pinned_index >= pinned_app_count)
    {
        return 0;
    }

    for (size_t index = 0; index < open_window_count; ++index)
    {
        if (reach_dock_find_pinned_for_window(pinned_apps, pinned_app_count, &open_windows[index],
                                              window_matches_pinned, match_user) == pinned_index)
        {
            if (out_window != nullptr)
            {
                *out_window = open_windows[index].id;
            }
            return 1;
        }
    }
    return 0;
}

static int32_t reach_dock_append_item(reach_dock_item_model *items, size_t *item_count,
                                      const reach_dock_item_model *item)
{
    if (items == nullptr || item_count == nullptr || item == nullptr ||
        *item_count >= REACH_MAX_PINNED_APPS)
    {
        return 0;
    }

    items[*item_count] = *item;
    ++(*item_count);
    return 1;
}

static void reach_dock_append_pinned_items(
    reach_dock_item_model *items, size_t *item_count, const reach_pinned_app_model *pinned_apps,
    size_t pinned_app_count, const reach_window_snapshot *open_windows, size_t open_window_count,
    reach_dock_window_matches_pinned_fn window_matches_pinned, void *match_user)
{
    if (items == nullptr || item_count == nullptr || pinned_apps == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < pinned_app_count && *item_count < REACH_MAX_PINNED_APPS; ++index)
    {
        uintptr_t window_id = 0;
        reach_dock_item_model item = {};
        item.pinned = 1;
        item.pin_id = pinned_apps[index].id;
        item.window = reach_dock_pinned_running(pinned_apps, pinned_app_count, open_windows,
                                                open_window_count, index, window_matches_pinned,
                                                match_user, &window_id)
                          ? window_id
                          : 0;
        item.pinned_index = index;
        item.open_index = REACH_MAX_PINNED_APPS;

        reach_dock_append_item(items, item_count, &item);
    }
}

static void reach_dock_append_unpinned_open_windows(
    reach_dock_item_model *items, size_t *item_count, const reach_pinned_app_model *pinned_apps,
    size_t pinned_app_count, const reach_window_snapshot *open_windows, size_t open_window_count,
    reach_dock_window_matches_pinned_fn window_matches_pinned, void *match_user)
{
    if (items == nullptr || item_count == nullptr || open_windows == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < open_window_count && *item_count < REACH_MAX_PINNED_APPS;
         ++index)
    {
        if (reach_dock_find_pinned_for_window(pinned_apps, pinned_app_count, &open_windows[index],
                                              window_matches_pinned,
                                              match_user) != REACH_MAX_PINNED_APPS)
        {
            continue;
        }

        reach_dock_item_model item = {};
        item.pinned = 0;
        item.pin_id = 0;
        item.window = open_windows[index].id;
        item.pinned_index = REACH_MAX_PINNED_APPS;
        item.open_index = index;

        reach_dock_append_item(items, item_count, &item);
    }
}

static void reach_dock_build_candidate_items(
    reach_dock_item_model *items, size_t *item_count, const reach_pinned_app_model *pinned_apps,
    size_t pinned_app_count, const reach_window_snapshot *open_windows, size_t open_window_count,
    reach_dock_window_matches_pinned_fn window_matches_pinned, void *match_user)
{
    if (item_count != nullptr)
    {
        *item_count = 0;
    }

    reach_dock_append_pinned_items(items, item_count, pinned_apps, pinned_app_count, open_windows,
                                   open_window_count, window_matches_pinned, match_user);

    reach_dock_append_unpinned_open_windows(items, item_count, pinned_apps, pinned_app_count,
                                            open_windows, open_window_count, window_matches_pinned,
                                            match_user);
}

static void reach_dock_apply_existing_order(reach_dock_feature_model *model,
                                            const reach_dock_item_model *candidates,
                                            size_t candidate_count, int32_t *used)
{
    if (model == nullptr || candidates == nullptr || used == nullptr)
    {
        return;
    }

    model->item_count = 0;

    for (size_t order_index = 0;
         order_index < model->order_count && model->item_count < REACH_MAX_PINNED_APPS;
         ++order_index)
    {
        for (size_t candidate_index = 0; candidate_index < candidate_count; ++candidate_index)
        {
            reach_dock_order_key candidate_key =
                reach_dock_order_key_for_item(&candidates[candidate_index]);
            if (!used[candidate_index] &&
                reach_dock_key_equal(&model->order[order_index], &candidate_key))
            {
                model->items[model->item_count++] = candidates[candidate_index];
                used[candidate_index] = 1;
                break;
            }
        }
    }
}

static void reach_dock_append_new_items(reach_dock_feature_model *model,
                                        const reach_dock_item_model *candidates,
                                        size_t candidate_count, const int32_t *used)
{
    if (model == nullptr || candidates == nullptr || used == nullptr)
    {
        return;
    }

    for (size_t candidate_index = 0;
         candidate_index < candidate_count && model->item_count < REACH_MAX_PINNED_APPS;
         ++candidate_index)
    {
        if (!used[candidate_index])
        {
            model->items[model->item_count++] = candidates[candidate_index];
        }
    }
}

static void reach_dock_store_current_order(reach_dock_feature_model *model)
{
    if (model == nullptr)
    {
        return;
    }

    model->order_count = model->item_count;
    for (size_t index = 0; index < model->item_count; ++index)
    {
        reach_dock_set_order_key(model, index, &model->items[index]);
    }
}

void reach_dock_feature_model_build_items(
    reach_dock_feature_model *model, const reach_pinned_app_model *pinned_apps,
    size_t pinned_app_count, const reach_window_snapshot *open_windows, size_t open_window_count,
    reach_dock_window_matches_pinned_fn window_matches_pinned, void *match_user)
{
    if (model == nullptr)
    {
        return;
    }

    reach_dock_item_model candidates[REACH_MAX_PINNED_APPS] = {};
    int32_t used[REACH_MAX_PINNED_APPS] = {};
    size_t candidate_count = 0;

    reach_dock_build_candidate_items(candidates, &candidate_count, pinned_apps, pinned_app_count,
                                     open_windows, open_window_count, window_matches_pinned,
                                     match_user);

    reach_dock_apply_existing_order(model, candidates, candidate_count, used);
    reach_dock_append_new_items(model, candidates, candidate_count, used);
    reach_dock_store_current_order(model);
}
