#include "reach/features/launcher.h"

reach_launcher_action reach_launcher_action_for_hit(const reach_ui_state *state, reach_launcher_hit_result hit)
{
    reach_launcher_action action = {};
    action.type = REACH_LAUNCHER_ACTION_NONE;
    action.pinned_index = hit.index;
    if (state == nullptr) {
        return action;
    }
    if (hit.type == REACH_LAUNCHER_HIT_PINNED_APP && hit.index < state->pinned_app_count) {
        action.type = REACH_LAUNCHER_ACTION_LAUNCH_PINNED;
        action.pin_id = state->pinned_apps[hit.index].id;
        return action;
    }
    if (hit.type == REACH_LAUNCHER_HIT_SEARCH_RESULT && hit.index < state->launcher.result_count) {
        action.type = REACH_LAUNCHER_ACTION_OPEN_RESULT;
    }
    return action;
}
