#ifndef REACH_PORTS_MEDIA_CONTROLS_H
#define REACH_PORTS_MEDIA_CONTROLS_H

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_media_controls_port
    {
        void *userdata;

        reach_result (*previous_track)(void *userdata);
        reach_result (*play_pause)(void *userdata);
        reach_result (*next_track)(void *userdata);

        void (*destroy)(void *userdata);
    } reach_media_controls_port;

#ifdef __cplusplus
}
#endif

#endif
