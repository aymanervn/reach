#ifndef REACH_FEATURES_COMMON_MARQUEE_H
#define REACH_FEATURES_COMMON_MARQUEE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_marquee_state
    {
        double elapsed_seconds;
    } reach_marquee_state;

    typedef struct reach_marquee_request
    {
        float content_width;
        float viewport_width;
        double delta_seconds;
    } reach_marquee_request;

    void reach_marquee_reset(reach_marquee_state *state);

    int32_t reach_marquee_scrolls(const reach_marquee_request *request);

    float reach_marquee_advance(reach_marquee_state *state, const reach_marquee_request *request);

#ifdef __cplusplus
}
#endif

#endif
