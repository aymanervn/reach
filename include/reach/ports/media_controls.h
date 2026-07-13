#ifndef REACH_PORTS_MEDIA_CONTROLS_H
#define REACH_PORTS_MEDIA_CONTROLS_H

#include "reach/core/media_controls.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void (*reach_media_controls_change_callback)(void *user);

    typedef struct reach_media_controls_port
    {
        void *userdata;

        reach_result (*get_state)(void *userdata, reach_media_controls_state *out_state);

        reach_result (*get_cover)(void *userdata, uint64_t media_generation,
                                  reach_media_cover *out_cover);
        void (*release_cover)(void *userdata, uint64_t cover_icon_id);
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
