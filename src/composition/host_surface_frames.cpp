#include "host_internal.h"

typedef struct reach_host_frame_state
{
    int32_t window_changed;
    int32_t visible;
} reach_host_frame_state;

void reach_host_sync_surface_input_regions(const reach_host *host, const reach_surface_desc *desc)
{
    if (desc->surface == nullptr || desc->capsule_ops == nullptr ||
        desc->capsule_ops->input_regions == nullptr ||
        desc->surface->window.ops.set_input_regions == nullptr)
    {
        return;
    }

    reach_rect_f32 regions[REACH_PLATFORM_WINDOW_MAX_INPUT_REGIONS] = {};
    size_t region_count = desc->capsule_ops->input_regions(desc->capsule, regions,
                                                           REACH_PLATFORM_WINDOW_MAX_INPUT_REGIONS);

    reach_shadow_pad pad = reach_host_surface_shadow_pad(host, desc->id);
    for (size_t index = 0; index < region_count; ++index)
    {
        regions[index].x += pad.left;
        regions[index].y += pad.top;
    }
    (void)desc->surface->window.ops.set_input_regions(desc->surface->window.window, regions,
                                                      region_count);
}

static void reach_host_set_surface_visible(reach_host *host, reach_surface_id id, int32_t visible)
{
    reach_layout_set_visible(&host->layout_manager, host->surface_participants[id], visible);
}

static reach_result reach_host_apply_transient_frame(reach_host *host, reach_surface_id id,
                                                     reach_host_surface_transition *transition,
                                                     reach_rect_f32 target_bounds, float radius,
                                                     reach_host_frame_state *out)
{
    *out = {};

    reach_surface_runtime *surface = host->surface_descs[id].surface;
    reach_rect_f32 bounds = reach_host_surface_transition_bounds(host, transition, target_bounds);
    float opacity = reach_host_surface_transition_opacity(host, transition);
    reach_result result = reach_host_apply_window_state(
        &surface->window, bounds, reach_host_surface_shadow_pad(host, id), opacity,
        &surface->last_bounds, &surface->last_opacity, &surface->bounds_valid,
        &surface->opacity_valid, &out->window_changed);
    if (result != REACH_OK)
    {
        return result;
    }

    if (out->window_changed && surface->window.ops.apply_rounded_corners != nullptr)
    {
        (void)surface->window.ops.apply_rounded_corners(surface->window.window, radius);
    }

    out->visible = reach_host_surface_transition_visible(transition);
    reach_host_set_surface_visible(host, id, out->visible);

    return REACH_OK;
}

reach_result reach_host_frame_launcher(reach_host *host, const reach_host_frame_context *ctx)
{
    const int32_t launcher_layout_changed = ctx->launcher_layout_changed;
    int32_t launcher_window_changed = 0;
    reach_rect_f32 launcher_bounds = reach_host_surface_transition_bounds(
        host, &host->launcher_transition, host->layout.launcher.bounds);
    float launcher_opacity =
        reach_host_surface_transition_opacity(host, &host->launcher_transition);
    reach_result result = reach_host_apply_window_state(
        &host->launcher.window, launcher_bounds,
        reach_host_surface_shadow_pad(host, REACH_SURFACE_ID_LAUNCHER), launcher_opacity,
        &host->launcher.last_bounds, &host->launcher.last_opacity, &host->launcher.bounds_valid,
        &host->launcher.opacity_valid, &launcher_window_changed);
    if (result != REACH_OK)
    {
        return result;
    }

    if (reach_launcher_is_open(host->launcher_capsule) &&
        (host->dirty.render || host->launcher.dirty_flags || launcher_layout_changed))
    {
        (void)reach_host_render_launcher_surface(host, &host->layout.launcher);
    }
    reach_host_set_surface_visible(
        host, REACH_SURFACE_ID_LAUNCHER,
        reach_host_surface_transition_visible(&host->launcher_transition));

    return REACH_OK;
}

reach_result reach_host_frame_clipboard(reach_host *host, const reach_host_frame_context *ctx)
{
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

    const reach_rect_f32 clipboard_bounds =
        reach_clipboard_feature_state_ptr(host->clipboard_capsule)->layout.bounds;
    reach_host_frame_state frame = {};
    reach_result result = reach_host_apply_transient_frame(
        host, REACH_SURFACE_ID_CLIPBOARD, &host->clipboard_transition, clipboard_bounds,
        host->theme->radius_large * reach_host_layout_dpi_scale(host), &frame);
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
    const int32_t dock_layout_changed = ctx->dock_layout_changed;
    if (host->dock.window.ops.set_bounds == nullptr)
    {
        return REACH_OK;
    }

    reach_host_set_surface_visible(host, REACH_SURFACE_ID_DOCK, 1);

    int32_t dock_window_changed = 0;
    float dock_radius =
        reach_theme_dock_corner_radius(host->theme, host->layout.dock.bounds.height);
    reach_result result = reach_host_apply_window_state(
        &host->dock.window, host->layout.dock.bounds,
        reach_host_surface_shadow_pad(host, REACH_SURFACE_ID_DOCK), 1.0f, &host->dock.last_bounds,
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

reach_result reach_host_frame_top_bar(reach_host *host, const reach_host_frame_context *ctx)
{
    if (host->top_bar.window.ops.set_bounds == nullptr)
    {
        return REACH_OK;
    }

    reach_host_build_top_bar_layout(host, ctx->monitor_bounds);

    const reach_top_bar_state *state = reach_top_bar_state_ptr(host->top_bar_capsule);
    reach_rect_f32 shown_bounds = state->layout.bounds;
    reach_rect_f32 bounds = reach_host_reconcile_bar_visibility(host, REACH_SURFACE_ID_TOP_BAR,
                                                                shown_bounds, ctx->monitor_bounds);

    reach_host_set_surface_visible(host, REACH_SURFACE_ID_TOP_BAR, 1);

    int32_t window_changed = 0;
    reach_result result = reach_host_apply_window_state(
        &host->top_bar.window, bounds,
        reach_host_surface_shadow_pad(host, REACH_SURFACE_ID_TOP_BAR), 1.0f,
        &host->top_bar.last_bounds, &host->top_bar.last_opacity, &host->top_bar.bounds_valid,
        &host->top_bar.opacity_valid, &window_changed);
    if (result != REACH_OK)
    {
        return result;
    }

    if (host->dirty.render || host->top_bar.dirty_flags || window_changed)
    {
        (void)reach_host_render_top_bar_surface(host);
    }
    return REACH_OK;
}

reach_result reach_host_frame_tray(reach_host *host, const reach_host_frame_context *ctx)
{
    (void)ctx;
    if (host->tray.window.ops.set_bounds == nullptr)
    {
        return REACH_OK;
    }

    reach_rect_f32 tray_bounds = {};
    reach_host_compute_tray_popup_layout(host, &tray_bounds);

    reach_host_frame_state frame = {};
    reach_result result = reach_host_apply_transient_frame(
        host, REACH_SURFACE_ID_TRAY, &host->tray_transition, tray_bounds,
        reach_popup_radius_scaled(host->theme, reach_host_layout_dpi_scale(host)), &frame);
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
    (void)ctx;
    if (host->quick_settings.window.ops.set_bounds == nullptr)
    {
        return REACH_OK;
    }

    reach_host_refresh_quick_settings_layout(host);
    reach_host_update_quick_settings_animation(host);

    const reach_rect_f32 quick_settings_bounds =
        reach_quick_settings_state_ptr(host->quick_settings_capsule)->bounds;
    reach_host_frame_state frame = {};
    reach_result result = reach_host_apply_transient_frame(
        host, REACH_SURFACE_ID_QUICK_SETTINGS, &host->quick_settings_transition,
        quick_settings_bounds,
        reach_popup_radius_scaled(host->theme, reach_host_layout_dpi_scale(host)), &frame);
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

reach_result reach_host_frame_battery(reach_host *host, const reach_host_frame_context *ctx)
{
    (void)ctx;
    if (host->battery.window.ops.set_bounds == nullptr)
    {
        return REACH_OK;
    }

    reach_host_sync_battery_saver_pending(host);
    reach_host_relayout_battery(host);

    const reach_rect_f32 battery_bounds = reach_battery_state_ptr(host->battery_capsule)->bounds;
    reach_host_frame_state frame = {};
    reach_result result = reach_host_apply_transient_frame(
        host, REACH_SURFACE_ID_BATTERY, &host->battery_transition, battery_bounds,
        reach_popup_radius_scaled(host->theme, reach_host_layout_dpi_scale(host)), &frame);
    if (result != REACH_OK)
    {
        return result;
    }
    if (frame.visible && reach_battery_is_open(host->battery_capsule) &&
        (host->dirty.render || host->battery.dirty_flags))
    {
        (void)reach_host_render_battery_surface(host);
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
        host, REACH_SURFACE_ID_SWITCHER, &host->switcher_transition, switcher_bounds,
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

reach_result reach_host_frame_stage(reach_host *host, const reach_host_frame_context *ctx)
{
    if (host->stage.window.ops.set_bounds == nullptr)
    {
        return REACH_OK;
    }

    const reach_stage_state *state = reach_stage_state_ptr(host->stage_capsule);
    if (state == nullptr)
    {
        return REACH_OK;
    }

    reach_host_set_surface_visible(
        host, REACH_SURFACE_ID_STAGE,
        state->open || reach_host_surface_transition_visible(&host->stage_transition));

    if (!state->open && reach_host_surface_transition_visible(&host->stage_transition))
    {
        reach_host_surface_transition_set(host, &host->stage_transition, 0);
    }

    reach_rect_f32 stage_bounds = state->open ? state->bounds : ctx->monitor_bounds;

    int32_t window_changed = 0;
    reach_rect_f32 bounds =
        reach_host_surface_transition_bounds(host, &host->stage_transition, stage_bounds);
    float opacity = reach_host_surface_transition_opacity(host, &host->stage_transition);
    reach_result result = reach_host_apply_window_state(
        &host->stage.window, bounds, reach_host_surface_shadow_pad(host, REACH_SURFACE_ID_STAGE),
        opacity, &host->stage.last_bounds, &host->stage.last_opacity, &host->stage.bounds_valid,
        &host->stage.opacity_valid, &window_changed);
    if (result != REACH_OK)
    {
        return result;
    }

    if (state->open)
    {
        if (reach_stage_is_open(host->stage_capsule))
        {
            reach_host_sync_stage_thumbnails(host);
            if (host->dirty.render || host->stage.dirty_flags || window_changed)
            {
                (void)reach_host_render_stage_surface(host, stage_bounds);
            }
        }
    }
    else
    {
        reach_host_cleanup_closed_stage(host);
    }

    return REACH_OK;
}

reach_result reach_host_frame_context_menu(reach_host *host, const reach_host_frame_context *ctx)
{
    (void)ctx;
    if (host->context_menu.window.ops.set_bounds == nullptr)
    {
        return REACH_OK;
    }

    reach_host_reanchor_context_menu(host);

    const reach_rect_f32 context_menu_bounds =
        reach_context_menu_state_ptr(host->context_menu_capsule)->bounds;
    reach_host_frame_state frame = {};
    reach_result result = reach_host_apply_transient_frame(
        host, REACH_SURFACE_ID_CONTEXT_MENU, &host->context_menu_transition, context_menu_bounds,
        reach_popup_radius_scaled(host->theme, reach_host_layout_dpi_scale(host)), &frame);
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
