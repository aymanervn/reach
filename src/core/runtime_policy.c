#include "reach/core/runtime_policy.h"

void reach_runtime_policy_init(reach_runtime_policy_state *state)
{
    if (state == 0)
    {
        return;
    }

    state->game_mode_enabled = 0;
}

void reach_runtime_policy_set_game_mode(reach_runtime_policy_state *state, int32_t enabled)
{
    if (state == 0)
    {
        return;
    }

    state->game_mode_enabled = enabled ? 1 : 0;
}

int32_t reach_runtime_policy_game_mode_enabled(const reach_runtime_policy_state *state)
{
    return state != 0 && state->game_mode_enabled;
}
