#ifndef REACH_SERVICES_SEARCH_H
#define REACH_SERVICES_SEARCH_H

#include <stdint.h>

#include "reach/core/ui_state.h"
#include "reach/ports/search_provider.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_search_service reach_search_service;

    reach_result reach_search_service_create(reach_search_provider_port provider,
                                             void (*notify)(void *user), void *notify_user,
                                             reach_search_service **out_service);
    void reach_search_service_destroy(reach_search_service *service);

    void reach_search_service_stop(reach_search_service *service);

    reach_result reach_search_service_submit(reach_search_service *service, const uint16_t *query,
                                             uint32_t generation);

    void reach_search_service_cancel(reach_search_service *service);

    int32_t reach_search_service_take_completed(reach_search_service *service,
                                                uint32_t *out_generation,
                                                reach_search_candidate *out_results,
                                                size_t *out_count);
    int32_t reach_search_service_work_pending(const reach_search_service *service);

#ifdef __cplusplus
}
#endif

#endif
