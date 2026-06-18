#ifndef REACH_FEATURES_DOCK_COMMON_H
#define REACH_FEATURES_DOCK_COMMON_H

#include "reach/core/geometry.h"

static inline reach_rect_f32 reach_dock_rect(float x, float y, float width, float height)
{
    reach_rect_f32 rect = {};
    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;
    return rect;
}

static inline reach_rect_f32 reach_dock_icon_box_for_slot(reach_rect_f32 slot, float icon_box_size)
{
    return reach_dock_rect(slot.x + (slot.width - icon_box_size) * 0.5f,
                           slot.y + (slot.height - icon_box_size) * 0.5f, icon_box_size,
                           icon_box_size);
}

#endif
