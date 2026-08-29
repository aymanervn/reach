#include "host_internal.h"

int32_t reach_host_get_pointer_position(reach_host *host, reach_point_i32 *out_pointer)
{
    return host != nullptr && out_pointer != nullptr &&
           host->input_source.ops.get_pointer_position != nullptr &&
           host->input_source.ops.get_pointer_position(host->input_source.source, out_pointer) ==
               REACH_OK;
}

static int32_t reach_host_game_mode_allows_event(reach_ui_event_type type)
{
    return type == REACH_UI_EVENT_CONFIG_CHANGED || type == REACH_UI_EVENT_DISPLAY_CHANGED ||
           type == REACH_UI_EVENT_WINDOW_STATE_CHANGED ||
           type == REACH_UI_EVENT_WALLPAPER_CHANGED || type == REACH_UI_EVENT_POINTER_CANCEL ||
           type == REACH_UI_EVENT_MEDIA_PREVIOUS || type == REACH_UI_EVENT_MEDIA_PLAY_PAUSE ||
           type == REACH_UI_EVENT_MEDIA_NEXT || type == REACH_UI_EVENT_VOLUME_UP ||
           type == REACH_UI_EVENT_VOLUME_DOWN || type == REACH_UI_EVENT_VOLUME_MUTE ||
           type == REACH_UI_EVENT_BRIGHTNESS_UP || type == REACH_UI_EVENT_BRIGHTNESS_DOWN ||
           type == REACH_UI_EVENT_NOW_PLAYING_CHANGED ||
           type == REACH_UI_EVENT_WINDOW_MANIPULATION_CHANGED;
}

static int32_t reach_rect_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return (float)x >= rect.x && (float)x <= rect.x + rect.width && (float)y >= rect.y &&
           (float)y <= rect.y + rect.height;
}

reach_pointer_event reach_host_surface_pointer_event(const reach_feature_runtime *desc,
                                                     const reach_ui_event *event,
                                                     reach_pointer_event_kind kind)
{
    reach_pointer_event pointer = {};
    pointer.kind = kind;
    if (desc == nullptr)
    {
        return pointer;
    }

    pointer.coordinate_space = desc->definition->surface.cls == REACH_SURFACE_CLASS_POPUP
                                   ? REACH_POINTER_COORDINATE_SURFACE_LOCAL
                                   : REACH_POINTER_COORDINATE_SCREEN;
    if (event == nullptr)
    {
        return pointer;
    }

    pointer.x = event->x;
    pointer.y = event->y;
    pointer.wheel_delta = event->wheel_delta;
    pointer.modifiers = event->modifiers;
    pointer.button = event->button;
    if (desc->surface != nullptr && desc->surface->bounds_valid)
    {
        pointer.surface_relation =
            reach_rect_contains(desc->surface->last_bounds, event->x, event->y)
                ? REACH_POINTER_SURFACE_INSIDE
                : REACH_POINTER_SURFACE_OUTSIDE;
    }
    if (desc->definition->surface.cls == REACH_SURFACE_CLASS_POPUP && desc->surface != nullptr &&
        desc->surface->bounds_valid)
    {
        pointer.x -= (int32_t)desc->surface->last_bounds.x;
        pointer.y -= (int32_t)desc->surface->last_bounds.y;
    }
    return pointer;
}

static reach_capsule_pointer_result
reach_host_dispatch_pointer_with_owner(reach_host *host, reach_surface_id surface_id,
                                       const reach_ui_event *event, reach_pointer_event_kind kind,
                                       int32_t owner_trigger)
{
    reach_capsule_pointer_result result = {};
    if (host == nullptr || surface_id >= REACH_HOST_SURFACE_COUNT)
    {
        return result;
    }
    const reach_feature_runtime *desc = &host->feature_runtimes[surface_id];
    const reach_feature_capsule_ops *ops = desc->definition->capsule_ops;
    if (ops == nullptr || ops->handle_pointer == nullptr)
    {
        return result;
    }

    reach_pointer_event pointer = reach_host_surface_pointer_event(desc, event, kind);
    pointer.owner_trigger = owner_trigger;
    ops->handle_pointer(desc->capsule, &pointer, &result);
    if (result.redraw && desc->surface != nullptr)
    {
        desc->surface->dirty_flags = 1;
    }
    if (result.relayout)
    {
        host->dirty.layout = 1;
        if ((desc->definition->surface.pointer_flags & REACH_SURFACE_POINTER_RELAYOUT_REDRAWS) !=
                0 &&
            desc->surface != nullptr)
        {
            desc->surface->dirty_flags = 1;
        }
    }
    if (result.capture != 0 && desc->surface != nullptr &&
        desc->surface->window.ops.set_pointer_capture != nullptr)
    {
        (void)desc->surface->window.ops.set_pointer_capture(desc->surface->window.window,
                                                            result.capture > 0 ? 1 : 0);
    }
    if (result.sync_pointer_subscriptions)
    {
        reach_host_sync_pointer_move_subscriptions(host);
    }
    if (result.handled || result.redraw || result.relayout || result.capture != 0)
    {
        reach_host_request_update(host);
    }
    return result;
}

static reach_capsule_pointer_result reach_host_dispatch_pointer(reach_host *host,
                                                                reach_surface_id surface_id,
                                                                const reach_ui_event *event,
                                                                reach_pointer_event_kind kind)
{
    return reach_host_dispatch_pointer_with_owner(host, surface_id, event, kind, 0);
}

static const reach_feature_runtime *reach_host_surface_for_role(reach_host *host,
                                                                reach_surface_role role)
{
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[index];
        if (desc->definition->surface.role == role && desc->definition->capsule_ops != nullptr)
        {
            return desc;
        }
    }
    return nullptr;
}

static size_t reach_host_pointer_order(reach_host *host,
                                       reach_surface_id out[REACH_HOST_SURFACE_COUNT])
{
    size_t count = 0;
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[index];
        if (desc->definition->capsule_ops == nullptr ||
            desc->definition->capsule_ops->handle_pointer == nullptr)
        {
            continue;
        }
        size_t at = count;
        while (at > 0 && host->feature_runtimes[out[at - 1]].definition->surface.pointer_priority >
                             desc->definition->surface.pointer_priority)
        {
            out[at] = out[at - 1];
            --at;
        }
        out[at] = desc->definition->id;
        ++count;
    }
    return count;
}

static int32_t reach_host_surface_pointer_open(const reach_feature_runtime *desc)
{
    return desc->definition->capsule_ops->is_open == nullptr ||
           desc->definition->capsule_ops->is_open(desc->capsule);
}

static int32_t reach_host_surface_source_gated(const reach_feature_runtime *desc)
{
    return (desc->definition->surface.pointer_flags & REACH_SURFACE_POINTER_SOURCE_GATED) != 0;
}

static int32_t reach_host_surface_owns_pointer(const reach_feature_runtime *desc,
                                               reach_surface_role source)
{
    return desc->definition->surface.role == source ||
           (desc->definition->capsule_ops->pointer_sequence_active != nullptr &&
            desc->definition->capsule_ops->pointer_sequence_active(desc->capsule));
}

static reach_result reach_host_apply_surface_action(reach_host *host, reach_surface_id id,
                                                    const reach_capsule_pointer_result *result)
{
    return reach_host_apply_feature_action(host, &host->feature_runtimes[id], &result->action);
}

static reach_result reach_host_handle_pointer_wheel(reach_host *host, const reach_ui_event *event)
{
    if (host == nullptr || event == nullptr || !host->has_layout)
    {
        return REACH_OK;
    }

    reach_surface_id order[REACH_HOST_SURFACE_COUNT];
    size_t order_count = reach_host_pointer_order(host, order);
    for (size_t index = 0; index < order_count; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[order[index]];
        if (!reach_host_surface_pointer_open(desc))
        {
            continue;
        }
        reach_capsule_pointer_result wheel = reach_host_dispatch_pointer(
            host, desc->definition->id, event, REACH_POINTER_EVENT_WHEEL);
        if (wheel.handled || wheel.action.kind != 0)
        {
            return reach_host_apply_surface_action(host, desc->definition->id, &wheel);
        }
    }

    return REACH_OK;
}

reach_result reach_host_dispatch_capsule_event(reach_host *host, reach_feature_runtime *runtime,
                                               const reach_ui_event *event, int32_t *out_handled)
{
    if (out_handled != nullptr)
    {
        *out_handled = 0;
    }
    if (host == nullptr || runtime == nullptr || event == nullptr || runtime->capsule == nullptr ||
        runtime->definition->capsule_ops == nullptr ||
        runtime->definition->capsule_ops->handle_event == nullptr)
    {
        return REACH_OK;
    }

    int32_t was_open = reach_host_surface_is_open(runtime);
    reach_capsule_event_result result = {};
    runtime->definition->capsule_ops->handle_event(runtime->capsule, event, &result);

    int32_t open = reach_host_surface_is_open(runtime);
    if (open != was_open)
    {
        if (open)
        {
            reach_host_surface_opening(host, runtime->definition->id,
                                       runtime->definition->surface.opening_origin);
        }
        else
        {
            reach_host_arm_focus_restore(host, runtime->definition->id);
        }
        reach_host_apply_surface_open_change(host, runtime, open);
    }

    reach_feature_tick_result tick = {};
    tick.redraw = result.redraw;
    tick.relayout = result.relayout;
    tick.request_update = result.request_update;
    reach_host_apply_feature_tick_result(host, runtime, &tick);

    if (out_handled != nullptr)
    {
        *out_handled = result.handled;
    }
    if (result.action.kind != REACH_FEATURE_ACTION_NONE)
    {
        return reach_host_apply_feature_action(host, runtime, &result.action);
    }
    return REACH_OK;
}

static int32_t reach_host_surface_sequence_active(const reach_feature_runtime *desc)
{
    return desc->definition->capsule_ops != nullptr &&
           desc->definition->capsule_ops->pointer_sequence_active != nullptr &&
           desc->definition->capsule_ops->pointer_sequence_active(desc->capsule);
}

static void reach_host_cancel_pointer_sequence(reach_host *host, reach_surface_id id)
{
    if (!reach_host_surface_sequence_active(&host->feature_runtimes[id]))
    {
        return;
    }
    reach_capsule_pointer_result cancel =
        reach_host_dispatch_pointer(host, id, nullptr, REACH_POINTER_EVENT_CANCEL);
    (void)reach_host_apply_surface_action(host, id, &cancel);
}

static void reach_host_cancel_other_pointer_sequences(reach_host *host, reach_surface_id keep)
{
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        if (host->feature_runtimes[index].definition->id != keep)
        {
            reach_host_cancel_pointer_sequence(host, host->feature_runtimes[index].definition->id);
        }
    }
}

int32_t reach_host_popup_owner_trigger(const reach_feature_runtime *popup,
                                       const reach_feature_runtime *source,
                                       const reach_capsule_pointer_result *source_result)
{
    if (popup == nullptr || source == nullptr || source_result == nullptr ||
        !source_result->control.valid)
    {
        return 0;
    }

    reach_feature_layout_anchor owner = {};
    owner.surface = popup->definition->layout.anchor;
    owner.slot = popup->definition->layout.anchor_slot;
    if (popup->definition->surface_ops->layout_anchor != nullptr)
    {
        (void)popup->definition->surface_ops->layout_anchor(popup->capsule, &owner);
    }
    return owner.surface == source->definition->id && owner.slot == source_result->control.slot &&
           owner.index == source_result->control.index;
}

typedef struct reach_host_popup_pointer_result
{
    reach_result result;
    int32_t consume_source;
} reach_host_popup_pointer_result;

static reach_host_popup_pointer_result
reach_host_route_open_popup_pointer_down(reach_host *host, const reach_ui_event *event,
                                         const reach_feature_runtime *source,
                                         const reach_capsule_pointer_result *source_down)
{
    reach_surface_id order[REACH_HOST_SURFACE_COUNT];
    size_t order_count = reach_host_pointer_order(host, order);
    for (size_t index = 0; index < order_count; ++index)
    {
        reach_feature_runtime *popup = &host->feature_runtimes[order[index]];
        if (popup == source || popup->definition->surface.cls != REACH_SURFACE_CLASS_POPUP ||
            !reach_host_surface_pointer_open(popup))
        {
            continue;
        }

        int32_t owner_trigger = reach_host_popup_owner_trigger(popup, source, source_down);
        reach_capsule_pointer_result popup_down = reach_host_dispatch_pointer_with_owner(
            host, popup->definition->id, event, REACH_POINTER_EVENT_DOWN, owner_trigger);
        if (!popup_down.handled && popup_down.action.kind == REACH_FEATURE_ACTION_NONE &&
            !popup_down.continue_source_sequence && !popup_down.cancel_source_sequence)
        {
            continue;
        }

        if (popup_down.cancel_source_sequence && source != nullptr)
        {
            reach_host_cancel_pointer_sequence(host, source->definition->id);
        }
        reach_result result =
            reach_host_apply_surface_action(host, popup->definition->id, &popup_down);
        if (result != REACH_OK || !popup_down.continue_source_sequence)
        {
            return {result, 1};
        }
        return {REACH_OK, 0};
    }
    return {REACH_OK, 0};
}

static const reach_feature_runtime *reach_host_pointer_capture_owner(reach_host *host,
                                                                     uint32_t flag)
{
    const reach_feature_runtime *owner = nullptr;
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[index];
        if (desc->definition->capsule_ops == nullptr ||
            desc->definition->capsule_ops->pointer_capture_active == nullptr ||
            !desc->definition->capsule_ops->pointer_capture_active(desc->capsule) ||
            (flag != 0 && (desc->definition->surface.pointer_flags & flag) == 0))
        {
            continue;
        }
        if (owner == nullptr || desc->definition->surface.pointer_priority >
                                    owner->definition->surface.pointer_priority)
        {
            owner = desc;
        }
    }
    return owner;
}

static const reach_feature_runtime *reach_host_pointer_exclusive_surface(reach_host *host)
{
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[index];
        if ((desc->definition->surface.pointer_flags &
             REACH_SURFACE_POINTER_EXCLUSIVE_WHILE_OPEN) != 0 &&
            desc->definition->capsule_ops != nullptr && reach_host_surface_pointer_open(desc))
        {
            return desc;
        }
    }
    return nullptr;
}

static reach_result reach_host_handle_secondary_pointer_up(reach_host *host,
                                                           const reach_ui_event *event,
                                                           reach_surface_role source)
{
    reach_surface_id order[REACH_HOST_SURFACE_COUNT];
    size_t order_count = reach_host_pointer_order(host, order);
    for (size_t index = 0; index < order_count; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[order[index]];
        if (!reach_host_surface_pointer_open(desc) ||
            (reach_host_surface_source_gated(desc) &&
             !reach_host_surface_owns_pointer(desc, source)))
        {
            continue;
        }
        reach_capsule_pointer_result up =
            reach_host_dispatch_pointer(host, desc->definition->id, event, REACH_POINTER_EVENT_UP);
        if (up.handled || up.action.kind != 0)
        {
            return reach_host_apply_surface_action(host, desc->definition->id, &up);
        }
    }
    return REACH_OK;
}

static reach_result reach_host_handle_pointer_up(reach_host *host, const reach_ui_event *event,
                                                 reach_surface_role source)
{
    if (host == nullptr || event == nullptr || !host->has_layout)
    {
        return REACH_OK;
    }
    if (event->button == REACH_POINTER_BUTTON_SECONDARY)
    {
        return reach_host_handle_secondary_pointer_up(host, event, source);
    }

    const reach_feature_runtime *exclusive = reach_host_pointer_exclusive_surface(host);
    if (exclusive != nullptr)
    {
        reach_capsule_pointer_result exclusive_up = reach_host_dispatch_pointer(
            host, exclusive->definition->id, event, REACH_POINTER_EVENT_UP);
        return reach_host_apply_surface_action(host, exclusive->definition->id, &exclusive_up);
    }

    const reach_feature_runtime *capture = reach_host_pointer_capture_owner(host, 0);
    if (capture != nullptr)
    {
        reach_capsule_pointer_result capture_up = reach_host_dispatch_pointer(
            host, capture->definition->id, event, REACH_POINTER_EVENT_UP);
        if (capture_up.handled || (capture->definition->surface.pointer_flags &
                                   REACH_SURFACE_POINTER_CAPTURE_CONSUMES_RELEASE) != 0)
        {
            reach_host_cancel_other_pointer_sequences(host, capture->definition->id);
            return reach_host_apply_surface_action(host, capture->definition->id, &capture_up);
        }
    }

    reach_surface_id order[REACH_HOST_SURFACE_COUNT];
    size_t order_count = reach_host_pointer_order(host, order);
    for (size_t index = 0; index < order_count; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[order[index]];
        if ((desc->definition->surface.pointer_flags &
             REACH_SURFACE_POINTER_EXCLUSIVE_WHILE_OPEN) != 0 ||
            !reach_host_surface_pointer_open(desc))
        {
            continue;
        }
        if (reach_host_surface_source_gated(desc) && !reach_host_surface_owns_pointer(desc, source))
        {
            continue;
        }
        reach_capsule_pointer_result up =
            reach_host_dispatch_pointer(host, desc->definition->id, event, REACH_POINTER_EVENT_UP);
        if (!up.handled)
        {
            continue;
        }
        reach_host_cancel_other_pointer_sequences(host, desc->definition->id);
        return reach_host_apply_surface_action(host, desc->definition->id, &up);
    }

    return REACH_OK;
}

static int32_t reach_host_consume_pointer_down(reach_host *host,
                                               const reach_feature_runtime *runtime,
                                               const reach_capsule_pointer_result *down,
                                               reach_result *out)
{
    uint32_t flags = runtime->definition->surface.pointer_flags;
    if (down->handled || (flags & REACH_SURFACE_POINTER_DOWN_APPLIES_UNHANDLED) != 0)
    {
        *out = reach_host_apply_surface_action(host, runtime->definition->id, down);
        return 1;
    }
    if ((flags & REACH_SURFACE_POINTER_DOWN_CLOSES_ON_UNHANDLED) != 0)
    {
        reach_host_close_registered_surface(host, runtime->definition->id,
                                            REACH_SURFACE_CLOSE_DISMISS);
        *out = REACH_OK;
        return 1;
    }
    return 0;
}

static reach_result reach_host_handle_pointer_down(reach_host *host, const reach_ui_event *event,
                                                   reach_surface_role source)
{
    if (host == nullptr || event == nullptr || !host->has_layout)
    {
        return REACH_OK;
    }

    reach_capsule_pointer_result clipboard_cancel = reach_host_dispatch_pointer(
        host, REACH_SURFACE_ID_CLIPBOARD, nullptr, REACH_POINTER_EVENT_CANCEL);
    (void)reach_host_apply_surface_action(host, REACH_SURFACE_ID_CLIPBOARD, &clipboard_cancel);

    reach_host_clear_sticky_dock_feedback(host);

    const reach_feature_runtime *source_runtime = reach_host_surface_for_role(host, source);
    if (source_runtime != nullptr && !reach_host_surface_pointer_open(source_runtime))
    {
        source_runtime = nullptr;
    }
    reach_capsule_pointer_result source_down = {};
    if (source_runtime != nullptr)
    {
        source_down = reach_host_dispatch_pointer(host, source_runtime->definition->id, event,
                                                  REACH_POINTER_EVENT_DOWN);
    }

    reach_host_popup_pointer_result popup =
        reach_host_route_open_popup_pointer_down(host, event, source_runtime, &source_down);
    if (popup.result != REACH_OK || popup.consume_source)
    {
        return popup.result;
    }

    if (source_runtime != nullptr)
    {
        if (source_down.handled && event->button == REACH_POINTER_BUTTON_PRIMARY &&
            source_runtime->definition->surface.cls == REACH_SURFACE_CLASS_PERSISTENT)
        {
            reach_host_close_surfaces_on_persistent_press(host);
        }
        reach_result source_result = REACH_OK;
        if (reach_host_consume_pointer_down(host, source_runtime, &source_down, &source_result))
        {
            return source_result;
        }
    }

    reach_surface_id order[REACH_HOST_SURFACE_COUNT];
    size_t order_count = reach_host_pointer_order(host, order);
    for (size_t index = 0; index < order_count; ++index)
    {
        const reach_feature_runtime *runtime = &host->feature_runtimes[order[index]];
        if (runtime == source_runtime ||
            (runtime->definition->surface.pointer_flags & REACH_SURFACE_POINTER_SOURCE_GATED) !=
                0 ||
            !reach_host_surface_pointer_open(runtime))
        {
            continue;
        }
        reach_capsule_pointer_result down = reach_host_dispatch_pointer(
            host, runtime->definition->id, event, REACH_POINTER_EVENT_DOWN);
        reach_result result = REACH_OK;
        if (reach_host_consume_pointer_down(host, runtime, &down, &result))
        {
            return result;
        }
    }

    return REACH_OK;
}

static reach_result reach_host_handle_pointer_move(reach_host *host, const reach_ui_event *event,
                                                   reach_surface_role source)
{
    if (host == nullptr || event == nullptr || !host->has_layout)
    {
        return REACH_OK;
    }

    if (host->pointer_moved_route != nullptr)
    {
        host->pointer_moved_route(host, reach_point_i32{event->x, event->y});
    }

    const reach_feature_runtime *move_owner =
        reach_host_pointer_capture_owner(host, REACH_SURFACE_POINTER_CAPTURE_OWNS_MOVE);
    if (move_owner != nullptr)
    {
        reach_capsule_pointer_result owned_move = reach_host_dispatch_pointer(
            host, move_owner->definition->id, event, REACH_POINTER_EVENT_MOVE);
        (void)reach_host_apply_surface_action(host, move_owner->definition->id, &owned_move);
        return REACH_OK;
    }

    reach_surface_id order[REACH_HOST_SURFACE_COUNT];
    size_t order_count = reach_host_pointer_order(host, order);
    for (size_t index = 0; index < order_count; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[order[index]];
        int32_t wants = desc->definition->capsule_ops->wants_pointer_move != nullptr &&
                        desc->definition->capsule_ops->wants_pointer_move(desc->capsule);
        if ((desc->definition->surface.role != source && !wants) ||
            !reach_host_surface_pointer_open(desc))
        {
            continue;
        }
        reach_capsule_pointer_result move = reach_host_dispatch_pointer(
            host, desc->definition->id, event, REACH_POINTER_EVENT_MOVE);
        if (move.handled || move.action.kind != 0)
        {
            return reach_host_apply_surface_action(host, desc->definition->id, &move);
        }
    }

    return REACH_OK;
}

static reach_result reach_host_handle_pointer_middle(reach_host *host, const reach_ui_event *event,
                                                     reach_surface_role source)
{
    if (host == nullptr || event == nullptr || !host->has_layout)
    {
        return REACH_OK;
    }

    reach_capsule_pointer_result tray_middle =
        reach_host_dispatch_pointer(host, REACH_SURFACE_ID_TRAY, event, REACH_POINTER_EVENT_MIDDLE);
    (void)reach_host_apply_surface_action(host, REACH_SURFACE_ID_TRAY, &tray_middle);

    const reach_feature_runtime *src = reach_host_surface_for_role(host, source);
    if (src != nullptr && src->definition->id != REACH_SURFACE_ID_TRAY &&
        src->definition->capsule_ops->handle_pointer != nullptr)
    {
        reach_capsule_pointer_result middle = reach_host_dispatch_pointer(
            host, src->definition->id, event, REACH_POINTER_EVENT_MIDDLE);
        return reach_host_apply_surface_action(host, src->definition->id, &middle);
    }

    return REACH_OK;
}

static reach_result reach_host_handle_pointer_leave(reach_host *host, reach_surface_role source)
{
    if (host == nullptr)
    {
        return REACH_OK;
    }

    const reach_feature_runtime *src = reach_host_surface_for_role(host, source);
    if (src == nullptr)
    {
        return REACH_OK;
    }

    if (src->definition->capsule_ops->handle_pointer != nullptr)
    {
        reach_capsule_pointer_result leave = reach_host_dispatch_pointer(
            host, src->definition->id, nullptr, REACH_POINTER_EVENT_LEAVE);
        (void)reach_host_apply_surface_action(host, src->definition->id, &leave);
    }
    if (src->definition->surface.bar_reveal.ops != nullptr)
    {
        reach_host_request_bar_visibility_update(host);
    }

    return REACH_OK;
}

static reach_result reach_host_handle_pointer_cancel(reach_host *host)
{
    if (host == nullptr)
    {
        return REACH_OK;
    }

    reach_surface_id order[REACH_HOST_SURFACE_COUNT];
    size_t order_count = reach_host_pointer_order(host, order);
    for (size_t index = 0; index < order_count; ++index)
    {
        reach_capsule_pointer_result cancel =
            reach_host_dispatch_pointer(host, order[index], nullptr, REACH_POINTER_EVENT_CANCEL);
        (void)reach_host_apply_surface_action(host, order[index], &cancel);
    }

    return REACH_OK;
}

static reach_result reach_host_handle_surface_event(reach_host *host, const reach_ui_event *event,
                                                    reach_surface_role source);

void reach_host_on_registered_surface_event(void *user, const reach_ui_event *event)
{
    reach_host_surface_event_binding *binding =
        static_cast<reach_host_surface_event_binding *>(user);
    if (binding != nullptr && binding->host != nullptr && binding->runtime != nullptr &&
        binding->runtime->definition != nullptr && event != nullptr)
    {
        (void)reach_host_handle_surface_event(binding->host, event,
                                              binding->runtime->definition->surface.role);
    }
}

static reach_result reach_host_handle_surface_event(reach_host *host, const reach_ui_event *event,
                                                    reach_surface_role source)
{
    REACH_ASSERT(host != nullptr);
    REACH_ASSERT(event != nullptr);
    if (host == nullptr || event == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (event->type == REACH_UI_EVENT_NOW_PLAYING_CHANGED ||
        event->type == REACH_UI_EVENT_SYSTEM_STATS_CHANGED)
    {
        reach_feature_notification notification = {};
        notification.kind = event->type == REACH_UI_EVENT_NOW_PLAYING_CHANGED
                                ? REACH_FEATURE_NOTIFICATION_NOW_PLAYING_CHANGED
                                : REACH_FEATURE_NOTIFICATION_SYSTEM_STATS_CHANGED;
        reach_host_notify_registered_features(host, &notification);
        reach_host_request_update(host);
        return REACH_OK;
    }

    if (reach_host_game_mode_enabled(host) && !reach_host_game_mode_allows_event(event->type))
    {
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_POINTER_DOWN)
    {
        const reach_feature_runtime *source_desc = reach_host_surface_for_role(host, source);
        if (source_desc != nullptr)
        {
            reach_host_invalidate_surface_z_order(host, source_desc->definition->id);
        }
    }

    if (event->type == REACH_UI_EVENT_POINTER_UP)
    {
        return reach_host_handle_pointer_up(host, event, source);
    }

    if (event->type == REACH_UI_EVENT_POINTER_DOWN)
    {
        return reach_host_handle_pointer_down(host, event, source);
    }

    if (event->type == REACH_UI_EVENT_POINTER_MOVE)
    {
        return reach_host_handle_pointer_move(host, event, source);
    }

    if (event->type == REACH_UI_EVENT_POINTER_WHEEL)
    {
        return reach_host_handle_pointer_wheel(host, event);
    }

    if (event->type == REACH_UI_EVENT_POINTER_MIDDLE)
    {
        return reach_host_handle_pointer_middle(host, event, source);
    }

    if (event->type == REACH_UI_EVENT_POINTER_LEAVE)
    {
        return reach_host_handle_pointer_leave(host, source);
    }

    if (event->type == REACH_UI_EVENT_POINTER_CANCEL)
    {
        return reach_host_handle_pointer_cancel(host);
    }

    if (event->type == REACH_UI_EVENT_WALLPAPER_CHANGED)
    {
        reach_host_reload_wallpaper(host, 1);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_CONFIG_CHANGED)
    {
        if (reach_host_apply_config_update(host))
        {
            return REACH_OK;
        }
        return reach_host_request_config_reload(host);
    }

    if (event->type == REACH_UI_EVENT_FEATURE_WORK_READY)
    {
        if (reach_icon_service_take_loads_completed(host->icon_service))
        {
            reach_host_mark_all_surfaces_dirty(host);
        }
        reach_host_request_update(host);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_DISPLAY_CHANGED)
    {
        host->dirty.monitors = 1;
        host->dirty.layout = 1;
        reach_host_mark_all_surfaces_dirty(host);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_WINDOW_STATE_CHANGED)
    {
        reach_host_refresh_window_world(host);
        reach_host_sync_window_manipulation(host);
        reach_host_sync_stage_window_states(host);
        reach_host_apply_foreground_change(host);
        (void)reach_host_update_game_mode(host);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_WINDOW_MANIPULATION_CHANGED)
    {
        reach_host_sync_window_manipulation(host);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_POINTER_REGION_CHANGED)
    {
        reach_host_request_bar_visibility_update(host);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_FOREGROUND_CHANGED)
    {
        reach_host_apply_foreground_change(host);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_WINDOW_FOCUS_LOST)
    {
        reach_host_close_activating_surfaces_on_focus_loss(host);
        reach_host_request_update(host);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_MINIMIZE_ALL)
    {
        (void)reach_host_schedule_minimize_open_windows(host);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_SNAP_LEFT)
    {
        return reach_host_snap_foreground_window(host, REACH_SPLIT_LEFT);
    }

    if (event->type == REACH_UI_EVENT_SNAP_RIGHT)
    {
        return reach_host_snap_foreground_window(host, REACH_SPLIT_RIGHT);
    }

    if (event->type == REACH_UI_EVENT_SNAP_TOP)
    {
        return reach_host_snap_foreground_window(host, REACH_SPLIT_TOP);
    }

    if (event->type == REACH_UI_EVENT_SNAP_BOTTOM)
    {
        return reach_host_snap_foreground_window(host, REACH_SPLIT_BOTTOM);
    }

    if (event->type == REACH_UI_EVENT_OPEN_TERMINAL)
    {
        (void)reach_host_schedule_open_terminal(host);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_MEDIA_PREVIOUS)
    {
        return reach_host_execute_media_action(host, REACH_NOW_PLAYING_ACTION_PREVIOUS);
    }

    if (event->type == REACH_UI_EVENT_MEDIA_PLAY_PAUSE)
    {
        return reach_host_execute_media_action(host, REACH_NOW_PLAYING_ACTION_PLAY_PAUSE);
    }

    if (event->type == REACH_UI_EVENT_MEDIA_NEXT)
    {
        return reach_host_execute_media_action(host, REACH_NOW_PLAYING_ACTION_NEXT);
    }

    if (event->type == REACH_UI_EVENT_VOLUME_UP)
    {
        return reach_host_step_main_volume(host, 0.02f);
    }

    if (event->type == REACH_UI_EVENT_VOLUME_DOWN)
    {
        return reach_host_step_main_volume(host, -0.02f);
    }

    if (event->type == REACH_UI_EVENT_VOLUME_MUTE)
    {
        return reach_host_toggle_main_volume_mute(host);
    }

    if (event->type == REACH_UI_EVENT_BRIGHTNESS_UP)
    {
        return reach_host_step_brightness(host, 0.02f);
    }

    if (event->type == REACH_UI_EVENT_BRIGHTNESS_DOWN)
    {
        return reach_host_step_brightness(host, -0.02f);
    }

    if (event->type == REACH_UI_EVENT_APP_SWITCH_BEGIN)
    {
        reach_host_refresh_window_world(host);
        reach_host_apply_foreground_change(host);
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_feature_runtime *desc = &host->feature_runtimes[index];
        for (size_t entry = 0; entry < desc->definition->routed_event_count; ++entry)
        {
            if (desc->definition->routed_events[entry] != event->type)
            {
                continue;
            }
            reach_result routed = reach_host_dispatch_capsule_event(host, desc, event, nullptr);
            if (routed != REACH_OK)
            {
                return routed;
            }
            break;
        }
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[index];
        for (size_t entry = 0; entry < desc->definition->toggle_event_count; ++entry)
        {
            if (desc->definition->toggle_events[entry] == event->type)
            {
                reach_host_toggle_registered_surface(host, desc->definition->id);
                break;
            }
        }
    }

    if (event->type == REACH_UI_EVENT_ESCAPE)
    {
        reach_host_set_registered_surface_open(host, REACH_SURFACE_ID_CLIPBOARD, 0);
        reach_host_close_stage(host);
    }

    return REACH_OK;
}

reach_result reach_host_handle_event(reach_host *host, const reach_ui_event *event)
{
    return reach_host_handle_surface_event(host, event, REACH_SURFACE_SETTINGS);
}
