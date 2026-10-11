#include "reach/features/switcher.h"
#include "reach/features/common/presentation.h"
#include "reach/support/animation.h"
#include "reach/features/common/window_preview_layout.h"

#include "switcher_common.h"

#include <math.h>
#include <new>

enum
{
    REACH_SWITCHER_ANIMATION_WIDTH = 0,
    REACH_SWITCHER_ANIMATION_COUNT
};

typedef enum reach_switcher_action_type
{
    REACH_SWITCHER_ACTION_NONE = 0,
    REACH_SWITCHER_ACTION_OPENED,
    REACH_SWITCHER_ACTION_CHANGED,
    REACH_SWITCHER_ACTION_CLOSED,
    REACH_SWITCHER_ACTION_COMMITTED
} reach_switcher_action_type;

typedef struct reach_switcher_action
{
    reach_switcher_action_type type;
    reach_window_id window;
} reach_switcher_action;

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
    reach_feature_transition surface_transition;
    reach_switcher_state state;
    reach_switcher_layout layout;
    size_t preview_generation;
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

const reach_switcher_state *reach_switcher_state_ptr(const reach_switcher *switcher)
{
    return switcher != nullptr ? &switcher->state : nullptr;
}

int32_t reach_switcher_is_open(const reach_switcher *switcher)
{
    return switcher != nullptr && switcher->state.open;
}

static void reach_switcher_reset_width_animation(reach_switcher *switcher)
{
    reach_animation_manager_reset(&switcher->animations, REACH_SWITCHER_ANIMATION_WIDTH);
}

static int32_t reach_switcher_width_animation_active(const reach_switcher *switcher)
{
    return switcher != nullptr &&
           reach_animation_manager_active(&switcher->animations, REACH_SWITCHER_ANIMATION_WIDTH);
}

static void reach_switcher_capsule_tick(void *capsule, double delta_seconds,
                                        reach_feature_tick_result *out)
{
    reach_switcher *switcher = static_cast<reach_switcher *>(capsule);
    if (out != nullptr)
    {
        *out = {};
    }
    int32_t transition_changed =
        reach_feature_transition_tick(&switcher->surface_transition, delta_seconds);
    reach_animation_manager_tick(&switcher->animations, delta_seconds);
    if (out != nullptr &&
        (transition_changed || reach_feature_transition_active(&switcher->surface_transition)))
    {
        out->redraw = 1;
        out->request_update = reach_feature_transition_active(&switcher->surface_transition) ||
                              reach_feature_transition_needs_update(&switcher->surface_transition);
    }
    if (out != nullptr && reach_switcher_width_animation_active(switcher))
    {
        out->redraw = 1;
    }
}

static int32_t reach_switcher_capsule_is_open(const void *capsule)
{
    return reach_switcher_is_open(static_cast<const reach_switcher *>(capsule));
}

static int32_t reach_switcher_capsule_needs_frame(const void *capsule)
{
    const reach_switcher *switcher = static_cast<const reach_switcher *>(capsule);
    return switcher != nullptr &&
           (reach_feature_transition_active(&switcher->surface_transition) ||
            reach_feature_transition_needs_update(&switcher->surface_transition) ||
            reach_switcher_width_animation_active(switcher));
}

static int32_t reach_switcher_capsule_presentation_visible(const void *capsule)
{
    const reach_switcher *switcher = static_cast<const reach_switcher *>(capsule);
    return switcher != nullptr && reach_feature_transition_visible(&switcher->surface_transition);
}

static void reach_switcher_capsule_surface_geometry(const void *capsule,
                                                    reach_feature_surface_geometry *out)
{
    if (out == nullptr)
    {
        return;
    }
    *out = {};
    const reach_switcher *switcher = static_cast<const reach_switcher *>(capsule);
    if (switcher == nullptr)
    {
        return;
    }
    out->visible_bounds = switcher->state.bounds;
    out->envelope_bounds = switcher->state.bounds;
    reach_feature_transition_presentation(&switcher->surface_transition, out);
}

static reach_switcher_action
reach_switcher_handle_event_with_context(reach_switcher *switcher, const reach_ui_event *event,
                                         const reach_switcher_window_context *ctx);
static reach_switcher_action
reach_switcher_sync_windows_with_context(reach_switcher *switcher,
                                         const reach_switcher_window_context *ctx);

const reach_ui_event_type *reach_switcher_routed_events(size_t *out_count)
{
    static const reach_ui_event_type events[] = {
        REACH_UI_EVENT_APP_SWITCH_BEGIN,    REACH_UI_EVENT_APP_SWITCH_NEXT,
        REACH_UI_EVENT_APP_SWITCH_PREVIOUS, REACH_UI_EVENT_APP_SWITCH_COMMIT,
        REACH_UI_EVENT_APP_SWITCH_CANCEL,
    };
    if (out_count != nullptr)
    {
        *out_count = sizeof(events) / sizeof(events[0]);
    }
    return events;
}

static void reach_switcher_apply_action(reach_switcher *switcher, reach_switcher_action action,
                                        reach_capsule_event_result *out)
{
    if (switcher != nullptr && action.type == REACH_SWITCHER_ACTION_OPENED)
    {
        (void)reach_feature_transition_set_open(&switcher->surface_transition, 1);
    }
    else if (switcher != nullptr && (action.type == REACH_SWITCHER_ACTION_CLOSED ||
                                     action.type == REACH_SWITCHER_ACTION_COMMITTED))
    {
        (void)reach_feature_transition_set_open(&switcher->surface_transition, 0);
    }
    switch (action.type)
    {
    case REACH_SWITCHER_ACTION_OPENED:
    case REACH_SWITCHER_ACTION_CLOSED:
    case REACH_SWITCHER_ACTION_CHANGED:
        out->handled = 1;
        out->redraw = 1;
        out->request_update = 1;
        break;
    case REACH_SWITCHER_ACTION_COMMITTED:
        out->handled = 1;
        out->redraw = 1;
        out->request_update = 1;
        out->action.kind = REACH_FEATURE_ACTION_ACTIVATE_WINDOW;
        out->action.window = action.window;
        break;
    case REACH_SWITCHER_ACTION_NONE:
    default:
        break;
    }
}

static void reach_switcher_capsule_handle_event(void *capsule, const reach_ui_event *event,
                                                reach_capsule_event_result *out)
{
    reach_switcher *switcher = static_cast<reach_switcher *>(capsule);
    if (switcher == nullptr || event == nullptr || out == nullptr)
    {
        return;
    }
    reach_switcher_window_context ctx = reach_switcher_service_context(switcher);
    reach_switcher_apply_action(
        switcher, reach_switcher_handle_event_with_context(switcher, event, &ctx), out);
}

int32_t reach_switcher_set_open(reach_switcher *switcher, int32_t open)
{
    if (switcher == nullptr || open || !reach_switcher_is_open(switcher))
    {
        return 0;
    }
    switcher->state.open = 0;
    (void)reach_feature_transition_set_open(&switcher->surface_transition, 0);
    return 1;
}

void reach_switcher_notify_windows_changed(reach_switcher *switcher, reach_feature_tick_result *out)
{
    if (switcher == nullptr || out == nullptr)
    {
        return;
    }
    reach_capsule_event_result result = {};
    reach_switcher_window_context ctx = reach_switcher_service_context(switcher);
    reach_switcher_apply_action(switcher, reach_switcher_sync_windows_with_context(switcher, &ctx),
                                &result);
    out->redraw = result.redraw;
    out->request_update = result.request_update;
}

const reach_feature_capsule_ops *reach_switcher_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {
        nullptr,
        reach_switcher_capsule_tick,
        reach_switcher_capsule_is_open,
        nullptr,
        reach_switcher_capsule_needs_frame,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        reach_switcher_capsule_surface_geometry,
        nullptr,
        reach_switcher_capsule_handle_event,
        nullptr,
        reach_switcher_capsule_presentation_visible,
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
    reach_feature_transition_init(&switcher->surface_transition,
                                  REACH_FEATURE_TRANSITION_FROM_BELOW);
    *out_switcher = switcher;
    return REACH_OK;
}

void reach_switcher_destroy(reach_switcher *switcher)
{
    delete switcher;
}

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
    if (state == nullptr || window_id == 0 || state->window_count >= REACH_MAX_OPEN_WINDOWS ||
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
    for (size_t index = 0; index < REACH_MAX_OPEN_WINDOWS; ++index)
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
        action.type = REACH_SWITCHER_ACTION_CHANGED;
        return action;
    }

    if (event->type == REACH_UI_EVENT_APP_SWITCH_PREVIOUS && state->window_count > 0)
    {
        state->selected_index =
            state->selected_index == 0 ? state->window_count - 1 : state->selected_index - 1;
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

    action.type = REACH_SWITCHER_ACTION_CHANGED;
    return action;
}

static reach_rect_f32 reach_switcher_apply_width_animation(
    reach_switcher *switcher, const reach_switcher_arrange_context *ctx, reach_rect_f32 target)
{
    if (!ctx->transition_visible)
    {
        return target;
    }

    float animation_target =
        reach_animation_manager_target(&switcher->animations, REACH_SWITCHER_ANIMATION_WIDTH);
    if (!ctx->bounds_valid || animation_target <= 0.0f)
    {
        reach_animation_manager_set(&switcher->animations, REACH_SWITCHER_ANIMATION_WIDTH,
                                    target.width);
    }
    else if (switcher->state.open && fabsf(animation_target - target.width) >= 0.5f)
    {
        float from = reach_switcher_width_animation_active(switcher)
                         ? reach_animation_manager_value(&switcher->animations,
                                                         REACH_SWITCHER_ANIMATION_WIDTH)
                         : ctx->last_bounds.width;
        reach_animation_manager_start(&switcher->animations, REACH_SWITCHER_ANIMATION_WIDTH, from,
                                      target.width, 0.18, REACH_EASING_EASE_IN_OUT);
    }

    float width = target.width;
    if (reach_switcher_width_animation_active(switcher))
    {
        width =
            reach_animation_manager_value(&switcher->animations, REACH_SWITCHER_ANIMATION_WIDTH);
    }
    else if (reach_animation_manager_target(&switcher->animations, REACH_SWITCHER_ANIMATION_WIDTH) >
             0.0f)
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

const reach_switcher_layout *reach_switcher_card_layout(const reach_switcher *switcher)
{
    return switcher != nullptr ? &switcher->layout : nullptr;
}

size_t reach_switcher_preview_generation(const reach_switcher *switcher)
{
    return switcher != nullptr ? switcher->preview_generation : 0;
}

size_t reach_switcher_thumbnail_count(const reach_switcher *switcher)
{
    return switcher != nullptr ? switcher->layout.count : 0;
}

reach_result reach_switcher_thumbnail_at(const reach_switcher *switcher, size_t index,
                                         reach_switcher_thumbnail_placement *out)
{
    if (switcher == nullptr || out == nullptr || index >= switcher->layout.count)
    {
        return REACH_INVALID_ARGUMENT;
    }
    const reach_switcher_card *card = &switcher->layout.cards[index];
    *out = {};
    out->window = card->preview_visible ? card->window : 0;
    out->destination = card->preview;
    out->destination.x += switcher->state.bounds.x;
    out->destination.y += switcher->state.bounds.y;
    out->visible = card->preview_visible;
    return REACH_OK;
}

static int32_t reach_switcher_update_layout(reach_switcher *switcher,
                                            const reach_switcher_layout *layout)
{
    int32_t membership_changed = layout->count != switcher->layout.count;
    int32_t changed = 0;
    for (size_t index = 0; index < layout->count; ++index)
    {
        const reach_switcher_card *card = &layout->cards[index];
        const reach_switcher_card *previous = &switcher->layout.cards[index];
        membership_changed |=
            card->window != previous->window || card->preview_visible != previous->preview_visible;
        changed |= !reach_rect_equal(card->preview, previous->preview) ||
                   card->selected != previous->selected;
    }
    if (membership_changed)
    {
        ++switcher->preview_generation;
        changed = 1;
    }
    switcher->layout = *layout;
    return changed;
}

int32_t reach_switcher_arrange(reach_switcher *switcher, const reach_switcher_arrange_context *ctx)
{
    if (switcher == nullptr || ctx == nullptr || ctx->theme == nullptr ||
        ctx->monitor_bounds.width <= 0.0f || ctx->monitor_bounds.height <= 0.0f)
    {
        return 0;
    }

    reach_switcher_state *state = &switcher->state;
    float scale = ctx->dpi_scale > 0.0f ? ctx->dpi_scale : 1.0f;
    float base_border = reach_theme_border_thickness(ctx->theme, 1.0f);
    float height_units = REACH_SWITCHER_PREVIEW_HEIGHT + REACH_SWITCHER_PADDING * 4.0f +
                         REACH_SWITCHER_LABEL_GAP + REACH_SWITCHER_LABEL_HEIGHT +
                         base_border * 2.0f;
    scale = fminf(scale, ctx->monitor_bounds.height / height_units);
    scale = fminf(scale, ctx->monitor_bounds.width /
                             (REACH_SWITCHER_PADDING * 4.0f + base_border * 2.0f + 1.0f));
    float border = reach_theme_border_thickness(ctx->theme, scale);
    float padding = REACH_SWITCHER_PADDING * scale;
    float gap = REACH_SWITCHER_GAP * scale;
    float room = fmaxf(0.0f, ctx->monitor_bounds.width - padding * 4.0f - border * 2.0f);
    float aspects[REACH_MAX_OPEN_WINDOWS] = {};
    int32_t preview_visible[REACH_MAX_OPEN_WINDOWS] = {};
    for (size_t index = 0; index < state->window_count; ++index)
    {
        reach_rect_f32 frame = {};
        int32_t valid = reach_window_tracking_window_frame_bounds(
                            switcher->windows, state->windows[index], &frame) == REACH_OK &&
                        isfinite(frame.width) && isfinite(frame.height) && frame.width > 0.0f &&
                        frame.height > 0.0f;
        aspects[index] = reach_window_preview_aspect(frame);
        const reach_window_snapshot *window =
            reach_window_tracking_window_by_id(switcher->windows, state->windows[index]);
        preview_visible[index] = valid && window != nullptr && !window->minimized;
    }
    float height = reach_window_preview_height(aspects, state->window_count,
                                               REACH_SWITCHER_PREVIEW_HEIGHT * scale,
                                               REACH_SWITCHER_PREVIEW_HEIGHT * scale, room);
    float widths[REACH_MAX_OPEN_WINDOWS] = {};
    for (size_t index = 0; index < state->window_count; ++index)
    {
        widths[index] = height * aspects[index];
    }

    size_t start = state->window_count > 0 && state->visible_start < state->window_count
                       ? state->visible_start
                       : 0;
    if (state->selected_index < start)
    {
        start = state->selected_index;
    }
    reach_window_preview_range range = reach_window_preview_pack(
        widths, state->window_count, start, REACH_SWITCHER_VISIBLE_MAX, room, gap);
    size_t end = range.end;
    float row_width = range.width;
    while (state->selected_index >= end && end < state->window_count)
    {
        ++start;
        range = reach_window_preview_pack(widths, state->window_count, start,
                                          REACH_SWITCHER_VISIBLE_MAX, room, gap);
        end = range.end;
        row_width = range.width;
    }
    while (start > 0 && end - start < REACH_SWITCHER_VISIBLE_MAX &&
           row_width + gap + widths[start - 1] <= room)
    {
        --start;
        row_width += gap + widths[start];
    }
    state->visible_start = start;
    state->visible_count = end - start;

    reach_feature_transition_configure(&switcher->surface_transition, ctx->theme, scale,
                                       REACH_FEATURE_TRANSITION_FROM_BELOW);
    reach_rect_f32 target = {};
    target.width =
        fminf(fmaxf(row_width + padding * 2.0f + border * 2.0f, REACH_SWITCHER_MIN_WIDTH * scale),
              ctx->monitor_bounds.width - padding * 2.0f);
    target.height = height + padding * 2.0f +
                    (REACH_SWITCHER_LABEL_GAP + REACH_SWITCHER_LABEL_HEIGHT) * scale +
                    border * 2.0f;
    target.x = ctx->monitor_bounds.x + (ctx->monitor_bounds.width - target.width) * 0.5f;
    target.y = ctx->monitor_bounds.y + (ctx->monitor_bounds.height - target.height) * 0.5f;
    reach_rect_f32 bounds = reach_switcher_apply_width_animation(switcher, ctx, target);
    bounds.width = fminf(bounds.width, ctx->monitor_bounds.width - padding * 2.0f);
    bounds.x = ctx->monitor_bounds.x + (ctx->monitor_bounds.width - bounds.width) * 0.5f;
    int32_t changed = !reach_rect_equal(bounds, state->bounds);
    state->bounds = bounds;

    reach_switcher_layout layout = {};
    layout.count = state->visible_count;
    layout.scale = scale;
    float fit =
        reach_window_preview_row_scale(row_width, bounds.width - padding * 2.0f - border * 2.0f);
    float x = (bounds.width - row_width * fit) * 0.5f;
    float y = border + padding + height * (1.0f - fit) * 0.5f;
    for (size_t visible = 0; visible < layout.count; ++visible)
    {
        size_t index = start + visible;
        reach_switcher_card *card = &layout.cards[visible];
        card->window = state->windows[index];
        card->preview = {x, y, widths[index] * fit, height * fit};
        card->preview_visible = preview_visible[index];
        card->selected = index == state->selected_index;
        x += card->preview.width + gap * fit;
    }
    return reach_switcher_update_layout(switcher, &layout) | changed;
}

void reach_switcher_force_close(reach_switcher *switcher)
{
    if (switcher == nullptr)
    {
        return;
    }

    reach_switcher_state *state = &switcher->state;
    state->open = 0;
    state->selected_index = 0;
    state->visible_start = 0;
    reach_feature_transition_reset(&switcher->surface_transition);
    reach_switcher_reset_width_animation(switcher);
    switcher->layout = {};
    state->visible_count = 0;
    ++switcher->preview_generation;
}
