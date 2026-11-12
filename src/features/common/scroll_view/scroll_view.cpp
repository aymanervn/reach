#include "reach/features/common/scroll_view.h"

#include "reach/core/scrollbar.h"
#include "reach/support/util.h"

#include <math.h>

void reach_scroll_view_init(reach_scroll_view *view)
{
    if (view == nullptr)
        return;
    *view = {};
    reach_motion_set(&view->offset, 0.0f);
}

int32_t reach_scroll_view_retarget(reach_scroll_view *view, const reach_theme *theme, float target,
                                   reach_scroll_view_mode mode, double now_seconds)
{
    REACH_ASSERT(view != nullptr);
    REACH_ASSERT(isfinite(target) && target >= 0.0f);
    REACH_ASSERT(mode == REACH_SCROLL_VIEW_SET || mode == REACH_SCROLL_VIEW_GLIDE ||
                 mode == REACH_SCROLL_VIEW_WHEEL);
    if (view == nullptr || !isfinite(target))
        return 0;
    if (view->offset.generation == 0)
        mode = REACH_SCROLL_VIEW_SET;
    if (mode == REACH_SCROLL_VIEW_SET)
    {
        int32_t changed = view->offset.generation == 0 || view->offset.to != target ||
                          reach_motion_active_at(&view->offset, now_seconds);
        reach_motion_set(&view->offset, target);
        return changed;
    }
    if (view->offset.to == target)
        return 0;
    if (mode == REACH_SCROLL_VIEW_WHEEL)
    {
        if (reach_scrollbar_motion_retarget(&view->offset, target, now_seconds) != REACH_OK)
            reach_motion_set(&view->offset, target);
        return 1;
    }
    reach_motion_animate_spec_to_at(&view->offset, target,
                                    reach_theme_motion_preset(theme, REACH_MOTION_PRESET_SMOOTH),
                                    now_seconds);
    return 1;
}

float reach_scroll_view_value_at(const reach_scroll_view *view, double now_seconds)
{
    return view != nullptr ? reach_motion_value_at(&view->offset, now_seconds) : 0.0f;
}

float reach_scroll_view_target(const reach_scroll_view *view)
{
    return view != nullptr ? view->offset.to : 0.0f;
}

reach_motion reach_scroll_view_content_offset(const reach_scroll_view *view)
{
    reach_motion offset = {};
    if (view == nullptr || view->offset.generation == 0)
    {
        reach_motion_set(&offset, 0.0f);
        return offset;
    }
    return reach_motion_affine(&view->offset, -1.0f, 0.0f);
}

int32_t reach_scroll_view_active(const reach_scroll_view *view)
{
    return view != nullptr && reach_motion_active(&view->offset);
}

double reach_scroll_view_deadline(const reach_scroll_view *view)
{
    return view != nullptr ? reach_motion_deadline(&view->offset) : 0.0;
}

void reach_scroll_view_settle(reach_scroll_view *view)
{
    if (view != nullptr && view->offset.generation != 0)
        reach_motion_set(&view->offset, view->offset.to);
}

reach_scroll_view_window reach_scroll_view_item_window(const reach_scroll_view *view, float pitch,
                                                       size_t item_count, size_t visible_count,
                                                       size_t overscan, size_t capacity)
{
    reach_scroll_view_window window = {};
    REACH_ASSERT(pitch > 0.0f && capacity > 0);
    if (view == nullptr || !(pitch > 0.0f) || item_count == 0 || capacity == 0)
        return window;
    float minimum = view->offset.to, maximum = view->offset.to;
    if (reach_motion_range(&view->offset, &minimum, &maximum) != REACH_OK)
        minimum = maximum = view->offset.to;
    auto index_at = [&](float offset, int32_t round_up)
    {
        float rows = fmaxf(0.0f, offset) / pitch;
        size_t index = (size_t)(round_up ? ceilf(rows - 0.001f) : floorf(rows + 0.001f));
        return index < item_count ? index : item_count;
    };
    auto span = [&](size_t first, size_t last)
    {
        first = first > overscan ? first - overscan : 0;
        last = last + visible_count + overscan;
        if (last > item_count)
            last = item_count;
        reach_scroll_view_window result = {first, last > first ? last - first : 0};
        return result;
    };
    window = span(index_at(minimum, 0), index_at(maximum, 1));
    if (window.count > capacity)
    {
        size_t target = index_at(view->offset.to, 0);
        window = span(target, target);
    }
    if (window.count > capacity)
        window.count = capacity;
    return window;
}
