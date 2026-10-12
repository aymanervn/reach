#include "reach/core/loader.h"
#include <math.h>

void reach_loader_model_init(reach_loader_model *model, float phase_seconds)
{
    reach_loader_model_init_at(model, phase_seconds, reach_motion_now_seconds());
}

void reach_loader_model_init_at(reach_loader_model *model, float phase_seconds, double now_seconds)
{
    if (model == 0)
        return;
    *model = (reach_loader_model){0};
    model->width.duration_seconds =
        isfinite(phase_seconds) && phase_seconds > 0 ? phase_seconds : 1;
    reach_loader_model_reset_at(model, now_seconds);
}

void reach_loader_model_reset(reach_loader_model *model)
{
    reach_loader_model_reset_at(model, reach_motion_now_seconds());
}

void reach_loader_model_reset_at(reach_loader_model *model, double now_seconds)
{
    if (model == 0)
        return;
    double duration = model->width.duration_seconds > 0 ? model->width.duration_seconds : 1;
    reach_motion_repeat_at(&model->width, REACH_LOADER_MINIMUM_WIDTH_RATIO, 1, duration, 0, 0,
                           REACH_EASING_EASE_IN_OUT, now_seconds);
    reach_motion_repeat_at(&model->x, 0, 1 - REACH_LOADER_MINIMUM_WIDTH_RATIO, duration,
                           duration * 2, 0, REACH_EASING_EASE_IN_OUT, now_seconds - duration);
}

reach_rect_f32 reach_loader_bar_rect(const reach_loader_model *model, reach_rect_f32 container)
{
    return reach_loader_bar_rect_at(model, container, reach_motion_now_seconds());
}

reach_rect_f32 reach_loader_bar_rect_at(const reach_loader_model *model, reach_rect_f32 container,
                                        double now_seconds)
{
    reach_rect_f32 bar = container;
    if (model == 0 || container.width <= 0)
    {
        bar.width = 0;
        return bar;
    }
    bar.x += container.width * reach_motion_value_at(&model->x, now_seconds);
    bar.width *= reach_motion_value_at(&model->width, now_seconds);
    return bar;
}
