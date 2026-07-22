#ifndef REACH_PORTS_APP_UPDATE_H
#define REACH_PORTS_APP_UPDATE_H

#include "reach/core/app_update.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void (*reach_app_update_download_progress)(void *user, uint64_t received,
                                                       uint64_t total);

    typedef struct reach_app_update_port
    {
        void *userdata;
        reach_result (*check)(void *userdata, const uint16_t *owner, const uint16_t *repo,
                              reach_app_update_info *out_info);
        reach_result (*download)(void *userdata, const uint16_t *url, const uint16_t *dest_path,
                                 reach_app_update_download_progress progress, void *progress_user);
        void (*cancel)(void *userdata);
        void (*destroy)(void *userdata);
    } reach_app_update_port;

#ifdef __cplusplus
}
#endif
#endif
