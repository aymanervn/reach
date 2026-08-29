#include "reach/features/clipboard.h"

#include "clipboard_common.h"
#include "clipboard_metrics.h"

#include <atomic>
#include <math.h>
#include <new>

static float reach_clipboard_max_float(float a, float b)
{
    return a > b ? a : b;
}

static float reach_clipboard_min_float(float a, float b)
{
    return a < b ? a : b;
}

static float reach_clipboard_clamp_float(float value, float minimum, float maximum)
{
    if (maximum < minimum)
    {
        return minimum;
    }
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

static int32_t reach_clipboard_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return rect.width > 0.0f && rect.height > 0.0f && (float)x >= rect.x &&
           (float)x < rect.x + rect.width && (float)y >= rect.y && (float)y < rect.y + rect.height;
}

reach_clipboard_layout reach_clipboard_compute_layout_animated(
    reach_clipboard_model *model, reach_rect_f32 monitor_bounds, reach_rect_f32 launcher_bounds,
    float dpi_scale, float border_thickness, float animated_height, float animated_item_width)
{
    reach_clipboard_layout layout = {};
    if (model == nullptr)
    {
        return layout;
    }

    (void)launcher_bounds;

    const reach_clipboard_metrics metrics = reach_clipboard_metrics_for_scale(dpi_scale);
    border_thickness = reach_clipboard_max_float(0.0f, border_thickness);
    const float screen_edge_margin = metrics.screen_edge_margin;
    const float available_width =
        reach_clipboard_max_float(0.0f, monitor_bounds.width - screen_edge_margin * 2.0f);
    const float available_height =
        reach_clipboard_max_float(0.0f, monitor_bounds.height - screen_edge_margin * 2.0f);

    const float width =
        reach_clipboard_min_float(metrics.panel_width + border_thickness * 2.0f, available_width);

    const float max_items_height = metrics.item_large_size * 4.0f + metrics.item_gap * 3.0f;
    const float chrome_height =
        border_thickness * 2.0f + 2.0f * metrics.padding + metrics.title_height + metrics.title_gap;
    const float default_height = chrome_height + metrics.item_default_size;
    const float max_height =
        reach_clipboard_min_float(chrome_height + max_items_height, available_height);

    const float available_for_items = reach_clipboard_max_float(0.0f, max_height - chrome_height);
    size_t max_visible_items = 0;
    if (available_for_items > 0.0f)
    {
        max_visible_items = (size_t)((available_for_items + metrics.item_gap) /
                                     (metrics.item_large_size + metrics.item_gap));
        if (max_visible_items > 4)
        {
            max_visible_items = 4;
        }
    }

    const size_t item_count = reach_clipboard_count_clamped(model);
    const size_t visible_count = item_count < max_visible_items ? item_count : max_visible_items;
    const float visible_items_height = visible_count > 0
                                           ? metrics.item_large_size * (float)visible_count +
                                                 metrics.item_gap * (float)(visible_count - 1)
                                           : 0.0f;

    float height = chrome_height + visible_items_height;
    if (height < default_height)
    {
        height = default_height;
    }
    height = reach_clipboard_min_float(height, available_height);
    const float target_height = height;
    if (animated_height > 0.0f)
    {
        height = reach_clipboard_clamp_float(animated_height, default_height, available_height);
    }

    const float monitor_left = monitor_bounds.x + screen_edge_margin;
    const float monitor_top = monitor_bounds.y + screen_edge_margin;
    const float monitor_right = monitor_bounds.x + monitor_bounds.width - screen_edge_margin;
    const float monitor_bottom = monitor_bounds.y + monitor_bounds.height - screen_edge_margin;

    layout.bounds.x =
        reach_clipboard_clamp_float(monitor_left, monitor_left, monitor_right - width);
    layout.bounds.y =
        reach_clipboard_clamp_float(monitor_bottom - height, monitor_top, monitor_bottom - height);
    layout.bounds.width = width;
    layout.bounds.height = height;

    const float horizontal_border =
        reach_clipboard_min_float(border_thickness, layout.bounds.width * 0.5f);
    const float vertical_border =
        reach_clipboard_min_float(border_thickness, layout.bounds.height * 0.5f);
    const float content_width =
        reach_clipboard_max_float(0.0f, width - horizontal_border * 2.0f - metrics.padding * 2.0f);
    const float clear_button_height =
        reach_clipboard_min_float(metrics.clear_button_height, metrics.title_height);
    layout.clear_button = {layout.bounds.x + layout.bounds.width - horizontal_border -
                               metrics.padding - metrics.clear_button_width,
                           layout.bounds.y + vertical_border + metrics.padding +
                               (metrics.title_height - clear_button_height) * 0.5f,
                           metrics.clear_button_width, clear_button_height};

    const float title_width = reach_clipboard_max_float(
        0.0f, content_width - metrics.clear_button_width - metrics.clear_button_gap);
    layout.title = {layout.bounds.x + horizontal_border + metrics.padding,
                    layout.bounds.y + vertical_border + metrics.padding, title_width,
                    metrics.title_height};

    const float target_viewport_height =
        reach_clipboard_max_float(0.0f, target_height - chrome_height);
    layout.viewport = {
        layout.bounds.x + horizontal_border + metrics.padding,
        layout.title.y + metrics.title_height + metrics.title_gap, content_width,
        reach_clipboard_max_float(0.0f, height - vertical_border * 2.0f - metrics.padding * 2.0f -
                                            metrics.title_height - metrics.title_gap)};

    layout.item_large_size = metrics.item_large_size;
    layout.content_height = item_count > 0 ? metrics.item_large_size * (float)item_count +
                                                 metrics.item_gap * (float)(item_count - 1)
                                           : 0.0f;

    const int32_t needs_scrollbar =
        layout.content_height > target_viewport_height && target_viewport_height > 0.0f;

    reach_scrollbar_set_extents(&model->scrollbar, layout.content_height, layout.viewport.height);

    float item_width = layout.viewport.width;
    if (needs_scrollbar)
    {
        item_width -= metrics.scrollbar_gutter + metrics.item_scrollbar_gap;
    }
    item_width = reach_clipboard_max_float(0.0f, item_width);
    const float target_item_width = item_width;
    if (animated_item_width > 0.0f)
    {
        item_width = reach_clipboard_clamp_float(animated_item_width, 0.0f, layout.viewport.width);
    }
    layout.item_width = item_width;

    for (size_t index = 0; index < item_count; ++index)
    {
        layout.items[index] = {layout.viewport.x,
                               layout.viewport.y +
                                   (metrics.item_large_size + metrics.item_gap) * (float)index -
                                   model->scrollbar.offset,
                               item_width, metrics.item_large_size};
        float item_border = reach_clipboard_min_float(
            border_thickness,
            reach_clipboard_min_float(layout.items[index].width, layout.items[index].height) *
                0.5f);
        layout.close_buttons[index] = {
            layout.items[index].x + layout.items[index].width - item_border -
                metrics.close_button_size - metrics.close_button_margin,
            layout.items[index].y + item_border + metrics.close_button_margin,
            metrics.close_button_size, metrics.close_button_size};
    }

    if (needs_scrollbar)
    {
        const reach_rect_f32 track = {
            layout.viewport.x + item_width + metrics.item_scrollbar_gap + metrics.track_x_offset,
            layout.viewport.y, metrics.track_width, layout.viewport.height};
        layout.scrollbar =
            reach_scrollbar_compute_layout(&model->scrollbar, track, layout.viewport.height,
                                           layout.content_height, metrics.thumb_size);
    }

    return layout;
}

reach_clipboard_layout reach_clipboard_compute_layout(reach_clipboard_model *model,
                                                      reach_rect_f32 monitor_bounds,
                                                      reach_rect_f32 launcher_bounds,
                                                      float dpi_scale, float border_thickness)
{
    return reach_clipboard_compute_layout_animated(model, monitor_bounds, launcher_bounds,
                                                   dpi_scale, border_thickness, 0.0f, 0.0f);
}

reach_clipboard_hit_result reach_clipboard_hit_test(const reach_clipboard_model *model,
                                                    const reach_clipboard_layout *layout, int32_t x,
                                                    int32_t y)
{
    reach_clipboard_hit_result result = {};
    result.index = REACH_CLIPBOARD_MAX_ITEMS;
    if (model == nullptr || layout == nullptr || !model->open)
    {
        return result;
    }

    if (model->count > 0 && reach_clipboard_contains(layout->clear_button, x, y))
    {
        result.type = REACH_CLIPBOARD_HIT_CLEAR;
        return result;
    }

    if (reach_clipboard_contains(layout->scrollbar.thumb, x, y))
    {
        result.type = REACH_CLIPBOARD_HIT_SCROLLBAR_THUMB;
        return result;
    }
    if (reach_clipboard_contains(layout->scrollbar.track, x, y))
    {
        result.type = REACH_CLIPBOARD_HIT_SCROLLBAR_TRACK;
        return result;
    }
    if (!reach_clipboard_contains(layout->viewport, x, y))
    {
        return result;
    }

    const size_t item_count = reach_clipboard_count_clamped(model);
    for (size_t index = 0; index < item_count; ++index)
    {
        if (reach_clipboard_contains(layout->close_buttons[index], x, y))
        {
            result.type = REACH_CLIPBOARD_HIT_ITEM_CLOSE;
            result.index = index;
            return result;
        }
        if (reach_clipboard_contains(layout->items[index], x, y))
        {
            result.type = REACH_CLIPBOARD_HIT_ITEM;
            result.index = index;
            return result;
        }
    }

    return result;
}

enum
{
    REACH_CLIPBOARD_ANIMATION_HOVER_BASE = 0,
    REACH_CLIPBOARD_ANIMATION_HEIGHT =
        REACH_CLIPBOARD_ANIMATION_HOVER_BASE + REACH_CLIPBOARD_MAX_ITEMS,
    REACH_CLIPBOARD_ANIMATION_ITEM_WIDTH,
    REACH_CLIPBOARD_ANIMATION_COUNT
};

struct reach_clipboard_feature
{
    reach_animation_manager animations;
    reach_animation_track animation_tracks[REACH_CLIPBOARD_ANIMATION_COUNT];
    reach_clipboard_state state;
    std::atomic<int32_t> refresh_requested;
    reach_clipboard_port port;
    int32_t started;
    void (*request_update)(void *user);
    void *request_update_user;
    reach_clipboard_retired_resource retired[REACH_CLIPBOARD_MAX_ITEMS * 2];
    size_t retired_count;
};

static void reach_clipboard_feature_retire(reach_clipboard_feature *clipboard,
                                           const reach_clipboard_item *item)
{
    if (clipboard == nullptr || item == nullptr || item->id == 0 ||
        clipboard->retired_count >= REACH_CLIPBOARD_MAX_ITEMS * 2)
    {
        return;
    }
    clipboard->retired[clipboard->retired_count++] = {item->id, item->thumbnail_id};
}

static void reach_clipboard_feature_capture(reach_clipboard_feature *clipboard,
                                            reach_feature_tick_result *out)
{
    if (clipboard == nullptr || out == nullptr ||
        !reach_clipboard_feature_take_refresh(clipboard) ||
        clipboard->port.ops.capture_current == nullptr)
    {
        return;
    }
    reach_clipboard_item item = {};
    if (clipboard->port.ops.capture_current(clipboard->port.provider, &item) != REACH_OK ||
        item.id == 0)
    {
        return;
    }
    reach_clipboard_insert_outcome outcome = {};
    reach_clipboard_insert_captured(clipboard, item, &outcome);
    reach_clipboard_feature_retire(clipboard, &outcome.release_rejected);
    reach_clipboard_feature_retire(clipboard, &outcome.release_evicted);
    if (outcome.accepted)
    {
        out->redraw = 1;
        out->relayout = 1;
        out->request_update = 1;
    }
}

const reach_clipboard_state *reach_clipboard_feature_state_ptr(reach_clipboard_feature *clipboard)
{
    return clipboard != nullptr ? &clipboard->state : nullptr;
}

reach_clipboard_state *reach_clipboard_feature_state_mut(reach_clipboard_feature *clipboard)
{
    return clipboard != nullptr ? &clipboard->state : nullptr;
}

void reach_clipboard_feature_reset(reach_clipboard_feature *clipboard)
{
    if (clipboard != nullptr)
    {
        reach_pressable_reset(&clipboard->state.pressable, nullptr);
        clipboard->state.press_identity = 0;
        reach_clipboard_model_init(&clipboard->state.model);
    }
}

static int32_t reach_clipboard_rect_equal(reach_rect_f32 a, reach_rect_f32 b)
{
    return fabsf(a.x - b.x) < 0.5f && fabsf(a.y - b.y) < 0.5f && fabsf(a.width - b.width) < 0.5f &&
           fabsf(a.height - b.height) < 0.5f;
}

int32_t reach_clipboard_feature_relayout(reach_clipboard_feature *clipboard,
                                         reach_rect_f32 monitor_bounds,
                                         reach_rect_f32 launcher_bounds, float dpi_scale,
                                         float border_thickness, int32_t *out_animating)
{
    if (out_animating != nullptr)
    {
        *out_animating = 0;
    }
    if (clipboard == nullptr)
    {
        return 0;
    }

    reach_clipboard_state *state = &clipboard->state;
    reach_clipboard_layout previous_layout = state->layout;
    reach_clipboard_layout target_layout = reach_clipboard_compute_layout(
        &state->model, monitor_bounds, launcher_bounds, dpi_scale, border_thickness);

    int32_t height_active = 0;
    float animated_height = reach_clipboard_feature_animate_layout_value(
        clipboard, REACH_CLIPBOARD_LAYOUT_HEIGHT, target_layout.bounds.height, &height_active);
    int32_t item_width_active = 0;
    float animated_item_width = reach_clipboard_feature_animate_layout_value(
        clipboard, REACH_CLIPBOARD_LAYOUT_ITEM_WIDTH, target_layout.item_width, &item_width_active);

    state->layout = reach_clipboard_compute_layout_animated(
        &state->model, monitor_bounds, launcher_bounds, dpi_scale, border_thickness,
        animated_height, animated_item_width);

    if (out_animating != nullptr)
    {
        *out_animating = height_active || item_width_active;
    }
    return !reach_clipboard_rect_equal(previous_layout.bounds, state->layout.bounds) ||
           !reach_clipboard_rect_equal(previous_layout.viewport, state->layout.viewport);
}

static void reach_clipboard_capsule_tick(void *capsule, double delta_seconds,
                                         reach_feature_tick_result *out)
{
    reach_clipboard_feature *clipboard = static_cast<reach_clipboard_feature *>(capsule);
    if (out != nullptr)
    {
        *out = {};
    }
    reach_clipboard_feature_tick(clipboard, delta_seconds);
    reach_clipboard_feature_capture(clipboard, out);
    if (out != nullptr && reach_clipboard_tick_scroll(clipboard, delta_seconds))
    {
        out->redraw = 1;
        out->relayout = 1;
        out->request_update = 1;
    }
    if (out != nullptr && reach_clipboard_feature_any_hover_active(clipboard))
    {
        out->redraw = 1;
    }
}

static void reach_clipboard_capsule_reset(void *capsule)
{
    reach_clipboard_feature *clipboard = static_cast<reach_clipboard_feature *>(capsule);
    reach_clipboard_feature_reset(clipboard);
    reach_clipboard_feature_clear_refresh(clipboard);
}

static int32_t reach_clipboard_capsule_is_open(const void *capsule)
{
    reach_clipboard_feature *clipboard = const_cast<reach_clipboard_feature *>(
        static_cast<const reach_clipboard_feature *>(capsule));
    return reach_clipboard_is_open(clipboard);
}

static int32_t reach_clipboard_capsule_needs_frame(const void *capsule)
{
    reach_clipboard_feature *clipboard = const_cast<reach_clipboard_feature *>(
        static_cast<const reach_clipboard_feature *>(capsule));
    if (clipboard == nullptr)
    {
        return 0;
    }
    return reach_clipboard_feature_any_animation_active(clipboard) ||
           clipboard->state.scrollbar_drag.active;
}

static int32_t reach_clipboard_capsule_wants_pointer_move(const void *capsule)
{
    reach_clipboard_feature *clipboard = const_cast<reach_clipboard_feature *>(
        static_cast<const reach_clipboard_feature *>(capsule));
    return reach_clipboard_is_open(clipboard);
}

static void
reach_clipboard_capsule_apply_event_result(const reach_clipboard_event_result *event_result,
                                           reach_clipboard_feature *clipboard,
                                           reach_capsule_pointer_result *out)
{
    if (event_result == nullptr || out == nullptr)
    {
        return;
    }
    out->handled = out->handled || event_result->handled;
    out->redraw = out->redraw || event_result->redraw;
    out->relayout = out->relayout || event_result->relayout;
    out->capture = event_result->capture_pointer;
    out->sync_pointer_subscriptions = event_result->sync_pointer_subscriptions;
    if (event_result->action == REACH_CLIPBOARD_ACTION_CLEAR_ALL)
    {
        size_t count = reach_clipboard_item_count(clipboard);
        for (size_t index = 0; index < count; ++index)
        {
            reach_clipboard_feature_retire(clipboard, reach_clipboard_item_at(clipboard, index));
        }
        reach_clipboard_clear_all(clipboard);
        out->redraw = 1;
        out->relayout = 1;
    }
    else if (event_result->action == REACH_CLIPBOARD_ACTION_REMOVE_ITEM)
    {
        const reach_clipboard_item *item =
            reach_clipboard_item_at(clipboard, event_result->item_index);
        if (item != nullptr && item->id == event_result->item_id)
        {
            reach_clipboard_item removed = *item;
            if (reach_clipboard_remove_item(clipboard, event_result->item_index,
                                            event_result->item_id))
            {
                reach_clipboard_feature_retire(clipboard, &removed);
                out->redraw = 1;
                out->relayout = 1;
            }
        }
    }
    else if (event_result->action == REACH_CLIPBOARD_ACTION_RESTORE_ITEM)
    {
        if (clipboard->port.ops.restore != nullptr &&
            clipboard->port.ops.restore(clipboard->port.provider, event_result->item_id) ==
                REACH_OK)
        {
            reach_clipboard_confirm_restore(clipboard, event_result->item_index);
            out->action.kind = REACH_FEATURE_ACTION_CLOSE_SELF;
            out->redraw = 1;
            out->relayout = 1;
        }
    }
}

static void reach_clipboard_capsule_handle_pointer(void *capsule, const reach_pointer_event *event,
                                                   reach_capsule_pointer_result *out)
{
    if (out != nullptr)
    {
        *out = {};
    }
    reach_clipboard_feature *clipboard = static_cast<reach_clipboard_feature *>(capsule);
    if (clipboard == nullptr || event == nullptr || out == nullptr)
    {
        return;
    }

    reach_clipboard_event_result event_result = {};
    switch (event->kind)
    {
    case REACH_POINTER_EVENT_DOWN:
        if (event->button != REACH_POINTER_BUTTON_PRIMARY)
        {
            return;
        }
        reach_clipboard_cancel_press(clipboard, &event_result);
        reach_clipboard_pointer_down(clipboard, event->x, event->y, &event_result);
        break;
    case REACH_POINTER_EVENT_UP:
        if (event->button != REACH_POINTER_BUTTON_PRIMARY)
        {
            return;
        }
        if (clipboard->state.scrollbar_drag.active)
        {
            reach_clipboard_scrollbar_release(clipboard, &event_result);
        }
        else
        {
            reach_clipboard_pointer_up(clipboard, event->x, event->y, &event_result);
        }
        break;
    case REACH_POINTER_EVENT_MOVE:
        if (clipboard->state.scrollbar_drag.active)
        {
            reach_clipboard_scrollbar_drag_move(clipboard, event->y, &event_result);
        }
        else
        {
            reach_clipboard_pointer_move(clipboard, event->x, event->y, &event_result);
        }
        break;
    case REACH_POINTER_EVENT_WHEEL:
        reach_clipboard_wheel(clipboard, event->x, event->y, event->wheel_delta, &event_result);
        break;
    case REACH_POINTER_EVENT_LEAVE:
        reach_clipboard_pointer_move(clipboard, event->x, event->y, &event_result);
        event_result.redraw |= reach_clipboard_pointer_leave(clipboard);
        break;
    case REACH_POINTER_EVENT_CANCEL:
        reach_clipboard_scrollbar_release(clipboard, &event_result);
        reach_clipboard_cancel_press(clipboard, &event_result);
        break;
    case REACH_POINTER_EVENT_MIDDLE:
    default:
        return;
    }
    reach_clipboard_capsule_apply_event_result(&event_result, clipboard, out);
}

static void reach_clipboard_capsule_surface_geometry(const void *capsule,
                                                     reach_feature_surface_geometry *out)
{
    if (out == nullptr)
    {
        return;
    }
    *out = {};
    const reach_clipboard_feature *clipboard =
        static_cast<const reach_clipboard_feature *>(capsule);
    if (clipboard == nullptr)
    {
        return;
    }
    out->visible_bounds = clipboard->state.layout.bounds;
    out->envelope_bounds = clipboard->state.layout.bounds;
}

const reach_ui_event_type *reach_clipboard_activation_events(size_t *out_count)
{
    static const reach_ui_event_type events[] = {REACH_UI_EVENT_CLIPBOARD_TOGGLE};
    if (out_count != nullptr)
    {
        *out_count = sizeof(events) / sizeof(events[0]);
    }
    return events;
}

const reach_feature_capsule_ops *reach_clipboard_feature_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {
        reach_clipboard_capsule_reset,
        reach_clipboard_capsule_tick,
        reach_clipboard_capsule_is_open,
        nullptr,
        reach_clipboard_capsule_needs_frame,
        reach_clipboard_capsule_wants_pointer_move,
        reach_clipboard_capsule_handle_pointer,
        [](const void *capsule) -> int32_t
        {
            const reach_clipboard_feature *clipboard =
                static_cast<const reach_clipboard_feature *>(capsule);
            return clipboard != nullptr && (clipboard->state.scrollbar_drag.active ||
                                            reach_pressable_tracking(&clipboard->state.pressable));
        },
        nullptr,
        reach_clipboard_capsule_surface_geometry,
    };
    return &ops;
}

void reach_clipboard_feature_request_refresh(reach_clipboard_feature *clipboard)
{
    if (clipboard != nullptr)
    {
        clipboard->refresh_requested.store(1);
    }
}

void reach_clipboard_feature_clear_refresh(reach_clipboard_feature *clipboard)
{
    if (clipboard != nullptr)
    {
        clipboard->refresh_requested.store(0);
    }
}

int32_t reach_clipboard_feature_take_refresh(reach_clipboard_feature *clipboard)
{
    return clipboard != nullptr ? clipboard->refresh_requested.exchange(0) : 0;
}

static size_t reach_clipboard_feature_hover_track(size_t index)
{
    return REACH_CLIPBOARD_ANIMATION_HOVER_BASE + index;
}

static size_t reach_clipboard_feature_layout_track(reach_clipboard_layout_track track)
{
    return track == REACH_CLIPBOARD_LAYOUT_ITEM_WIDTH ? (size_t)REACH_CLIPBOARD_ANIMATION_ITEM_WIDTH
                                                      : (size_t)REACH_CLIPBOARD_ANIMATION_HEIGHT;
}

reach_result reach_clipboard_feature_create(reach_clipboard_feature **out_clipboard)
{
    if (out_clipboard == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_clipboard_feature *clipboard = new (std::nothrow) reach_clipboard_feature();
    if (clipboard == nullptr)
    {
        return REACH_ERROR;
    }
    reach_animation_manager_init(&clipboard->animations, clipboard->animation_tracks,
                                 REACH_CLIPBOARD_ANIMATION_COUNT);
    reach_pressable_init(&clipboard->state.pressable);
    clipboard->refresh_requested.store(0);
    clipboard->port = {};
    clipboard->retired_count = 0;
    reach_clipboard_model_init(&clipboard->state.model);
    *out_clipboard = clipboard;
    return REACH_OK;
}

void reach_clipboard_feature_destroy(reach_clipboard_feature *clipboard)
{
    delete clipboard;
}

static void reach_clipboard_feature_on_changed(void *user)
{
    reach_clipboard_feature *clipboard = static_cast<reach_clipboard_feature *>(user);
    reach_clipboard_feature_request_refresh(clipboard);
    if (clipboard != nullptr && clipboard->request_update != nullptr)
    {
        clipboard->request_update(clipboard->request_update_user);
    }
}

void reach_clipboard_feature_attach_port(reach_clipboard_feature *clipboard,
                                         const reach_clipboard_port *port,
                                         void (*request_update)(void *user), void *user)
{
    if (clipboard != nullptr)
    {
        clipboard->port = port != nullptr ? *port : reach_clipboard_port{};
        clipboard->request_update = request_update;
        clipboard->request_update_user = user;
    }
}

reach_result reach_clipboard_feature_start(reach_clipboard_feature *clipboard)
{
    if (clipboard == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (clipboard->started)
    {
        return REACH_OK;
    }
    reach_clipboard_feature_request_refresh(clipboard);
    reach_result result =
        clipboard->port.ops.start != nullptr
            ? clipboard->port.ops.start(clipboard->port.provider,
                                        reach_clipboard_feature_on_changed, clipboard)
            : REACH_OK;
    clipboard->started = result == REACH_OK;
    return result;
}

void reach_clipboard_feature_stop(reach_clipboard_feature *clipboard)
{
    if (clipboard != nullptr && clipboard->started)
    {
        if (clipboard->port.ops.stop != nullptr)
        {
            (void)clipboard->port.ops.stop(clipboard->port.provider);
        }
        clipboard->started = 0;
    }
}

size_t reach_clipboard_feature_take_retired_resources(reach_clipboard_feature *clipboard,
                                                      reach_clipboard_retired_resource *out,
                                                      size_t cap)
{
    if (clipboard == nullptr || out == nullptr)
    {
        return 0;
    }
    size_t count = clipboard->retired_count < cap ? clipboard->retired_count : cap;
    for (size_t index = 0; index < count; ++index)
    {
        out[index] = clipboard->retired[index];
    }
    for (size_t index = count; index < clipboard->retired_count; ++index)
    {
        clipboard->retired[index - count] = clipboard->retired[index];
    }
    clipboard->retired_count -= count;
    return count;
}

void reach_clipboard_feature_release_resource(reach_clipboard_feature *clipboard,
                                              const reach_clipboard_retired_resource *resource)
{
    if (clipboard != nullptr && resource != nullptr && resource->item_id != 0 &&
        clipboard->port.ops.release != nullptr)
    {
        clipboard->port.ops.release(clipboard->port.provider, resource->item_id);
    }
}

void reach_clipboard_feature_tick(reach_clipboard_feature *clipboard, double delta_seconds)
{
    if (clipboard == nullptr)
    {
        return;
    }
    reach_animation_manager_tick(&clipboard->animations, delta_seconds);
}

void reach_clipboard_feature_collapse_all_hover(reach_clipboard_feature *clipboard)
{
    if (clipboard == nullptr)
    {
        return;
    }
    for (size_t index = 0; index < REACH_CLIPBOARD_MAX_ITEMS; ++index)
    {
        reach_animation_manager_animate_to(&clipboard->animations,
                                           reach_clipboard_feature_hover_track(index), 0.0f, 0.10,
                                           REACH_EASING_EASE_OUT);
    }
}

void reach_clipboard_feature_move_hover(reach_clipboard_feature *clipboard, size_t previous,
                                        size_t next)
{
    if (clipboard == nullptr)
    {
        return;
    }
    if (previous < REACH_CLIPBOARD_MAX_ITEMS)
    {
        reach_animation_manager_animate_to(&clipboard->animations,
                                           reach_clipboard_feature_hover_track(previous), 0.0f,
                                           0.12, REACH_EASING_EASE_OUT);
    }
    if (next < REACH_CLIPBOARD_MAX_ITEMS)
    {
        reach_animation_manager_animate_to(&clipboard->animations,
                                           reach_clipboard_feature_hover_track(next), 1.0f, 0.14,
                                           REACH_EASING_EASE_OUT);
    }
}

void reach_clipboard_feature_clear_hover(reach_clipboard_feature *clipboard, size_t previous)
{
    if (clipboard == nullptr || previous >= REACH_CLIPBOARD_MAX_ITEMS)
    {
        return;
    }
    reach_animation_manager_animate_to(&clipboard->animations,
                                       reach_clipboard_feature_hover_track(previous), 0.0f, 0.12,
                                       REACH_EASING_EASE_OUT);
}

int32_t reach_clipboard_feature_any_hover_active(const reach_clipboard_feature *clipboard)
{
    if (clipboard == nullptr)
    {
        return 0;
    }
    for (size_t index = 0; index < REACH_CLIPBOARD_MAX_ITEMS; ++index)
    {
        if (reach_animation_manager_active(&clipboard->animations,
                                           reach_clipboard_feature_hover_track(index)))
        {
            return 1;
        }
    }
    return 0;
}

int32_t reach_clipboard_feature_any_animation_active(const reach_clipboard_feature *clipboard)
{
    return clipboard != nullptr && reach_animation_manager_any_active(&clipboard->animations);
}

void reach_clipboard_feature_fill_hover_values(const reach_clipboard_feature *clipboard,
                                               float *out_values, size_t count)
{
    if (out_values == nullptr)
    {
        return;
    }
    for (size_t index = 0; index < count; ++index)
    {
        out_values[index] =
            clipboard != nullptr
                ? reach_animation_manager_value(&clipboard->animations,
                                                reach_clipboard_feature_hover_track(index))
                : 0.0f;
    }
}

float reach_clipboard_feature_animate_layout_value(reach_clipboard_feature *clipboard,
                                                   reach_clipboard_layout_track track, float target,
                                                   int32_t *out_active)
{
    if (out_active != nullptr)
    {
        *out_active = 0;
    }
    if (clipboard == nullptr || target <= 0.0f)
    {
        return target;
    }

    size_t track_id = reach_clipboard_feature_layout_track(track);
    float value = reach_animation_manager_value(&clipboard->animations, track_id);
    if (value <= 0.0f)
    {
        reach_animation_manager_set(&clipboard->animations, track_id, target);
        return target;
    }

    if (fabsf(reach_animation_manager_target(&clipboard->animations, track_id) - target) >= 0.5f)
    {
        reach_animation_manager_animate_to(&clipboard->animations, track_id, target, 0.16,
                                           REACH_EASING_EASE_OUT);
    }

    if (reach_animation_manager_active(&clipboard->animations, track_id))
    {
        if (out_active != nullptr)
        {
            *out_active = 1;
        }
        return reach_animation_manager_value(&clipboard->animations, track_id);
    }

    return target;
}

int32_t reach_clipboard_is_open(reach_clipboard_feature *clipboard)
{
    return clipboard != nullptr && reach_clipboard_feature_state_mut(clipboard)->model.open;
}

size_t reach_clipboard_item_count(reach_clipboard_feature *clipboard)
{
    return clipboard != nullptr ? reach_clipboard_feature_state_mut(clipboard)->model.count : 0;
}

const reach_clipboard_item *reach_clipboard_item_at(reach_clipboard_feature *clipboard,
                                                    size_t index)
{
    if (clipboard == nullptr || index >= reach_clipboard_feature_state_mut(clipboard)->model.count)
    {
        return nullptr;
    }
    return &reach_clipboard_feature_state_mut(clipboard)->model.items[index];
}

void reach_clipboard_reset_items(reach_clipboard_feature *clipboard)
{
    if (clipboard != nullptr)
    {
        reach_clipboard_model_init(&reach_clipboard_feature_state_mut(clipboard)->model);
    }
}

void reach_clipboard_clear_all(reach_clipboard_feature *clipboard)
{
    if (clipboard == nullptr)
    {
        return;
    }
    reach_clipboard_state *state = reach_clipboard_feature_state_mut(clipboard);
    reach_clipboard_model_clear_items(&state->model);
    reach_scrollbar_end_drag(&state->scrollbar_drag);
}

int32_t reach_clipboard_set_open(reach_clipboard_feature *clipboard, int32_t open)
{
    if (clipboard == nullptr)
    {
        return 0;
    }
    reach_clipboard_state *state = reach_clipboard_feature_state_mut(clipboard);
    int32_t next = open ? 1 : 0;
    if (state->model.open == next)
    {
        return 0;
    }
    state->model.open = next;
    state->model.hovered_index = REACH_CLIPBOARD_MAX_ITEMS;
    reach_pressable_reset(&state->pressable, nullptr);
    state->press_identity = 0;
    reach_scrollbar_end_drag(&state->scrollbar_drag);
    reach_clipboard_feature_collapse_all_hover(clipboard);
    return 1;
}

void reach_clipboard_insert_captured(reach_clipboard_feature *clipboard, reach_clipboard_item item,
                                     reach_clipboard_insert_outcome *out)
{
    if (clipboard == nullptr || out == nullptr)
    {
        return;
    }
    reach_clipboard_state *state = reach_clipboard_feature_state_mut(clipboard);

    reach_clipboard_item evicted = {};
    if (state->model.count == REACH_CLIPBOARD_MAX_ITEMS)
    {
        evicted = state->model.items[REACH_CLIPBOARD_MAX_ITEMS - 1];
    }
    reach_clipboard_insert_result insertion = reach_clipboard_model_insert(&state->model, item);
    if (insertion.rejected_id != 0)
    {
        out->release_rejected = item;
    }
    if (insertion.evicted_id != 0)
    {
        out->release_evicted = evicted;
    }
    if (insertion.inserted || insertion.promoted_existing)
    {
        reach_scrollbar_set_target(&state->model.scrollbar, 0.0f);
        state->model.scrollbar.offset = 0.0f;
        out->accepted = 1;
    }
}

int32_t reach_clipboard_pointer_leave(reach_clipboard_feature *clipboard)
{
    if (clipboard == nullptr)
    {
        return 0;
    }
    reach_clipboard_state *state = reach_clipboard_feature_state_mut(clipboard);
    if (state->model.hovered_index >= REACH_CLIPBOARD_MAX_ITEMS)
    {
        return 0;
    }
    size_t previous = state->model.hovered_index;
    state->model.hovered_index = REACH_CLIPBOARD_MAX_ITEMS;
    reach_clipboard_feature_clear_hover(clipboard, previous);
    return 1;
}

int32_t reach_clipboard_tick_scroll(reach_clipboard_feature *clipboard, double delta_seconds)
{
    if (clipboard == nullptr)
    {
        return 0;
    }
    return reach_scrollbar_update(&reach_clipboard_feature_state_mut(clipboard)->model.scrollbar,
                                  delta_seconds);
}
