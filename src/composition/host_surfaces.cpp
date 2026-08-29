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
                                           float opacity, reach_rect_f32 *last_bounds,
                                           float *last_opacity, int32_t *bounds_valid,
                                           int32_t *opacity_valid, int32_t *out_changed)
{
    REACH_ASSERT(window != nullptr);
    REACH_ASSERT(last_bounds != nullptr);
    REACH_ASSERT(last_opacity != nullptr);
    REACH_ASSERT(bounds_valid != nullptr);
    REACH_ASSERT(opacity_valid != nullptr);
    REACH_ASSERT(out_changed != nullptr);
    if (window == nullptr || last_bounds == nullptr || last_opacity == nullptr ||
        bounds_valid == nullptr || opacity_valid == nullptr || out_changed == nullptr)
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

    if (window->ops.set_opacity != nullptr &&
        (!*opacity_valid || !reach_host_scalar_equal(*last_opacity, opacity)))
    {
        reach_result result = window->ops.set_opacity(window->window, opacity);
        if (result != REACH_OK)
        {
            return result;
        }
        *last_opacity = opacity;
        *opacity_valid = 1;
        *out_changed = 1;
    }

    return REACH_OK;
}

void reach_host_surface_transition_init(reach_host *host, reach_host_surface_transition *transition,
                                        size_t y_track, size_t opacity_track, float settle_offset)
{
    if (host == nullptr || transition == nullptr)
    {
        return;
    }
    *transition = {};
    transition->y_track = y_track;
    transition->opacity_track = opacity_track;
    transition->scale_track = REACH_HOST_ANIMATION_COUNT;
    transition->settle_offset = settle_offset;
    transition->start_scale = 1.0f;
    reach_animation_manager_set(&host->animations, y_track, settle_offset);
    reach_animation_manager_set(&host->animations, opacity_track, 0.0f);
}

void reach_host_surface_transition_set_scale(reach_host *host,
                                             reach_host_surface_transition *transition,
                                             size_t scale_track, float start_scale)
{
    if (host == nullptr || transition == nullptr || scale_track >= REACH_HOST_ANIMATION_COUNT)
    {
        return;
    }
    transition->scale_track = scale_track;
    transition->start_scale = start_scale > 0.0f ? start_scale : 1.0f;
    reach_animation_manager_set(&host->animations, scale_track, transition->start_scale);
}

void reach_host_surface_transition_set_settle_offset(reach_host *host,
                                                     reach_host_surface_transition *transition,
                                                     float settle_offset)
{
    if (host == nullptr || transition == nullptr || transition->settle_offset == settle_offset)
    {
        return;
    }
    transition->settle_offset = settle_offset;
    if (!transition->visible)
    {
        reach_animation_manager_set(&host->animations, transition->y_track, settle_offset);
    }
}

void reach_host_surface_transitions_init(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }
    reach_host_surface_transition_init(
        host, &host->launcher_transition, REACH_HOST_ANIMATION_LAUNCHER_TRANSITION_Y,
        REACH_HOST_ANIMATION_LAUNCHER_TRANSITION_OPACITY, REACH_HOST_TRANSITION_SETTLE_FROM_BELOW);
    reach_host_surface_transition_set_scale(host, &host->launcher_transition,
                                            REACH_HOST_ANIMATION_LAUNCHER_TRANSITION_SCALE,
                                            REACH_HOST_LAUNCHER_TRANSITION_SCALE);
    reach_host_surface_transition_init(
        host, &host->tray_transition, REACH_HOST_ANIMATION_TRAY_TRANSITION_Y,
        REACH_HOST_ANIMATION_TRAY_TRANSITION_OPACITY, REACH_HOST_TRANSITION_SETTLE_FROM_ABOVE);
    reach_host_surface_transition_init(host, &host->quick_settings_transition,
                                       REACH_HOST_ANIMATION_QUICK_SETTINGS_TRANSITION_Y,
                                       REACH_HOST_ANIMATION_QUICK_SETTINGS_TRANSITION_OPACITY,
                                       REACH_HOST_TRANSITION_SETTLE_FROM_ABOVE);
    reach_host_surface_transition_init(
        host, &host->battery_transition, REACH_HOST_ANIMATION_BATTERY_TRANSITION_Y,
        REACH_HOST_ANIMATION_BATTERY_TRANSITION_OPACITY, REACH_HOST_TRANSITION_SETTLE_FROM_ABOVE);
    reach_host_surface_transition_init(
        host, &host->switcher_transition, REACH_HOST_ANIMATION_SWITCHER_TRANSITION_Y,
        REACH_HOST_ANIMATION_SWITCHER_TRANSITION_OPACITY, REACH_HOST_TRANSITION_SETTLE_FROM_BELOW);
    reach_host_surface_transition_init(host, &host->context_menu_transition,
                                       REACH_HOST_ANIMATION_CONTEXT_MENU_TRANSITION_Y,
                                       REACH_HOST_ANIMATION_CONTEXT_MENU_TRANSITION_OPACITY,
                                       REACH_HOST_TRANSITION_SETTLE_FROM_BELOW);
    reach_host_surface_transition_init(
        host, &host->clipboard_transition, REACH_HOST_ANIMATION_CLIPBOARD_TRANSITION_Y,
        REACH_HOST_ANIMATION_CLIPBOARD_TRANSITION_OPACITY, REACH_HOST_TRANSITION_SETTLE_FROM_BELOW);
    reach_host_surface_transition_init(
        host, &host->stage_transition, REACH_HOST_ANIMATION_STAGE_TRANSITION_Y,
        REACH_HOST_ANIMATION_STAGE_TRANSITION_OPACITY, REACH_HOST_TRANSITION_SETTLE_FROM_BELOW);
}

void reach_host_surface_transition_set(reach_host *host, reach_host_surface_transition *transition,
                                       int32_t open)
{
    if (host == nullptr || transition == nullptr)
    {
        return;
    }

    int32_t target_open = open ? 1 : 0;
    if (transition->target_open == target_open &&
        (target_open || !transition->visible ||
         reach_host_surface_transition_active(host, transition)))
    {
        return;
    }

    const reach_theme *theme = host->theme != nullptr ? host->theme : reach_theme_default();

    transition->target_open = target_open;
    if (target_open)
    {
        double open_seconds = transition->open_seconds > 0.0 ? transition->open_seconds
                                                             : (double)theme->surface_open_seconds;
        if (!transition->visible)
        {
            transition->visible = 1;
            reach_animation_manager_set(&host->animations, transition->y_track,
                                        transition->settle_offset);
            reach_animation_manager_set(&host->animations, transition->opacity_track, 0.0f);
            if (transition->scale_track < REACH_HOST_ANIMATION_COUNT)
            {
                reach_animation_manager_set(&host->animations, transition->scale_track,
                                            transition->start_scale);
            }
        }
        reach_animation_manager_animate_to(&host->animations, transition->y_track, 0.0f,
                                           open_seconds, REACH_EASING_EASE_OUT);
        reach_animation_manager_animate_to(&host->animations, transition->opacity_track, 1.0f,
                                           open_seconds, REACH_EASING_EASE_OUT);
        if (transition->scale_track < REACH_HOST_ANIMATION_COUNT)
        {
            reach_animation_manager_animate_to(&host->animations, transition->scale_track, 1.0f,
                                               open_seconds, REACH_EASING_EASE_OUT);
        }
    }
    else if (transition->visible)
    {
        double close_seconds = transition->close_seconds > 0.0
                                   ? transition->close_seconds
                                   : (double)theme->surface_close_seconds;
        reach_animation_manager_animate_to(&host->animations, transition->y_track,
                                           transition->settle_offset, close_seconds,
                                           REACH_EASING_EASE_IN);
        reach_animation_manager_animate_to(&host->animations, transition->opacity_track, 0.0f,
                                           close_seconds, REACH_EASING_EASE_IN);
        if (transition->scale_track < REACH_HOST_ANIMATION_COUNT)
        {
            reach_animation_manager_animate_to(&host->animations, transition->scale_track,
                                               transition->start_scale, close_seconds,
                                               REACH_EASING_EASE_IN);
        }
    }
    reach_host_request_update(host);
}

reach_rect_f32 reach_host_surface_transition_bounds(const reach_host *host,
                                                    const reach_host_surface_transition *transition,
                                                    reach_rect_f32 target_bounds)
{
    if (host != nullptr && transition != nullptr)
    {
        target_bounds.y += reach_animation_manager_value(&host->animations, transition->y_track) *
                           reach_host_layout_dpi_scale(host);
    }
    return target_bounds;
}

float reach_host_surface_transition_opacity(const reach_host *host,
                                            const reach_host_surface_transition *transition)
{
    return host != nullptr && transition != nullptr
               ? reach_animation_manager_value(&host->animations, transition->opacity_track)
               : 0.0f;
}

reach_host_surface_transition_frame reach_host_surface_transition_frame_compute(
    const reach_host *host, const reach_host_surface_transition *transition,
    reach_rect_f32 target_bounds, reach_shadow_pad shadow_pad)
{
    return reach_host_surface_transition_frame_compute_in_envelope(host, transition, target_bounds,
                                                                   target_bounds, shadow_pad);
}

reach_host_surface_transition_frame reach_host_surface_transition_frame_compute_in_envelope(
    const reach_host *host, const reach_host_surface_transition *transition,
    reach_rect_f32 target_bounds, reach_rect_f32 envelope_bounds, reach_shadow_pad shadow_pad)
{
    reach_host_surface_transition_frame frame = {};
    frame.window_bounds = reach_host_surface_transition_bounds(host, transition, target_bounds);
    frame.content_rect = {shadow_pad.left, shadow_pad.top, target_bounds.width,
                          target_bounds.height};
    frame.render_transform = {1.0f, 1.0f, shadow_pad.left, shadow_pad.top};
    frame.pointer_transform = {1.0f, 1.0f, 0.0f, frame.window_bounds.y - target_bounds.y};
    frame.scale = 1.0f;

    if (host == nullptr || transition == nullptr || !transition->visible ||
        transition->scale_track >= REACH_HOST_ANIMATION_COUNT)
    {
        return frame;
    }

    float scale = reach_animation_manager_value(&host->animations, transition->scale_track);
    scale = scale > 0.0f ? scale : 1.0f;
    float max_scale = transition->start_scale > 1.0f ? transition->start_scale : 1.0f;
    float center_x = target_bounds.x + target_bounds.width * 0.5f;
    float center_y = target_bounds.y + target_bounds.height * 0.5f;
    float y_offset = reach_animation_manager_value(&host->animations, transition->y_track) *
                     reach_host_layout_dpi_scale(host);

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

int32_t reach_host_surface_transition_visible(const reach_host_surface_transition *transition)
{
    return transition != nullptr && transition->visible;
}

int32_t reach_host_surface_transition_active(const reach_host *host,
                                             const reach_host_surface_transition *transition)
{
    return host != nullptr && transition != nullptr &&
           (reach_animation_manager_active(&host->animations, transition->y_track) ||
            reach_animation_manager_active(&host->animations, transition->opacity_track) ||
            (transition->scale_track < REACH_HOST_ANIMATION_COUNT &&
             reach_animation_manager_active(&host->animations, transition->scale_track)));
}

void reach_host_surface_transition_finish(reach_host *host,
                                          reach_host_surface_transition *transition)
{
    if (host == nullptr || transition == nullptr || transition->target_open ||
        !transition->visible || reach_host_surface_transition_active(host, transition))
    {
        return;
    }

    transition->visible = 0;
    reach_animation_manager_set(&host->animations, transition->y_track, transition->settle_offset);
    if (transition->scale_track < REACH_HOST_ANIMATION_COUNT)
    {
        reach_animation_manager_set(&host->animations, transition->scale_track,
                                    transition->start_scale);
    }
    reach_animation_manager_set(&host->animations, transition->opacity_track, 0.0f);
    reach_host_request_update(host);
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
            desc->definition->force_close == nullptr || !reach_host_surface_is_open(desc))
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
            desc->definition->force_close(host);
        }
    }

    if (self_exclusive)
    {
        reach_host_clear_sticky_dock_feedback(host);
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
        reach_host_surface_opening(host, id, runtime->definition->surface.opening_origin);
    }
    reach_feature_tick_result result = {};
    if (!control->set_open(runtime->capsule, next, &result))
    {
        return;
    }
    reach_host_surface_transition_set(host, runtime->transition, next);
    reach_host_sync_pointer_move_subscriptions(host);
    reach_host_sync_popup_mouse_hook(host);
    if (!next)
    {
        reach_host_request_bar_visibility_update(host);
    }
    reach_host_apply_feature_tick_result(host, runtime, &result);
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
    if (host != nullptr && id < REACH_HOST_SURFACE_COUNT)
    {
        reach_host_set_registered_surface_open(
            host, id, !reach_host_surface_is_open(&host->feature_runtimes[id]));
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
        reach_feature_tick_result result = {};
        control->notify(runtime->capsule, notification, &result);
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
    return reach_host_surface_is_open(desc) || reach_host_surface_needs_frame(desc);
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
            desc->definition->force_close == nullptr || !reach_host_surface_is_open(desc))
        {
            continue;
        }

        if (desc->surface->window.ops.is_active != nullptr &&
            desc->surface->window.ops.is_active(desc->surface->window.window))
        {
            continue;
        }

        desc->definition->force_close(host);
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
