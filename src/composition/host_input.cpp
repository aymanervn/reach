#include "host_internal.h"

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
           type == REACH_UI_EVENT_NOW_PLAYING_CHANGED;
}

static int32_t reach_rect_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return (float)x >= rect.x && (float)x <= rect.x + rect.width && (float)y >= rect.y &&
           (float)y <= rect.y + rect.height;
}

static reach_result reach_host_open_launcher_result_and_close_transients(reach_host *host);

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
    const reach_surface_desc *desc = &host->surface_descs[surface_id];
    const reach_feature_capsule_ops *ops = desc->capsule_ops;
    if (ops == nullptr || ops->handle_pointer == nullptr)
    {
        return result;
    }

    reach_pointer_event pointer = {};
    pointer.kind = kind;
    if (event != nullptr)
    {
        pointer.x = event->x;
        pointer.y = event->y;
        pointer.wheel_delta = event->wheel_delta;
        pointer.modifiers = event->modifiers;
    }
    ops->handle_pointer(desc->capsule, &pointer, &result);
    if (result.redraw && desc->surface != nullptr)
    {
        desc->surface->dirty_flags = 1;
    }
    if (result.relayout)
    {
        host->dirty.layout = 1;
        if ((desc->pointer_flags & REACH_SURFACE_POINTER_RELAYOUT_REDRAWS) != 0 &&
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
        if ((desc->pointer_flags & REACH_SURFACE_POINTER_UPDATES_DOCK_VISIBILITY) != 0)
        {
            reach_host_request_dock_visibility_update(host);
        }
        reach_host_request_update(host);
    }
    return result;
}

static const reach_surface_desc *reach_host_surface_for_role(reach_host *host,
                                                             reach_surface_role role)
{
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->role == role && desc->capsule_ops != nullptr)
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
        const reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->capsule_ops == nullptr || desc->capsule_ops->handle_pointer == nullptr)
        {
            continue;
        }
        size_t at = count;
        while (at > 0 && host->surface_descs[out[at - 1]].pointer_priority > desc->pointer_priority)
        {
            out[at] = out[at - 1];
            --at;
        }
        out[at] = desc->id;
        ++count;
    }
    return count;
}

static int32_t reach_host_surface_pointer_open(const reach_surface_desc *desc)
{
    return desc->capsule_ops->is_open == nullptr || desc->capsule_ops->is_open(desc->capsule);
}

static reach_result reach_host_apply_pointer(reach_host *host, reach_surface_id id,
                                             const reach_ui_event *event,
                                             const reach_capsule_pointer_result *result)
{
    const reach_surface_desc *desc = &host->surface_descs[id];
    return desc->apply_pointer_action != nullptr ? desc->apply_pointer_action(host, event, result)
                                                 : REACH_OK;
}

reach_result reach_host_apply_launcher_pointer_action(reach_host *host, const reach_ui_event *event,
                                                      const reach_capsule_pointer_result *result)
{
    (void)event;
    if (host == nullptr || result == nullptr)
    {
        return REACH_OK;
    }

    if (result->action.kind == REACH_LAUNCHER_POINTER_ACTION_LAUNCH_PINNED)
    {
        reach_ui_event routed = {};
        routed.type = REACH_UI_EVENT_DOCK_APP_CLICK;
        routed.id = static_cast<uint32_t>(result->action.id);
        return reach_host_handle_event(host, &routed);
    }
    if (result->action.kind == REACH_LAUNCHER_POINTER_ACTION_OPEN_RESULT)
    {
        return reach_host_open_launcher_result_and_close_transients(host);
    }
    if (result->action.kind == REACH_LAUNCHER_POINTER_ACTION_REVEAL_RESULT)
    {
        reach_result reveal_result = reach_host_reveal_launcher_result(host, result->action.index);
        if (reveal_result == REACH_OK)
        {
            reach_host_close_launcher(host);
        }
        return reveal_result;
    }

    return REACH_OK;
}

reach_result
reach_host_apply_context_menu_pointer_action(reach_host *host, const reach_ui_event *event,
                                             const reach_capsule_pointer_result *result)
{
    (void)event;
    if (host == nullptr || result == nullptr)
    {
        return REACH_OK;
    }

    if (result->action.kind == REACH_CONTEXT_MENU_POINTER_ACTION_DISMISS)
    {
        reach_host_close_context_menu(host);
    }
    else if (result->action.kind == REACH_CONTEXT_MENU_POINTER_ACTION_EXECUTE)
    {
        return reach_host_execute_context_command(host, static_cast<uint32_t>(result->action.id));
    }
    return REACH_OK;
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
        const reach_surface_desc *desc = &host->surface_descs[order[index]];
        if (!reach_host_surface_pointer_open(desc))
        {
            continue;
        }
        reach_capsule_pointer_result wheel =
            reach_host_dispatch_pointer(host, desc->id, event, REACH_POINTER_EVENT_WHEEL);
        if (wheel.handled || wheel.action.kind != 0)
        {
            return reach_host_apply_pointer(host, desc->id, event, &wheel);
        }
    }

    return REACH_OK;
}

reach_result reach_host_apply_dock_pointer_action(reach_host *host, const reach_ui_event *event,
                                                  const reach_capsule_pointer_result *result)
{
    if (host == nullptr || result == nullptr)
    {
        return REACH_OK;
    }

    switch ((reach_dock_pointer_action_kind)result->action.kind)
    {
    case REACH_DOCK_POINTER_ACTION_LAUNCH_PINNED:
        return reach_host_open_pinned_app(host, result->action.index, 0, 0);
    case REACH_DOCK_POINTER_ACTION_FOCUS_WINDOW:
        return reach_host_focus_window(host, result->action.window, 1);
    case REACH_DOCK_POINTER_ACTION_LAUNCH_NEW_INSTANCE:
        return reach_host_launch_dock_item(host, result->action.index, 1);
    case REACH_DOCK_POINTER_ACTION_SHOW_ITEM_CONTEXT:
    {
        if (event == nullptr)
        {
            return REACH_INVALID_ARGUMENT;
        }
        (void)reach_host_render_dock_surface(host, &host->layout.dock);
        reach_result show_result =
            reach_host_show_dock_app_context_menu(host, result->action.index, event->x, event->y);
        if (reach_dock_feedback_stick(host->dock_capsule))
        {
            host->dock.dirty_flags = 1;
        }
        (void)reach_host_render_dock_surface(host, &host->layout.dock);
        return show_result;
    }
    case REACH_DOCK_POINTER_ACTION_TOGGLE_TRAY:
        reach_host_toggle_tray_popup(host);
        return REACH_OK;
    case REACH_DOCK_POINTER_ACTION_TOGGLE_QUICK_SETTINGS:
        reach_host_toggle_quick_settings(host);
        return REACH_OK;
    case REACH_DOCK_POINTER_ACTION_TOGGLE_POWER:
        return reach_host_show_power_context_menu(host);
    case REACH_DOCK_POINTER_ACTION_MEDIA_PREVIOUS:
        return reach_host_execute_media_action(host, REACH_NOW_PLAYING_ACTION_PREVIOUS);
    case REACH_DOCK_POINTER_ACTION_MEDIA_PLAY_PAUSE:
        return reach_host_execute_media_action(host, REACH_NOW_PLAYING_ACTION_PLAY_PAUSE);
    case REACH_DOCK_POINTER_ACTION_MEDIA_NEXT:
        return reach_host_execute_media_action(host, REACH_NOW_PLAYING_ACTION_NEXT);
    case REACH_DOCK_POINTER_ACTION_REBUILD_ITEMS:
    {
        reach_dock_build_context build_ctx = reach_host_dock_build_context(host);
        reach_dock_rebuild_items(host->dock_capsule, &build_ctx, &host->layout.dock,
                                 &host->layout.dock);
        return REACH_OK;
    }
    case REACH_DOCK_POINTER_ACTION_MOVE_PIN:
        return reach_host_schedule_move_pin(host, static_cast<uint32_t>(result->action.id),
                                            result->action.index);
    default:
        return REACH_OK;
    }
}

static reach_result reach_host_open_launcher_result_and_close_transients(reach_host *host)
{
    reach_result activation_result = reach_host_open_launcher_result(host);
    if (activation_result == REACH_OK)
    {
        reach_host_close_transient_surfaces(host, 0);
    }
    return activation_result;
}

static void reach_host_cancel_dock_pointer_sequence(reach_host *host)
{
    if (!reach_dock_pointer_sequence_active(host->dock_capsule))
    {
        return;
    }
    reach_capsule_pointer_result dock_cancel = reach_host_dispatch_pointer(
        host, REACH_SURFACE_ID_DOCK, nullptr, REACH_POINTER_EVENT_CANCEL);
    (void)reach_host_apply_dock_pointer_action(host, nullptr, &dock_cancel);
}

static reach_result reach_host_handle_pointer_up(reach_host *host, const reach_ui_event *event,
                                                 reach_surface_role source)
{
    if (host == nullptr || event == nullptr || !host->has_layout)
    {
        return REACH_OK;
    }

    if (reach_context_menu_is_open(host->context_menu_capsule))
    {
        reach_capsule_pointer_result context_up = reach_host_dispatch_pointer(
            host, REACH_SURFACE_ID_CONTEXT_MENU, event, REACH_POINTER_EVENT_UP);
        return reach_host_apply_context_menu_pointer_action(host, event, &context_up);
    }

    if (reach_dock_state_ptr(host->dock_capsule)->drag.active)
    {
        reach_capsule_pointer_result dock_drag =
            reach_host_dispatch_pointer(host, REACH_SURFACE_ID_DOCK, event, REACH_POINTER_EVENT_UP);
        return reach_host_apply_dock_pointer_action(host, event, &dock_drag);
    }

    const reach_surface_desc *quick_settings_desc =
        &host->surface_descs[REACH_SURFACE_ID_QUICK_SETTINGS];
    if (quick_settings_desc->capsule_ops->wants_pointer_move != nullptr &&
        quick_settings_desc->capsule_ops->wants_pointer_move(quick_settings_desc->capsule))
    {
        reach_capsule_pointer_result quick_settings_up = reach_host_dispatch_pointer(
            host, REACH_SURFACE_ID_QUICK_SETTINGS, event, REACH_POINTER_EVENT_UP);
        (void)reach_host_apply_quick_settings_pointer_action(host, event, &quick_settings_up);
        reach_host_cancel_dock_pointer_sequence(host);
        return REACH_OK;
    }

    const reach_surface_desc *launcher_desc = &host->surface_descs[REACH_SURFACE_ID_LAUNCHER];
    if (launcher_desc->capsule_ops->wants_pointer_move != nullptr &&
        launcher_desc->capsule_ops->wants_pointer_move(launcher_desc->capsule))
    {
        reach_capsule_pointer_result launcher_release = reach_host_dispatch_pointer(
            host, REACH_SURFACE_ID_LAUNCHER, event, REACH_POINTER_EVENT_UP);
        if (launcher_release.handled)
        {
            reach_host_cancel_dock_pointer_sequence(host);
            return reach_host_apply_launcher_pointer_action(host, event, &launcher_release);
        }
    }

    reach_surface_id order[REACH_HOST_SURFACE_COUNT];
    size_t order_count = reach_host_pointer_order(host, order);
    for (size_t index = 0; index < order_count; ++index)
    {
        const reach_surface_desc *desc = &host->surface_descs[order[index]];
        if (desc->cls == REACH_SURFACE_CLASS_PERSISTENT ||
            desc->id == REACH_SURFACE_ID_CONTEXT_MENU || !reach_host_surface_pointer_open(desc))
        {
            continue;
        }
        reach_capsule_pointer_result up =
            reach_host_dispatch_pointer(host, desc->id, event, REACH_POINTER_EVENT_UP);
        if (up.handled)
        {
            reach_host_cancel_dock_pointer_sequence(host);
            return reach_host_apply_pointer(host, desc->id, event, &up);
        }
    }

    if (source == REACH_SURFACE_DOCK || reach_dock_pointer_sequence_active(host->dock_capsule))
    {
        reach_capsule_pointer_result dock_up =
            reach_host_dispatch_pointer(host, REACH_SURFACE_ID_DOCK, event, REACH_POINTER_EVENT_UP);
        if (dock_up.handled || dock_up.action.kind != REACH_DOCK_POINTER_ACTION_NONE)
        {

            if (reach_tray_state_ptr(host->tray_capsule)->popup_open &&
                (dock_up.action.kind == REACH_DOCK_POINTER_ACTION_LAUNCH_PINNED ||
                 dock_up.action.kind == REACH_DOCK_POINTER_ACTION_FOCUS_WINDOW) &&
                !reach_rect_contains(host->tray.last_bounds, event->x, event->y))
            {
                reach_host_set_tray_popup_open(host, 0);
            }
            return reach_host_apply_dock_pointer_action(host, event, &dock_up);
        }
    }

    if (reach_tray_state_ptr(host->tray_capsule)->popup_open &&
        !reach_rect_contains(host->tray.last_bounds, event->x, event->y))
    {
        reach_host_set_tray_popup_open(host, 0);
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

    reach_capsule_pointer_result clipboard_cancel = reach_host_dispatch_pointer(
        host, REACH_SURFACE_ID_CLIPBOARD, nullptr, REACH_POINTER_EVENT_CANCEL);
    (void)reach_host_apply_clipboard_pointer_action(host, nullptr, &clipboard_cancel);

    reach_host_clear_sticky_dock_feedback(host);

    if (source == REACH_SURFACE_CLIPBOARD)
    {
        reach_capsule_pointer_result clipboard_down = reach_host_dispatch_pointer(
            host, REACH_SURFACE_ID_CLIPBOARD, event, REACH_POINTER_EVENT_DOWN);
        if (clipboard_down.handled)
        {
            return reach_host_apply_clipboard_pointer_action(host, event, &clipboard_down);
        }
    }

    if (reach_quick_settings_state_ptr(host->quick_settings_capsule)->open)
    {
        if (source == REACH_SURFACE_DOCK)
        {
            reach_capsule_pointer_result dock_down = reach_host_dispatch_pointer(
                host, REACH_SURFACE_ID_DOCK, event, REACH_POINTER_EVENT_DOWN);
            if (dock_down.action.kind == REACH_DOCK_POINTER_ACTION_PRESS_QUICK_SETTINGS)
            {
                return REACH_OK;
            }
            reach_capsule_pointer_result dock_cancel = reach_host_dispatch_pointer(
                host, REACH_SURFACE_ID_DOCK, nullptr, REACH_POINTER_EVENT_CANCEL);
            (void)reach_host_apply_dock_pointer_action(host, nullptr, &dock_cancel);
        }

        if (reach_rect_contains(
                reach_quick_settings_state_ptr(host->quick_settings_capsule)->bounds, event->x,
                event->y))
        {
            reach_capsule_pointer_result quick_settings_down = reach_host_dispatch_pointer(
                host, REACH_SURFACE_ID_QUICK_SETTINGS, event, REACH_POINTER_EVENT_DOWN);
            reach_host_apply_quick_settings_pointer_action(host, event, &quick_settings_down);
            return REACH_OK;
        }

        reach_host_set_quick_settings_open(host, 0);
        return REACH_OK;
    }

    if (reach_context_menu_is_open(host->context_menu_capsule))
    {
        reach_capsule_pointer_result dock_down = {};
        if (source == REACH_SURFACE_DOCK)
        {
            dock_down = reach_host_dispatch_pointer(host, REACH_SURFACE_ID_DOCK, event,
                                                    REACH_POINTER_EVENT_DOWN);
        }
        if (reach_context_menu_state_ptr(host->context_menu_capsule)->power_open &&
            dock_down.action.kind == REACH_DOCK_POINTER_ACTION_PRESS_POWER)
        {
            reach_host_close_context_menu(host);
            reach_host_clear_sticky_dock_feedback(host);
            reach_dock_suppress_power_release(host->dock_capsule);
            return REACH_OK;
        }

        if (source == REACH_SURFACE_DOCK)
        {
            reach_capsule_pointer_result dock_cancel = reach_host_dispatch_pointer(
                host, REACH_SURFACE_ID_DOCK, nullptr, REACH_POINTER_EVENT_CANCEL);
            (void)reach_host_apply_dock_pointer_action(host, nullptr, &dock_cancel);
        }

        reach_capsule_pointer_result context_down = reach_host_dispatch_pointer(
            host, REACH_SURFACE_ID_CONTEXT_MENU, event, REACH_POINTER_EVENT_DOWN);
        if (context_down.handled)
        {
            return reach_host_apply_context_menu_pointer_action(host, event, &context_down);
        }

        reach_host_close_context_menu(host);
        return REACH_OK;
    }

    reach_capsule_pointer_result dock_down = {};
    if (source == REACH_SURFACE_DOCK)
    {
        dock_down = reach_host_dispatch_pointer(host, REACH_SURFACE_ID_DOCK, event,
                                                REACH_POINTER_EVENT_DOWN);
        if (dock_down.handled)
        {
            if (reach_tray_state_ptr(host->tray_capsule)->popup_open &&
                dock_down.action.kind == REACH_DOCK_POINTER_ACTION_PRESS_ITEM)
            {
                reach_capsule_pointer_result dock_cancel = reach_host_dispatch_pointer(
                    host, REACH_SURFACE_ID_DOCK, nullptr, REACH_POINTER_EVENT_CANCEL);
                (void)reach_host_apply_dock_pointer_action(host, nullptr, &dock_cancel);
                return REACH_OK;
            }
            if (reach_launcher_is_open(host->launcher_capsule) &&
                dock_down.action.kind != REACH_DOCK_POINTER_ACTION_PRESS_NOW_PLAYING)
            {
                reach_host_close_launcher_without_focus_restore(host);
            }
            return REACH_OK;
        }
    }

    reach_surface_id order[REACH_HOST_SURFACE_COUNT];
    size_t order_count = reach_host_pointer_order(host, order);
    for (size_t index = 0; index < order_count; ++index)
    {
        const reach_surface_desc *desc = &host->surface_descs[order[index]];
        if ((desc->pointer_flags & REACH_SURFACE_POINTER_SOURCE_GATED) != 0 ||
            !reach_host_surface_pointer_open(desc))
        {
            continue;
        }
        reach_capsule_pointer_result down =
            reach_host_dispatch_pointer(host, desc->id, event, REACH_POINTER_EVENT_DOWN);
        if (down.handled ||
            (desc->pointer_flags & REACH_SURFACE_POINTER_DOWN_APPLIES_UNHANDLED) != 0)
        {
            return reach_host_apply_pointer(host, desc->id, event, &down);
        }
        if ((desc->pointer_flags & REACH_SURFACE_POINTER_DOWN_CLOSES_ON_UNHANDLED) != 0)
        {

            if (desc->dismiss != nullptr)
            {
                desc->dismiss(host);
            }
            else if (desc->force_close != nullptr)
            {
                desc->force_close(host);
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

    const reach_surface_desc *quick_settings_desc =
        &host->surface_descs[REACH_SURFACE_ID_QUICK_SETTINGS];
    if (quick_settings_desc->capsule_ops->wants_pointer_move != nullptr &&
        quick_settings_desc->capsule_ops->wants_pointer_move(quick_settings_desc->capsule))
    {
        reach_capsule_pointer_result quick_settings_move = reach_host_dispatch_pointer(
            host, REACH_SURFACE_ID_QUICK_SETTINGS, event, REACH_POINTER_EVENT_MOVE);
        (void)reach_host_apply_quick_settings_pointer_action(host, event, &quick_settings_move);
        return REACH_OK;
    }

    reach_surface_id order[REACH_HOST_SURFACE_COUNT];
    size_t order_count = reach_host_pointer_order(host, order);
    for (size_t index = 0; index < order_count; ++index)
    {
        const reach_surface_desc *desc = &host->surface_descs[order[index]];
        int32_t wants = desc->capsule_ops->wants_pointer_move != nullptr &&
                        desc->capsule_ops->wants_pointer_move(desc->capsule);
        if ((desc->role != source && !wants) || !reach_host_surface_pointer_open(desc))
        {
            continue;
        }
        reach_capsule_pointer_result move =
            reach_host_dispatch_pointer(host, desc->id, event, REACH_POINTER_EVENT_MOVE);
        if (move.handled || move.action.kind != 0)
        {
            return reach_host_apply_pointer(host, desc->id, event, &move);
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
    (void)reach_host_apply_tray_pointer_action(host, event, &tray_middle);

    const reach_surface_desc *src = reach_host_surface_for_role(host, source);
    if (src != nullptr && src->id != REACH_SURFACE_ID_TRAY &&
        src->capsule_ops->handle_pointer != nullptr)
    {
        reach_capsule_pointer_result middle =
            reach_host_dispatch_pointer(host, src->id, event, REACH_POINTER_EVENT_MIDDLE);
        return reach_host_apply_pointer(host, src->id, event, &middle);
    }

    return REACH_OK;
}

static reach_result reach_host_handle_pointer_leave(reach_host *host, reach_surface_role source)
{
    if (host == nullptr)
    {
        return REACH_OK;
    }

    const reach_surface_desc *src = reach_host_surface_for_role(host, source);
    if (src != nullptr && src->capsule_ops->handle_pointer != nullptr)
    {
        reach_capsule_pointer_result leave =
            reach_host_dispatch_pointer(host, src->id, nullptr, REACH_POINTER_EVENT_LEAVE);
        (void)reach_host_apply_pointer(host, src->id, nullptr, &leave);
        if (src->id == REACH_SURFACE_ID_DOCK)
        {

            reach_host_request_dock_visibility_update(host);
        }
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
        (void)reach_host_apply_pointer(host, order[index], nullptr, &cancel);
    }

    return REACH_OK;
}

static reach_result reach_host_handle_pointer_context(reach_host *host, const reach_ui_event *event,
                                                      reach_surface_role source)
{
    if (host == nullptr || event == nullptr || !host->has_layout)
    {
        return REACH_OK;
    }

    reach_host_clear_sticky_dock_feedback(host);

    if (reach_context_menu_is_open(host->context_menu_capsule))
    {
        reach_capsule_pointer_result context_context = reach_host_dispatch_pointer(
            host, REACH_SURFACE_ID_CONTEXT_MENU, event, REACH_POINTER_EVENT_CONTEXT);
        (void)reach_host_apply_context_menu_pointer_action(host, event, &context_context);
    }

    if (reach_quick_settings_state_ptr(host->quick_settings_capsule)->open)
    {
        reach_host_set_quick_settings_open(host, 0);
    }

    reach_surface_id order[REACH_HOST_SURFACE_COUNT];
    size_t order_count = reach_host_pointer_order(host, order);
    for (size_t index = 0; index < order_count; ++index)
    {
        const reach_surface_desc *desc = &host->surface_descs[order[index]];
        if (desc->id == REACH_SURFACE_ID_CONTEXT_MENU)
        {
            continue;
        }
        if ((desc->pointer_flags & REACH_SURFACE_POINTER_SOURCE_GATED) != 0 && desc->role != source)
        {
            continue;
        }
        if (!reach_host_surface_pointer_open(desc))
        {
            continue;
        }
        reach_capsule_pointer_result context =
            reach_host_dispatch_pointer(host, desc->id, event, REACH_POINTER_EVENT_CONTEXT);
        if (context.handled || context.action.kind != 0)
        {
            return reach_host_apply_pointer(host, desc->id, event, &context);
        }
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

void reach_host_on_clipboard_window_event(void *user, const reach_ui_event *event)
{
    reach_host_on_surface_event(user, event, REACH_SURFACE_CLIPBOARD);
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
        reach_clipboard_feature_request_refresh(host->clipboard_capsule);
        reach_host_request_update(host);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_NOW_PLAYING_CHANGED)
    {
        reach_host_request_update(host);
        return REACH_OK;
    }

    if (reach_host_game_mode_enabled(host) && !reach_host_game_mode_allows_event(event->type))
    {
        return REACH_OK;
    }

    int32_t launcher_was_open = reach_launcher_is_open(host->launcher_capsule);

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

    if (event->type == REACH_UI_EVENT_POINTER_CONTEXT)
    {
        return reach_host_handle_pointer_context(host, event, source);
    }

    if (event->type == REACH_UI_EVENT_WALLPAPER_CHANGED)
    {
        reach_host_reload_wallpaper(host, 1);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_CONFIG_CHANGED)
    {
        if (reach_host_apply_config_reload_result(host))
        {
            return REACH_OK;
        }
        return reach_host_schedule_config_reload(host);
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
        if (host->window_manager.ops.refresh != nullptr)
        {
            (void)host->window_manager.ops.refresh(host->window_manager.manager);
        }

        int32_t open_windows_changed = 0;
        (void)reach_host_refresh_open_windows(host, &open_windows_changed);

        uintptr_t foreground_window =
            host->window_manager.ops.foreground != nullptr
                ? host->window_manager.ops.foreground(host->window_manager.manager)
                : 0;
        int32_t foreground_changed = reach_host_foreground_window(host) != foreground_window;
        reach_host_note_foreground_window(host, foreground_window);

        if (open_windows_changed || foreground_changed)
        {
            reach_host_refresh_switcher_windows(host);
            host->dock.dirty_flags = 1;
            host->switcher.dirty_flags = 1;
        }
        if (foreground_changed && foreground_window != 0 &&
            reach_launcher_is_open(host->launcher_capsule))
        {
            reach_host_clear_launcher_restore_window(host);
            reach_host_close_launcher_without_focus_restore(host);
        }
        (void)reach_host_update_game_mode(host);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_WINDOW_FOCUS_LOST)
    {
        if (source == REACH_SURFACE_LAUNCHER && reach_launcher_is_open(host->launcher_capsule))
        {
            reach_host_clear_launcher_restore_window(host);
            reach_host_close_launcher_without_focus_restore(host);
            reach_host_request_update(host);
        }
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
        const reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->handle_routed == nullptr)
        {
            continue;
        }
        for (size_t entry = 0; entry < desc->routed_event_count; ++entry)
        {
            if (desc->routed_events[entry] == event->type)
            {
                return desc->handle_routed(host, event);
            }
        }
    }

    if (event->type == REACH_UI_EVENT_TEXT_CHAR || event->type == REACH_UI_EVENT_TEXT_EDIT)
    {

        reach_launcher_text_event_result text_result = {};
        reach_launcher_handle_text_event(host->launcher_capsule, event, &text_result);
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
        const reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->toggle == nullptr)
        {
            continue;
        }
        for (size_t entry = 0; entry < desc->toggle_event_count; ++entry)
        {
            if (desc->toggle_events[entry] == event->type)
            {
                desc->toggle(host);
                break;
            }
        }
    }

    if (event->type == REACH_UI_EVENT_ESCAPE)
    {
        reach_host_set_clipboard_open(host, 0);
    }

    reach_result result = reach_launcher_handle_event(host->launcher_capsule, event, &intent);
    if (result != REACH_OK)
    {
        return result;
    }

    reach_host_mark_dirty_for_event(host, event);

    if (launcher_was_open != reach_launcher_is_open(host->launcher_capsule))
    {
        if (reach_launcher_is_open(host->launcher_capsule))
        {
            reach_launcher_reset_text_edit(host->launcher_capsule);
        }
        reach_host_surface_transition_set(host, &host->launcher_transition,
                                          reach_launcher_is_open(host->launcher_capsule));

        reach_host_sync_popup_mouse_hook(host);
    }

    else if (intent.type == REACH_UI_INTENT_LAUNCH_APP)
    {
        return reach_host_open_pinned_app_id(host, intent.id, 0,
                                             reach_launcher_is_open(host->launcher_capsule));
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
