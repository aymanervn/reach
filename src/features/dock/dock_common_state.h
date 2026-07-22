#ifndef REACH_FEATURES_DOCK_COMMON_STATE_H
#define REACH_FEATURES_DOCK_COMMON_STATE_H

#include "reach/features/dock.h"
#include "dock_now_playing.h"

reach_dock_state *reach_dock_state_mut(reach_dock *dock);
reach_icon_service *reach_dock_icons(reach_dock *dock);
reach_window_tracking *reach_dock_windows(reach_dock *dock);
reach_dock_now_playing *reach_dock_now_playing_subfeature(reach_dock *dock);

float reach_dock_item_reveal(reach_dock *dock, size_t item_index);

float reach_dock_slot_box_x(const reach_theme *theme, const reach_dock_layout *layout,
                            size_t index);
float reach_dock_item_current_x(reach_dock *dock, const reach_theme *theme,
                                const reach_dock_layout *layout, size_t index);

typedef struct reach_dock_item_x_snapshot
{
    reach_dock_order_key keys[REACH_MAX_PINNED_APPS];
    float x[REACH_MAX_PINNED_APPS];
    size_t count;
} reach_dock_item_x_snapshot;

void reach_dock_item_x_snapshot_take(reach_dock *dock, const reach_theme *theme,
                                     const reach_dock_layout *old_layout,
                                     reach_dock_item_x_snapshot *out_snapshot);
void reach_dock_item_x_rebind(reach_dock *dock, const reach_theme *theme,
                              const reach_dock_layout *layout,
                              const reach_dock_item_x_snapshot *snapshot);

float reach_dock_now_playing_reveal_width(reach_dock *dock, float scaled_gap);

#endif
