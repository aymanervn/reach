#ifndef REACH_FEATURES_COMMON_LAYOUT_H
#define REACH_FEATURES_COMMON_LAYOUT_H

#include <stddef.h>
#include <stdint.h>

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_LAYOUT_MAX_PARTICIPANTS 16

    typedef enum reach_layout_condition
    {
        REACH_LAYOUT_CONDITION_GAME_MODE = 0,
        REACH_LAYOUT_CONDITION_TOP_BAR_REVEALED = 1,
        REACH_LAYOUT_CONDITION_BARS_FORCED = 2,
        REACH_LAYOUT_CONDITION_BARS_HELD = 3,
        REACH_LAYOUT_CONDITION_COUNT = 4
    } reach_layout_condition;

    typedef uint32_t reach_layout_participant;

    typedef struct reach_layout_participant_state
    {
        int32_t base_layer;
        int32_t wants_visible;
        uint32_t layer_conditions;
        int32_t layer_overrides[REACH_LAYOUT_CONDITION_COUNT];
        uint32_t visibility_conditions;
        int32_t visibility_overrides[REACH_LAYOUT_CONDITION_COUNT];
    } reach_layout_participant_state;

    typedef struct reach_layout
    {
        reach_layout_participant_state participants[REACH_LAYOUT_MAX_PARTICIPANTS];
        size_t participant_count;
        uint32_t active_conditions;
    } reach_layout;

    typedef struct reach_layout_entry
    {
        reach_layout_participant participant;
        int32_t layer;
        int32_t visible;
    } reach_layout_entry;

    typedef struct reach_layout_plan
    {
        reach_layout_entry entries[REACH_LAYOUT_MAX_PARTICIPANTS];
        size_t count;
    } reach_layout_plan;

    reach_result reach_layout_register(reach_layout *layout, int32_t base_layer,
                                       reach_layout_participant *out_participant);
    reach_result reach_layout_register_override(reach_layout *layout,
                                                reach_layout_participant participant,
                                                reach_layout_condition condition, int32_t layer);
    reach_result reach_layout_register_visibility(reach_layout *layout,
                                                  reach_layout_participant participant,
                                                  reach_layout_condition condition,
                                                  int32_t visible);

    void reach_layout_set_condition(reach_layout *layout, reach_layout_condition condition,
                                    int32_t active);
    void reach_layout_set_visible(reach_layout *layout, reach_layout_participant participant,
                                  int32_t wants_visible);

    void reach_layout_resolve(const reach_layout *layout, reach_layout_plan *out_plan);
    int32_t reach_layout_plan_equal(const reach_layout_plan *left, const reach_layout_plan *right);

#ifdef __cplusplus
}
#endif

#endif
