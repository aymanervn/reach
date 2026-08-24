#include "host_internal.h"

#include <math.h>

int32_t reach_host_rect_equal(reach_rect_f32 a, reach_rect_f32 b)
{
    return fabsf(a.x - b.x) < 0.5f && fabsf(a.y - b.y) < 0.5f && fabsf(a.width - b.width) < 0.5f &&
           fabsf(a.height - b.height) < 0.5f;
}

int32_t reach_host_opacity_equal(float a, float b)
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
    switch (host->surface_descs[id].shadow)
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

    const reach_surface_runtime *surface = host->surface_descs[id].surface;
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
        (!*bounds_valid || !reach_host_rect_equal(*last_bounds, bounds)))
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
        (!*opacity_valid || !reach_host_opacity_equal(*last_opacity, opacity)))
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
    transition->settle_offset = settle_offset;
    reach_animation_manager_set(&host->animations, y_track, settle_offset);
    reach_animation_manager_set(&host->animations, opacity_track, 0.0f);
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
        }
        reach_animation_manager_animate_to(&host->animations, transition->y_track, 0.0f,
                                           open_seconds, REACH_EASING_EASE_OUT);
        reach_animation_manager_animate_to(&host->animations, transition->opacity_track, 1.0f,
                                           open_seconds, REACH_EASING_EASE_OUT);
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

int32_t reach_host_surface_transition_visible(const reach_host_surface_transition *transition)
{
    return transition != nullptr && transition->visible;
}

int32_t reach_host_surface_transition_active(const reach_host *host,
                                             const reach_host_surface_transition *transition)
{
    return host != nullptr && transition != nullptr &&
           (reach_animation_manager_active(&host->animations, transition->y_track) ||
            reach_animation_manager_active(&host->animations, transition->opacity_track));
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
    reach_animation_manager_set(&host->animations, transition->opacity_track, 0.0f);
    reach_host_request_update(host);
}

static void reach_host_surface_launcher_close(reach_host *host)
{
    reach_host_close_launcher_without_focus_restore(host);
}

static void reach_host_surface_clipboard_close(reach_host *host)
{
    reach_host_set_clipboard_open(host, 0);
}

static void reach_host_surface_tray_close(reach_host *host)
{
    reach_host_set_tray_popup_open(host, 0);
}

static void reach_host_surface_quick_settings_close(reach_host *host)
{
    reach_host_set_quick_settings_open(host, 0);
}

static void reach_host_surface_battery_close(reach_host *host)
{
    reach_host_set_battery_open(host, 0);
}

static void reach_host_surface_context_menu_close(reach_host *host)
{
    reach_host_close_context_menu(host);
}

static void reach_host_surface_stage_close(reach_host *host)
{
    reach_host_close_stage(host);
}

static void reach_host_surface_switcher_close(reach_host *host)
{
    reach_switcher_force_close(host->switcher_capsule);
    reach_host_surface_transition_set(host, &host->switcher_transition, 0);
    host->switcher.dirty_flags = 1;
}

#define REACH_HOST_LAYER_DOCK_EDGE_REVEAL 120
#define REACH_HOST_LAYER_BAR_ACTIVE 130
#define REACH_HOST_LAYER_TOP_BAR_EDGE_REVEAL 140
#define REACH_HOST_LAYER_STAGE_EDGE_REVEAL 150

void reach_host_init_surface_descriptors(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_surface_desc *descs = host->surface_descs;

    descs[REACH_SURFACE_ID_DOCK] = {REACH_SURFACE_ID_DOCK,
                                    REACH_SURFACE_CLASS_PERSISTENT,
                                    &host->dock,
                                    nullptr,
                                    nullptr,
                                    host->dock_capsule,
                                    reach_dock_capsule_ops(),
                                    REACH_SURFACE_POINTER_SOURCE_GATED};
    descs[REACH_SURFACE_ID_TOP_BAR] = {REACH_SURFACE_ID_TOP_BAR,
                                       REACH_SURFACE_CLASS_PERSISTENT,
                                       &host->top_bar,
                                       nullptr,
                                       nullptr,
                                       host->top_bar_capsule,
                                       reach_top_bar_capsule_ops(),
                                       REACH_SURFACE_POINTER_SOURCE_GATED};
    descs[REACH_SURFACE_ID_LAUNCHER] = {REACH_SURFACE_ID_LAUNCHER,
                                        REACH_SURFACE_CLASS_TRANSIENT,
                                        &host->launcher,
                                        &host->launcher_transition,
                                        reach_host_surface_launcher_close,
                                        host->launcher_capsule,
                                        reach_launcher_capsule_ops(),
                                        REACH_SURFACE_POINTER_RELAYOUT_REDRAWS |
                                            REACH_SURFACE_POINTER_DOWN_CLOSES_ON_UNHANDLED};
    descs[REACH_SURFACE_ID_CLIPBOARD] = {REACH_SURFACE_ID_CLIPBOARD,
                                         REACH_SURFACE_CLASS_TRANSIENT,
                                         &host->clipboard_surface,
                                         &host->clipboard_transition,
                                         reach_host_surface_clipboard_close,
                                         host->clipboard_capsule,
                                         reach_clipboard_feature_capsule_ops(),
                                         REACH_SURFACE_POINTER_SOURCE_GATED};
    descs[REACH_SURFACE_ID_TRAY] = {REACH_SURFACE_ID_TRAY,
                                    REACH_SURFACE_CLASS_POPUP,
                                    &host->tray,
                                    &host->tray_transition,
                                    reach_host_surface_tray_close,
                                    host->top_bar_capsule,
                                    reach_top_bar_tray_capsule_ops(),
                                    REACH_SURFACE_POINTER_DOWN_APPLIES_UNHANDLED};
    descs[REACH_SURFACE_ID_QUICK_SETTINGS] = {REACH_SURFACE_ID_QUICK_SETTINGS,
                                              REACH_SURFACE_CLASS_POPUP,
                                              &host->quick_settings,
                                              &host->quick_settings_transition,
                                              reach_host_surface_quick_settings_close,
                                              host->quick_settings_capsule,
                                              reach_quick_settings_capsule_ops(),
                                              REACH_SURFACE_POINTER_NONE};
    descs[REACH_SURFACE_ID_BATTERY] = {
        REACH_SURFACE_ID_BATTERY,    REACH_SURFACE_CLASS_POPUP,        &host->battery,
        &host->battery_transition,   reach_host_surface_battery_close, host->battery_capsule,
        reach_battery_capsule_ops(), REACH_SURFACE_POINTER_NONE};
    descs[REACH_SURFACE_ID_CONTEXT_MENU] = {REACH_SURFACE_ID_CONTEXT_MENU,
                                            REACH_SURFACE_CLASS_POPUP,
                                            &host->context_menu,
                                            &host->context_menu_transition,
                                            reach_host_surface_context_menu_close,
                                            host->context_menu_capsule,
                                            reach_context_menu_capsule_ops(),
                                            REACH_SURFACE_POINTER_NONE};
    descs[REACH_SURFACE_ID_SWITCHER] = {
        REACH_SURFACE_ID_SWITCHER,    REACH_SURFACE_CLASS_OVERLAY,       &host->switcher,
        &host->switcher_transition,   reach_host_surface_switcher_close, host->switcher_capsule,
        reach_switcher_capsule_ops(), REACH_SURFACE_POINTER_NONE};

    descs[REACH_SURFACE_ID_STAGE] = {
        REACH_SURFACE_ID_STAGE,    REACH_SURFACE_CLASS_TRANSIENT,  &host->stage,
        &host->stage_transition,   reach_host_surface_stage_close, host->stage_capsule,
        reach_stage_capsule_ops(), REACH_SURFACE_POINTER_NONE};

    descs[REACH_SURFACE_ID_DOCK].shadow = REACH_SURFACE_SHADOW_BAR;
    descs[REACH_SURFACE_ID_TOP_BAR].shadow = REACH_SURFACE_SHADOW_BAR;
    descs[REACH_SURFACE_ID_LAUNCHER].shadow = REACH_SURFACE_SHADOW_POPUP;
    descs[REACH_SURFACE_ID_CLIPBOARD].shadow = REACH_SURFACE_SHADOW_POPUP;
    descs[REACH_SURFACE_ID_SWITCHER].shadow = REACH_SURFACE_SHADOW_POPUP;
    descs[REACH_SURFACE_ID_TRAY].shadow = REACH_SURFACE_SHADOW_POPUP;
    descs[REACH_SURFACE_ID_QUICK_SETTINGS].shadow = REACH_SURFACE_SHADOW_POPUP;
    descs[REACH_SURFACE_ID_BATTERY].shadow = REACH_SURFACE_SHADOW_POPUP;
    descs[REACH_SURFACE_ID_CONTEXT_MENU].shadow = REACH_SURFACE_SHADOW_POPUP;

    descs[REACH_SURFACE_ID_STAGE].layer = 50;
    descs[REACH_SURFACE_ID_LAUNCHER].layer = 100;
    descs[REACH_SURFACE_ID_DOCK].layer = 110;
    descs[REACH_SURFACE_ID_TOP_BAR].layer = 0;
    descs[REACH_SURFACE_ID_CONTEXT_MENU].layer = 160;
    descs[REACH_SURFACE_ID_QUICK_SETTINGS].layer = 170;
    descs[REACH_SURFACE_ID_BATTERY].layer = 175;
    descs[REACH_SURFACE_ID_TRAY].layer = 180;
    descs[REACH_SURFACE_ID_CLIPBOARD].layer = 190;
    descs[REACH_SURFACE_ID_SWITCHER].layer = 200;

    descs[REACH_SURFACE_ID_CONTEXT_MENU].role = REACH_SURFACE_CONTEXT_MENU;
    descs[REACH_SURFACE_ID_CONTEXT_MENU].pointer_priority = 10;
    descs[REACH_SURFACE_ID_CONTEXT_MENU].apply_pointer_action =
        reach_host_apply_context_menu_pointer_action;
    descs[REACH_SURFACE_ID_CLIPBOARD].role = REACH_SURFACE_CLIPBOARD;
    descs[REACH_SURFACE_ID_CLIPBOARD].pointer_priority = 20;
    descs[REACH_SURFACE_ID_CLIPBOARD].apply_pointer_action =
        reach_host_apply_clipboard_pointer_action;
    descs[REACH_SURFACE_ID_LAUNCHER].role = REACH_SURFACE_LAUNCHER;
    descs[REACH_SURFACE_ID_LAUNCHER].pointer_priority = 30;
    descs[REACH_SURFACE_ID_LAUNCHER].apply_pointer_action =
        reach_host_apply_launcher_pointer_action;
    descs[REACH_SURFACE_ID_LAUNCHER].dismiss = reach_host_close_launcher;
    descs[REACH_SURFACE_ID_LAUNCHER].behavior_flags =
        REACH_SURFACE_BEHAVIOR_ACTIVATES | REACH_SURFACE_BEHAVIOR_EXCLUSIVE;
    descs[REACH_SURFACE_ID_TRAY].role = REACH_SURFACE_TRAY_MENU;
    descs[REACH_SURFACE_ID_TRAY].pointer_priority = 40;
    descs[REACH_SURFACE_ID_TRAY].apply_pointer_action = reach_host_apply_tray_pointer_action;
    descs[REACH_SURFACE_ID_BATTERY].role = REACH_SURFACE_BATTERY;
    descs[REACH_SURFACE_ID_BATTERY].pointer_priority = 55;
    descs[REACH_SURFACE_ID_BATTERY].apply_pointer_action = reach_host_apply_battery_pointer_action;
    descs[REACH_SURFACE_ID_QUICK_SETTINGS].role = REACH_SURFACE_QUICK_SETTINGS;
    descs[REACH_SURFACE_ID_QUICK_SETTINGS].pointer_priority = 50;
    descs[REACH_SURFACE_ID_QUICK_SETTINGS].apply_pointer_action =
        reach_host_apply_quick_settings_pointer_action;
    descs[REACH_SURFACE_ID_DOCK].edge_reveal = {1, REACH_HOST_LAYER_DOCK_EDGE_REVEAL, {}, nullptr};
    descs[REACH_SURFACE_ID_DOCK].bar_reveal = {reach_dock_reveal_ops(), 0, 0.0f};
    descs[REACH_SURFACE_ID_TOP_BAR].edge_reveal = {
        1, REACH_HOST_LAYER_TOP_BAR_EDGE_REVEAL, {}, nullptr};
    descs[REACH_SURFACE_ID_TOP_BAR].bar_reveal = {reach_top_bar_reveal_ops(),
                                                  REACH_HOST_LAYER_BAR_ACTIVE, 4.0f};
    descs[REACH_SURFACE_ID_TOP_BAR].role = REACH_SURFACE_TOP_BAR;
    descs[REACH_SURFACE_ID_TOP_BAR].pointer_priority = 80;
    descs[REACH_SURFACE_ID_TOP_BAR].apply_pointer_action = reach_host_apply_top_bar_pointer_action;
    descs[REACH_SURFACE_ID_DOCK].role = REACH_SURFACE_DOCK;
    descs[REACH_SURFACE_ID_DOCK].pointer_priority = 90;
    descs[REACH_SURFACE_ID_DOCK].apply_pointer_action = reach_host_apply_dock_pointer_action;
    descs[REACH_SURFACE_ID_SWITCHER].role = REACH_SURFACE_SWITCHER;
    descs[REACH_SURFACE_ID_SWITCHER].pointer_priority = 100;
    descs[REACH_SURFACE_ID_SWITCHER].behavior_flags = REACH_SURFACE_BEHAVIOR_EXCLUSIVE;
    descs[REACH_SURFACE_ID_STAGE].role = REACH_SURFACE_STAGE;
    descs[REACH_SURFACE_ID_STAGE].pointer_priority = 60;
    descs[REACH_SURFACE_ID_STAGE].apply_pointer_action = reach_host_apply_stage_pointer_action;
    descs[REACH_SURFACE_ID_STAGE].dismiss = reach_host_close_stage;
    descs[REACH_SURFACE_ID_STAGE].bar_shown_while_open = 1;
    descs[REACH_SURFACE_ID_STAGE].behavior_flags = REACH_SURFACE_BEHAVIOR_EXCLUSIVE;
    descs[REACH_SURFACE_ID_STAGE].edge_reveal = {1,
                                                 REACH_HOST_LAYER_STAGE_EDGE_REVEAL,
                                                 {REACH_EDGE_REVEAL_ANCHOR_TOP_LEFT, 4.0f, 4.0f, 1},
                                                 reach_host_on_stage_edge_reveal};

    descs[REACH_SURFACE_ID_LAUNCHER].frame = reach_host_frame_launcher;
    descs[REACH_SURFACE_ID_LAUNCHER].frame_priority = 10;
    descs[REACH_SURFACE_ID_CLIPBOARD].frame = reach_host_frame_clipboard;
    descs[REACH_SURFACE_ID_CLIPBOARD].frame_priority = 20;
    descs[REACH_SURFACE_ID_DOCK].frame = reach_host_frame_dock;
    descs[REACH_SURFACE_ID_DOCK].frame_priority = 30;
    descs[REACH_SURFACE_ID_TOP_BAR].frame = reach_host_frame_top_bar;
    descs[REACH_SURFACE_ID_TOP_BAR].frame_priority = 35;
    descs[REACH_SURFACE_ID_TRAY].frame = reach_host_frame_tray;
    descs[REACH_SURFACE_ID_TRAY].frame_priority = 40;
    descs[REACH_SURFACE_ID_QUICK_SETTINGS].frame = reach_host_frame_quick_settings;
    descs[REACH_SURFACE_ID_QUICK_SETTINGS].frame_priority = 50;
    descs[REACH_SURFACE_ID_BATTERY].frame = reach_host_frame_battery;
    descs[REACH_SURFACE_ID_BATTERY].frame_priority = 55;
    descs[REACH_SURFACE_ID_SWITCHER].frame = reach_host_frame_switcher;
    descs[REACH_SURFACE_ID_SWITCHER].frame_priority = 60;
    descs[REACH_SURFACE_ID_STAGE].frame = reach_host_frame_stage;
    descs[REACH_SURFACE_ID_STAGE].frame_priority = 65;
    descs[REACH_SURFACE_ID_CONTEXT_MENU].frame = reach_host_frame_context_menu;
    descs[REACH_SURFACE_ID_CONTEXT_MENU].frame_priority = 70;

    descs[REACH_SURFACE_ID_LAUNCHER].toggle_events =
        reach_launcher_activation_events(&descs[REACH_SURFACE_ID_LAUNCHER].toggle_event_count);
    descs[REACH_SURFACE_ID_LAUNCHER].toggle = reach_host_toggle_launcher;
    descs[REACH_SURFACE_ID_CLIPBOARD].toggle_events =
        reach_clipboard_activation_events(&descs[REACH_SURFACE_ID_CLIPBOARD].toggle_event_count);
    descs[REACH_SURFACE_ID_CLIPBOARD].toggle = reach_host_toggle_clipboard;
    descs[REACH_SURFACE_ID_SWITCHER].routed_events =
        reach_switcher_routed_events(&descs[REACH_SURFACE_ID_SWITCHER].routed_event_count);
    descs[REACH_SURFACE_ID_SWITCHER].handle_routed = reach_host_handle_switcher_event;
}

static void reach_host_register_edge_reveal_participant(reach_host *host,
                                                        reach_host_edge_reveal_runtime *runtime)
{
    reach_layout_participant participant = 0;
    reach_result result = reach_layout_register(&host->layout_manager,
                                                runtime->owner->edge_reveal.layer, &participant);
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
        reach_surface_desc *desc = &host->surface_descs[index];
        reach_layout_participant participant = 0;
        reach_result result =
            reach_layout_register(&host->layout_manager, desc->layer, &participant);
        REACH_ASSERT(result == REACH_OK);
        if (result != REACH_OK)
        {
            continue;
        }
        host->layout_targets[participant].desc = desc;
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
        reach_layout_register_visibility(&host->layout_manager, participant,
                                         REACH_LAYOUT_CONDITION_GAME_MODE, 0);
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->bar_reveal.ops == nullptr || desc->bar_reveal.active_layer <= 0)
        {
            continue;
        }
        reach_layout_participant participant = host->surface_participants[index];
        reach_layout_register_override(&host->layout_manager, participant,
                                       REACH_LAYOUT_CONDITION_BARS_FORCED,
                                       desc->bar_reveal.active_layer);
        reach_layout_register_override(&host->layout_manager, participant,
                                       REACH_LAYOUT_CONDITION_BARS_HELD,
                                       desc->bar_reveal.active_layer);
    }
}

void reach_host_surface_opening(reach_host *host, reach_surface_id opening, reach_surface_id origin)
{
    if (host == nullptr || opening >= REACH_HOST_SURFACE_COUNT)
    {
        return;
    }

    const reach_surface_desc *self = &host->surface_descs[opening];
    const int32_t self_exclusive = (self->behavior_flags & REACH_SURFACE_BEHAVIOR_EXCLUSIVE) != 0;
    const int32_t self_dismissable =
        self->cls == REACH_SURFACE_CLASS_TRANSIENT || self->cls == REACH_SURFACE_CLASS_POPUP;

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->id == opening || desc->id == origin || desc->force_close == nullptr ||
            !reach_host_surface_is_open(desc))
        {
            continue;
        }

        const int32_t other_exclusive =
            (desc->behavior_flags & REACH_SURFACE_BEHAVIOR_EXCLUSIVE) != 0;
        const int32_t other_dismissable =
            desc->cls == REACH_SURFACE_CLASS_TRANSIENT || desc->cls == REACH_SURFACE_CLASS_POPUP;

        if (other_exclusive || ((self_exclusive || self_dismissable) && other_dismissable))
        {
            desc->force_close(host);
        }
    }

    if (self_exclusive)
    {
        reach_host_clear_sticky_dock_feedback(host);
    }
}

int32_t reach_host_surface_is_open(const reach_surface_desc *desc)
{
    return desc->capsule_ops->is_open == nullptr || desc->capsule_ops->is_open(desc->capsule);
}

void reach_host_close_activating_surfaces_on_focus_loss(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_surface_desc *desc = &host->surface_descs[index];
        if ((desc->behavior_flags & REACH_SURFACE_BEHAVIOR_ACTIVATES) == 0 ||
            desc->force_close == nullptr || !reach_host_surface_is_open(desc))
        {
            continue;
        }

        if (desc->surface->window.ops.is_active != nullptr &&
            desc->surface->window.ops.is_active(desc->surface->window.window))
        {
            continue;
        }

        desc->force_close(host);
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
        const reach_surface_desc *desc = &host->surface_descs[index];
        if ((class_mask & reach_surface_class_bit(desc->cls)) != 0 &&
            desc->cls != REACH_SURFACE_CLASS_PERSISTENT && desc->capsule_ops != nullptr &&
            desc->capsule_ops->is_open != nullptr && desc->capsule_ops->is_open(desc->capsule))
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
        reach_surface_runtime *surface = host->surface_descs[index].surface;
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
        const reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->surface != nullptr && desc->surface->dirty_flags)
        {
            return 1;
        }
    }
    return 0;
}
