#include "top_bar_window_push.h"

#include <new>

enum
{
    REACH_TOP_BAR_PUSH_MAX_WINDOWS = 32
};

typedef struct reach_top_bar_pushed_window
{
    uintptr_t window;
    reach_point_f32 origin;
} reach_top_bar_pushed_window;

struct reach_top_bar_window_push
{
    reach_app_control *apps;
    reach_window_tracking *windows;
    reach_top_bar_pushed_window pushed[REACH_TOP_BAR_PUSH_MAX_WINDOWS];
    size_t pushed_count;
    int32_t captured;
    int32_t recovered;
    float target_top;
    float applied_progress;
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
    push->recovered = 0;
}

static int32_t reach_top_bar_push_window_on_monitor(reach_rect_f32 bounds, reach_rect_f32 monitor)
{
    if (bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return 0;
    }
    float center_x = bounds.x + bounds.width * 0.5f;
    float center_y = bounds.y + bounds.height * 0.5f;
    return center_x >= monitor.x && center_x < monitor.x + monitor.width && center_y >= monitor.y &&
           center_y < monitor.y + monitor.height;
}

static int32_t reach_top_bar_push_window_pushable(const reach_window_snapshot *window)
{
    return window->visible && !window->minimized && window->id != 0;
}

static int32_t reach_top_bar_push_recover(reach_top_bar_window_push *push, reach_rect_f32 monitor)
{
    reach_window_move moves[REACH_TOP_BAR_PUSH_MAX_WINDOWS] = {};
    size_t move_count = 0;

    const reach_window_snapshot *windows = reach_window_tracking_windows(push->windows);
    size_t window_count = reach_window_tracking_window_count(push->windows);
    if (windows == nullptr || window_count == 0)
    {
        return 0;
    }

    for (size_t index = 0; index < window_count && move_count < REACH_TOP_BAR_PUSH_MAX_WINDOWS;
         ++index)
    {
        const reach_window_snapshot *window = &windows[index];
        if (!reach_top_bar_push_window_pushable(window) || !window->maximized)
        {
            continue;
        }

        reach_rect_f32 bounds = {};
        reach_rect_f32 frame = {};
        if (reach_app_control_window_bounds(push->apps, window->id, &bounds) != REACH_OK ||
            reach_app_control_window_frame_bounds(push->apps, window->id, &frame) != REACH_OK ||
            !reach_top_bar_push_window_on_monitor(bounds, monitor))
        {
            continue;
        }
        if (frame.x == monitor.x && frame.y == monitor.y)
        {
            continue;
        }

        moves[move_count].window = window->id;
        moves[move_count].position.x = bounds.x + (monitor.x - frame.x);
        moves[move_count].position.y = bounds.y + (monitor.y - frame.y);
        ++move_count;
    }

    if (move_count > 0)
    {
        (void)reach_app_control_move_windows(push->apps, moves, move_count);
    }
    return 1;
}

static void reach_top_bar_push_collect(reach_top_bar_window_push *push, reach_rect_f32 monitor)
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
        if (!reach_top_bar_push_window_pushable(window))
        {
            continue;
        }

        reach_rect_f32 bounds = {};
        if (reach_app_control_window_bounds(push->apps, window->id, &bounds) != REACH_OK ||
            !reach_top_bar_push_window_on_monitor(bounds, monitor) || bounds.y >= push->target_top)
        {
            continue;
        }

        push->pushed[push->pushed_count].window = window->id;
        push->pushed[push->pushed_count].origin.x = bounds.x;
        push->pushed[push->pushed_count].origin.y = bounds.y;
        ++push->pushed_count;
    }
}

static void reach_top_bar_push_write(reach_top_bar_window_push *push, float progress)
{
    reach_window_move moves[REACH_TOP_BAR_PUSH_MAX_WINDOWS] = {};

    for (size_t index = 0; index < push->pushed_count; ++index)
    {
        const reach_top_bar_pushed_window *pushed = &push->pushed[index];
        moves[index].window = pushed->window;
        moves[index].position.x = pushed->origin.x;
        moves[index].position.y =
            pushed->origin.y + (push->target_top - pushed->origin.y) * progress;
    }

    (void)reach_app_control_move_windows(push->apps, moves, push->pushed_count);
    push->applied_progress = progress;
}

void reach_top_bar_window_push_apply(reach_top_bar_window_push *push,
                                     const reach_top_bar_window_push_request *request)
{
    if (push == nullptr || request == nullptr || push->apps == nullptr || push->windows == nullptr)
    {
        return;
    }

    if (!push->recovered && !push->captured && request->push_depth > 0.0f)
    {
        push->recovered = reach_top_bar_push_recover(push, request->monitor_bounds);
    }

    float progress = request->reveal_progress;
    if (!request->bar_can_hide || request->push_depth <= 0.0f || progress <= 0.0f)
    {
        reach_top_bar_window_push_release(push);
        return;
    }

    if (!push->captured)
    {
        push->target_top = request->monitor_bounds.y + request->push_depth;
        reach_top_bar_push_collect(push, request->monitor_bounds);
        push->captured = 1;
    }

    if (push->pushed_count == 0 || progress == push->applied_progress)
    {
        return;
    }
    reach_top_bar_push_write(push, progress);
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
    push->target_top = 0.0f;
    push->applied_progress = 0.0f;
}
