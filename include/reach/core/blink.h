#ifndef REACH_CORE_BLINK_H
#define REACH_CORE_BLINK_H

#include <stdint.h>
#include "reach/support/motion.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_blink
    {
        double started_seconds;
        double deadline_seconds;
        double visible_seconds;
        double hidden_seconds;
        int32_t visible;
        uint64_t generation;
    } reach_blink;

    void reach_blink_reset_at(reach_blink *blink, double visible_seconds, double hidden_seconds,
                              double now_seconds);
    reach_motion reach_blink_opacity(const reach_blink *blink);
    void reach_blink_stop(reach_blink *blink);
    int32_t reach_blink_update_at(reach_blink *blink, double now_seconds);

#ifdef __cplusplus
}
#endif

#endif
