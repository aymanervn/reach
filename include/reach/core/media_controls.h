#ifndef REACH_CORE_MEDIA_CONTROLS_H
#define REACH_CORE_MEDIA_CONTROLS_H

#include <stdint.h>

#include "reach/core/theme.h"

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

    typedef struct reach_media_controls_state
    {
        int32_t has_media;
        uint64_t media_generation;
        uint16_t title[260];
        uint16_t artist[260];
        reach_media_playback_state playback;
        int32_t previous_enabled;
        int32_t play_pause_enabled;
        int32_t next_enabled;
    } reach_media_controls_state;

    typedef struct reach_media_cover
    {
        uint64_t media_generation;
        uint64_t cover_icon_id;
        reach_color cover_accent;
        int32_t current;
    } reach_media_cover;

#ifdef __cplusplus
}
#endif

#endif
