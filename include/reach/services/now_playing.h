#ifndef REACH_SERVICES_NOW_PLAYING_H
#define REACH_SERVICES_NOW_PLAYING_H

#include <stdint.h>

#include "reach/core/media_controls.h"
#include "reach/ports/media_controls.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_now_playing_service reach_now_playing_service;

    typedef enum reach_now_playing_action
    {
        REACH_NOW_PLAYING_ACTION_NONE = 0,
        REACH_NOW_PLAYING_ACTION_PREVIOUS = 1,
        REACH_NOW_PLAYING_ACTION_PLAY_PAUSE = 2,
        REACH_NOW_PLAYING_ACTION_NEXT = 3
    } reach_now_playing_action;

    typedef struct reach_now_playing_snapshot
    {
        int32_t has_session;
        uint16_t title[260];
        uint16_t artist[260];
        uint64_t cover_image_id;
        reach_color cover_accent;
        reach_media_playback_state playback;
        int32_t previous_enabled;
        int32_t play_pause_enabled;
        int32_t next_enabled;
        int32_t transport_pending;
        uint64_t generation;
    } reach_now_playing_snapshot;

    reach_result reach_now_playing_service_create(reach_media_controls_port media_controls,
                                                  void (*notify)(void *user), void *notify_user,
                                                  reach_now_playing_service **out_service);
    void reach_now_playing_service_destroy(reach_now_playing_service *service);
    reach_result reach_now_playing_service_start(reach_now_playing_service *service);
    void reach_now_playing_service_stop(reach_now_playing_service *service);

    void reach_now_playing_service_snapshot(const reach_now_playing_service *service,
                                            reach_now_playing_snapshot *out_snapshot);

    int32_t reach_now_playing_service_try_action(reach_now_playing_service *service,
                                                 reach_now_playing_action action);

    int32_t reach_now_playing_service_take_retired_cover(reach_now_playing_service *service,
                                                         uint64_t *out_cover_image_id);
    void reach_now_playing_service_release_cover(reach_now_playing_service *service,
                                                 uint64_t cover_image_id);

#ifdef __cplusplus
}
#endif

#endif
