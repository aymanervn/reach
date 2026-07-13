#include "reach/features/switcher.h"

#include "switcher_common.h"

#include <math.h>
#include <new>

static size_t reach_switcher_min_size(size_t a, size_t b)
{
    return a < b ? a : b;
}

size_t reach_switcher_visible_count(size_t window_count)
{
    return reach_switcher_min_size(window_count, REACH_SWITCHER_VISIBLE_MAX);
}

static float reach_switcher_scale(float value, float dpi_scale)
{
    return value * (dpi_scale > 0.0f ? dpi_scale : 1.0f);
}

reach_rect_f32 reach_switcher_bounds_for_count(reach_rect_f32 monitor_bounds, size_t visible_count)
{
    return reach_switcher_bounds_for_count_scaled(monitor_bounds, visible_count, 1.0f);
}

reach_rect_f32 reach_switcher_bounds_for_count_scaled(reach_rect_f32 monitor_bounds,
                                                      size_t visible_count, float dpi_scale)
{
    float padding = reach_switcher_scale(24.0f, dpi_scale);
    float item_size = reach_switcher_scale(112.0f, dpi_scale);
    float gap = reach_switcher_scale(14.0f, dpi_scale);
    reach_rect_f32 bounds = {};
    size_t count = visible_count > 0 ? visible_count : 1;
    bounds.width = padding * 2.0f + (float)count * item_size + (float)(count - 1) * gap;
    float max_width = monitor_bounds.width - reach_switcher_scale(48.0f, dpi_scale);
    if (bounds.width > max_width)
    {
        bounds.width = max_width;
    }
    float min_width = reach_switcher_scale(280.0f, dpi_scale);
    if (bounds.width < min_width)
    {
        bounds.width = monitor_bounds.width < min_width ? monitor_bounds.width : min_width;
    }
    bounds.height = reach_switcher_scale(184.0f, dpi_scale);
    bounds.x = monitor_bounds.x + (monitor_bounds.width - bounds.width) * 0.5f;
    bounds.y = monitor_bounds.y + (monitor_bounds.height - bounds.height) * 0.5f;
    return bounds;
}

void reach_switcher_update_visible_start(reach_switcher_model *model)
{
    if (model == nullptr || model->window_count == 0)
    {
        if (model != nullptr)
        {
            model->visible_start = 0;
        }
        return;
    }
    size_t visible_count = reach_switcher_visible_count(model->window_count);
    if (visible_count == 0 || visible_count >= model->window_count)
    {
        model->visible_start = 0;
        return;
    }
    if (model->selected_index < model->visible_start)
    {
        model->visible_start = model->selected_index;
    }
    else if (model->selected_index >= model->visible_start + visible_count)
    {
        model->visible_start = model->selected_index - visible_count + 1;
    }
    size_t max_start = model->window_count - visible_count;
    if (model->visible_start > max_start)
    {
        model->visible_start = max_start;
    }
}

enum
{
    REACH_SWITCHER_ANIMATION_WIDTH = 0,
    REACH_SWITCHER_ANIMATION_COUNT
};

/* Internal ring-source view over the attached window service. */
typedef struct reach_switcher_window_context
{
    const reach_window_snapshot *open_windows;
    size_t open_window_count;
    uintptr_t foreground_window;
    const uintptr_t *focus_history;
    size_t focus_history_count;
} reach_switcher_window_context;

struct reach_switcher
{
    reach_animation_manager animations;
    reach_animation_track animation_tracks[REACH_SWITCHER_ANIMATION_COUNT];
    reach_switcher_state state;
    reach_icon_service *icons;
    reach_window_tracking *windows;
};

void reach_switcher_attach_services(reach_switcher *switcher, reach_icon_service *icons,
                                    reach_window_tracking *windows)
{
    if (switcher != nullptr)
    {
        switcher->icons = icons;
        switcher->windows = windows;
    }
}

reach_icon_service *reach_switcher_icons(reach_switcher *switcher)
{
    return switcher != nullptr ? switcher->icons : nullptr;
}

reach_window_tracking *reach_switcher_windows(reach_switcher *switcher)
{
    return switcher != nullptr ? switcher->windows : nullptr;
}

static reach_switcher_window_context reach_switcher_service_context(reach_switcher *switcher)
{
    reach_switcher_window_context ctx = {};
    reach_window_tracking *windows = switcher != nullptr ? switcher->windows : nullptr;
    ctx.open_windows = reach_window_tracking_windows(windows);
    ctx.open_window_count = reach_window_tracking_window_count(windows);
    ctx.foreground_window = reach_window_tracking_foreground(windows);
    ctx.focus_history = reach_window_tracking_focus_history(windows);
    ctx.focus_history_count = reach_window_tracking_focus_history_count(windows);
    return ctx;
}

const reach_switcher_state *reach_switcher_state_ptr(reach_switcher *switcher)
{
    return switcher != nullptr ? &switcher->state : nullptr;
}

reach_switcher_state *reach_switcher_state_mut(reach_switcher *switcher)
{
    return switcher != nullptr ? &switcher->state : nullptr;
}

int32_t reach_switcher_is_open(const reach_switcher *switcher)
{
    return switcher != nullptr && switcher->state.open;
}

/* Uniform capsule hooks (reach_feature_capsule_ops). */

static void reach_switcher_capsule_tick(void *capsule, double delta_seconds,
                                        reach_feature_tick_result *out)
{
    reach_switcher *switcher = static_cast<reach_switcher *>(capsule);
    reach_switcher_tick(switcher, delta_seconds);
    if (out != nullptr && reach_switcher_width_animation_active(switcher))
    {
        out->redraw = 1;
    }
}

static int32_t reach_switcher_capsule_is_open(const void *capsule)
{
    return reach_switcher_is_open(static_cast<const reach_switcher *>(capsule));
}

static void reach_switcher_capsule_force_close(void *capsule)
{
    reach_switcher_force_close(static_cast<reach_switcher *>(capsule));
}

static int32_t reach_switcher_capsule_needs_frame(const void *capsule)
{
    return reach_switcher_width_animation_active(static_cast<const reach_switcher *>(capsule));
}

static reach_switcher_action
reach_switcher_handle_event_with_context(reach_switcher *switcher, const reach_ui_event *event,
                                         const reach_switcher_window_context *ctx);
static reach_switcher_action
reach_switcher_sync_windows_with_context(reach_switcher *switcher,
                                         const reach_switcher_window_context *ctx);

reach_switcher_action reach_switcher_handle_event(reach_switcher *switcher,
                                                  const reach_ui_event *event)
{
    reach_switcher_window_context ctx = reach_switcher_service_context(switcher);
    return reach_switcher_handle_event_with_context(switcher, event, &ctx);
}

reach_switcher_action reach_switcher_sync_windows(reach_switcher *switcher)
{
    reach_switcher_window_context ctx = reach_switcher_service_context(switcher);
    return reach_switcher_sync_windows_with_context(switcher, &ctx);
}

const reach_ui_event_type *reach_switcher_routed_events(size_t *out_count)
{
    static const reach_ui_event_type events[] = {
        REACH_UI_EVENT_APP_SWITCH_BEGIN,   REACH_UI_EVENT_APP_SWITCH_NEXT,
        REACH_UI_EVENT_APP_SWITCH_PREVIOUS, REACH_UI_EVENT_APP_SWITCH_COMMIT,
        REACH_UI_EVENT_APP_SWITCH_CANCEL,
    };
    if (out_count != nullptr)
    {
        *out_count = sizeof(events) / sizeof(events[0]);
    }
    return events;
}

const reach_feature_capsule_ops *reach_switcher_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {
        nullptr /* reset */,
        reach_switcher_capsule_tick,
        reach_switcher_capsule_is_open,
        reach_switcher_capsule_force_close,
        nullptr /* on_game_mode */,
        reach_switcher_capsule_needs_frame,
        nullptr /* wants_pointer_move */,
    };
    return &ops;
}

reach_result reach_switcher_create(reach_switcher **out_switcher)
{
    if (out_switcher == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_switcher *switcher = new (std::nothrow) reach_switcher();
    if (switcher == nullptr)
    {
        return REACH_ERROR;
    }
    reach_animation_manager_init(&switcher->animations, switcher->animation_tracks,
                                 REACH_SWITCHER_ANIMATION_COUNT);
    *out_switcher = switcher;
    return REACH_OK;
}

void reach_switcher_destroy(reach_switcher *switcher)
{
    delete switcher;
}

void reach_switcher_tick(reach_switcher *switcher, double delta_seconds)
{
    if (switcher == nullptr)
    {
        return;
    }
    reach_animation_manager_tick(&switcher->animations, delta_seconds);
}

void reach_switcher_reset_width_animation(reach_switcher *switcher)
{
    if (switcher == nullptr)
    {
        return;
    }
    reach_animation_manager_reset(&switcher->animations, REACH_SWITCHER_ANIMATION_WIDTH);
}

int32_t reach_switcher_width_animation_active(const reach_switcher *switcher)
{
    return switcher != nullptr &&
           reach_animation_manager_active(&switcher->animations, REACH_SWITCHER_ANIMATION_WIDTH);
}

/* --- ring rebuild + alt-tab interaction (moved out of composition) --- */

static int32_t reach_switcher_ctx_open_index(const reach_switcher_window_context *ctx,
                                             uintptr_t window_id, size_t *out_index)
{
    if (ctx == nullptr || ctx->open_windows == nullptr || window_id == 0)
    {
        return 0;
    }
    for (size_t index = 0; index < ctx->open_window_count; ++index)
    {
        if ((uintptr_t)ctx->open_windows[index].id == window_id)
        {
            if (out_index != nullptr)
            {
                *out_index = index;
            }
            return 1;
        }
    }
    return 0;
}

static int32_t reach_switcher_state_contains(const reach_switcher_state *state, uintptr_t window_id)
{
    if (state == nullptr || window_id == 0)
    {
        return 0;
    }
    for (size_t index = 0; index < state->window_count; ++index)
    {
        if (state->windows[index] == window_id)
        {
            return 1;
        }
    }
    return 0;
}

static int32_t reach_switcher_state_window_index(const reach_switcher_state *state,
                                                 uintptr_t window_id, size_t *out_index)
{
    if (state == nullptr || window_id == 0)
    {
        return 0;
    }
    for (size_t index = 0; index < state->window_count; ++index)
    {
        if (state->windows[index] == window_id)
        {
            if (out_index != nullptr)
            {
                *out_index = index;
            }
            return 1;
        }
    }
    return 0;
}

static void reach_switcher_append_ring_window(reach_switcher_state *state,
                                              const reach_switcher_window_context *ctx,
                                              uintptr_t window_id)
{
    if (state == nullptr || window_id == 0 || state->window_count >= REACH_MAX_PINNED_APPS ||
        reach_switcher_state_contains(state, window_id) ||
        !reach_switcher_ctx_open_index(ctx, window_id, nullptr))
    {
        return;
    }
    state->windows[state->window_count++] = window_id;
}

static void reach_switcher_rebuild_ring(reach_switcher_state *state,
                                        const reach_switcher_window_context *ctx)
{
    state->window_count = 0;
    for (size_t index = 0; index < REACH_MAX_PINNED_APPS; ++index)
    {
        state->windows[index] = 0;
    }
    if (ctx == nullptr)
    {
        return;
    }

    reach_switcher_append_ring_window(state, ctx, ctx->foreground_window);
    for (size_t index = 0; index < ctx->focus_history_count; ++index)
    {
        reach_switcher_append_ring_window(state, ctx, ctx->focus_history[index]);
    }
    for (size_t index = 0; index < ctx->open_window_count; ++index)
    {
        reach_switcher_append_ring_window(state, ctx, (uintptr_t)ctx->open_windows[index].id);
    }
}

static void reach_switcher_apply_visible_start(reach_switcher_state *state)
{
    reach_switcher_model model = {};
    model.window_count = state->window_count;
    model.selected_index = state->selected_index;
    model.visible_start = state->visible_start;
    reach_switcher_update_visible_start(&model);
    state->visible_start = model.visible_start;
}

static reach_switcher_action
reach_switcher_handle_event_with_context(reach_switcher *switcher, const reach_ui_event *event,
                                         const reach_switcher_window_context *ctx)
{
    reach_switcher_action action = {};
    if (switcher == nullptr || event == nullptr || ctx == nullptr)
    {
        return action;
    }
    reach_switcher_state *state = &switcher->state;

    if (event->type == REACH_UI_EVENT_APP_SWITCH_BEGIN)
    {
        reach_switcher_rebuild_ring(state, ctx);
        state->open = state->window_count > 0 ? 1 : 0;
        state->selected_index = state->window_count > 1 ? 1 : 0;
        state->visible_start = 0;
        reach_switcher_reset_width_animation(switcher);
        reach_switcher_apply_visible_start(state);
        action.type = state->open ? REACH_SWITCHER_ACTION_OPENED : REACH_SWITCHER_ACTION_CLOSED;
        return action;
    }

    if (!state->open)
    {
        return action;
    }

    if (event->type == REACH_UI_EVENT_APP_SWITCH_NEXT && state->window_count > 0)
    {
        state->selected_index = (state->selected_index + 1) % state->window_count;
        reach_switcher_apply_visible_start(state);
        action.type = REACH_SWITCHER_ACTION_CHANGED;
        return action;
    }

    if (event->type == REACH_UI_EVENT_APP_SWITCH_PREVIOUS && state->window_count > 0)
    {
        state->selected_index =
            state->selected_index == 0 ? state->window_count - 1 : state->selected_index - 1;
        reach_switcher_apply_visible_start(state);
        action.type = REACH_SWITCHER_ACTION_CHANGED;
        return action;
    }

    if (event->type == REACH_UI_EVENT_APP_SWITCH_CANCEL)
    {
        state->open = 0;
        action.type = REACH_SWITCHER_ACTION_CLOSED;
        return action;
    }

    if (event->type == REACH_UI_EVENT_APP_SWITCH_COMMIT)
    {
        uintptr_t window = 0;
        if (state->selected_index < state->window_count)
        {
            window = state->windows[state->selected_index];
        }
        state->open = 0;
        action.type = REACH_SWITCHER_ACTION_COMMITTED;
        action.window = window;
        return action;
    }

    return action;
}

static reach_switcher_action
reach_switcher_sync_windows_with_context(reach_switcher *switcher,
                                         const reach_switcher_window_context *ctx)
{
    reach_switcher_action action = {};
    if (switcher == nullptr || ctx == nullptr)
    {
        return action;
    }
    reach_switcher_state *state = &switcher->state;
    if (!state->open)
    {
        return action;
    }

    uintptr_t selected_window = 0;
    size_t old_selected = state->selected_index;
    if (old_selected < state->window_count)
    {
        selected_window = state->windows[old_selected];
    }

    reach_switcher_rebuild_ring(state, ctx);
    if (state->window_count == 0)
    {
        state->open = 0;
        state->selected_index = 0;
        state->visible_start = 0;
        reach_switcher_reset_width_animation(switcher);
        action.type = REACH_SWITCHER_ACTION_CLOSED;
        return action;
    }

    size_t selected_index = 0;
    if (reach_switcher_state_window_index(state, selected_window, &selected_index))
    {
        state->selected_index = selected_index;
    }
    else if (old_selected < state->window_count)
    {
        state->selected_index = old_selected;
    }
    else
    {
        state->selected_index = state->window_count - 1;
    }

    reach_switcher_apply_visible_start(state);
    action.type = REACH_SWITCHER_ACTION_CHANGED;
    return action;
}

reach_rect_f32 reach_switcher_apply_width_animation(reach_switcher *switcher,
                                                    int32_t transition_visible, int32_t open,
                                                    int32_t bounds_valid, float last_bounds_width,
                                                    reach_rect_f32 target, int32_t *out_request_redraw)
{
    if (switcher == nullptr || !transition_visible)
    {
        return target;
    }

    float animation_target =
        reach_animation_manager_target(&switcher->animations, REACH_SWITCHER_ANIMATION_WIDTH);
    if (!bounds_valid || animation_target <= 0.0f)
    {
        reach_animation_manager_set(&switcher->animations, REACH_SWITCHER_ANIMATION_WIDTH,
                                    target.width);
    }
    else if (open && fabsf(animation_target - target.width) >= 0.5f)
    {
        float from = reach_switcher_width_animation_active(switcher)
                         ? reach_animation_manager_value(&switcher->animations,
                                                         REACH_SWITCHER_ANIMATION_WIDTH)
                         : last_bounds_width;
        reach_animation_manager_start(&switcher->animations, REACH_SWITCHER_ANIMATION_WIDTH, from,
                                      target.width, 0.18, REACH_EASING_EASE_IN_OUT);
    }

    float width = target.width;
    if (reach_switcher_width_animation_active(switcher))
    {
        width =
            reach_animation_manager_value(&switcher->animations, REACH_SWITCHER_ANIMATION_WIDTH);
        if (out_request_redraw != nullptr)
        {
            *out_request_redraw = 1;
        }
    }
    else if (reach_animation_manager_target(&switcher->animations,
                                            REACH_SWITCHER_ANIMATION_WIDTH) > 0.0f)
    {
        width =
            reach_animation_manager_target(&switcher->animations, REACH_SWITCHER_ANIMATION_WIDTH);
    }

    if (fabsf(width - target.width) < 0.5f)
    {
        width = target.width;
    }

    reach_rect_f32 animated = target;
    float center = target.x + target.width * 0.5f;
    animated.width = width;
    animated.x = center - width * 0.5f;
    return animated;
}

void reach_switcher_force_close(reach_switcher *switcher)
{
    if (switcher == nullptr)
    {
        return;
    }

    reach_switcher_state *state = reach_switcher_state_mut(switcher);
    state->open = 0;
    state->selected_index = 0;
    state->visible_start = 0;
}
