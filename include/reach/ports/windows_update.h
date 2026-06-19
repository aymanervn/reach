#ifndef REACH_PORTS_WINDOWS_UPDATE_H
#define REACH_PORTS_WINDOWS_UPDATE_H

#include "reach/core/windows_update.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_windows_update_progress
    {
        REACH_WINDOWS_UPDATE_PROGRESS_DOWNLOADING = 1,
        REACH_WINDOWS_UPDATE_PROGRESS_INSTALLING = 2,
        REACH_WINDOWS_UPDATE_PROGRESS_VERIFYING = 3
    } reach_windows_update_progress;

    typedef void (*reach_windows_update_progress_callback)(void *user,
                                                           reach_windows_update_progress progress);

    typedef struct reach_windows_update_port
    {
        void *userdata;
        reach_result (*scan)(void *userdata, reach_windows_update_list *out_updates,
                             int32_t *out_hresult);
        reach_result (*install)(void *userdata,
                                const reach_windows_update_identity *selected_updates,
                                size_t selected_update_count,
                                reach_windows_update_progress_callback progress,
                                void *progress_user,
                                reach_windows_update_operation_result *out_result);
        reach_result (*verify)(void *userdata,
                               const reach_windows_update_identity *installed_updates,
                               size_t installed_update_count,
                               reach_windows_update_operation_result *out_result);
        reach_result (*load_pending_verification)(void *userdata,
                                                  reach_windows_update_identity *out_updates,
                                                  size_t update_capacity, size_t *out_update_count);
        void (*cancel)(void *userdata);
        void (*destroy)(void *userdata);
    } reach_windows_update_port;

#ifdef __cplusplus
}
#endif
#endif
