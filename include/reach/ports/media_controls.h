#ifndef REACH_PORTS_MEDIA_CONTROLS_H
#define REACH_PORTS_MEDIA_CONTROLS_H

#include "reach/core/geometry.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_media_playback_state
    {
        REACH_MEDIA_PLAYBACK_UNKNOWN = 0,
        REACH_MEDIA_PLAYBACK_STOPPED = 1,
        REACH_MEDIA_PLAYBACK_PAUSED = 2,
        REACH_MEDIA_PLAYBACK_PLAYING = 3
    } reach_media_playback_state;

    typedef void (*reach_media_controls_change_callback)(void *user);

    typedef struct reach_media_controls_state
    {
        int32_t has_media;
        uint16_t title[260];
        uint16_t artist[260];
        uint64_t cover_icon_id;
        reach_color cover_accent;
        reach_media_playback_state playback;
        int32_t previous_enabled;
        int32_t next_enabled;
    } reach_media_controls_state;

    typedef struct reach_media_controls_port
    {
        void *userdata;

        reach_result (*get_state)(void *userdata, reach_media_controls_state *out_state);
        reach_result (*previous_track)(void *userdata);
        reach_result (*play_pause)(void *userdata);
        reach_result (*next_track)(void *userdata);

        reach_result (*start_watching)(void *userdata,
                                       reach_media_controls_change_callback callback,
                                       void *callback_user);

        void (*stop_watching)(void *userdata);

        void (*destroy)(void *userdata);
    } reach_media_controls_port;

#ifdef __cplusplus
}
#endif

#endif
