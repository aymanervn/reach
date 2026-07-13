#include "host_internal.h"

#include <math.h>

static const float REACH_HOST_TRANSITION_OFFSET = 8.0f;
static const double REACH_HOST_TRANSITION_OPEN_SECONDS = 0.16;
static const double REACH_HOST_TRANSITION_CLOSE_SECONDS = 0.12;

int32_t reach_host_rect_equal(reach_rect_f32 a, reach_rect_f32 b)
{
    return fabsf(a.x - b.x) < 0.5f && fabsf(a.y - b.y) < 0.5f && fabsf(a.width - b.width) < 0.5f &&
           fabsf(a.height - b.height) < 0.5f;
}

int32_t reach_host_opacity_equal(float a, float b)
{
    return fabsf(a - b) < 0.001f;
}

reach_result reach_host_apply_window_state(reach_platform_window_port *window,
                                           reach_rect_f32 bounds, float opacity,
                                           reach_rect_f32 *last_bounds, float *last_opacity,
                                           int32_t *bounds_valid, int32_t *opacity_valid,
                                           int32_t *out_changed)
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
        reach_result result = window->ops.set_bounds(window->window, bounds);
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
                                        size_t y_track, size_t opacity_track)
{
    if (host == nullptr || transition == nullptr)
    {
        return;
    }
    *transition = {};
    transition->y_track = y_track;
    transition->opacity_track = opacity_track;
    reach_animation_manager_set(&host->animations, y_track, REACH_HOST_TRANSITION_OFFSET);
    reach_animation_manager_set(&host->animations, opacity_track, 0.0f);
}

void reach_host_surface_transitions_init(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }
    reach_host_surface_transition_init(host, &host->launcher_transition,
                                       REACH_HOST_ANIMATION_LAUNCHER_TRANSITION_Y,
                                       REACH_HOST_ANIMATION_LAUNCHER_TRANSITION_OPACITY);
    reach_host_surface_transition_init(host, &host->tray_transition,
                                       REACH_HOST_ANIMATION_TRAY_TRANSITION_Y,
                                       REACH_HOST_ANIMATION_TRAY_TRANSITION_OPACITY);
    reach_host_surface_transition_init(host, &host->quick_settings_transition,
                                       REACH_HOST_ANIMATION_QUICK_SETTINGS_TRANSITION_Y,
                                       REACH_HOST_ANIMATION_QUICK_SETTINGS_TRANSITION_OPACITY);
    reach_host_surface_transition_init(host, &host->switcher_transition,
                                       REACH_HOST_ANIMATION_SWITCHER_TRANSITION_Y,
                                       REACH_HOST_ANIMATION_SWITCHER_TRANSITION_OPACITY);
    reach_host_surface_transition_init(host, &host->context_menu_transition,
                                       REACH_HOST_ANIMATION_CONTEXT_MENU_TRANSITION_Y,
                                       REACH_HOST_ANIMATION_CONTEXT_MENU_TRANSITION_OPACITY);
    reach_host_surface_transition_init(host, &host->clipboard_transition,
                                       REACH_HOST_ANIMATION_CLIPBOARD_TRANSITION_Y,
                                       REACH_HOST_ANIMATION_CLIPBOARD_TRANSITION_OPACITY);
}

void reach_host_surface_transition_set(reach_host *host, reach_host_surface_transition *transition,
                                       int32_t open)
{
    if (host == nullptr || transition == nullptr)
    {
        return;
    }

    int32_t target_open = open ? 1 : 0;
    if (transition->target_open == target_open && (target_open || !transition->visible))
    {
        return;
    }

    transition->target_open = target_open;
    if (target_open)
    {
        if (!transition->visible)
        {
            transition->visible = 1;
            reach_animation_manager_set(&host->animations, transition->y_track,
                                        REACH_HOST_TRANSITION_OFFSET);
            reach_animation_manager_set(&host->animations, transition->opacity_track, 0.0f);
        }
        reach_animation_manager_animate_to(&host->animations, transition->y_track, 0.0f,
                                           REACH_HOST_TRANSITION_OPEN_SECONDS,
                                           REACH_EASING_EASE_OUT);
        reach_animation_manager_animate_to(&host->animations, transition->opacity_track, 1.0f,
                                           REACH_HOST_TRANSITION_OPEN_SECONDS,
                                           REACH_EASING_EASE_OUT);
    }
    else if (transition->visible)
    {
        reach_animation_manager_animate_to(
            &host->animations, transition->y_track, REACH_HOST_TRANSITION_OFFSET,
            REACH_HOST_TRANSITION_CLOSE_SECONDS, REACH_EASING_EASE_IN);
        reach_animation_manager_animate_to(&host->animations, transition->opacity_track, 0.0f,
                                           REACH_HOST_TRANSITION_CLOSE_SECONDS,
                                           REACH_EASING_EASE_IN);
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
    reach_animation_manager_set(&host->animations, transition->y_track,
                                REACH_HOST_TRANSITION_OFFSET);
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

static void reach_host_surface_context_menu_close(reach_host *host)
{
    reach_host_close_context_menu(host);
}

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
                                    REACH_SURFACE_POINTER_UPDATES_DOCK_VISIBILITY |
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
                                    host->tray_capsule,
                                    reach_tray_capsule_ops(),
                                    REACH_SURFACE_POINTER_DOWN_APPLIES_UNHANDLED};
    descs[REACH_SURFACE_ID_QUICK_SETTINGS] = {REACH_SURFACE_ID_QUICK_SETTINGS,
                                              REACH_SURFACE_CLASS_POPUP,
                                              &host->quick_settings,
                                              &host->quick_settings_transition,
                                              reach_host_surface_quick_settings_close,
                                              host->quick_settings_capsule,
                                              reach_quick_settings_capsule_ops(),
                                              REACH_SURFACE_POINTER_NONE};
    descs[REACH_SURFACE_ID_CONTEXT_MENU] = {REACH_SURFACE_ID_CONTEXT_MENU,
                                            REACH_SURFACE_CLASS_POPUP,
                                            &host->context_menu,
                                            &host->context_menu_transition,
                                            reach_host_surface_context_menu_close,
                                            host->context_menu_capsule,
                                            reach_context_menu_capsule_ops(),
                                            REACH_SURFACE_POINTER_NONE};
    descs[REACH_SURFACE_ID_SWITCHER] = {REACH_SURFACE_ID_SWITCHER,
                                        REACH_SURFACE_CLASS_OVERLAY,
                                        &host->switcher,
                                        &host->switcher_transition,
                                        nullptr,
                                        host->switcher_capsule,
                                        reach_switcher_capsule_ops(),
                                        REACH_SURFACE_POINTER_NONE};

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
    descs[REACH_SURFACE_ID_TRAY].role = REACH_SURFACE_TRAY_MENU;
    descs[REACH_SURFACE_ID_TRAY].pointer_priority = 40;
    descs[REACH_SURFACE_ID_TRAY].apply_pointer_action = reach_host_apply_tray_pointer_action;
    descs[REACH_SURFACE_ID_QUICK_SETTINGS].role = REACH_SURFACE_QUICK_SETTINGS;
    descs[REACH_SURFACE_ID_QUICK_SETTINGS].pointer_priority = 50;
    descs[REACH_SURFACE_ID_QUICK_SETTINGS].apply_pointer_action =
        reach_host_apply_quick_settings_pointer_action;
    descs[REACH_SURFACE_ID_DOCK].role = REACH_SURFACE_DOCK;
    descs[REACH_SURFACE_ID_DOCK].pointer_priority = 90;
    descs[REACH_SURFACE_ID_DOCK].apply_pointer_action = reach_host_apply_dock_pointer_action;
    descs[REACH_SURFACE_ID_SWITCHER].role = REACH_SURFACE_SWITCHER;
    descs[REACH_SURFACE_ID_SWITCHER].pointer_priority = 100;

    descs[REACH_SURFACE_ID_LAUNCHER].frame = reach_host_frame_launcher;
    descs[REACH_SURFACE_ID_LAUNCHER].frame_priority = 10;
    descs[REACH_SURFACE_ID_CLIPBOARD].frame = reach_host_frame_clipboard;
    descs[REACH_SURFACE_ID_CLIPBOARD].frame_priority = 20;
    descs[REACH_SURFACE_ID_DOCK].frame = reach_host_frame_dock;
    descs[REACH_SURFACE_ID_DOCK].frame_priority = 30;
    descs[REACH_SURFACE_ID_TRAY].frame = reach_host_frame_tray;
    descs[REACH_SURFACE_ID_TRAY].frame_priority = 40;
    descs[REACH_SURFACE_ID_QUICK_SETTINGS].frame = reach_host_frame_quick_settings;
    descs[REACH_SURFACE_ID_QUICK_SETTINGS].frame_priority = 50;
    descs[REACH_SURFACE_ID_SWITCHER].frame = reach_host_frame_switcher;
    descs[REACH_SURFACE_ID_SWITCHER].frame_priority = 60;
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

void reach_host_close_other_popups(reach_host *host, reach_surface_id keep)
{
    if (host == nullptr)
    {
        return;
    }
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->cls == REACH_SURFACE_CLASS_POPUP && desc->id != keep &&
            desc->force_close != nullptr)
        {
            desc->force_close(host);
        }
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
