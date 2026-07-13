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

    /* The media-controls port is borrowed and must outlive the service. Notify
     * runs on a service worker after a new generation is published; consumers
     * wake their owning loop and pull the snapshot. Core metadata/playback and
     * controls publish atomically. Cover/accent may publish later. A transition
     * from media to no media retains the last snapshot for four seconds and is
     * confirmed by a fresh read before publishing disappearance. Cover loading
     * waits for a 300-millisecond quiet period on the latest media generation
     * so transient provider thumbnails are not published. */
    reach_result reach_now_playing_service_create(reach_media_controls_port media_controls,
                                                  void (*notify)(void *user), void *notify_user,
                                                  reach_now_playing_service **out_service);
    void reach_now_playing_service_destroy(reach_now_playing_service *service);
    reach_result reach_now_playing_service_start(reach_now_playing_service *service);
    void reach_now_playing_service_stop(reach_now_playing_service *service);

    void reach_now_playing_service_snapshot(const reach_now_playing_service *service,
                                            reach_now_playing_snapshot *out_snapshot);

    /* Accepts exactly one supported action while idle. An accepted action
     * immediately publishes every transport control disabled, executes on the
     * worker, and remains pending until both 300 milliseconds and a stable core
     * refresh have completed. Cover loading never gates transport. Requests
     * made while pending are rejected. */
    int32_t reach_now_playing_service_try_action(reach_now_playing_service *service,
                                                 reach_now_playing_action action);

    /* Replaced covers stay retained until composition evicts the id from every
     * renderer. take transfers that retained reference to the caller; release
     * drops it through the media port after eviction. */
    int32_t reach_now_playing_service_take_retired_cover(reach_now_playing_service *service,
                                                         uint64_t *out_cover_image_id);
    void reach_now_playing_service_release_cover(reach_now_playing_service *service,
                                                 uint64_t cover_image_id);

#ifdef __cplusplus
}
#endif

#endif
