#ifndef REACH_FEATURES_DOCK_COMMON_STATE_H
#define REACH_FEATURES_DOCK_COMMON_STATE_H

#include "reach/features/dock.h"
#include "dock_now_playing.h"

/*
 * Feature-internal mutable access to the capsule state. The public
 * reach_dock_state_ptr() is const; everything outside src/features/dock
 * mutates through the semantic ops only.
 */
reach_dock_state *reach_dock_state_mut(reach_dock *dock);
reach_icon_service *reach_dock_icons(reach_dock *dock);
reach_window_tracking *reach_dock_windows(reach_dock *dock);
reach_dock_now_playing *reach_dock_now_playing_subfeature(reach_dock *dock);

/* The item's reveal (0..1) derived from its slot's width progress: 0 below
   the reveal threshold, ramping to 1 as the slot lands. Steady slots = 1. */
float reach_dock_item_reveal(reach_dock *dock, size_t item_index);

/* Feature-internal geometry + icon-motion bookkeeping (slots decide resting
   positions; icons ride them plus a decaying reorder offset). */
float reach_dock_slot_box_x(const reach_theme *theme, const reach_dock_layout *layout,
                            size_t index);
float reach_dock_item_current_x(reach_dock *dock, const reach_theme *theme,
                                const reach_dock_layout *layout, size_t index);

typedef struct reach_dock_item_x_snapshot
{
    int32_t pinned[REACH_MAX_PINNED_APPS];
    uint32_t pin_ids[REACH_MAX_PINNED_APPS];
    uintptr_t windows[REACH_MAX_PINNED_APPS];
    float x[REACH_MAX_PINNED_APPS];
    size_t count;
} reach_dock_item_x_snapshot;

void reach_dock_item_x_snapshot_take(reach_dock *dock, const reach_theme *theme,
                                     const reach_dock_layout *old_layout,
                                     reach_dock_item_x_snapshot *out_snapshot);
void reach_dock_item_x_rebind(reach_dock *dock, const reach_theme *theme,
                              const reach_dock_layout *layout,
                              const reach_dock_item_x_snapshot *snapshot);

/* Visible now-playing content width: slot 0's animated width minus its
   trailing gap, clamped to zero. */
float reach_dock_now_playing_reveal_width(reach_dock *dock, float scaled_gap);

#endif
