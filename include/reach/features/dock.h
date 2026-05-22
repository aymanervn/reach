#ifndef REACH_FEATURES_DOCK_H
#define REACH_FEATURES_DOCK_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/ui_state.h"
#include "reach/ports/window_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t (*reach_dock_path_matches_pinned_fn)(
    void *user,
    const reach_pinned_app_model *pinned_app,
    const uint16_t *path);

typedef struct reach_dock_order_key {
    int32_t pinned;
    uint32_t pin_id;
    uintptr_t window;
} reach_dock_order_key;

typedef struct reach_dock_item_model {
    int32_t pinned;
    uint32_t pin_id;
    uintptr_t window;
    size_t pinned_index;
    size_t open_index;
} reach_dock_item_model;

typedef struct reach_dock_feature_model {
    reach_dock_item_model items[REACH_MAX_PINNED_APPS];
    size_t item_count;
    reach_dock_order_key order[REACH_MAX_PINNED_APPS];
    size_t order_count;
} reach_dock_feature_model;

void reach_dock_feature_model_init(reach_dock_feature_model *model);
int32_t reach_dock_key_equal(const reach_dock_order_key *a, const reach_dock_order_key *b);
uint32_t reach_dock_feature_model_item_pin_id(const reach_dock_feature_model *model, size_t index);
int32_t reach_dock_feature_model_item_matches_key(const reach_dock_feature_model *model, size_t index, reach_dock_order_key key);
size_t reach_dock_feature_model_find_item_key(const reach_dock_feature_model *model, reach_dock_order_key key);
size_t reach_dock_feature_model_find_order_key(const reach_dock_feature_model *model, reach_dock_order_key key);
void reach_dock_feature_model_move_order(reach_dock_feature_model *model, size_t source, size_t target);
size_t reach_dock_feature_model_pinned_order_index(const reach_dock_feature_model *model, uint32_t pin_id);
size_t reach_dock_find_pinned_for_path(
    const reach_pinned_app_model *pinned_apps,
    size_t pinned_app_count,
    const uint16_t *path,
    reach_dock_path_matches_pinned_fn path_matches_pinned,
    void *path_match_user);
void reach_dock_feature_model_build_items(
    reach_dock_feature_model *model,
    const reach_pinned_app_model *pinned_apps,
    size_t pinned_app_count,
    const reach_window_snapshot *open_windows,
    size_t open_window_count,
    reach_dock_path_matches_pinned_fn path_matches_pinned,
    void *path_match_user);

#ifdef __cplusplus
}
#endif

#endif
