#ifndef REACH_SERVICES_INPUT_LANGUAGE_H
#define REACH_SERVICES_INPUT_LANGUAGE_H

#include <stdint.h>

#include "reach/core/window_id.h"
#include "reach/ports/input_language.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_input_language_service reach_input_language_service;

    typedef struct reach_input_language_snapshot
    {
        uint16_t code[8];
        int32_t valid;
    } reach_input_language_snapshot;

    reach_result reach_input_language_service_create(reach_input_language_port source,
                                                     reach_input_language_service **out_service);
    void reach_input_language_service_destroy(reach_input_language_service *service);

    int32_t reach_input_language_service_refresh(reach_input_language_service *service,
                                                 reach_window_id foreground_window);
    int32_t reach_input_language_service_settling(const reach_input_language_service *service);
    int32_t reach_input_language_service_tick_settle(reach_input_language_service *service,
                                                     double delta_seconds,
                                                     reach_window_id foreground_window);
    reach_result reach_input_language_service_cycle_next(reach_input_language_service *service,
                                                         reach_window_id foreground_window);
    void reach_input_language_service_snapshot_take(const reach_input_language_service *service,
                                                    reach_input_language_snapshot *out_snapshot);

#ifdef __cplusplus
}
#endif

#endif
