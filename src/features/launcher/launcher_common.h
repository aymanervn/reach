#ifndef REACH_FEATURES_LAUNCHER_COMMON_H
#define REACH_FEATURES_LAUNCHER_COMMON_H

#include "reach/features/launcher.h"

/*
 * Feature-internal API. The public header targets the capsule; these
 * state-based versions are what the capsule wrappers and the launcher's own
 * interaction/render code use. The public reach_launcher_state_ptr() is
 * const; nothing outside src/features/launcher mutates launcher state.
 */

reach_launcher_state *reach_launcher_state_mut(reach_launcher *launcher);
reach_icon_service *reach_launcher_icons(reach_launcher *launcher);

typedef enum reach_launcher_hit_type
{
    REACH_LAUNCHER_HIT_NONE = 0,
    REACH_LAUNCHER_HIT_PINNED_APP = 1,
    REACH_LAUNCHER_HIT_SEARCH_RESULT = 2,
    REACH_LAUNCHER_HIT_SCROLLBAR_TRACK = 3,
    REACH_LAUNCHER_HIT_SCROLLBAR_THUMB = 4
} reach_launcher_hit_type;

typedef struct reach_launcher_hit_result
{
    reach_launcher_hit_type type;
    size_t index;
} reach_launcher_hit_result;

typedef enum reach_launcher_action_type
{
    REACH_LAUNCHER_ACTION_NONE = 0,
    REACH_LAUNCHER_ACTION_LAUNCH_PINNED = 1,
    REACH_LAUNCHER_ACTION_OPEN_RESULT = 2
} reach_launcher_action_type;

typedef struct reach_launcher_action
{
    reach_launcher_action_type type;
    size_t pinned_index;
    uint32_t pin_id;
} reach_launcher_action;

typedef struct reach_launcher_event_context
{
    const reach_launcher_layout *layout;
    const reach_pinned_app_model *pinned_apps;
    size_t pinned_app_count;
} reach_launcher_event_context;

typedef struct reach_launcher_event_result
{
    int32_t handled;
    int32_t redraw;
    int32_t viewport_changed;
    int32_t capture_pointer;
    int32_t sync_pointer_subscriptions;
    reach_launcher_action action;
} reach_launcher_event_result;

reach_launcher_hit_result reach_launcher_hit_test(const reach_launcher_model *model,
                                                  const reach_launcher_layout *layout, int32_t x,
                                                  int32_t y);
reach_launcher_action reach_launcher_action_for_hit(const reach_launcher_model *model,
                                                    const reach_pinned_app_model *pinned_apps,
                                                    size_t pinned_app_count,
                                                    reach_launcher_hit_result hit);
void reach_launcher_pointer_down(reach_launcher *launcher, int32_t x, int32_t y,
                                 const reach_launcher_event_context *ctx,
                                 reach_launcher_event_result *out);
void reach_launcher_pointer_up(reach_launcher *launcher, int32_t x, int32_t y,
                               const reach_launcher_event_context *ctx,
                               reach_launcher_event_result *out);
void reach_launcher_scrollbar_drag_move(reach_launcher *launcher, int32_t y,
                                        const reach_launcher_event_context *ctx,
                                        reach_launcher_event_result *out);
void reach_launcher_scrollbar_release(reach_launcher *launcher,
                                      reach_launcher_event_result *out);
void reach_launcher_wheel(reach_launcher *launcher, int32_t x, int32_t y, int32_t wheel_delta,
                          const reach_launcher_event_context *ctx,
                          reach_launcher_event_result *out);

void reach_launcher_state_init(reach_launcher_state *state);
reach_result reach_launcher_open_state(reach_launcher_state *state);
reach_result reach_launcher_close_state(reach_launcher_state *state);
reach_result reach_launcher_toggle_state(reach_launcher_state *state);
reach_result reach_launcher_set_query_state(reach_launcher_state *state, const uint16_t *query);
reach_result reach_launcher_set_results_state(reach_launcher_state *state,
                                              const reach_search_candidate *results, size_t count);
reach_result reach_launcher_set_selected_result_state(reach_launcher_state *state, size_t index);
reach_result reach_launcher_select_next_result_state(reach_launcher_state *state);
reach_result reach_launcher_select_previous_result_state(reach_launcher_state *state);
reach_result reach_launcher_scroll_results_state(reach_launcher_state *state, int32_t delta);
reach_result reach_launcher_set_result_scroll_offset_state(reach_launcher_state *state,
                                                           size_t offset);
size_t reach_launcher_result_scroll_offset_state(const reach_launcher_state *state);
reach_result reach_launcher_clear_results_state(reach_launcher_state *state);
int32_t reach_launcher_should_show_pinned_apps_state(const reach_launcher_state *state);
reach_result reach_launcher_handle_event_state(reach_launcher_state *state,
                                               const reach_ui_event *event,
                                               reach_ui_intent *out_intent);

#endif
