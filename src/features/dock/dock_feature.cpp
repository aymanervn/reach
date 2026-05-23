#include "reach/features/dock.h"

size_t reach_dock_find_pinned_for_path(
    const reach_pinned_app_model *pinned_apps,
    size_t pinned_app_count,
    const uint16_t *path,
    reach_dock_path_matches_pinned_fn path_matches_pinned,
    void *path_match_user)
{
    if (pinned_apps == nullptr || path == nullptr || path[0] == 0 || path_matches_pinned == nullptr) {
        return REACH_MAX_PINNED_APPS;
    }

    for (size_t index = 0; index < pinned_app_count; ++index) {
        if (path_matches_pinned(path_match_user, &pinned_apps[index], path)) {
            return index;
        }
    }
    return REACH_MAX_PINNED_APPS;
}

static int32_t reach_dock_pinned_running(
    const reach_pinned_app_model *pinned_apps,
    size_t pinned_app_count,
    const reach_window_snapshot *open_windows,
    size_t open_window_count,
    size_t pinned_index,
    reach_dock_path_matches_pinned_fn path_matches_pinned,
    void *path_match_user,
    uintptr_t *out_window)
{
    if (out_window != nullptr) {
        *out_window = 0;
    }
    if (pinned_apps == nullptr || open_windows == nullptr || pinned_index >= pinned_app_count) {
        return 0;
    }

    for (size_t index = 0; index < open_window_count; ++index) {
        if (reach_dock_find_pinned_for_path(
                pinned_apps,
                pinned_app_count,
                open_windows[index].path,
                path_matches_pinned,
                path_match_user) == pinned_index) {
            if (out_window != nullptr) {
                *out_window = open_windows[index].id;
            }
            return 1;
        }
    }
    return 0;
}

static void reach_dock_set_order_key(reach_dock_feature_model *model, size_t index, reach_dock_order_key key)
{
    if (model == nullptr || index >= REACH_MAX_PINNED_APPS) {
        return;
    }
    model->order[index] = key;
}

void reach_dock_feature_model_build_items(
    reach_dock_feature_model *model,
    const reach_pinned_app_model *pinned_apps,
    size_t pinned_app_count,
    const reach_window_snapshot *open_windows,
    size_t open_window_count,
    reach_dock_path_matches_pinned_fn path_matches_pinned,
    void *path_match_user)
{
    if (model == nullptr) {
        return;
    }

    reach_dock_item_model natural_items[REACH_MAX_PINNED_APPS] = {};
    int32_t used[REACH_MAX_PINNED_APPS] = {};
    size_t natural_count = 0;

    for (size_t index = 0; pinned_apps != nullptr && index < pinned_app_count && natural_count < REACH_MAX_PINNED_APPS; ++index) {
        uintptr_t window_id = 0;
        natural_items[natural_count].pinned = 1;
        natural_items[natural_count].pin_id = pinned_apps[index].id;
        natural_items[natural_count].window = reach_dock_pinned_running(
            pinned_apps,
            pinned_app_count,
            open_windows,
            open_window_count,
            index,
            path_matches_pinned,
            path_match_user,
            &window_id) ? window_id : 0;
        natural_items[natural_count].pinned_index = index;
        natural_items[natural_count].open_index = REACH_MAX_PINNED_APPS;
        ++natural_count;
    }
    for (size_t index = 0; open_windows != nullptr && index < open_window_count && natural_count < REACH_MAX_PINNED_APPS; ++index) {
        if (reach_dock_find_pinned_for_path(
                pinned_apps,
                pinned_app_count,
                open_windows[index].path,
                path_matches_pinned,
                path_match_user) != REACH_MAX_PINNED_APPS) {
            continue;
        }
        natural_items[natural_count].pinned = 0;
        natural_items[natural_count].pin_id = 0;
        natural_items[natural_count].window = open_windows[index].id;
        natural_items[natural_count].pinned_index = REACH_MAX_PINNED_APPS;
        natural_items[natural_count].open_index = index;
        ++natural_count;
    }

    model->item_count = 0;
    for (size_t order_index = 0; order_index < model->order_count; ++order_index) {
        for (size_t natural_index = 0; natural_index < natural_count; ++natural_index) {
            reach_dock_order_key natural_key = {};
            natural_key.pinned = natural_items[natural_index].pinned;
            natural_key.pin_id = natural_items[natural_index].pin_id;
            natural_key.window = natural_items[natural_index].window;
            if (!used[natural_index] && reach_dock_key_equal(&model->order[order_index], &natural_key)) {
                model->items[model->item_count++] = natural_items[natural_index];
                used[natural_index] = 1;
                break;
            }
        }
    }
    for (size_t natural_index = 0; natural_index < natural_count && model->item_count < REACH_MAX_PINNED_APPS; ++natural_index) {
        if (!used[natural_index]) {
            model->items[model->item_count++] = natural_items[natural_index];
        }
    }

    model->order_count = model->item_count;
    for (size_t index = 0; index < model->item_count; ++index) {
        reach_dock_order_key key = {};
        key.pinned = model->items[index].pinned;
        key.pin_id = model->items[index].pin_id;
        key.window = model->items[index].window;
        reach_dock_set_order_key(model, index, key);
    }
}
