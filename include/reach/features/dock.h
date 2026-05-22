#ifndef REACH_FEATURES_DOCK_H
#define REACH_FEATURES_DOCK_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/ui_state.h"
#include "reach/core/render_commands.h"
#include "reach/ports/icon_provider.h"
#include "reach/ports/window_manager.h"
#include "reach/theme.h"

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

typedef struct reach_dock_icon_cache {
    reach_icon_handle pinned_icons[REACH_MAX_PINNED_APPS];
    size_t pinned_icon_count;
    uint16_t pinned_icon_initials[REACH_MAX_PINNED_APPS];
    reach_icon_handle open_window_icons[REACH_MAX_PINNED_APPS];
    uint16_t open_window_initials[REACH_MAX_PINNED_APPS];
} reach_dock_icon_cache;

void reach_dock_feature_model_init(reach_dock_feature_model *model);
void reach_dock_icon_cache_init(reach_dock_icon_cache *cache);
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
reach_result reach_dock_load_pinned_icons(
    reach_dock_icon_cache *cache,
    reach_icon_provider_port *icon_provider,
    const reach_pinned_app_model *pinned_apps,
    size_t pinned_app_count,
    int32_t size_px);
void reach_dock_release_pinned_icons(reach_dock_icon_cache *cache, reach_icon_provider_port *icon_provider);
reach_result reach_dock_load_open_window_icons(
    reach_dock_icon_cache *cache,
    reach_icon_provider_port *icon_provider,
    const reach_window_snapshot *open_windows,
    size_t open_window_count,
    int32_t size_px);
void reach_dock_release_open_window_icons(reach_dock_icon_cache *cache, reach_icon_provider_port *icon_provider, size_t open_window_count);
void reach_dock_release_all_icons(reach_dock_icon_cache *cache, reach_icon_provider_port *icon_provider, size_t open_window_count);
reach_icon_handle reach_dock_icon_for_item(
    const reach_dock_icon_cache *cache,
    const reach_dock_feature_model *model,
    size_t item_index,
    uint16_t *out_fallback_initial);

typedef struct reach_dock_render_input {
    const reach_theme *theme;
    const reach_dock_layout *layout;
    const reach_dock_feature_model *model;
    const reach_dock_icon_cache *icons;
    const float *item_box_x;
    size_t item_box_x_count;
    uintptr_t focused_window;
    size_t dragged_render_index;
    float dragged_box_x;
    size_t click_feedback_index;
    float click_feedback_opacity;
    size_t tray_feedback_index;
    int32_t text_alignment_center;
} reach_dock_render_input;

reach_result reach_dock_build_render_commands(const reach_dock_render_input *input, reach_render_command_buffer *out_commands);

#ifdef __cplusplus
}
#endif

#endif
