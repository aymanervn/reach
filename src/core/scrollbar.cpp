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
    if (model == nullptr || model->step <= 0.0f)
    {
        return value;
    }
    return floorf(value / model->step + 0.5f) * model->step;
}

void reach_scrollbar_model_init(reach_scrollbar_model *model, float step)
{
    if (model == nullptr)
    {
        return;
    }
    *model = {};
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
    model->position = reach_scrollbar_clamp(model->position, 0.0f, model->maximum);
}

void reach_scrollbar_set_position(reach_scrollbar_model *model, float position)
{
    if (model == nullptr)
    {
        return;
    }
    position = reach_scrollbar_quantize(model, position);
    model->position = reach_scrollbar_clamp(position, 0.0f, model->maximum);
}

reach_scrollbar_layout reach_scrollbar_compute_layout(const reach_scrollbar_model *model,
                                                      reach_rect_f32 track, float position,
                                                      float viewport_extent, float content_extent,
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
    float progress = model->maximum > 0.0f ? position / model->maximum : 0.0f;
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
        reach_scrollbar_set_position(model, 0.0f);
        return;
    }
    float progress = (pointer_position - drag->grab_offset - layout->track.y) / travel;
    reach_scrollbar_set_position(model,
                                 reach_scrollbar_clamp(progress, 0.0f, 1.0f) * model->maximum);
}

void reach_scrollbar_end_drag(reach_scrollbar_drag *drag)
{
    if (drag != nullptr)
    {
        *drag = {};
    }
}
