#ifndef REACH_SERVICES_TRAY_H
#define REACH_SERVICES_TRAY_H

#include "reach/ports/tray_provider.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_tray_service reach_tray_service;

    reach_result reach_tray_service_create(reach_tray_provider_port provider,
                                           reach_tray_service **out_service);
    void reach_tray_service_destroy(reach_tray_service *service);

    reach_result reach_tray_service_refresh(reach_tray_service *service);
    int32_t reach_tray_service_needs_refresh(const reach_tray_service *service);
    size_t reach_tray_service_item_count(const reach_tray_service *service);
    const reach_tray_item *reach_tray_service_item_at(const reach_tray_service *service,
                                                      size_t index);
    reach_result reach_tray_service_activate(reach_tray_service *service, uint32_t item_id,
                                             reach_tray_action action);
    int32_t reach_tray_service_take_retired_icon(reach_tray_service *service,
                                                 uint64_t *out_icon_id);
    void reach_tray_service_release_retired_icon(reach_tray_service *service, uint64_t icon_id);

#ifdef __cplusplus
}
#endif

#endif
