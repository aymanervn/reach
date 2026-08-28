#include "host_internal.h"

int32_t reach_host_get_pointer_position(reach_host *host, reach_point_i32 *out_pointer)
{
    return host != nullptr && out_pointer != nullptr &&
           host->input_source.ops.get_pointer_position != nullptr &&
           host->input_source.ops.get_pointer_position(host->input_source.source, out_pointer) ==
               REACH_OK;
}

static void reach_host_mark_dirty_for_event(reach_host *host, const reach_ui_event *event)
{
    REACH_ASSERT(host != nullptr);
    REACH_ASSERT(event != nullptr);
    if (host == nullptr || event == nullptr)
    {
        return;
    }

    switch (event->type)
    {
    case REACH_UI_EVENT_WINDOWS_KEY:
    case REACH_UI_EVENT_ESCAPE:
    case REACH_UI_EVENT_ENTER:
    case REACH_UI_EVENT_ARROW_UP:
    case REACH_UI_EVENT_ARROW_DOWN:
        host->dirty.layout = 1;
        host->launcher.dirty_flags = 1;
        break;

    case REACH_UI_EVENT_DOCK_APP_CLICK:
    case REACH_UI_EVENT_TRAY_BUTTON_CLICK:
    case REACH_UI_EVENT_POINTER_UP:
    case REACH_UI_EVENT_POINTER_MOVE:
    case REACH_UI_EVENT_POINTER_LEAVE:
    case REACH_UI_EVENT_POINTER_MIDDLE:
    case REACH_UI_EVENT_POINTER_DOWN:
    case REACH_UI_EVENT_POINTER_CANCEL:
    case REACH_UI_EVENT_POINTER_WHEEL:
        break;

    case REACH_UI_EVENT_NONE:
    default:
        break;
    }
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
    if (desc->definition->surface.cls == REACH_SURFACE_CLASS_POPUP && desc->surface != nullptr &&
        desc->surface->bounds_valid)
    {
        pointer.x -= (int32_t)desc->surface->last_bounds.x;
        pointer.y -= (int32_t)desc->surface->last_bounds.y;
    }
    return pointer;
}

static reach_capsule_pointer_result reach_host_dispatch_pointer(reach_host *host,
                                                                reach_surface_id surface_id,
                                                                const reach_ui_event *event,
                                                                reach_pointer_event_kind kind)
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

static const reach_feature_runtime *reach_host_source_gated_surface(reach_host *host,
                                                                    reach_surface_role source)
{
    const reach_feature_runtime *desc = reach_host_surface_for_role(host, source);
    return desc != nullptr && reach_host_surface_source_gated(desc) ? desc : nullptr;
}

static reach_result reach_host_apply_surface_action(reach_host *host, reach_surface_id id,
                                                    const reach_capsule_pointer_result *result)
{
    return reach_host_apply_feature_action(host, &host->feature_runtimes[id], result);
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

reach_result reach_host_open_launcher_result_and_close_transients(reach_host *host)
{
    reach_result activation_result = reach_host_open_launcher_result(host);
    if (activation_result == REACH_OK)
    {
        reach_host_close_transient_surfaces(host, 0);
    }
    return activation_result;
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

        if (desc->definition->id == REACH_SURFACE_ID_DOCK &&
            reach_top_bar_tray_popup_is_open(
                reach_host_feature_capsule<reach_top_bar>(host, REACH_SURFACE_ID_TOP_BAR)) &&
            (up.action.kind == REACH_FEATURE_ACTION_OPEN_PINNED_APP ||
             up.action.kind == REACH_FEATURE_ACTION_TOGGLE_WINDOW_FOCUS) &&
            !reach_rect_contains(host->tray.last_bounds, event->x, event->y))
        {
            reach_host_set_tray_popup_open(host, 0);
        }
        return reach_host_apply_surface_action(host, desc->definition->id, &up);
    }

    if (reach_top_bar_tray_popup_is_open(
            reach_host_feature_capsule<reach_top_bar>(host, REACH_SURFACE_ID_TOP_BAR)) &&
        !reach_rect_contains(host->tray.last_bounds, event->x, event->y))
    {
        reach_host_set_tray_popup_open(host, 0);
    }

    return REACH_OK;
}

static reach_result reach_host_handle_secondary_pointer_down(reach_host *host,
                                                             const reach_ui_event *event,
                                                             reach_surface_role source)
{
    reach_capsule_pointer_result clipboard_cancel = reach_host_dispatch_pointer(
        host, REACH_SURFACE_ID_CLIPBOARD, nullptr, REACH_POINTER_EVENT_CANCEL);
    (void)reach_host_apply_surface_action(host, REACH_SURFACE_ID_CLIPBOARD, &clipboard_cancel);

    reach_host_clear_sticky_dock_feedback(host);
    host->window_list.dwell_active = 0;

    if (reach_context_menu_is_open(
            reach_host_feature_capsule<reach_context_menu>(host, REACH_SURFACE_ID_CONTEXT_MENU)))
    {
        if (source == REACH_SURFACE_CONTEXT_MENU)
        {
            reach_capsule_pointer_result context_down = reach_host_dispatch_pointer(
                host, REACH_SURFACE_ID_CONTEXT_MENU, event, REACH_POINTER_EVENT_DOWN);
            if (context_down.handled)
            {
                return reach_host_apply_surface_action(host, REACH_SURFACE_ID_CONTEXT_MENU,
                                                       &context_down);
            }
        }
        reach_host_close_context_menu(host);
    }

    if (reach_quick_settings_state_ptr(
            reach_host_feature_capsule<reach_quick_settings>(host, REACH_SURFACE_ID_QUICK_SETTINGS))
            ->open)
    {
        reach_host_set_quick_settings_open(host, 0);
    }

    const reach_feature_runtime *source_desc = reach_host_source_gated_surface(host, source);
    if (source_desc != nullptr && reach_host_surface_pointer_open(source_desc))
    {
        reach_capsule_pointer_result source_down = reach_host_dispatch_pointer(
            host, source_desc->definition->id, event, REACH_POINTER_EVENT_DOWN);
        if (source_down.handled || source_down.action.kind != 0)
        {
            return reach_host_apply_surface_action(host, source_desc->definition->id, &source_down);
        }
    }

    reach_surface_id order[REACH_HOST_SURFACE_COUNT];
    size_t order_count = reach_host_pointer_order(host, order);
    for (size_t index = 0; index < order_count; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[order[index]];
        if (reach_host_surface_source_gated(desc) || !reach_host_surface_pointer_open(desc))
        {
            continue;
        }
        reach_capsule_pointer_result down = reach_host_dispatch_pointer(
            host, desc->definition->id, event, REACH_POINTER_EVENT_DOWN);
        if (down.handled || down.action.kind != 0)
        {
            return reach_host_apply_surface_action(host, desc->definition->id, &down);
        }
    }
    return REACH_OK;
}

static reach_result reach_host_handle_pointer_down(reach_host *host, const reach_ui_event *event,
                                                   reach_surface_role source)
{
    if (host == nullptr || event == nullptr || !host->has_layout)
    {
        return REACH_OK;
    }
    if (event->button == REACH_POINTER_BUTTON_SECONDARY)
    {
        return reach_host_handle_secondary_pointer_down(host, event, source);
    }

    reach_capsule_pointer_result clipboard_cancel = reach_host_dispatch_pointer(
        host, REACH_SURFACE_ID_CLIPBOARD, nullptr, REACH_POINTER_EVENT_CANCEL);
    (void)reach_host_apply_surface_action(host, REACH_SURFACE_ID_CLIPBOARD, &clipboard_cancel);

    reach_host_clear_sticky_dock_feedback(host);
    host->window_list.dwell_active = 0;

    if (reach_quick_settings_state_ptr(
            reach_host_feature_capsule<reach_quick_settings>(host, REACH_SURFACE_ID_QUICK_SETTINGS))
            ->open)
    {
        if (source == REACH_SURFACE_TOP_BAR)
        {
            reach_capsule_pointer_result top_bar_down = reach_host_dispatch_pointer(
                host, REACH_SURFACE_ID_TOP_BAR, event, REACH_POINTER_EVENT_DOWN);
            if (top_bar_down.action.kind == REACH_TOP_BAR_POINTER_ACTION_PRESS_QUICK_SETTINGS)
            {
                return REACH_OK;
            }
            reach_capsule_pointer_result top_bar_cancel = reach_host_dispatch_pointer(
                host, REACH_SURFACE_ID_TOP_BAR, nullptr, REACH_POINTER_EVENT_CANCEL);
            (void)reach_host_apply_surface_action(host, REACH_SURFACE_ID_TOP_BAR, &top_bar_cancel);
        }
        if (source == REACH_SURFACE_DOCK)
        {
            reach_capsule_pointer_result dock_cancel = reach_host_dispatch_pointer(
                host, REACH_SURFACE_ID_DOCK, nullptr, REACH_POINTER_EVENT_CANCEL);
            (void)reach_host_apply_surface_action(host, REACH_SURFACE_ID_DOCK, &dock_cancel);
        }

        if (reach_rect_contains(
                reach_quick_settings_state_ptr(reach_host_feature_capsule<reach_quick_settings>(
                                                   host, REACH_SURFACE_ID_QUICK_SETTINGS))
                    ->bounds,
                event->x, event->y))
        {
            reach_capsule_pointer_result quick_settings_down = reach_host_dispatch_pointer(
                host, REACH_SURFACE_ID_QUICK_SETTINGS, event, REACH_POINTER_EVENT_DOWN);
            reach_host_apply_surface_action(host, REACH_SURFACE_ID_QUICK_SETTINGS,
                                            &quick_settings_down);
            return REACH_OK;
        }

        reach_host_set_quick_settings_open(host, 0);
        return REACH_OK;
    }

    if (reach_context_menu_window_list_is_open(
            reach_host_feature_capsule<reach_context_menu>(host, REACH_SURFACE_ID_CONTEXT_MENU)) &&
        source == REACH_SURFACE_DOCK)
    {
        reach_host_close_context_menu(host);
    }

    if (reach_context_menu_is_open(
            reach_host_feature_capsule<reach_context_menu>(host, REACH_SURFACE_ID_CONTEXT_MENU)))
    {
        reach_capsule_pointer_result dock_down = {};
        if (source == REACH_SURFACE_DOCK)
        {
            dock_down = reach_host_dispatch_pointer(host, REACH_SURFACE_ID_DOCK, event,
                                                    REACH_POINTER_EVENT_DOWN);
        }
        reach_capsule_pointer_result top_bar_down = {};
        if (source == REACH_SURFACE_TOP_BAR)
        {
            top_bar_down = reach_host_dispatch_pointer(host, REACH_SURFACE_ID_TOP_BAR, event,
                                                       REACH_POINTER_EVENT_DOWN);
        }
        if (reach_context_menu_state_ptr(
                reach_host_feature_capsule<reach_context_menu>(host, REACH_SURFACE_ID_CONTEXT_MENU))
                ->power_open &&
            top_bar_down.action.kind == REACH_TOP_BAR_POINTER_ACTION_PRESS_POWER)
        {
            reach_host_close_context_menu(host);
            reach_host_clear_sticky_dock_feedback(host);
            reach_top_bar_suppress_power_release(
                reach_host_feature_capsule<reach_top_bar>(host, REACH_SURFACE_ID_TOP_BAR));
            return REACH_OK;
        }

        if (source == REACH_SURFACE_DOCK)
        {
            reach_capsule_pointer_result dock_cancel = reach_host_dispatch_pointer(
                host, REACH_SURFACE_ID_DOCK, nullptr, REACH_POINTER_EVENT_CANCEL);
            (void)reach_host_apply_surface_action(host, REACH_SURFACE_ID_DOCK, &dock_cancel);
        }

        reach_capsule_pointer_result context_down = reach_host_dispatch_pointer(
            host, REACH_SURFACE_ID_CONTEXT_MENU, event, REACH_POINTER_EVENT_DOWN);
        if (context_down.handled)
        {
            return reach_host_apply_surface_action(host, REACH_SURFACE_ID_CONTEXT_MENU,
                                                   &context_down);
        }

        reach_host_close_context_menu(host);
        return REACH_OK;
    }

    const reach_feature_runtime *source_desc = reach_host_source_gated_surface(host, source);
    if (source_desc != nullptr)
    {
        reach_capsule_pointer_result source_down = reach_host_dispatch_pointer(
            host, source_desc->definition->id, event, REACH_POINTER_EVENT_DOWN);
        if (source_down.handled)
        {

            if (source_desc->definition->id == REACH_SURFACE_ID_DOCK &&
                reach_top_bar_tray_popup_is_open(
                    reach_host_feature_capsule<reach_top_bar>(host, REACH_SURFACE_ID_TOP_BAR)) &&
                source_down.action.kind == REACH_DOCK_POINTER_ACTION_PRESS_ITEM)
            {
                reach_host_cancel_pointer_sequence(host, source_desc->definition->id);
                return REACH_OK;
            }
            if (source_desc->definition->surface.cls == REACH_SURFACE_CLASS_PERSISTENT &&
                reach_launcher_is_open(
                    reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER)))
            {
                reach_host_close_launcher_without_focus_restore(host);
            }
            return reach_host_apply_surface_action(host, source_desc->definition->id, &source_down);
        }
    }

    reach_surface_id order[REACH_HOST_SURFACE_COUNT];
    size_t order_count = reach_host_pointer_order(host, order);
    for (size_t index = 0; index < order_count; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[order[index]];
        if ((desc->definition->surface.pointer_flags & REACH_SURFACE_POINTER_SOURCE_GATED) != 0 ||
            !reach_host_surface_pointer_open(desc))
        {
            continue;
        }
        reach_capsule_pointer_result down = reach_host_dispatch_pointer(
            host, desc->definition->id, event, REACH_POINTER_EVENT_DOWN);
        if (down.handled || (desc->definition->surface.pointer_flags &
                             REACH_SURFACE_POINTER_DOWN_APPLIES_UNHANDLED) != 0)
        {
            return reach_host_apply_surface_action(host, desc->definition->id, &down);
        }
        if ((desc->definition->surface.pointer_flags &
             REACH_SURFACE_POINTER_DOWN_CLOSES_ON_UNHANDLED) != 0)
        {

            if (desc->definition->dismiss != nullptr)
            {
                desc->definition->dismiss(host);
            }
            else if (desc->definition->force_close != nullptr)
            {
                desc->definition->force_close(host);
            }
            return REACH_OK;
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

static void reach_host_on_surface_event(void *user, const reach_ui_event *event,
                                        reach_surface_role source)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host != nullptr && event != nullptr)
    {
        (void)reach_host_handle_surface_event(host, event, source);
    }
}

void reach_host_on_launcher_window_event(void *user, const reach_ui_event *event)
{
    reach_host_on_surface_event(user, event, REACH_SURFACE_LAUNCHER);
}

void reach_host_on_dock_window_event(void *user, const reach_ui_event *event)
{
    reach_host_on_surface_event(user, event, REACH_SURFACE_DOCK);
}

void reach_host_on_top_bar_window_event(void *user, const reach_ui_event *event)
{
    reach_host_on_surface_event(user, event, REACH_SURFACE_TOP_BAR);
}

void reach_host_on_tray_window_event(void *user, const reach_ui_event *event)
{
    reach_host_on_surface_event(user, event, REACH_SURFACE_TRAY_MENU);
}

void reach_host_on_switcher_window_event(void *user, const reach_ui_event *event)
{
    reach_host_on_surface_event(user, event, REACH_SURFACE_SWITCHER);
}

void reach_host_on_context_menu_window_event(void *user, const reach_ui_event *event)
{
    reach_host_on_surface_event(user, event, REACH_SURFACE_CONTEXT_MENU);
}

void reach_host_on_quick_settings_window_event(void *user, const reach_ui_event *event)
{
    reach_host_on_surface_event(user, event, REACH_SURFACE_QUICK_SETTINGS);
}

void reach_host_on_battery_window_event(void *user, const reach_ui_event *event)
{
    reach_host_on_surface_event(user, event, REACH_SURFACE_BATTERY);
}

void reach_host_on_system_hud_window_event(void *user, const reach_ui_event *event)
{
    reach_host_on_surface_event(user, event, REACH_SURFACE_SYSTEM_HUD);
}

void reach_host_on_clipboard_window_event(void *user, const reach_ui_event *event)
{
    reach_host_on_surface_event(user, event, REACH_SURFACE_CLIPBOARD);
}

void reach_host_on_stage_window_event(void *user, const reach_ui_event *event)
{
    reach_host_on_surface_event(user, event, REACH_SURFACE_STAGE);
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

    reach_ui_intent intent = {};

    if (event->type == REACH_UI_EVENT_CLIPBOARD_CHANGED)
    {
        reach_clipboard_feature_request_refresh(
            reach_host_feature_capsule<reach_clipboard_feature>(host, REACH_SURFACE_ID_CLIPBOARD));
        reach_host_request_update(host);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_NOW_PLAYING_CHANGED ||
        event->type == REACH_UI_EVENT_SYSTEM_STATS_CHANGED)
    {
        if (event->type == REACH_UI_EVENT_NOW_PLAYING_CHANGED &&
            reach_host_feature_capsule<reach_system_hud>(host, REACH_SURFACE_ID_SYSTEM_HUD) !=
                nullptr)
        {
            reach_system_hud_refresh_media(
                reach_host_feature_capsule<reach_system_hud>(host, REACH_SURFACE_ID_SYSTEM_HUD));
            if (reach_system_hud_state_ptr(
                    reach_host_feature_capsule<reach_system_hud>(host, REACH_SURFACE_ID_SYSTEM_HUD))
                    ->kind == REACH_SYSTEM_HUD_MEDIA)
            {
                host->system_hud.dirty_flags = 1;
            }
        }
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

    int32_t launcher_was_open = reach_launcher_is_open(
        reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER));

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

    if (event->type == REACH_UI_EVENT_LAUNCHER_SEARCH_READY)
    {
        reach_host_apply_launcher_search_results(host);

        if (reach_icon_service_take_loads_completed(host->icon_service))
        {
            host->dock.dirty_flags = 1;
            host->launcher.dirty_flags = 1;
            host->switcher.dirty_flags = 1;
            reach_host_request_update(host);
        }
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_DISPLAY_CHANGED)
    {
        host->dirty.monitors = 1;
        host->dirty.layout = 1;
        host->dock.dirty_flags = 1;
        host->launcher.dirty_flags = 1;
        host->tray.dirty_flags = 1;
        host->switcher.dirty_flags = 1;
        host->context_menu.dirty_flags = 1;
        host->quick_settings.dirty_flags = 1;
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

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[index];
        if (desc->definition->handle_routed == nullptr)
        {
            continue;
        }
        for (size_t entry = 0; entry < desc->definition->routed_event_count; ++entry)
        {
            if (desc->definition->routed_events[entry] == event->type)
            {
                return desc->definition->handle_routed(host, event);
            }
        }
    }

    if (event->type == REACH_UI_EVENT_TEXT_CHAR || event->type == REACH_UI_EVENT_TEXT_EDIT)
    {

        reach_launcher_text_event_result text_result = {};
        reach_launcher_handle_text_event(
            reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER), event,
            &text_result);
        if (text_result.redraw)
        {
            host->launcher.dirty_flags = 1;
        }
        if (text_result.relayout)
        {
            host->dirty.layout = 1;
        }
        if (text_result.redraw || text_result.relayout)
        {
            reach_host_request_update(host);
        }
        return REACH_OK;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[index];
        if (desc->definition->toggle == nullptr)
        {
            continue;
        }
        for (size_t entry = 0; entry < desc->definition->toggle_event_count; ++entry)
        {
            if (desc->definition->toggle_events[entry] == event->type)
            {
                desc->definition->toggle(host);
                break;
            }
        }
    }

    if (event->type == REACH_UI_EVENT_ESCAPE)
    {
        reach_host_set_clipboard_open(host, 0);
        reach_host_close_stage(host);
    }

    reach_result result = reach_launcher_handle_event(
        reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER), event,
        &intent);
    if (result != REACH_OK)
    {
        return result;
    }

    reach_host_mark_dirty_for_event(host, event);

    if (launcher_was_open != reach_launcher_is_open(reach_host_feature_capsule<reach_launcher>(
                                 host, REACH_SURFACE_ID_LAUNCHER)))
    {
        if (reach_launcher_is_open(
                reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER)))
        {
            reach_launcher_reset_text_edit(
                reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER));
        }
        else
        {
            reach_host_request_launcher_focus_restore(host);
        }
        reach_host_surface_transition_set(
            host, &host->launcher_transition,
            reach_launcher_is_open(
                reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER)));

        reach_host_sync_popup_mouse_hook(host);
    }

    else if (intent.type == REACH_UI_INTENT_LAUNCH_APP)
    {
        return reach_host_open_pinned_app_id(
            host, intent.id, 0,
            reach_launcher_is_open(
                reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER)));
    }
    else if (intent.type == REACH_UI_INTENT_OPEN_LAUNCHER_RESULT)
    {
        return reach_host_open_launcher_result_and_close_transients(host);
    }

    return REACH_OK;
}

reach_result reach_host_handle_event(reach_host *host, const reach_ui_event *event)
{
    return reach_host_handle_surface_event(host, event, REACH_SURFACE_SETTINGS);
}
