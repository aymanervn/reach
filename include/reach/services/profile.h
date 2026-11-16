#ifndef REACH_SERVICES_PROFILE_H
#define REACH_SERVICES_PROFILE_H

#include <stdint.h>

#include "reach/ports/image_loader.h"
#include "reach/ports/user_account.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_PROFILE_PATH_CAPACITY 260

    typedef struct reach_profile_service reach_profile_service;

    typedef struct reach_profile_snapshot
    {
        uint16_t display_name[REACH_USER_ACCOUNT_NAME_CAPACITY];
        uint64_t picture_image_id;
        uint64_t wallpaper_image_id;
    } reach_profile_snapshot;

    reach_result reach_profile_service_create(reach_user_account_port account,
                                              const reach_image_loader_port *images,
                                              void (*notify)(void *user), void *notify_user,
                                              reach_profile_service **out_service);
    void reach_profile_service_stop(reach_profile_service *service);
    void reach_profile_service_destroy(reach_profile_service *service);

    void reach_profile_service_request(reach_profile_service *service,
                                       const uint16_t *wallpaper_path, int32_t width_px,
                                       int32_t height_px);
    int32_t reach_profile_service_take(reach_profile_service *service,
                                       reach_profile_snapshot *out_snapshot);
    int32_t reach_profile_service_take_retired_image(reach_profile_service *service,
                                                     uint64_t *out_image_id);
    void reach_profile_service_release_image(reach_profile_service *service, uint64_t image_id);

#ifdef __cplusplus
}
#endif

#endif
