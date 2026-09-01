#include "reach/features/launcher.h"

#include "launcher_common.h"

reach_launcher_action reach_launcher_action_for_hit(const reach_launcher_model *model,
                                                    reach_launcher_hit_result hit)
{
    reach_launcher_action action = {};
    action.type = REACH_LAUNCHER_ACTION_NONE;
    if (model == nullptr)
    {
        return action;
    }
    if (hit.type == REACH_LAUNCHER_HIT_SEARCH_RESULT && hit.index < model->result_count)
    {
        action.type = REACH_LAUNCHER_ACTION_OPEN_RESULT;
    }
    return action;
}
