#include "top_bar_window_push.h"

#include <new>

enum
{
    REACH_TOP_BAR_PUSH_MAX_WINDOWS = 32
};

typedef struct reach_top_bar_pushed_window
{
    uintptr_t window;
    reach_rect_f32 original;
} reach_top_bar_pushed_window;

struct reach_top_bar_window_push
{
    reach_app_control *apps;
    reach_window_tracking *windows;
    reach_top_bar_pushed_window pushed[REACH_TOP_BAR_PUSH_MAX_WINDOWS];
    size_t pushed_count;
    int32_t captured;
    float applied_offset;
};

reach_result reach_top_bar_window_push_create(reach_top_bar_window_push **out_push)
{
    if (out_push == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_top_bar_window_push *push = new (std::nothrow) reach_top_bar_window_push();
    if (push == nullptr)
    {
        return REACH_ERROR;
    }
    *out_push = push;
    return REACH_OK;
}

void reach_top_bar_window_push_destroy(reach_top_bar_window_push *push)
{
    delete push;
}

void reach_top_bar_window_push_attach(reach_top_bar_window_push *push, reach_app_control *apps,
                                      reach_window_tracking *windows)
{
    if (push == nullptr)
    {
        return;
    }
    reach_top_bar_window_push_release(push);
    push->apps = apps;
    push->windows = windows;
}

static float reach_top_bar_push_bar_overlap(reach_rect_f32 bar, reach_rect_f32 monitor)
{
    float overlap = bar.y + bar.height - monitor.y;
    return overlap > 0.0f ? overlap : 0.0f;
}

static int32_t reach_top_bar_push_window_spans_monitor(reach_rect_f32 bounds,
                                                       reach_rect_f32 monitor)
{
    return bounds.width > 0.0f && bounds.height > 0.0f && bounds.x < monitor.x + monitor.width &&
           bounds.x + bounds.width > monitor.x && bounds.y < monitor.y + monitor.height &&
           bounds.y + bounds.height > monitor.y;
}

static void reach_top_bar_push_collect(reach_top_bar_window_push *push, reach_rect_f32 monitor,
                                       float shown_overlap)
{
    push->pushed_count = 0;

    const reach_window_snapshot *windows = reach_window_tracking_windows(push->windows);
    size_t window_count = reach_window_tracking_window_count(push->windows);
    if (windows == nullptr)
    {
        return;
    }

    for (size_t index = 0;
         index < window_count && push->pushed_count < REACH_TOP_BAR_PUSH_MAX_WINDOWS; ++index)
    {
        const reach_window_snapshot *window = &windows[index];
        if (!window->visible || window->minimized || window->id == 0)
        {
            continue;
        }

        reach_rect_f32 bounds = {};
        if (reach_app_control_window_bounds(push->apps, window->id, &bounds) != REACH_OK)
        {
            continue;
        }
        if (!reach_top_bar_push_window_spans_monitor(bounds, monitor) ||
            bounds.y >= monitor.y + shown_overlap || bounds.height <= shown_overlap)
        {
            continue;
        }

        push->pushed[push->pushed_count].window = window->id;
        push->pushed[push->pushed_count].original = bounds;
        ++push->pushed_count;
    }
}

static void reach_top_bar_push_write(reach_top_bar_window_push *push, float offset)
{
    reach_window_outer_bounds requests[REACH_TOP_BAR_PUSH_MAX_WINDOWS] = {};

    for (size_t index = 0; index < push->pushed_count; ++index)
    {
        const reach_top_bar_pushed_window *pushed = &push->pushed[index];
        requests[index].window = pushed->window;
        requests[index].bounds = pushed->original;
        requests[index].bounds.y += offset;
        requests[index].bounds.height -= offset;
    }

    (void)reach_app_control_set_window_bounds(push->apps, requests, push->pushed_count);
    push->applied_offset = offset;
}

void reach_top_bar_window_push_apply(reach_top_bar_window_push *push,
                                     const reach_top_bar_window_push_request *request)
{
    if (push == nullptr || request == nullptr || push->apps == nullptr || push->windows == nullptr)
    {
        return;
    }

    float shown_overlap =
        reach_top_bar_push_bar_overlap(request->shown_bounds, request->monitor_bounds);
    float overlap =
        reach_top_bar_push_bar_overlap(request->animated_bounds, request->monitor_bounds);
    if (overlap > shown_overlap)
    {
        overlap = shown_overlap;
    }

    if (!request->bar_can_hide || shown_overlap <= 0.0f || overlap <= 0.0f)
    {
        reach_top_bar_window_push_release(push);
        return;
    }

    if (!push->captured)
    {
        reach_top_bar_push_collect(push, request->monitor_bounds, shown_overlap);
        push->captured = 1;
    }

    if (push->pushed_count == 0 || overlap == push->applied_offset)
    {
        return;
    }
    reach_top_bar_push_write(push, overlap);
}

void reach_top_bar_window_push_release(reach_top_bar_window_push *push)
{
    if (push == nullptr || !push->captured)
    {
        return;
    }
    if (push->pushed_count > 0)
    {
        reach_top_bar_push_write(push, 0.0f);
    }
    push->captured = 0;
    push->pushed_count = 0;
    push->applied_offset = 0.0f;
}
