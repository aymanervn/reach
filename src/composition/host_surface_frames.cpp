#include "host_internal.h"

typedef struct reach_host_frame_state
{
    int32_t window_changed;
    int32_t visible;
} reach_host_frame_state;

static void reach_host_apply_surface_activation(const reach_surface_desc *desc, int32_t active)
{
    reach_surface_runtime *surface = desc->surface;
    if ((desc->behavior_flags & REACH_SURFACE_BEHAVIOR_ACTIVATES) == 0)
    {
        return;
    }

    if (!active)
    {
        surface->activated = 0;
        return;
    }

    if (!surface->activated && surface->window.ops.show != nullptr)
    {
        (void)surface->window.ops.show(surface->window.window);
        surface->activated = 1;
    }
}

static reach_result reach_host_apply_transient_frame(
    reach_host *host, reach_surface_runtime *surface, reach_host_surface_transition *transition,
    int32_t game_mode, reach_rect_f32 target_bounds, float radius, reach_host_frame_state *out)
{
    *out = {};

    reach_rect_f32 bounds = reach_host_surface_transition_bounds(host, transition, target_bounds);
    float opacity = reach_host_surface_transition_opacity(host, transition);
    reach_result result = reach_host_apply_window_state(
        &surface->window, bounds, opacity, &surface->last_bounds, &surface->last_opacity,
        &surface->bounds_valid, &surface->opacity_valid, &out->window_changed);
    if (result != REACH_OK)
    {
        return result;
    }

    if (out->window_changed && surface->window.ops.apply_rounded_corners != nullptr)
    {
        (void)surface->window.ops.apply_rounded_corners(surface->window.window, radius);
    }

    if (!game_mode && reach_host_surface_transition_visible(transition))
    {
        if (surface->window.ops.show != nullptr)
        {
            (void)surface->window.ops.show(surface->window.window);
        }
        out->visible = 1;
    }
    else if (surface->window.ops.hide != nullptr)
    {
        (void)surface->window.ops.hide(surface->window.window);
    }

    return REACH_OK;
}

reach_result reach_host_frame_launcher(reach_host *host, const reach_host_frame_context *ctx)
{
    const int32_t game_mode = ctx->game_mode;
    const int32_t launcher_layout_changed = ctx->launcher_layout_changed;
    int32_t launcher_window_changed = 0;
    reach_rect_f32 launcher_bounds = reach_host_surface_transition_bounds(
        host, &host->launcher_transition, host->layout.launcher.bounds);
    float launcher_opacity =
        reach_host_surface_transition_opacity(host, &host->launcher_transition);
    reach_result result = reach_host_apply_window_state(
        &host->launcher.window, launcher_bounds, launcher_opacity, &host->launcher.last_bounds,
        &host->launcher.last_opacity, &host->launcher.bounds_valid, &host->launcher.opacity_valid,
        &launcher_window_changed);
    if (result != REACH_OK)
    {
        return result;
    }

    if (!game_mode && reach_launcher_is_open(host->launcher_capsule) &&
        (host->dirty.render || host->launcher.dirty_flags || launcher_layout_changed))
    {
        (void)reach_host_render_launcher_surface(host, &host->layout.launcher);
    }
    const reach_surface_desc *desc = &host->surface_descs[REACH_SURFACE_ID_LAUNCHER];
    if (!game_mode && reach_host_surface_transition_visible(&host->launcher_transition))
    {
        reach_host_apply_surface_activation(desc, reach_host_surface_is_open(desc));
    }
    else
    {
        if (host->launcher.window.ops.hide != nullptr)
        {
            (void)host->launcher.window.ops.hide(host->launcher.window.window);
        }
        reach_host_apply_surface_activation(desc, 0);
    }

    return REACH_OK;
}

reach_result reach_host_frame_clipboard(reach_host *host, const reach_host_frame_context *ctx)
{
    const int32_t game_mode = ctx->game_mode;
    const reach_rect_f32 monitor_bounds = ctx->monitor_bounds;
    int32_t clipboard_animating = 0;
    int32_t clipboard_layout_changed = reach_clipboard_feature_relayout(
        host->clipboard_capsule, monitor_bounds, host->layout.launcher.bounds,
        reach_host_layout_dpi_scale(host), &clipboard_animating);
    if (clipboard_animating)
    {
        host->dirty.layout = 1;
        host->clipboard_surface.dirty_flags = 1;
        reach_host_request_update(host);
    }

    reach_host_frame_state frame = {};
    reach_result result = reach_host_apply_transient_frame(
        host, &host->clipboard_surface, &host->clipboard_transition, game_mode,
        reach_clipboard_feature_state_ptr(host->clipboard_capsule)->layout.bounds,
        host->theme->clipboard_panel_radius * reach_host_layout_dpi_scale(host), &frame);
    if (result != REACH_OK)
    {
        return result;
    }
    if (frame.visible && reach_clipboard_is_open(host->clipboard_capsule) &&
        (host->dirty.render || host->clipboard_surface.dirty_flags || clipboard_layout_changed ||
         frame.window_changed))
    {
        (void)reach_host_render_clipboard_surface(host);
    }
    return REACH_OK;
}

reach_result reach_host_frame_dock(reach_host *host, const reach_host_frame_context *ctx)
{
    const int32_t game_mode = ctx->game_mode;
    const int32_t dock_layout_changed = ctx->dock_layout_changed;
    if (game_mode)
    {
        if (host->dock.window.ops.hide != nullptr)
        {
            (void)host->dock.window.ops.hide(host->dock.window.window);
        }
        return REACH_OK;
    }
    if (host->dock.window.ops.set_bounds == nullptr)
    {
        return REACH_OK;
    }

    if (host->dock.window.ops.show != nullptr)
    {
        (void)host->dock.window.ops.show(host->dock.window.window);
    }
    int32_t dock_window_changed = 0;
    float dock_radius =
        reach_theme_dock_corner_radius(host->theme, host->layout.dock.bounds.height);
    reach_result result = reach_host_apply_window_state(
        &host->dock.window, host->layout.dock.bounds, 1.0f, &host->dock.last_bounds,
        &host->dock.last_opacity, &host->dock.bounds_valid, &host->dock.opacity_valid,
        &dock_window_changed);
    if (result != REACH_OK)
    {
        return result;
    }

    if (dock_window_changed && host->dock.window.ops.apply_rounded_corners != nullptr)
    {
        (void)host->dock.window.ops.apply_rounded_corners(host->dock.window.window, dock_radius);
    }

    int32_t dock_reveal_position_only =
        reach_animation_manager_active(reach_dock_manager(host->dock_capsule), REACH_DOCK_ANIM_Y) &&
        !reach_dock_slots_animating(host->dock_capsule) && !host->dirty.render &&
        !host->dock.dirty_flags && !reach_dock_state_ptr(host->dock_capsule)->drag.active &&
        !reach_animation_manager_active(reach_dock_manager(host->dock_capsule),
                                        REACH_DOCK_ANIM_DRAG_SNAP) &&
        !reach_animation_manager_active(reach_dock_manager(host->dock_capsule),
                                        REACH_DOCK_ANIM_FEEDBACK_OPACITY);

    if (host->dirty.render || host->dock.dirty_flags ||
        (!dock_reveal_position_only && (dock_window_changed || dock_layout_changed)))
    {
        (void)reach_host_render_dock_surface(host, &host->layout.dock);
    }
    return REACH_OK;
}

reach_result reach_host_frame_tray(reach_host *host, const reach_host_frame_context *ctx)
{
    const int32_t game_mode = ctx->game_mode;
    if (host->tray.window.ops.set_bounds == nullptr)
    {
        return REACH_OK;
    }

    reach_rect_f32 tray_bounds = {};
    reach_dock_layout screen_dock = reach_dock_layout_to_screen(host->layout.dock);
    reach_host_compute_tray_popup_layout(host, &screen_dock, &tray_bounds);

    reach_host_frame_state frame = {};
    reach_result result = reach_host_apply_transient_frame(
        host, &host->tray, &host->tray_transition, game_mode, tray_bounds,
        reach_popup_radius_scaled(reach_host_layout_dpi_scale(host)), &frame);
    if (result != REACH_OK)
    {
        return result;
    }
    if (frame.visible && reach_tray_popup_is_open(host->tray_capsule) &&
        (host->dirty.render || host->tray.dirty_flags))
    {
        (void)reach_host_render_tray_surface(host, tray_bounds);
    }
    return REACH_OK;
}

reach_result reach_host_frame_quick_settings(reach_host *host, const reach_host_frame_context *ctx)
{
    const int32_t game_mode = ctx->game_mode;
    if (host->quick_settings.window.ops.set_bounds == nullptr)
    {
        return REACH_OK;
    }

    if (!game_mode)
    {
        reach_host_refresh_quick_settings_layout(host);
        reach_host_update_quick_settings_animation(host);
    }

    reach_host_frame_state frame = {};
    reach_result result = reach_host_apply_transient_frame(
        host, &host->quick_settings, &host->quick_settings_transition, game_mode,
        reach_quick_settings_state_ptr(host->quick_settings_capsule)->bounds,
        reach_popup_radius_scaled(reach_host_layout_dpi_scale(host)), &frame);
    if (result != REACH_OK)
    {
        return result;
    }
    if (frame.visible && reach_quick_settings_is_open(host->quick_settings_capsule) &&
        (host->dirty.render || host->quick_settings.dirty_flags))
    {
        (void)reach_host_render_quick_settings_surface(host);
    }
    return REACH_OK;
}

static reach_rect_f32 reach_host_apply_switcher_bounds_animation(reach_host *host,
                                                                 reach_rect_f32 target)
{
    if (host == nullptr)
    {
        return target;
    }
    int32_t request_redraw = 0;
    reach_rect_f32 animated = reach_switcher_apply_width_animation(
        host->switcher_capsule, reach_host_surface_transition_visible(&host->switcher_transition),
        reach_switcher_is_open(host->switcher_capsule), host->switcher.bounds_valid,
        host->switcher.last_bounds.width, target, &request_redraw);
    if (request_redraw)
    {
        host->switcher.dirty_flags = 1;
    }
    return animated;
}

reach_result reach_host_frame_switcher(reach_host *host, const reach_host_frame_context *ctx)
{
    const int32_t game_mode = ctx->game_mode;
    const reach_rect_f32 monitor_bounds = ctx->monitor_bounds;
    if (host->switcher.window.ops.set_bounds == nullptr)
    {
        return REACH_OK;
    }

    reach_rect_f32 target_switcher_bounds = reach_switcher_bounds_for_count_scaled(
        monitor_bounds, reach_host_switcher_visible_count(host), reach_host_layout_dpi_scale(host));
    reach_rect_f32 switcher_bounds =
        reach_host_apply_switcher_bounds_animation(host, target_switcher_bounds);

    reach_host_frame_state frame = {};
    reach_result result = reach_host_apply_transient_frame(
        host, &host->switcher, &host->switcher_transition, game_mode, switcher_bounds,
        16.0f * reach_host_layout_dpi_scale(host), &frame);
    if (result != REACH_OK)
    {
        return result;
    }
    if (frame.visible && reach_switcher_is_open(host->switcher_capsule) &&
        (host->dirty.render || host->switcher.dirty_flags))
    {

        reach_rect_f32 transitioned_bounds =
            reach_host_surface_transition_bounds(host, &host->switcher_transition, switcher_bounds);
        (void)reach_host_render_switcher_surface(host, transitioned_bounds);
    }
    return REACH_OK;
}

reach_result reach_host_frame_context_menu(reach_host *host, const reach_host_frame_context *ctx)
{
    const int32_t game_mode = ctx->game_mode;
    if (host->context_menu.window.ops.set_bounds == nullptr)
    {
        return REACH_OK;
    }

    if (!game_mode)
    {

        reach_host_reanchor_context_menu(host);
    }

    reach_host_frame_state frame = {};
    reach_result result = reach_host_apply_transient_frame(
        host, &host->context_menu, &host->context_menu_transition, game_mode,
        reach_context_menu_state_ptr(host->context_menu_capsule)->bounds,
        reach_popup_radius_scaled(reach_host_layout_dpi_scale(host)), &frame);
    if (result != REACH_OK)
    {
        return result;
    }
    if (frame.visible && reach_context_menu_is_open(host->context_menu_capsule) &&
        (host->dirty.render || host->context_menu.dirty_flags))
    {
        (void)reach_host_render_context_menu_surface(host);
    }
    return REACH_OK;
}
