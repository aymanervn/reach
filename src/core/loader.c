#include "reach/core/loader.h"

#include <stddef.h>

static float reach_loader_ease(float t)
{
    if (t <= 0.0f)
    {
        return 0.0f;
    }
    if (t >= 1.0f)
    {
        return 1.0f;
    }
    if (t < 0.5f)
    {
        return 4.0f * t * t * t;
    }
    float remaining = -2.0f * t + 2.0f;
    return 1.0f - remaining * remaining * remaining * 0.5f;
}

static reach_loader_phase reach_loader_next_phase(reach_loader_phase phase)
{
    switch (phase)
    {
    case REACH_LOADER_PHASE_GROW:
        return REACH_LOADER_PHASE_SLIDE_OUT;
    case REACH_LOADER_PHASE_SLIDE_OUT:
        return REACH_LOADER_PHASE_SLIDE_BACK;
    case REACH_LOADER_PHASE_SLIDE_BACK:
        return REACH_LOADER_PHASE_SHRINK;
    case REACH_LOADER_PHASE_SHRINK:
    default:
        return REACH_LOADER_PHASE_GROW;
    }
}

void reach_loader_model_init(reach_loader_model *model, float phase_seconds)
{
    if (model == NULL)
    {
        return;
    }
    model->phase = REACH_LOADER_PHASE_GROW;
    model->phase_progress = 0.0f;
    model->phase_seconds = phase_seconds > 0.0f ? phase_seconds : 1.0f;
}

void reach_loader_model_reset(reach_loader_model *model)
{
    if (model == NULL)
    {
        return;
    }
    model->phase = REACH_LOADER_PHASE_GROW;
    model->phase_progress = 0.0f;
}

int32_t reach_loader_update(reach_loader_model *model, double delta_seconds)
{
    if (model == NULL || model->phase_seconds <= 0.0f || delta_seconds <= 0.0)
    {
        return 0;
    }

    const float maximum_advance = 4.0f;
    float advance = (float)(delta_seconds / (double)model->phase_seconds);
    if (advance > maximum_advance)
    {
        advance = maximum_advance;
    }

    model->phase_progress += advance;
    while (model->phase_progress >= 1.0f)
    {
        model->phase_progress -= 1.0f;
        model->phase = reach_loader_next_phase(model->phase);
    }
    return 1;
}

reach_rect_f32 reach_loader_bar_rect(const reach_loader_model *model, reach_rect_f32 container)
{
    reach_rect_f32 bar = container;
    if (model == NULL || container.width <= 0.0f)
    {
        bar.width = 0.0f;
        return bar;
    }

    float full = container.width;
    float minimum = full * REACH_LOADER_MINIMUM_WIDTH_RATIO;
    float maximum_origin = full - minimum;
    float t = reach_loader_ease(model->phase_progress);
    float origin = 0.0f;
    float width = full;

    switch (model->phase)
    {
    case REACH_LOADER_PHASE_GROW:
        origin = 0.0f;
        width = minimum + (full - minimum) * t;
        break;
    case REACH_LOADER_PHASE_SLIDE_OUT:
        origin = maximum_origin * t;
        width = full - origin;
        break;
    case REACH_LOADER_PHASE_SLIDE_BACK:
        origin = maximum_origin * (1.0f - t);
        width = full - origin;
        break;
    case REACH_LOADER_PHASE_SHRINK:
    default:
        origin = 0.0f;
        width = full - (full - minimum) * t;
        break;
    }

    bar.x = container.x + origin;
    bar.width = width;
    return bar;
}
