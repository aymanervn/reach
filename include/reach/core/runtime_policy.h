#ifndef REACH_CORE_RUNTIME_POLICY_H
#define REACH_CORE_RUNTIME_POLICY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_runtime_policy_state
    {
        int32_t game_mode_enabled;
    } reach_runtime_policy_state;

    void reach_runtime_policy_init(reach_runtime_policy_state *state);
    void reach_runtime_policy_set_game_mode(reach_runtime_policy_state *state, int32_t enabled);
    int32_t reach_runtime_policy_game_mode_enabled(const reach_runtime_policy_state *state);

#ifdef __cplusplus
}
#endif

#endif
