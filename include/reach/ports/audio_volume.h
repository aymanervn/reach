#ifndef REACH_PORTS_AUDIO_VOLUME_H
#define REACH_PORTS_AUDIO_VOLUME_H

#include "reach/support/util.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_audio_volume_state {
    float level;
    int32_t muted;
} reach_audio_volume_state;

typedef struct reach_audio_volume_port {
    void *userdata;

    reach_result (*get_state)(
        void *userdata,
        reach_audio_volume_state *out_state
    );

    reach_result (*set_level)(
        void *userdata,
        float level
    );

    reach_result (*set_muted)(
        void *userdata,
        int32_t muted
    );

    void (*destroy)(
        void *userdata
    );
} reach_audio_volume_port;

#ifdef __cplusplus
}
#endif

#endif
