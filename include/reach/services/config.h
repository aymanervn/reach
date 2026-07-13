#ifndef REACH_SERVICES_CONFIG_H
#define REACH_SERVICES_CONFIG_H

#include <stdint.h>

#include "reach/ports/config_store.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_config_service reach_config_service;

    reach_result reach_config_service_create(reach_config_store_port store,
                                             void (*notify)(void *user), void *notify_user,
                                             reach_config_service **out_service);
    void reach_config_service_destroy(reach_config_service *service);

    void reach_config_service_stop(reach_config_service *service);

    reach_result reach_config_service_schedule_reload(reach_config_service *service);
    reach_result reach_config_service_schedule_pin_app(reach_config_service *service,
                                                       const reach_pinned_app_model *app);
    reach_result reach_config_service_schedule_unpin(reach_config_service *service,
                                                     uint32_t pin_id);
    reach_result reach_config_service_schedule_move_pin(reach_config_service *service,
                                                        uint32_t pin_id, size_t target_index);

    int32_t reach_config_service_take_completed(reach_config_service *service,
                                                reach_result *out_result,
                                                reach_config_snapshot *out_snapshot,
                                                int32_t *out_current);
    int32_t reach_config_service_work_pending(const reach_config_service *service);

#ifdef __cplusplus
}
#endif

#endif
