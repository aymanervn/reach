#include "reach/features/switcher.h"
#include "reach/core/typography.h"
#include "reach/features/common/presentation.h"
#include "reach/features/common/selection_pill.h"
#include "reach/features/common/window_preview_layout.h"

#include "switcher_common.h"

#include <math.h>
#include <new>

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
    reach_motion width;
    reach_selection_pill selection;
    reach_rect_f32 monitor;
    float border;
    int32_t layout_initialized;
    int32_t width_cleanup_pending;
    uint64_t next_card_id = UINT64_C(0x10000);
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
    if (switcher != nullptr && switcher->layout_initialized)
    {
        auto *state = &const_cast<reach_switcher *>(switcher)->state;
        state->bounds =
            reach_rect_motion_value_at(&switcher->layout.body, reach_motion_now_seconds());
        state->bounds.x += switcher->layout.envelope.x;
        state->bounds.y += switcher->layout.envelope.y;
    }
    return switcher != nullptr ? &switcher->state : nullptr;
}

int32_t reach_switcher_is_open(const reach_switcher *switcher)
{
    return switcher != nullptr && switcher->state.open;
}

static void reach_switcher_reset_width_animation(reach_switcher *switcher)
{
    reach_motion_set(&switcher->width, 0);
    switcher->width_cleanup_pending = 0;
    switcher->layout_initialized = 0;
}

static void reach_switcher_capsule_tick(void *capsule, double delta_seconds,
                                        reach_feature_tick_result *out)
{
    reach_switcher *switcher = static_cast<reach_switcher *>(capsule);
    if (out != nullptr)
    {
        *out = {};
    }
    int32_t was_visible = reach_feature_transition_visible(&switcher->surface_transition);
    int32_t transition_changed =
        reach_feature_transition_tick(&switcher->surface_transition, delta_seconds);
    if (was_visible && !reach_feature_transition_visible(&switcher->surface_transition) &&
        !switcher->state.open)
        reach_switcher_reset_width_animation(switcher);
    if (out != nullptr && transition_changed)
    {
        out->redraw = 1;
        out->request_update = reach_feature_transition_needs_update(&switcher->surface_transition);
    }
    if (switcher->width_cleanup_pending && !reach_motion_active(&switcher->width))
    {
        reach_motion_set(&switcher->width, switcher->width.to);
        switcher->width_cleanup_pending = 0;
        if (out != nullptr)
            out->redraw = out->request_update = 1;
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
           reach_feature_transition_needs_update(&switcher->surface_transition);
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
    out->visible_bounds = switcher->layout.envelope;
    out->envelope_bounds = switcher->layout.envelope;
    out->has_input_bounds = switcher->layout_initialized;
    out->input_bounds = switcher->layout.body;
    reach_feature_transition_presentation(&switcher->surface_transition, out);
    if (switcher->width_cleanup_pending)
        out->deadline_seconds =
            fmax(out->deadline_seconds, reach_motion_deadline(&switcher->width));
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
    reach_feature_transition_init(&switcher->surface_transition, REACH_FEATURE_PRESENTATION_MENU,
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

static void reach_switcher_retarget_width(reach_switcher *switcher,
                                          const reach_switcher_arrange_context *ctx, float target,
                                          int32_t cut, double now)
{
    if (cut || !ctx->transition_visible || switcher->width.to <= 0)
    {
        if (switcher->width.generation == 0 || switcher->width.to != target ||
            switcher->width.duration_seconds > 0)
            reach_motion_set(&switcher->width, target);
        switcher->width_cleanup_pending = 0;
    }
    else if (switcher->state.open && fabsf(switcher->width.to - target) >= .5f)
    {
        reach_motion_animate_spec_to_at(
            &switcher->width, target,
            reach_theme_motion_preset(ctx->theme, REACH_MOTION_PRESET_SMOOTH), now);
        switcher->width_cleanup_pending = 1;
    }
    else if (switcher->state.open && !reach_motion_active_at(&switcher->width, now) &&
             switcher->width.to != target)
        reach_motion_set(&switcher->width, target);
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

reach_result reach_switcher_thumbnail_motion_at(const reach_switcher *switcher, size_t index,
                                                reach_window_id *out_window,
                                                reach_rect_motion *out_destination,
                                                int32_t *out_visible)
{
    if (switcher == nullptr || out_window == nullptr || out_destination == nullptr ||
        out_visible == nullptr || index >= switcher->layout.count)
        return REACH_INVALID_ARGUMENT;
    const auto &card = switcher->layout.cards[index];
    *out_window = card.preview_visible ? card.window : 0;
    *out_visible = card.preview_visible;
    *out_destination = card.preview_motion;
    if (reach_motion_combine(&switcher->layout.body.x, 1, &card.preview_motion.x, 1,
                             switcher->layout.envelope.x, &out_destination->x) != REACH_OK ||
        reach_motion_combine(&switcher->layout.body.y, 1, &card.preview_motion.y, 1,
                             switcher->layout.envelope.y, &out_destination->y) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    return REACH_OK;
}

reach_result reach_switcher_thumbnail_at(const reach_switcher *switcher, size_t index,
                                         reach_switcher_thumbnail_placement *out)
{
    if (out == nullptr)
        return REACH_INVALID_ARGUMENT;
    reach_rect_motion motion = {};
    reach_result result =
        reach_switcher_thumbnail_motion_at(switcher, index, &out->window, &motion, &out->visible);
    if (result == REACH_OK)
        out->destination = reach_rect_motion_value_at(&motion, reach_motion_now_seconds());
    return result;
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
        changed |=
            !reach_rect_equal(card->preview, previous->preview) ||
            !reach_motion_equal(&card->preview_motion.x, &previous->preview_motion.x) ||
            !reach_motion_equal(&card->preview_motion.y, &previous->preview_motion.y) ||
            !reach_motion_equal(&card->preview_motion.width, &previous->preview_motion.width) ||
            !reach_motion_equal(&card->preview_motion.height, &previous->preview_motion.height) ||
            card->selected != previous->selected;
    }
    changed |= layout->has_selection != switcher->layout.has_selection ||
               !reach_motion_equal(&layout->selection.x, &switcher->layout.selection.x) ||
               !reach_motion_equal(&layout->selection.y, &switcher->layout.selection.y) ||
               !reach_motion_equal(&layout->selection.width, &switcher->layout.selection.width) ||
               !reach_motion_equal(&layout->selection.height, &switcher->layout.selection.height);
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
    float fit_scale = reach_window_preview_row_fit(aspects, state->window_count, height, gap, room);
    height *= fit_scale;
    gap *= fit_scale;
    float widths[REACH_MAX_OPEN_WINDOWS] = {};
    float row_width = 0.0f;
    for (size_t index = 0; index < state->window_count; ++index)
    {
        widths[index] = height * aspects[index];
        row_width += (index > 0 ? gap : 0.0f) + widths[index];
    }
    state->visible_count = state->window_count;

    reach_feature_transition_configure(&switcher->surface_transition, ctx->theme, scale,
                                       REACH_FEATURE_TRANSITION_FROM_BELOW);
    reach_rect_f32 target = {};
    target.width =
        fminf(fmaxf(row_width + padding * 2.0f + border * 2.0f, REACH_SWITCHER_MIN_WIDTH * scale),
              ctx->monitor_bounds.width - padding * 2.0f);
    float bottom_padding = (REACH_SWITCHER_PADDING - REACH_SWITCHER_SELECTION_INSET -
                            (REACH_SWITCHER_LABEL_HEIGHT - REACH_TEXT_SIZE_MEDIUM)) *
                           scale;
    target.height = height + padding + bottom_padding +
                    (REACH_SWITCHER_LABEL_GAP + REACH_SWITCHER_LABEL_HEIGHT) * scale +
                    border * 2.0f;
    double now = reach_motion_now_seconds();
    int32_t cut = !ctx->bounds_valid || !switcher->layout_initialized ||
                  !reach_rect_equal(switcher->monitor, ctx->monitor_bounds) ||
                  scale != switcher->layout.scale || border != switcher->border;
    reach_switcher_retarget_width(switcher, ctx, target.width, cut, now);
    float minimum = 0, maximum = 0;
    if (reach_motion_range(&switcher->width, &minimum, &maximum) != REACH_OK)
        return 0;
    reach_switcher_layout layout = {};
    layout.count = state->visible_count;
    layout.scale = scale;
    layout.card_scale = scale * fit_scale;
    layout.envelope.width = cut ? maximum : fmaxf(maximum, switcher->layout.envelope.width);
    layout.envelope.height =
        cut ? target.height : fmaxf(target.height, switcher->layout.envelope.height);
    layout.envelope.x =
        ctx->monitor_bounds.x + (ctx->monitor_bounds.width - layout.envelope.width) * .5f;
    layout.envelope.y =
        ctx->monitor_bounds.y + (ctx->monitor_bounds.height - layout.envelope.height) * .5f;
    layout.body.x = reach_motion_affine(&switcher->width, -.5f, layout.envelope.width * .5f);
    reach_motion_set(&layout.body.y, (layout.envelope.height - target.height) * .5f);
    layout.body.width = switcher->width;
    reach_motion_set(&layout.body.height, target.height);
    int32_t changed = !reach_rect_equal(layout.envelope, switcher->layout.envelope) ||
                      !reach_motion_equal(&layout.body.width, &switcher->layout.body.width);
    reach_motion fit = {};
    if (row_width > 0)
    {
        fit = reach_motion_affine(&switcher->width, 1 / row_width,
                                  -(padding + border) * 2 / row_width);
        fit = reach_motion_clamp(&fit, 0, 1);
    }
    else
        reach_motion_set(&fit, 1);
    layout.fit = fit;
    float prefix = 0;
    reach_rect_f32 selection = {};
    for (size_t visible = 0; visible < layout.count; ++visible)
    {
        size_t index = visible;
        reach_switcher_card *card = &layout.cards[visible];
        card->window = state->windows[index];
        card->preview = {0, 0, widths[index], height};
        auto center = reach_motion_affine(&switcher->width, .5f, 0);
        if (reach_motion_combine(&center, 1, &fit, prefix - row_width * .5f, 0,
                                 &card->preview_motion.x) != REACH_OK)
            return 0;
        card->preview_motion.y =
            reach_motion_affine(&fit, -height * .5f, border + padding + height * .5f);
        card->preview_motion.width = reach_motion_affine(&fit, widths[index], 0);
        card->preview_motion.height = reach_motion_affine(&fit, height, 0);
        for (size_t old = 0; old < switcher->layout.count; ++old)
            if (switcher->layout.cards[old].window == card->window)
                card->render_id = switcher->layout.cards[old].render_id;
        if (card->render_id == 0)
        {
            card->render_id = switcher->next_card_id;
            switcher->next_card_id += 16;
        }
        card->preview_visible = preview_visible[index];
        card->selected = index == state->selected_index;
        if (card->selected)
        {
            float settled_fit =
                row_width > 0
                    ? fminf(fmaxf((target.width - (padding + border) * 2) / row_width, 0), 1)
                    : 1;
            float inset = REACH_SWITCHER_SELECTION_INSET * scale;
            selection.x = settled_fit * (prefix - row_width * .5f) - inset;
            selection.y = border + padding + height * (1 - settled_fit) * .5f - inset;
            selection.width = widths[index] * settled_fit + inset * 2;
            selection.height = height * settled_fit + inset * 2;
            layout.has_selection = 1;
        }
        prefix += widths[index] + gap;
    }
    REACH_ASSERT(layout.count == 0 || layout.has_selection);
    if (layout.has_selection)
    {
        int32_t settle =
            cut || !ctx->transition_visible || switcher->selection.rect.width.generation == 0;
        reach_selection_pill_retarget(&switcher->selection, ctx->theme, selection,
                                      state->selected_index,
                                      settle ? REACH_SELECTION_PILL_SET
                                             : reach_selection_pill_mode_for_distance(
                                                   switcher->selection.slot, state->selected_index),
                                      now);
        layout.selection = switcher->selection.rect;
        layout.selection.x =
            reach_motion_affine(&switcher->selection.rect.x, 1, layout.envelope.width * .5f);
    }
    reach_feature_transition_set_anchor(&switcher->surface_transition, layout.envelope.width * .5f,
                                        layout.envelope.height * .5f);
    switcher->monitor = ctx->monitor_bounds;
    switcher->border = border;
    switcher->layout_initialized = 1;
    int32_t result = reach_switcher_update_layout(switcher, &layout) | changed;
    (void)reach_switcher_state_ptr(switcher);
    return result;
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
    reach_feature_transition_reset(&switcher->surface_transition);
    reach_switcher_reset_width_animation(switcher);
    switcher->layout = {};
    state->visible_count = 0;
    ++switcher->preview_generation;
}
