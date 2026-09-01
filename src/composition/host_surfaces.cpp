#include "host_internal.h"

#include <math.h>

int32_t reach_host_scalar_equal(float a, float b)
{
    return fabsf(a - b) < 0.001f;
}

const reach_shadow *reach_host_surface_shadow(const reach_host *host, reach_surface_id id)
{
    if (host == nullptr || id >= REACH_HOST_SURFACE_COUNT)
    {
        return nullptr;
    }

    const reach_theme *theme = host->theme != nullptr ? host->theme : reach_theme_default();
    switch (host->feature_runtimes[id].definition->surface.shadow)
    {
    case REACH_SURFACE_SHADOW_BAR:
        return &theme->bar_shadow;
    case REACH_SURFACE_SHADOW_POPUP:
        return &theme->popup_shadow;
    default:
        return nullptr;
    }
}

reach_shadow_pad reach_host_surface_shadow_pad(const reach_host *host, reach_surface_id id)
{
    reach_shadow_pad pad = {};
    const reach_shadow *shadow = reach_host_surface_shadow(host, id);
    if (shadow == nullptr)
    {
        return pad;
    }
    return reach_theme_shadow_pad(shadow, reach_host_layout_dpi_scale(host));
}

static reach_rect_f32 reach_host_surface_window_bounds(reach_rect_f32 content, reach_shadow_pad pad)
{
    content.x -= pad.left;
    content.y -= pad.top;
    content.width += pad.left + pad.right;
    content.height += pad.top + pad.bottom;
    return content;
}

void reach_host_stamp_surface_content(const reach_host *host, reach_surface_id id,
                                      reach_render_command_buffer *commands)
{
    if (host == nullptr || commands == nullptr || id >= REACH_HOST_SURFACE_COUNT)
    {
        return;
    }

    const reach_surface_runtime *surface = host->feature_runtimes[id].surface;
    if (surface == nullptr || !surface->bounds_valid)
    {
        return;
    }

    reach_shadow_pad pad = reach_host_surface_shadow_pad(host, id);
    reach_rect_f32 content = {pad.left, pad.top, surface->last_bounds.width,
                              surface->last_bounds.height};
    reach_render_command_buffer_set_content_rect(commands, content);
}

reach_result reach_host_apply_window_state(reach_platform_window_port *window,
                                           reach_rect_f32 bounds, reach_shadow_pad pad,
                                           reach_rect_f32 *last_bounds, int32_t *bounds_valid,
                                           int32_t *out_changed)
{
    REACH_ASSERT(window != nullptr);
    REACH_ASSERT(last_bounds != nullptr);
    REACH_ASSERT(bounds_valid != nullptr);
    REACH_ASSERT(out_changed != nullptr);
    if (window == nullptr || last_bounds == nullptr || bounds_valid == nullptr ||
        out_changed == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_changed = 0;
    if (window->ops.set_bounds != nullptr &&
        (!*bounds_valid || !reach_rect_equal(*last_bounds, bounds)))
    {
        reach_result result =
            window->ops.set_bounds(window->window, reach_host_surface_window_bounds(bounds, pad));
        if (result != REACH_OK)
        {
            return result;
        }
        *last_bounds = bounds;
        *bounds_valid = 1;
        *out_changed = 1;
    }

    return REACH_OK;
}

reach_host_surface_presentation_frame reach_host_surface_presentation_frame_compute(
    reach_rect_f32 target_bounds, reach_rect_f32 envelope_bounds, reach_shadow_pad shadow_pad,
    float y_offset, float scale, float max_scale)
{
    reach_host_surface_presentation_frame frame = {};
    frame.window_bounds = target_bounds;
    frame.content_rect = {shadow_pad.left, shadow_pad.top, target_bounds.width,
                          target_bounds.height};
    frame.render_transform = {1.0f, 1.0f, shadow_pad.left, shadow_pad.top};
    frame.pointer_transform = {1.0f, 1.0f, 0.0f, y_offset};
    frame.scale = 1.0f;

    if (scale <= 0.0f || max_scale <= 1.0f)
    {
        frame.window_bounds.y += y_offset;
        return frame;
    }

    float center_x = target_bounds.x + target_bounds.width * 0.5f;
    float center_y = target_bounds.y + target_bounds.height * 0.5f;

    float envelope_center_x = envelope_bounds.x + envelope_bounds.width * 0.5f;
    float envelope_center_y = envelope_bounds.y + envelope_bounds.height * 0.5f;
    frame.window_bounds.width = envelope_bounds.width * max_scale;
    frame.window_bounds.height = envelope_bounds.height * max_scale;
    frame.window_bounds.x = envelope_center_x - frame.window_bounds.width * 0.5f;
    frame.window_bounds.y = envelope_center_y - frame.window_bounds.height * 0.5f;

    frame.content_rect.width = target_bounds.width * scale;
    frame.content_rect.height = target_bounds.height * scale;
    float transformed_x = center_x - frame.content_rect.width * 0.5f;
    float transformed_y = center_y - frame.content_rect.height * 0.5f + y_offset;
    frame.content_rect.x = shadow_pad.left + transformed_x - frame.window_bounds.x;
    frame.content_rect.y = shadow_pad.top + transformed_y - frame.window_bounds.y;
    frame.render_transform = {scale, scale, frame.content_rect.x, frame.content_rect.y};
    frame.pointer_transform = {scale, scale, center_x - center_x * scale,
                               center_y + y_offset - center_y * scale};
    frame.scale = scale;
    frame.scale_envelope_active = 1;
    return frame;
}

static void reach_host_register_edge_reveal_participant(reach_host *host,
                                                        reach_host_edge_reveal_runtime *runtime)
{
    reach_layout_participant participant = 0;
    reach_result result = reach_layout_register(
        &host->layout_manager, runtime->owner->definition->surface.edge_reveal.layer, &participant);
    REACH_ASSERT(result == REACH_OK);
    if (result != REACH_OK)
    {
        return;
    }
    runtime->participant = participant;
    host->layout_targets[participant].edge_reveal = &runtime->port;
    reach_layout_set_visible(&host->layout_manager, participant, 0);
}

void reach_host_init_layout(reach_host *host)
{
    REACH_ASSERT(host != nullptr);
    if (host == nullptr)
    {
        return;
    }

    host->layout_manager = {};
    host->applied_layout_plan = {};
    host->has_applied_layout_plan = 0;
    host->dirty.z_order = 0;
    for (size_t index = 0; index < REACH_LAYOUT_MAX_PARTICIPANTS; ++index)
    {
        host->layout_targets[index] = {};
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_feature_runtime *desc = &host->feature_runtimes[index];
        reach_layout_participant participant = 0;
        reach_result result = reach_layout_register(&host->layout_manager,
                                                    desc->definition->surface.layer, &participant);
        REACH_ASSERT(result == REACH_OK);
        if (result != REACH_OK)
        {
            continue;
        }
        host->layout_targets[participant].runtime = desc;
        host->surface_participants[index] = participant;
        reach_layout_set_visible(&host->layout_manager, participant, 0);
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_host_edge_reveal_runtime *runtime = &host->edge_reveals[index];
        if (runtime->port.hotspot != nullptr)
        {
            reach_host_register_edge_reveal_participant(host, runtime);
        }
    }

    for (reach_layout_participant participant = 0;
         participant < (reach_layout_participant)host->layout_manager.participant_count;
         ++participant)
    {
        const reach_host_layout_target *target = &host->layout_targets[participant];
        if (target->runtime != nullptr && (target->runtime->definition->surface.behavior_flags &
                                           REACH_SURFACE_BEHAVIOR_GAME_MODE_VISIBLE) != 0)
        {
            continue;
        }
        reach_layout_register_visibility(&host->layout_manager, participant,
                                         REACH_LAYOUT_CONDITION_GAME_MODE, 0);
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[index];
        if (desc->definition->surface.bar_reveal.ops == nullptr ||
            desc->definition->surface.bar_reveal.active_layer <= 0)
        {
            continue;
        }
        reach_layout_participant participant = host->surface_participants[index];
        reach_layout_register_override(&host->layout_manager, participant,
                                       REACH_LAYOUT_CONDITION_BARS_FORCED,
                                       desc->definition->surface.bar_reveal.active_layer);
        reach_layout_register_override(&host->layout_manager, participant,
                                       REACH_LAYOUT_CONDITION_BARS_HELD,
                                       desc->definition->surface.bar_reveal.active_layer);
    }
}

void reach_host_surface_opening(reach_host *host, reach_surface_id opening, reach_surface_id origin)
{
    if (host == nullptr || opening >= REACH_HOST_SURFACE_COUNT)
    {
        return;
    }

    const reach_feature_runtime *self = &host->feature_runtimes[opening];
    const int32_t self_exclusive =
        (self->definition->surface.behavior_flags & REACH_SURFACE_BEHAVIOR_EXCLUSIVE) != 0;
    const int32_t self_dismissable =
        self->definition->surface.cls == REACH_SURFACE_CLASS_TRANSIENT ||
        self->definition->surface.cls == REACH_SURFACE_CLASS_POPUP;

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[index];
        if (desc->definition->id == opening || desc->definition->id == origin ||
            !reach_host_surface_closable(desc) || !reach_host_surface_is_open(desc))
        {
            continue;
        }

        const int32_t other_exclusive =
            (desc->definition->surface.behavior_flags & REACH_SURFACE_BEHAVIOR_EXCLUSIVE) != 0;
        const int32_t other_dismissable =
            desc->definition->surface.cls == REACH_SURFACE_CLASS_TRANSIENT ||
            desc->definition->surface.cls == REACH_SURFACE_CLASS_POPUP;

        if (other_exclusive || ((self_exclusive || self_dismissable) && other_dismissable))
        {
            reach_host_close_registered_surface(host, desc->definition->id,
                                                REACH_SURFACE_CLOSE_SUPERSEDED);
        }
    }

    if (self_exclusive)
    {
        reach_host_notify_popups_closed(host);
    }
}

void reach_host_apply_feature_tick_result(reach_host *host, reach_feature_runtime *runtime,
                                          const reach_feature_tick_result *result)
{
    if (host == nullptr || runtime == nullptr || result == nullptr)
    {
        return;
    }
    if (result->redraw && runtime->surface != nullptr)
    {
        runtime->surface->dirty_flags = 1;
        host->dirty.render = 1;
    }
    if (result->relayout)
    {
        host->dirty.layout = 1;
    }
    if (result->request_update)
    {
        reach_host_request_update(host);
    }
}

void reach_host_apply_surface_open_change(reach_host *host, reach_feature_runtime *runtime,
                                          int32_t open)
{
    if (host == nullptr || runtime == nullptr)
    {
        return;
    }
    int32_t next = open ? 1 : 0;
    if (next)
    {
        reach_host_capture_focus_restore(host, runtime->definition->id);
        runtime->presentation_visible = 1;
    }
    reach_host_sync_pointer_move_subscriptions(host);
    reach_host_sync_popup_mouse_hook(host);
    if (!next)
    {
        reach_host_request_bar_visibility_update(host);
    }
}

int32_t reach_host_surface_closable(const reach_feature_runtime *runtime)
{
    if (runtime == nullptr || runtime->definition == nullptr)
    {
        return 0;
    }
    const reach_feature_control_ops *control = runtime->definition->control_ops;
    return (runtime->capsule != nullptr && control != nullptr && control->set_open != nullptr) ||
           runtime->definition->force_close != nullptr;
}

void reach_host_set_registered_surface_open(reach_host *host, reach_surface_id id, int32_t open)
{
    if (host == nullptr || id >= REACH_HOST_SURFACE_COUNT)
    {
        return;
    }
    reach_feature_runtime *runtime = &host->feature_runtimes[id];
    const reach_feature_control_ops *control = runtime->definition->control_ops;
    if (runtime->capsule == nullptr || control == nullptr || control->set_open == nullptr)
    {
        return;
    }
    int32_t next = open ? 1 : 0;
    if (next && !reach_host_surface_is_open(runtime))
    {
        if (runtime->definition->surface.refresh_world_on_open)
        {
            if (host->monitors.list != nullptr && host->monitors.ops.refresh != nullptr)
            {
                (void)host->monitors.ops.refresh(host->monitors.list);
            }
            reach_host_refresh_window_world(host);
            reach_host_notify_display_changed(host);
        }
        reach_host_surface_opening(host, id, runtime->definition->surface.opening_origin);
    }
    reach_feature_tick_result result = {};
    if (!control->set_open(runtime->capsule, next, &result))
    {
        return;
    }
    reach_host_apply_surface_open_change(host, runtime, next);
    reach_host_apply_feature_tick_result(host, runtime, &result);
}

static int32_t reach_host_layout_anchor_equal(const reach_feature_layout_anchor *left,
                                              const reach_feature_layout_anchor *right)
{
    return left != nullptr && right != nullptr && left->surface == right->surface &&
           left->slot == right->slot && left->index == right->index;
}

reach_popup_activation_decision reach_host_popup_activation_decide(
    int32_t open, const reach_feature_layout_anchor *current,
    const reach_feature_layout_anchor *requested, reach_popup_activation_mode mode)
{
    if (requested == nullptr || requested->surface >= REACH_HOST_SURFACE_COUNT)
    {
        return REACH_POPUP_ACTIVATION_NONE;
    }
    if (!open || !reach_host_layout_anchor_equal(current, requested))
    {
        return REACH_POPUP_ACTIVATION_PRESENT;
    }
    if (mode == REACH_POPUP_ACTIVATION_REPLACE)
    {
        return REACH_POPUP_ACTIVATION_PRESENT;
    }
    return mode == REACH_POPUP_ACTIVATION_TOGGLE ? REACH_POPUP_ACTIVATION_CLOSE
                                                 : REACH_POPUP_ACTIVATION_NONE;
}

reach_popup_activation_decision reach_host_prepare_registered_popup(
    reach_host *host, reach_surface_id id, const reach_feature_layout_anchor *owner,
    reach_popup_activation_mode mode)
{
    if (host == nullptr || id >= REACH_HOST_SURFACE_COUNT)
    {
        return REACH_POPUP_ACTIVATION_NONE;
    }
    reach_feature_runtime *runtime = &host->feature_runtimes[id];
    if (runtime->definition == nullptr ||
        runtime->definition->surface.cls != REACH_SURFACE_CLASS_POPUP)
    {
        return REACH_POPUP_ACTIVATION_NONE;
    }

    reach_feature_layout_anchor current = {};
    const int32_t open = reach_host_surface_is_open(runtime);
    const reach_feature_layout_anchor *current_owner =
        open && reach_host_resolve_popup_owner(runtime, &current) ? &current : nullptr;
    reach_popup_activation_decision decision =
        reach_host_popup_activation_decide(open, current_owner, owner, mode);
    if (decision == REACH_POPUP_ACTIVATION_CLOSE)
    {
        reach_host_close_registered_surface(host, id, REACH_SURFACE_CLOSE_DISMISS);
    }
    else if (decision == REACH_POPUP_ACTIVATION_PRESENT && open && runtime->surface != nullptr &&
             runtime->surface->window.ops.hide != nullptr &&
             runtime->surface->window.ops.hide(runtime->surface->window.window) == REACH_OK)
    {
        runtime->surface->activated = 0;
        runtime->surface->native_visibility_invalidated = 1;
        runtime->surface->dirty_flags = 1;
        reach_host_request_update(host);
    }
    return decision;
}

void reach_host_toggle_registered_popup(reach_host *host, reach_surface_id id)
{
    if (host == nullptr || id >= REACH_HOST_SURFACE_COUNT)
    {
        return;
    }
    reach_feature_layout_anchor owner = {};
    if (!reach_host_resolve_popup_owner(&host->feature_runtimes[id], &owner) ||
        reach_host_prepare_registered_popup(host, id, &owner, REACH_POPUP_ACTIVATION_TOGGLE) !=
            REACH_POPUP_ACTIVATION_PRESENT)
    {
        return;
    }
    reach_host_set_registered_surface_open(host, id, 1);
}

void reach_host_present_registered_popup(reach_host *host, reach_surface_id id,
                                         reach_surface_id origin)
{
    if (host == nullptr || id >= REACH_HOST_SURFACE_COUNT)
    {
        return;
    }
    reach_feature_runtime *runtime = &host->feature_runtimes[id];
    reach_host_surface_opening(host, id, origin);
    reach_host_apply_surface_open_change(host, runtime, 1);
    if (runtime->surface != nullptr)
    {
        runtime->surface->dirty_flags = 1;
    }
    reach_host_sync_pointer_move_subscriptions(host);
    reach_host_sync_popup_mouse_hook(host);
    reach_host_request_bar_visibility_update(host);
}

void reach_host_close_surfaces_on_persistent_press(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *runtime = &host->feature_runtimes[index];
        if (runtime->definition->surface.close_on_persistent_press &&
            reach_host_surface_is_open(runtime))
        {
            reach_host_close_registered_surface(host, runtime->definition->id,
                                                REACH_SURFACE_CLOSE_SUPERSEDED);
        }
    }
}

void reach_host_close_registered_surface(reach_host *host, reach_surface_id id,
                                         reach_surface_close_intent intent)
{
    if (host == nullptr || id >= REACH_HOST_SURFACE_COUNT)
    {
        return;
    }
    reach_feature_runtime *runtime = &host->feature_runtimes[id];
    if (!reach_host_surface_is_open(runtime))
    {
        return;
    }
    if (intent == REACH_SURFACE_CLOSE_DISMISS)
    {
        reach_host_arm_focus_restore(host, id);
    }
    else
    {
        reach_host_cancel_focus_restore(host, id);
    }

    const reach_feature_control_ops *control = runtime->definition->control_ops;
    if (runtime->capsule != nullptr && control != nullptr && control->set_open != nullptr)
    {
        reach_host_set_registered_surface_open(host, id, 0);
        return;
    }
    if (runtime->definition->force_close != nullptr)
    {
        runtime->definition->force_close(host);
    }
}

void reach_host_post_feature_work_ready(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_surface_runtime *surface = host->feature_runtimes[index].surface;
        if (surface != nullptr && surface->window.ops.post_event != nullptr)
        {
            (void)surface->window.ops.post_event(surface->window.window,
                                                 REACH_UI_EVENT_FEATURE_WORK_READY);
            return;
        }
    }
    reach_host_request_update(host);
}

void reach_host_toggle_registered_surface(reach_host *host, reach_surface_id id)
{
    if (host == nullptr || id >= REACH_HOST_SURFACE_COUNT)
    {
        return;
    }
    if (reach_host_surface_is_open(&host->feature_runtimes[id]))
    {
        reach_host_close_registered_surface(host, id, REACH_SURFACE_CLOSE_DISMISS);
    }
    else
    {
        reach_host_set_registered_surface_open(host, id, 1);
    }
}

void reach_host_notify_registered_features(reach_host *host,
                                           const reach_feature_notification *notification)
{
    if (host == nullptr || notification == nullptr)
    {
        return;
    }
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_feature_runtime *runtime = &host->feature_runtimes[index];
        const reach_feature_control_ops *control = runtime->definition->control_ops;
        if (runtime->capsule == nullptr || control == nullptr || control->notify == nullptr)
        {
            continue;
        }
        int32_t was_open = reach_host_surface_is_open(runtime);
        reach_feature_tick_result result = {};
        control->notify(runtime->capsule, notification, &result);
        int32_t open = reach_host_surface_is_open(runtime);
        if (open != was_open)
        {
            if (!open)
            {
                reach_host_cancel_focus_restore(host, runtime->definition->id);
            }
            reach_host_apply_surface_open_change(host, runtime, open);
        }
        reach_host_apply_feature_tick_result(host, runtime, &result);
    }
}

int32_t reach_host_surface_is_open(const reach_feature_runtime *desc)
{
    return desc->definition->capsule_ops->is_open == nullptr ||
           desc->definition->capsule_ops->is_open(desc->capsule);
}

int32_t reach_host_surface_needs_frame(const reach_feature_runtime *desc)
{
    return desc->definition->capsule_ops->needs_frame != nullptr &&
           desc->definition->capsule_ops->needs_frame(desc->capsule);
}

int32_t reach_host_surface_presented(const reach_feature_runtime *desc)
{
    return reach_host_surface_is_open(desc) ||
           (desc->definition->capsule_ops->presentation_visible != nullptr &&
            desc->definition->capsule_ops->presentation_visible(desc->capsule));
}

void reach_host_close_activating_surfaces_on_focus_loss(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[index];
        if ((desc->definition->surface.behavior_flags & REACH_SURFACE_BEHAVIOR_ACTIVATES) == 0 ||
            !reach_host_surface_closable(desc) || !reach_host_surface_is_open(desc))
        {
            continue;
        }

        if (desc->surface->window.ops.is_active != nullptr &&
            desc->surface->window.ops.is_active(desc->surface->window.window))
        {
            continue;
        }

        reach_host_close_registered_surface(host, desc->definition->id,
                                            REACH_SURFACE_CLOSE_SUPERSEDED);
    }
}

int32_t reach_host_any_surface_open(reach_host *host, uint32_t class_mask)
{
    if (host == nullptr)
    {
        return 0;
    }
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[index];
        if ((class_mask & reach_surface_class_bit(desc->definition->surface.cls)) != 0 &&
            desc->definition->surface.cls != REACH_SURFACE_CLASS_PERSISTENT &&
            desc->definition->capsule_ops != nullptr &&
            desc->definition->capsule_ops->is_open != nullptr &&
            desc->definition->capsule_ops->is_open(desc->capsule))
        {
            return 1;
        }
    }
    return 0;
}

void reach_host_mark_all_surfaces_dirty(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_surface_runtime *surface = host->feature_runtimes[index].surface;
        if (surface != nullptr)
        {
            surface->dirty_flags = 1;
        }
    }
}

int32_t reach_host_any_surface_dirty(const reach_host *host)
{
    if (host == nullptr)
    {
        return 0;
    }
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *desc = &host->feature_runtimes[index];
        if (desc->surface != nullptr && desc->surface->dirty_flags)
        {
            return 1;
        }
    }
    return 0;
}
