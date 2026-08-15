#ifndef REACH_CORE_LOADER_H
#define REACH_CORE_LOADER_H

#include <stdint.h>

#include "reach/core/geometry.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_LOADER_MINIMUM_WIDTH_RATIO 0.1f

    typedef enum reach_loader_phase
    {
        REACH_LOADER_PHASE_GROW = 0,
        REACH_LOADER_PHASE_SLIDE_OUT,
        REACH_LOADER_PHASE_SLIDE_BACK,
        REACH_LOADER_PHASE_SHRINK
    } reach_loader_phase;

    typedef struct reach_loader_model
    {
        reach_loader_phase phase;
        float phase_progress;
        float phase_seconds;
    } reach_loader_model;

    void reach_loader_model_init(reach_loader_model *model, float phase_seconds);
    void reach_loader_model_reset(reach_loader_model *model);
    int32_t reach_loader_update(reach_loader_model *model, double delta_seconds);
    reach_rect_f32 reach_loader_bar_rect(const reach_loader_model *model, reach_rect_f32 container);

#ifdef __cplusplus
}
#endif

#endif
