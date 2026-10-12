#ifndef REACH_CORE_LOADER_H
#define REACH_CORE_LOADER_H

#include <stdint.h>

#include "reach/core/geometry.h"
#include "reach/support/motion.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_LOADER_MINIMUM_WIDTH_RATIO 0.1f

    typedef struct reach_loader_model
    {
        reach_motion width;
        reach_motion x;
    } reach_loader_model;

    void reach_loader_model_init(reach_loader_model *model, float phase_seconds);
    void reach_loader_model_init_at(reach_loader_model *model, float phase_seconds,
                                    double now_seconds);
    void reach_loader_model_reset(reach_loader_model *model);
    void reach_loader_model_reset_at(reach_loader_model *model, double now_seconds);
    reach_rect_f32 reach_loader_bar_rect(const reach_loader_model *model, reach_rect_f32 container);
    reach_rect_f32 reach_loader_bar_rect_at(const reach_loader_model *model,
                                            reach_rect_f32 container, double now_seconds);

#ifdef __cplusplus
}
#endif

#endif
