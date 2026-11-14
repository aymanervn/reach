#include "reach/features/common/scroll_view.h"

#include "reach/support/util.h"

#include <math.h>

static int32_t reach_scroll_view_contains(reach_rect_f32 rect, float x, float y)
{
    return rect.width > 0.0f && rect.height > 0.0f && x >= rect.x && x < rect.x + rect.width &&
           y >= rect.y && y < rect.y + rect.height;
}

static reach_scrollbar_layout reach_scroll_view_bar(const reach_scroll_view *view, float position)
{
    reach_scrollbar_model model = view->model;
    model.offset = position;
    return reach_scrollbar_compute_layout(&model, view->track, view->viewport_extent,
                                          view->content_extent, view->config.minimum_thumb);
}

static reach_scrollbar_layout reach_scroll_view_resting_bar(const reach_scroll_view *view)
{
    return reach_scroll_view_bar(view, view->model.target);
}

static float reach_scroll_view_pixels(const reach_scroll_view *view, float position)
{
    return view->config.unit > 0.0f ? position * view->config.unit : 0.0f;
}

static void reach_scroll_view_commit(reach_scroll_view *view)
{
    view->model.offset = view->model.target;
}

static reach_scrollbar_drag_mode reach_scroll_view_drag_mode(float step)
{
    return step > 0.0f ? REACH_SCROLLBAR_DRAG_STEPPED : REACH_SCROLLBAR_DRAG_FREE;
}

void reach_scroll_view_init(reach_scroll_view *view, const reach_scroll_view_config *config)
{
    if (view == nullptr)
        return;
    reach_scroll_view_config settings = config != nullptr ? *config : reach_scroll_view_config{};
    *view = {};
    view->config = settings;
    reach_scrollbar_model_init(&view->model, reach_scroll_view_drag_mode(settings.step),
                               settings.step);
    reach_motion_set(&view->offset, 0.0f);
}

void reach_scroll_view_configure(reach_scroll_view *view, const reach_scroll_view_config *config)
{
    if (view == nullptr || config == nullptr || !(config->unit >= 0.0f))
        return;
    int32_t unit_changed = view->config.unit != config->unit;
    view->config = *config;
    view->model.step = config->step;
    view->model.drag_mode = reach_scroll_view_drag_mode(config->step);
    if (unit_changed)
        reach_motion_set(&view->offset, reach_scroll_view_pixels(view, view->model.target));
}

void reach_scroll_view_set_extents(reach_scroll_view *view, float content_extent,
                                   float viewport_extent)
{
    if (view == nullptr)
        return;
    view->content_extent = content_extent;
    view->viewport_extent = viewport_extent;
    reach_scrollbar_set_extents(&view->model, content_extent, viewport_extent);
    reach_scroll_view_commit(view);
}

void reach_scroll_view_set_frame(reach_scroll_view *view, reach_rect_f32 viewport,
                                 reach_rect_f32 track)
{
    if (view == nullptr)
        return;
    view->viewport = viewport;
    view->track = track;
}

int32_t reach_scroll_view_set_position(reach_scroll_view *view, float position)
{
    if (view == nullptr || !isfinite(position))
        return 0;
    float previous = view->model.target;
    reach_scrollbar_set_target(&view->model, position);
    reach_scroll_view_commit(view);
    return view->model.target != previous;
}

float reach_scroll_view_position(const reach_scroll_view *view)
{
    return view != nullptr ? view->model.target : 0.0f;
}

float reach_scroll_view_maximum(const reach_scroll_view *view)
{
    return view != nullptr ? view->model.maximum : 0.0f;
}

int32_t reach_scroll_view_sync(reach_scroll_view *view, const reach_theme *theme,
                               reach_scroll_view_mode mode, double now_seconds)
{
    REACH_ASSERT(view != nullptr);
    REACH_ASSERT(mode == REACH_SCROLL_VIEW_SET || mode == REACH_SCROLL_VIEW_GLIDE ||
                 mode == REACH_SCROLL_VIEW_WHEEL);
    if (view == nullptr)
        return 0;
    float target = reach_scroll_view_pixels(view, view->model.target);
    REACH_ASSERT(isfinite(target) && target >= 0.0f);
    if (!isfinite(target))
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

int32_t reach_scroll_view_scroll_by(reach_scroll_view *view, float delta, double now_seconds)
{
    if (view == nullptr || !isfinite(delta))
        return 0;
    int32_t changed = reach_scroll_view_set_position(view, view->model.target + delta);
    (void)reach_scroll_view_sync(view, nullptr, REACH_SCROLL_VIEW_WHEEL, now_seconds);
    return changed;
}

uint32_t reach_scroll_view_wheel(reach_scroll_view *view, float x, float y, int32_t wheel_delta,
                                 double now_seconds)
{
    if (view == nullptr || wheel_delta == 0 || !reach_scroll_view_contains(view->viewport, x, y))
        return 0;
    float delta = wheel_delta > 0 ? -view->config.wheel_delta : view->config.wheel_delta;
    uint32_t flags = REACH_SCROLL_VIEW_HANDLED;
    if (reach_scroll_view_scroll_by(view, delta, now_seconds))
        flags |= REACH_SCROLL_VIEW_CHANGED;
    return flags;
}

int32_t reach_scroll_view_hit(const reach_scroll_view *view, float x, float y)
{
    if (!reach_scroll_view_has_scrollbar(view))
        return 0;
    reach_scrollbar_layout bar = reach_scroll_view_resting_bar(view);
    if (reach_scroll_view_contains(bar.thumb, x, y))
        return 2;
    return reach_scroll_view_contains(bar.track, x, y) ? 1 : 0;
}

uint32_t reach_scroll_view_pointer_down(reach_scroll_view *view, float x, float y,
                                        double now_seconds)
{
    int32_t hit = reach_scroll_view_hit(view, x, y);
    if (hit == 0)
        return 0;
    float previous = view->model.target;
    reach_scrollbar_layout bar = reach_scroll_view_resting_bar(view);
    reach_scrollbar_begin_drag(&view->model, &view->drag, &bar, y, hit == 2);
    reach_scroll_view_commit(view);
    (void)reach_scroll_view_sync(view, nullptr, REACH_SCROLL_VIEW_SET, now_seconds);
    uint32_t flags = REACH_SCROLL_VIEW_HANDLED | REACH_SCROLL_VIEW_CAPTURE;
    if (view->model.target != previous)
        flags |= REACH_SCROLL_VIEW_CHANGED;
    return flags;
}

uint32_t reach_scroll_view_pointer_move(reach_scroll_view *view, float y, double now_seconds)
{
    if (view == nullptr || !view->drag.active)
        return 0;
    float previous = view->model.target;
    reach_scrollbar_layout bar = reach_scroll_view_resting_bar(view);
    reach_scrollbar_update_drag(&view->model, &view->drag, &bar, y);
    reach_scroll_view_commit(view);
    (void)reach_scroll_view_sync(view, nullptr, REACH_SCROLL_VIEW_SET, now_seconds);
    uint32_t flags = REACH_SCROLL_VIEW_HANDLED;
    if (view->model.target != previous)
        flags |= REACH_SCROLL_VIEW_CHANGED;
    return flags;
}

uint32_t reach_scroll_view_pointer_up(reach_scroll_view *view)
{
    if (view == nullptr || !view->drag.active)
        return 0;
    reach_scrollbar_end_drag(&view->drag);
    return REACH_SCROLL_VIEW_HANDLED | REACH_SCROLL_VIEW_RELEASE;
}

int32_t reach_scroll_view_dragging(const reach_scroll_view *view)
{
    return view != nullptr && view->drag.active;
}

float reach_scroll_view_value_at(const reach_scroll_view *view, double now_seconds)
{
    return view != nullptr ? reach_motion_value_at(&view->offset, now_seconds) : 0.0f;
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

int32_t reach_scroll_view_active_at(const reach_scroll_view *view, double now_seconds)
{
    return view != nullptr && reach_motion_active_at(&view->offset, now_seconds);
}

int32_t reach_scroll_view_pending(const reach_scroll_view *view)
{
    return view != nullptr && view->offset.duration_seconds > 0.0;
}

int32_t reach_scroll_view_advance(reach_scroll_view *view, double now_seconds)
{
    if (!reach_scroll_view_pending(view))
        return 0;
    if (!reach_motion_active_at(&view->offset, now_seconds))
        reach_scroll_view_settle(view);
    return 1;
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

int32_t reach_scroll_view_has_scrollbar(const reach_scroll_view *view)
{
    return view != nullptr && view->track.width > 0.0f && view->track.height > 0.0f &&
           view->content_extent > view->viewport_extent && view->viewport_extent > 0.0f;
}

reach_rect_f32 reach_scroll_view_thumb_at(const reach_scroll_view *view, double now_seconds)
{
    if (!reach_scroll_view_has_scrollbar(view))
        return reach_rect_f32{};
    float position = view->model.target;
    if (view->config.unit > 0.0f)
        position = reach_scroll_view_value_at(view, now_seconds) / view->config.unit;
    return reach_scroll_view_bar(view, position).thumb;
}

reach_result reach_scroll_view_push_viewport(const reach_scroll_view *view,
                                             const reach_scroll_view_group *group,
                                             reach_render_command_buffer *out)
{
    if (view == nullptr || group == nullptr || out == nullptr)
        return REACH_INVALID_ARGUMENT;
    reach_render_group viewport = {};
    viewport.id = group->id;
    viewport.parent_id = group->parent_id;
    viewport.bounds = group->bounds;
    viewport.clip = group->bounds;
    viewport.has_clip = 1;
    reach_result result = reach_render_command_buffer_begin_group(out, &viewport);
    reach_render_command_buffer_end_group(out);
    if (result != REACH_OK)
        return result;
    reach_render_group content = {};
    content.id = group->id + 1;
    content.parent_id = group->id;
    content.bounds.x = floorf(viewport.bounds.x);
    content.bounds.y = floorf(viewport.bounds.y);
    content.bounds.width = ceilf(viewport.bounds.x + viewport.bounds.width) - content.bounds.x;
    content.bounds.height = ceilf(viewport.bounds.y + viewport.bounds.height) - content.bounds.y;
    content.y = reach_scroll_view_content_offset(view);
    result = reach_render_command_buffer_begin_group(out, &content);
    reach_render_command_buffer_end_group(out);
    return result;
}

static reach_render_command reach_scroll_view_bar_command(reach_rect_f32 rect,
                                                          reach_rect_f32 origin, reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = {rect.x - origin.x, rect.y - origin.y, rect.width, rect.height};
    command.color = color;
    command.radius = command.rect.width * 0.5f;
    return command;
}

reach_result reach_scroll_view_push_scrollbar(const reach_scroll_view *view, reach_rect_f32 origin,
                                              reach_scroll_view_colors colors,
                                              const reach_scroll_view_group *retained,
                                              double now_seconds, reach_render_command_buffer *out)
{
    if (view == nullptr || out == nullptr)
        return REACH_INVALID_ARGUMENT;
    if (!reach_scroll_view_has_scrollbar(view))
        return REACH_OK;
    reach_render_command track = reach_scroll_view_bar_command(view->track, origin, colors.track);
    if (retained == nullptr)
    {
        reach_render_command thumb = reach_scroll_view_bar_command(
            reach_scroll_view_thumb_at(view, now_seconds), origin, colors.thumb);
        reach_render_command_buffer_push(out, &track);
        reach_render_command_buffer_push(out, &thumb);
        return REACH_OK;
    }
    reach_scrollbar_layout bar = reach_scroll_view_resting_bar(view);
    reach_render_command thumb = reach_scroll_view_bar_command(bar.thumb, origin, colors.thumb);
    reach_motion stationary = {};
    reach_motion thumb_offset = {};
    reach_motion_set(&thumb_offset, 0.0f);
    float travel = bar.track.height - bar.thumb.height;
    float maximum = reach_scroll_view_pixels(view, view->model.maximum);
    if (travel > 0.0f && maximum > 0.0f && view->offset.generation != 0)
    {
        float ratio = travel / maximum;
        thumb_offset = reach_motion_affine(&view->offset, ratio, -ratio * view->offset.to);
    }
    size_t first = out->count;
    reach_render_command_buffer_push(out, &track);
    reach_result result = reach_render_retain_commands(
        out, first, retained->id, retained->parent_id, retained->opacity, &stationary);
    if (result != REACH_OK)
        return result;
    first = out->count;
    reach_render_command_buffer_push(out, &thumb);
    return reach_render_retain_commands(out, first, retained->id + 1, retained->parent_id,
                                        retained->opacity, &thumb_offset);
}
