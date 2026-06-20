#include "reach/core/scrollbar.h"

#include <math.h>

static float reach_scrollbar_clamp(float value, float minimum, float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    return value > maximum ? maximum : value;
}

static float reach_scrollbar_quantize(const reach_scrollbar_model *model, float value)
{
    if (model == nullptr || model->drag_mode != REACH_SCROLLBAR_DRAG_STEPPED || model->step <= 0.0f)
    {
        return value;
    }
    return floorf(value / model->step + 0.5f) * model->step;
}

void reach_scrollbar_model_init(reach_scrollbar_model *model, reach_scrollbar_drag_mode drag_mode,
                                float step)
{
    if (model == nullptr)
    {
        return;
    }
    *model = {};
    model->drag_mode = drag_mode;
    model->step = step;
}

void reach_scrollbar_set_extents(reach_scrollbar_model *model, float content_extent,
                                 float viewport_extent)
{
    if (model == nullptr)
    {
        return;
    }
    model->maximum = content_extent > viewport_extent ? content_extent - viewport_extent : 0.0f;
    model->target = reach_scrollbar_clamp(model->target, 0.0f, model->maximum);
    model->offset = reach_scrollbar_clamp(model->offset, 0.0f, model->maximum);
}

void reach_scrollbar_set_target(reach_scrollbar_model *model, float target)
{
    if (model == nullptr)
    {
        return;
    }
    target = reach_scrollbar_quantize(model, target);
    model->target = reach_scrollbar_clamp(target, 0.0f, model->maximum);
}

void reach_scrollbar_scroll(reach_scrollbar_model *model, float delta)
{
    if (model != nullptr && delta != 0.0f)
    {
        reach_scrollbar_set_target(model, model->target + delta);
    }
}

int32_t reach_scrollbar_update(reach_scrollbar_model *model, double delta_seconds)
{
    if (model == nullptr)
    {
        return 0;
    }
    float difference = model->target - model->offset;
    if (fabsf(difference) < 0.1f)
    {
        model->offset = model->target;
        return 0;
    }
    float blend = (float)(delta_seconds * 18.0);
    if (blend > 1.0f)
    {
        blend = 1.0f;
    }
    model->offset += difference * blend;
    return 1;
}

reach_scrollbar_layout reach_scrollbar_compute_layout(const reach_scrollbar_model *model,
                                                      reach_rect_f32 track, float viewport_extent,
                                                      float content_extent,
                                                      float minimum_thumb_extent)
{
    reach_scrollbar_layout layout = {};
    if (model == nullptr || track.height <= 0.0f || viewport_extent <= 0.0f ||
        content_extent <= viewport_extent)
    {
        return layout;
    }

    layout.track = track;
    float thumb_extent = track.height * viewport_extent / content_extent;
    if (thumb_extent < minimum_thumb_extent)
    {
        thumb_extent = minimum_thumb_extent;
    }
    if (thumb_extent > track.height)
    {
        thumb_extent = track.height;
    }
    float travel = track.height - thumb_extent;
    float progress = model->maximum > 0.0f ? model->offset / model->maximum : 0.0f;
    layout.thumb = {track.x, track.y + travel * reach_scrollbar_clamp(progress, 0.0f, 1.0f),
                    track.width, thumb_extent};
    return layout;
}

void reach_scrollbar_begin_drag(reach_scrollbar_model *model, reach_scrollbar_drag *drag,
                                const reach_scrollbar_layout *layout, float pointer_position,
                                int32_t on_thumb)
{
    if (model == nullptr || drag == nullptr || layout == nullptr || layout->thumb.height <= 0.0f)
    {
        return;
    }
    drag->active = 1;
    drag->grab_offset = on_thumb ? pointer_position - layout->thumb.y : layout->thumb.height * 0.5f;
    reach_scrollbar_update_drag(model, drag, layout, pointer_position);
}

void reach_scrollbar_update_drag(reach_scrollbar_model *model, const reach_scrollbar_drag *drag,
                                 const reach_scrollbar_layout *layout, float pointer_position)
{
    if (model == nullptr || drag == nullptr || layout == nullptr || !drag->active)
    {
        return;
    }
    float travel = layout->track.height - layout->thumb.height;
    if (travel <= 0.0f)
    {
        reach_scrollbar_set_target(model, 0.0f);
        model->offset = model->target;
        return;
    }
    float progress = (pointer_position - drag->grab_offset - layout->track.y) / travel;
    reach_scrollbar_set_target(model, reach_scrollbar_clamp(progress, 0.0f, 1.0f) * model->maximum);
    model->offset = model->target;
}

void reach_scrollbar_end_drag(reach_scrollbar_drag *drag)
{
    if (drag != nullptr)
    {
        *drag = {};
    }
}
