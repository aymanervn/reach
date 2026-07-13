#ifndef REACH_SERVICES_ICON_SERVICE_H
#define REACH_SERVICES_ICON_SERVICE_H

#include <stdint.h>

#include "reach/ports/icon_provider.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_icon_service reach_icon_service;

    reach_result reach_icon_service_create(reach_icon_provider_port provider,
                                           reach_icon_service **out_service);
    void reach_icon_service_destroy(reach_icon_service *service);

    void reach_icon_service_stop(reach_icon_service *service);

    void reach_icon_service_set_notify(reach_icon_service *service, void (*notify)(void *user),
                                       void *notify_user);

    uint64_t reach_icon_service_get(reach_icon_service *service, const uint16_t *path,
                                    int32_t size_px);

    void reach_icon_service_touch(reach_icon_service *service, const uint16_t *path,
                                  int32_t size_px);

    void reach_icon_service_trim(reach_icon_service *service, double max_age_seconds);
    size_t reach_icon_service_take_evicted(reach_icon_service *service, uint64_t *out_icon_ids,
                                           size_t capacity);

    int32_t reach_icon_service_take_loads_completed(reach_icon_service *service);

    int32_t reach_icon_service_work_pending(const reach_icon_service *service);

    void reach_icon_service_clear(reach_icon_service *service);

#ifdef __cplusplus
}
#endif

#endif
