#ifndef REACH_SERVICES_IDLE_WATCH_H
#define REACH_SERVICES_IDLE_WATCH_H

#include <stdint.h>

#include "reach/ports/power_session.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_idle_watch_action
    {
        REACH_IDLE_WATCH_ACTION_SLEEP = 0,
        REACH_IDLE_WATCH_ACTION_LOCK = 1,
        REACH_IDLE_WATCH_ACTION_SHUTDOWN = 2,
        REACH_IDLE_WATCH_ACTION_RESTART = 3,
        REACH_IDLE_WATCH_ACTION_COUNT = 4
    } reach_idle_watch_action;

    typedef struct reach_idle_watch_config
    {
        int32_t timeout_minutes[REACH_IDLE_WATCH_ACTION_COUNT];
        int32_t wait_awake_apps[REACH_IDLE_WATCH_ACTION_COUNT];
    } reach_idle_watch_config;

    typedef struct reach_idle_watch_sample
    {
        uint64_t now_milliseconds;
        uint64_t input_idle_milliseconds;
        int32_t awake_required;
    } reach_idle_watch_sample;

    typedef struct reach_idle_watch_state
    {
        uint64_t baseline_milliseconds;
        uint64_t last_sample_milliseconds;
        uint64_t last_idle_milliseconds;
        int32_t fired[REACH_IDLE_WATCH_ACTION_COUNT];
    } reach_idle_watch_state;

    typedef struct reach_idle_watch reach_idle_watch;

    void reach_idle_watch_state_init(reach_idle_watch_state *state, uint64_t now_milliseconds);
    uint32_t reach_idle_watch_evaluate(reach_idle_watch_state *state,
                                       const reach_idle_watch_config *config,
                                       const reach_idle_watch_sample *sample,
                                       uint64_t poll_interval_milliseconds);

    reach_result reach_idle_watch_create(reach_power_session_port power_session,
                                         reach_idle_watch **out_service);
    void reach_idle_watch_set_config(reach_idle_watch *service,
                                     const reach_idle_watch_config *config);
    void reach_idle_watch_stop(reach_idle_watch *service);
    void reach_idle_watch_destroy(reach_idle_watch *service);

#ifdef __cplusplus
}
#endif

#endif
