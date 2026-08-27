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
                                                     reach_rect_f32 target_bounds,
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

    out->visible = reach_host_surface_transition_visible(transition);
    reach_host_set_surface_visible(host, id, out->visible);

    return REACH_OK;
}

static void reach_host_fill_surface_context(reach_host *host, const reach_surface_desc *desc,
                                            const reach_host_frame_context *ctx,
                                            reach_feature_surface_context *out)
{
    *out = {};
    out->theme = host->theme != nullptr ? host->theme : reach_theme_default();
    out->monitor_bounds = ctx != nullptr ? ctx->monitor_bounds : reach_rect_f32{};
    out->last_bounds = desc->surface->last_bounds;
    out->text_measure.context = desc->surface->renderer.backend;
    out->text_measure.measure = desc->surface->renderer.ops.measure_text;
    out->dpi_scale = reach_host_layout_dpi_scale(host);
    out->icon_size_px = reach_host_icon_size_px(host);
    out->bounds_valid = desc->surface->bounds_valid;
}

static reach_result
reach_host_execute_registered_surface(reach_host *host, reach_surface_desc *desc,
                                      const reach_feature_surface_context *ctx,
                                      const reach_feature_surface_geometry *geometry)
{
    reach_render_command_buffer *commands = &host->render_commands;
    reach_render_command_buffer_clear(commands);
    reach_result result = desc->surface_ops->append_render_commands(desc->capsule, ctx, commands);
    if (result != REACH_OK)
    {
        return result;
    }

    const reach_feature_definition *definition = desc->definition;
    if (definition != nullptr && definition->surface.popup_chrome)
    {
        return reach_host_render_popup_surface(host, desc->id, desc->surface,
                                               geometry->visible_bounds, geometry->notch_anchor_x,
                                               geometry->notch_side, commands);
    }
    reach_host_stamp_surface_content(host, desc->id, commands);

    if (desc->surface->renderer.ops.begin_frame == nullptr)
    {
        return REACH_OK;
    }
    result = desc->surface->renderer.ops.begin_frame(desc->surface->renderer.backend);
    if (result != REACH_OK)
    {
        return result;
    }
    result = desc->surface->renderer.ops.execute(desc->surface->renderer.backend, commands);
    reach_result end_result =
        desc->surface->renderer.ops.end_frame(desc->surface->renderer.backend);
    return result != REACH_OK ? result : end_result;
}

static int32_t reach_host_bar_position_only(const reach_surface_desc *desc)
{
    if (desc->bar_reveal.ops == nullptr || desc->bar_reveal.ops->animation == nullptr)
    {
        return 0;
    }
    reach_bar_reveal_animation animation = desc->bar_reveal.ops->animation(desc->capsule);
    return animation.position_animating && !animation.content_animating;
}

reach_result reach_host_redraw_registered_surface(reach_host *host, reach_surface_id id)
{
    if (host == nullptr || id >= REACH_HOST_SURFACE_COUNT)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_surface_desc *desc = &host->surface_descs[id];
    if (desc->surface == nullptr || desc->surface_ops == nullptr ||
        desc->surface_ops->append_render_commands == nullptr || desc->capsule_ops == nullptr ||
        desc->capsule_ops->surface_geometry == nullptr)
    {
        return REACH_ERROR;
    }

    reach_feature_surface_geometry geometry = {};
    desc->capsule_ops->surface_geometry(desc->capsule, &geometry);
    reach_feature_surface_context surface_ctx = {};
    reach_host_fill_surface_context(host, desc, nullptr, &surface_ctx);
    surface_ctx.visible_bounds = geometry.visible_bounds;
    surface_ctx.render_bounds =
        desc->surface->bounds_valid ? desc->surface->last_bounds : geometry.visible_bounds;
    return reach_host_execute_registered_surface(host, desc, &surface_ctx, &geometry);
}

reach_result reach_host_frame_registered_surface(reach_host *host, reach_surface_desc *desc,
                                                 const reach_host_frame_context *ctx)
{
    REACH_ASSERT(host != nullptr);
    REACH_ASSERT(desc != nullptr);
    REACH_ASSERT(ctx != nullptr);
    if (host == nullptr || desc == nullptr || ctx == nullptr || desc->surface == nullptr ||
        desc->surface->window.ops.set_bounds == nullptr)
    {
        return REACH_OK;
    }
    if (desc->surface_ops == nullptr || desc->surface_ops->arrange == nullptr ||
        desc->surface_ops->append_render_commands == nullptr || desc->capsule_ops == nullptr ||
        desc->capsule_ops->surface_geometry == nullptr)
    {
        return REACH_ERROR;
    }

    int32_t open =
        desc->capsule_ops->is_open == nullptr ? 1 : desc->capsule_ops->is_open(desc->capsule);
    int32_t visible = desc->transition != nullptr
                          ? reach_host_surface_transition_visible(desc->transition)
                          : open;
    reach_host_set_surface_visible(host, desc->id, visible);
    if (!visible)
    {
        return REACH_OK;
    }

    reach_feature_surface_context surface_ctx = {};
    reach_host_fill_surface_context(host, desc, ctx, &surface_ctx);
    surface_ctx.transition_visible = visible;
    const reach_feature_definition *definition = desc->definition;
    reach_surface_id anchor_id =
        definition != nullptr ? definition->layout.anchor : desc->layout_anchor;
    if (anchor_id < REACH_HOST_SURFACE_COUNT)
    {
        const reach_surface_desc *anchor = &host->surface_descs[anchor_id];
        if (anchor->resolved_bounds_valid)
        {
            surface_ctx.anchor_bounds = anchor->resolved_bounds;
        }
        if (anchor->definition != nullptr && anchor->definition->resolve_anchor != nullptr &&
            definition != nullptr)
        {
            reach_feature_anchor resolved = {};
            if (anchor->definition->resolve_anchor(anchor->capsule, definition->layout.anchor_slot,
                                                   0, &resolved))
            {
                surface_ctx.anchor_button = resolved.button;
                surface_ctx.anchor_bar_edge_y = resolved.bar_edge_y;
                surface_ctx.anchor_bar_height = resolved.bar_height;
                surface_ctx.anchor_direction = resolved.direction;
                surface_ctx.anchor_valid = 1;
            }
        }
    }

    int32_t layout_changed = desc->surface_ops->arrange(desc->capsule, &surface_ctx);
    if (desc->capsule_ops->needs_frame != nullptr && desc->capsule_ops->needs_frame(desc->capsule))
    {
        desc->surface->dirty_flags = 1;
        reach_host_request_update(host);
    }
    reach_feature_surface_geometry geometry = {};
    desc->capsule_ops->surface_geometry(desc->capsule, &geometry);
    int32_t geometry_changed =
        !desc->resolved_bounds_valid ||
        !reach_host_rect_equal(desc->resolved_bounds, geometry.visible_bounds);
    desc->resolved_bounds = geometry.visible_bounds;
    desc->resolved_bounds_valid = 1;
    surface_ctx.visible_bounds = geometry.visible_bounds;

    reach_rect_f32 bounds = geometry.visible_bounds;
    if (desc->bar_reveal.ops != nullptr)
    {
        bounds = reach_host_reconcile_bar_visibility(host, desc->id, geometry.visible_bounds,
                                                     ctx->monitor_bounds);
    }
    float opacity = 1.0f;
    if (desc->transition != nullptr)
    {
        bounds = reach_host_surface_transition_bounds(host, desc->transition, bounds);
        opacity = reach_host_surface_transition_opacity(host, desc->transition);
    }
    surface_ctx.render_bounds = bounds;

    int32_t window_changed = 0;
    reach_result result = reach_host_apply_window_state(
        &desc->surface->window, bounds, reach_host_surface_shadow_pad(host, desc->id), opacity,
        &desc->surface->last_bounds, &desc->surface->last_opacity, &desc->surface->bounds_valid,
        &desc->surface->opacity_valid, &window_changed);
    int32_t position_only = reach_host_bar_position_only(desc);
    int32_t render_needed = host->dirty.render || desc->surface->dirty_flags || layout_changed ||
                            geometry_changed || (window_changed && !position_only);
    if (result != REACH_OK || !open || !render_needed)
    {
        return result;
    }
    return reach_host_execute_registered_surface(host, desc, &surface_ctx, &geometry);
}

reach_result reach_host_frame_launcher(reach_host *host, const reach_host_frame_context *ctx)
{
    const int32_t launcher_layout_changed = ctx->launcher_layout_changed;
    reach_shadow_pad launcher_shadow_pad =
        reach_host_surface_shadow_pad(host, REACH_SURFACE_ID_LAUNCHER);
    reach_feature_surface_geometry launcher_geometry = {};
    launcher_geometry.visible_bounds = host->layout.launcher.bounds;
    launcher_geometry.envelope_bounds = host->layout.launcher.envelope_bounds;
    const reach_feature_capsule_ops *launcher_ops =
        host->surface_descs[REACH_SURFACE_ID_LAUNCHER].capsule_ops;
    if (launcher_ops != nullptr && launcher_ops->surface_geometry != nullptr)
    {
        launcher_ops->surface_geometry(host->launcher_capsule, &launcher_geometry);
    }
    reach_surface_desc *launcher_desc = &host->surface_descs[REACH_SURFACE_ID_LAUNCHER];
    launcher_desc->resolved_bounds = launcher_geometry.visible_bounds;
    launcher_desc->resolved_bounds_valid = 1;
    reach_host_surface_transition_frame frame =
        reach_host_surface_transition_frame_compute_in_envelope(
            host, &host->launcher_transition, launcher_geometry.visible_bounds,
            launcher_geometry.envelope_bounds, launcher_shadow_pad);
    if (frame.scale_envelope_active)
    {
        launcher_shadow_pad.left *= REACH_HOST_LAUNCHER_TRANSITION_SCALE;
        launcher_shadow_pad.top *= REACH_HOST_LAUNCHER_TRANSITION_SCALE;
        launcher_shadow_pad.right *= REACH_HOST_LAUNCHER_TRANSITION_SCALE;
        launcher_shadow_pad.bottom *= REACH_HOST_LAUNCHER_TRANSITION_SCALE;
        frame = reach_host_surface_transition_frame_compute_in_envelope(
            host, &host->launcher_transition, launcher_geometry.visible_bounds,
            launcher_geometry.envelope_bounds, launcher_shadow_pad);
    }
    int32_t launcher_scale_changed =
        !host->launcher.transition_scale_valid ||
        !reach_host_scalar_equal(host->launcher.last_transition_scale, frame.scale);

    int32_t launcher_window_changed = 0;
    float launcher_opacity =
        reach_host_surface_transition_opacity(host, &host->launcher_transition);
    reach_result result = reach_host_apply_window_state(
        &host->launcher.window, frame.window_bounds, launcher_shadow_pad, launcher_opacity,
        &host->launcher.last_bounds, &host->launcher.last_opacity, &host->launcher.bounds_valid,
        &host->launcher.opacity_valid, &launcher_window_changed);
    if (result != REACH_OK)
    {
        return result;
    }

    reach_launcher_set_pointer_transform(host->launcher_capsule, frame.pointer_transform);

    if (host->launcher.window.ops.set_input_regions != nullptr)
    {
        if (frame.scale_envelope_active)
        {
            (void)host->launcher.window.ops.set_input_regions(host->launcher.window.window,
                                                              &frame.content_rect, 1);
            host->launcher.transition_input_region_active = 1;
        }
        else if (host->launcher.transition_input_region_active)
        {
            (void)host->launcher.window.ops.set_input_regions(host->launcher.window.window, nullptr,
                                                              0);
            host->launcher.transition_input_region_active = 0;
        }
    }

    if (reach_host_surface_transition_visible(&host->launcher_transition) &&
        (host->dirty.render || host->launcher.dirty_flags || launcher_layout_changed ||
         launcher_window_changed || launcher_scale_changed ||
         reach_host_surface_transition_active(host, &host->launcher_transition)))
    {
        (void)reach_host_render_launcher_surface(host, &host->layout.launcher, &frame);
        host->launcher.last_transition_scale = frame.scale;
        host->launcher.transition_scale_valid = 1;
    }
    reach_host_set_surface_visible(
        host, REACH_SURFACE_ID_LAUNCHER,
        reach_host_surface_transition_visible(&host->launcher_transition));

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
    reach_result result = reach_host_apply_transient_frame(host, REACH_SURFACE_ID_CONTEXT_MENU,
                                                           &host->context_menu_transition,
                                                           context_menu_bounds, &frame);
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
