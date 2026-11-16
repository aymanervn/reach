#ifndef REACH_SERVICES_PROFILE_H
#define REACH_SERVICES_PROFILE_H

#include <stdint.h>

#include "reach/ports/user_account.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_profile_service reach_profile_service;

    typedef struct reach_profile_snapshot
    {
        uint16_t display_name[REACH_USER_ACCOUNT_NAME_CAPACITY];
        uint64_t picture_image_id;
    } reach_profile_snapshot;

    reach_result reach_profile_service_create(reach_user_account_port account,
                                              void (*notify)(void *user), void *notify_user,
                                              reach_profile_service **out_service);
    void reach_profile_service_stop(reach_profile_service *service);
    void reach_profile_service_destroy(reach_profile_service *service);

    int32_t reach_profile_service_take(reach_profile_service *service,
                                       reach_profile_snapshot *out_snapshot);

#ifdef __cplusplus
}
#endif

#endif
