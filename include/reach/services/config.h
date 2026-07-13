#ifndef REACH_SERVICES_CONFIG_H
#define REACH_SERVICES_CONFIG_H

#include <stdint.h>

#include "reach/ports/config_store.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * Config service — the async reload + pin-persistence worker over the
     * config-store port (store I/O can block, so one worker serializes reload
     * / pin / unpin / move-pin jobs; mutating jobs reload afterwards so the
     * completed snapshot is always current). Latest-wins: take_completed only
     * returns a snapshot no newer job superseded. `notify` fires on the worker
     * thread after a job completes. The port is borrowed.
     */
    typedef struct reach_config_service reach_config_service;

    reach_result reach_config_service_create(reach_config_store_port store,
                                             void (*notify)(void *user), void *notify_user,
                                             reach_config_service **out_service);
    void reach_config_service_destroy(reach_config_service *service);
    /* Join the worker early (shutdown ordering); idempotent. */
    void reach_config_service_stop(reach_config_service *service);

    reach_result reach_config_service_schedule_reload(reach_config_service *service);
    reach_result reach_config_service_schedule_pin_app(reach_config_service *service,
                                                       const reach_pinned_app_model *app);
    reach_result reach_config_service_schedule_unpin(reach_config_service *service,
                                                     uint32_t pin_id);
    reach_result reach_config_service_schedule_move_pin(reach_config_service *service,
                                                        uint32_t pin_id, size_t target_index);

    /* Returns 1 when a completed job was consumed; *out_current is 0 when a
       newer job superseded it (the caller drops the snapshot but must still
       treat the completion as handled). */
    int32_t reach_config_service_take_completed(reach_config_service *service,
                                                reach_result *out_result,
                                                reach_config_snapshot *out_snapshot,
                                                int32_t *out_current);
    int32_t reach_config_service_work_pending(const reach_config_service *service);

#ifdef __cplusplus
}
#endif

#endif
