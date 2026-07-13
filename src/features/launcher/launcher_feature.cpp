#include "reach/features/launcher.h"

#include "launcher_common.h"

reach_launcher_action reach_launcher_action_for_hit(const reach_launcher_model *model,
                                                    const reach_pinned_app_model *pinned_apps,
                                                    size_t pinned_app_count,
                                                    reach_launcher_hit_result hit)
{
    reach_launcher_action action = {};
    action.type = REACH_LAUNCHER_ACTION_NONE;
    action.pinned_index = hit.index;
    if (model == nullptr)
    {
        return action;
    }
    if (hit.type == REACH_LAUNCHER_HIT_PINNED_APP && pinned_apps != nullptr &&
        hit.index < pinned_app_count)
    {
        action.type = REACH_LAUNCHER_ACTION_LAUNCH_PINNED;
        action.pin_id = pinned_apps[hit.index].id;
        return action;
    }
    if (hit.type == REACH_LAUNCHER_HIT_SEARCH_RESULT && hit.index < model->result_count)
    {
        action.type = REACH_LAUNCHER_ACTION_OPEN_RESULT;
    }
    return action;
}
