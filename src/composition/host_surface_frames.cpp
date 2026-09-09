#include "host_internal.h"

#include <math.h>

void reach_host_sync_surface_input_regions(const reach_host *host,
                                           const reach_feature_runtime *desc)
{
    if (desc->surface == nullptr || desc->definition->capsule_ops == nullptr ||
        desc->definition->capsule_ops->input_regions == nullptr ||
        desc->surface->window.ops.set_input_regions == nullptr)
    {
        return;
    }

    reach_rect_f32 regions[REACH_PLATFORM_WINDOW_MAX_INPUT_REGIONS] = {};
    size_t region_count = desc->definition->capsule_ops->input_regions(
        desc->capsule, regions, REACH_PLATFORM_WINDOW_MAX_INPUT_REGIONS);

    reach_shadow_pad pad = reach_host_surface_shadow_pad(host, desc->definition->id);
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

static reach_rect_i32 reach_host_monitor_rect(reach_rect_f32 bounds)
{
    return {(int32_t)floorf(bounds.x), (int32_t)floorf(bounds.y),
            (int32_t)ceilf(bounds.x + bounds.width), (int32_t)ceilf(bounds.y + bounds.height)};
}

static void reach_host_sync_work_area_reservation(const reach_host *host,
                                                  const reach_feature_runtime *desc,
                                                  const reach_feature_surface_geometry *geometry,
                                                  reach_rect_f32 monitor_bounds)
{
    if (host == nullptr || desc == nullptr || geometry == nullptr ||
        !geometry->manage_monitor_work_area || host->monitors.list == nullptr ||
        host->monitors.ops.set_work_area == nullptr)
    {
        return;
    }

    reach_rect_i32 monitor = reach_host_monitor_rect(monitor_bounds);
    reach_rect_i32 work_area = monitor;
    if (geometry->reserve_monitor_work_area)
    {
        float clearance =
            geometry->work_area_clearance > 0.0f ? geometry->work_area_clearance : 0.0f;
        if (desc->definition->layout.reservation_edge == REACH_LAYOUT_RESERVATION_TOP)
        {
            work_area.top = (int32_t)ceilf(
                reach_bar_reserved_edge(REACH_BAR_EDGE_TOP, geometry->visible_bounds, clearance));
        }
        else if (desc->definition->layout.reservation_edge == REACH_LAYOUT_RESERVATION_BOTTOM)
        {
            work_area.bottom = (int32_t)floorf(reach_bar_reserved_edge(
                REACH_BAR_EDGE_BOTTOM, geometry->visible_bounds, clearance));
        }
    }
    if (work_area.right > work_area.left && work_area.bottom > work_area.top)
    {
        (void)host->monitors.ops.set_work_area(host->monitors.list, monitor, work_area);
    }
}

static reach_rect_f32 reach_host_available_bounds(const reach_host *host,
                                                  reach_rect_f32 monitor_bounds)
{
    reach_rect_f32 available = monitor_bounds;
    float bottom = monitor_bounds.y + monitor_bounds.height;
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *runtime = &host->feature_runtimes[index];
        if (!runtime->resolved_bounds_valid || runtime->definition == nullptr)
        {
            continue;
        }

        reach_rect_f32 reserved = runtime->resolved_bounds;
        if (runtime->definition->layout.reservation_edge == REACH_LAYOUT_RESERVATION_TOP)
        {
            float edge = reserved.y + reserved.height;
            if (edge > available.y)
            {
                available.y = edge;
            }
        }
        else if (runtime->definition->layout.reservation_edge == REACH_LAYOUT_RESERVATION_BOTTOM)
        {
            if (reserved.y < bottom)
            {
                bottom = reserved.y;
            }
        }
    }

    available.height = bottom > available.y ? bottom - available.y : 0.0f;
    return available;
}

static void reach_host_fill_surface_context(reach_host *host, const reach_feature_runtime *desc,
                                            const reach_host_frame_context *ctx,
                                            reach_feature_surface_context *out)
{
    *out = {};
    out->theme = host->theme != nullptr ? host->theme : reach_theme_default();
    out->monitor_bounds = ctx != nullptr ? ctx->monitor_bounds : reach_rect_f32{};
    out->available_bounds = out->monitor_bounds;
    if (ctx != nullptr && desc->definition != nullptr &&
        desc->definition->layout.uses_reserved_bounds)
    {
        out->available_bounds = reach_host_available_bounds(host, out->monitor_bounds);
    }
    out->last_bounds = desc->surface->last_bounds;
    out->text_measure.context = desc->surface->renderer.backend;
    out->text_measure.measure = desc->surface->renderer.ops.measure_text;
    out->dpi_scale = reach_host_layout_dpi_scale(host);
    out->icon_size_px = reach_host_icon_size_px(host);
    out->bounds_valid = desc->surface->bounds_valid;
}

static reach_result
reach_host_execute_registered_surface(reach_host *host, reach_feature_runtime *desc,
                                      const reach_feature_surface_context *ctx,
                                      const reach_feature_surface_geometry *geometry)
{
    reach_render_command_buffer *commands = &host->render_commands;
    reach_render_command_buffer_clear(commands);
    reach_result result =
        desc->definition->surface_ops->append_render_commands(desc->capsule, ctx, commands);
    if (result != REACH_OK)
    {
        return result;
    }
    const reach_feature_definition *definition = desc->definition;
    if (definition != nullptr && definition->surface.popup_chrome)
    {
        return reach_host_render_popup_surface(
            host, desc->definition->id, desc->surface, geometry->visible_bounds,
            geometry->notch_anchor_x, geometry->notch_side, commands,
            geometry->presentation.managed ? geometry->presentation.opacity : 1.0f);
    }
    reach_host_stamp_surface_content(host, desc->definition->id, commands);
    if (geometry->presentation.managed)
    {
        reach_render_command_buffer_multiply_opacity(commands, geometry->presentation.opacity);
    }
    if (ctx->content_transform_active)
    {
        reach_render_command_buffer_set_content_transform(commands, ctx->content_rect,
                                                          ctx->render_transform);
    }

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

static int32_t reach_host_bar_position_only(const reach_feature_runtime *desc)
{
    if (!reach_host_bar_reveal_enabled(desc) ||
        desc->definition->surface.bar_reveal.ops->animation == nullptr)
    {
        return 0;
    }
    reach_bar_reveal_animation animation =
        desc->definition->surface.bar_reveal.ops->animation(desc->capsule);
    return animation.position_animating && !animation.content_animating;
}

void reach_host_release_native_overlay(reach_host *host, reach_feature_runtime *desc)
{
    if (!desc->native_overlay_registered)
    {
        return;
    }
    for (size_t index = 0; index < REACH_SURFACE_NATIVE_OVERLAY_CAPACITY; ++index)
    {
        desc->native_overlay_ids[index] = REACH_WINDOW_THUMBNAIL_NONE;
        desc->native_overlay_sources[index] = 0;
        desc->native_overlay_planes[index] = REACH_WINDOW_THUMBNAIL_PLANE_TARGET;
    }
    desc->native_overlay_target = 0;
    desc->native_overlay_registered = 0;
    if (host->window_thumbnails.ops.destroy_all != nullptr)
    {
        (void)host->window_thumbnails.ops.destroy_all(host->window_thumbnails.thumbnails);
    }
}

static void reach_host_reconcile_native_overlay(reach_host *host, reach_feature_runtime *desc,
                                                const reach_feature_native_overlay_ops *ops)
{
    if (host->window_thumbnails.ops.create == nullptr ||
        host->window_thumbnails.ops.release == nullptr ||
        host->window_thumbnails.ops.set_target == nullptr ||
        desc->surface->window.ops.native_id == nullptr)
    {
        return;
    }

    reach_window_id target = desc->surface->window.ops.native_id(desc->surface->window.window);
    if (desc->native_overlay_registered && desc->native_overlay_target != target)
    {
        reach_host_release_native_overlay(host, desc);
    }
    if (target == 0 || host->window_thumbnails.ops.set_target(host->window_thumbnails.thumbnails,
                                                              target) != REACH_OK)
    {
        return;
    }

    reach_window_thumbnail_id previous_ids[REACH_SURFACE_NATIVE_OVERLAY_CAPACITY] = {};
    reach_window_id previous_sources[REACH_SURFACE_NATIVE_OVERLAY_CAPACITY] = {};
    reach_window_thumbnail_plane previous_planes[REACH_SURFACE_NATIVE_OVERLAY_CAPACITY] = {};
    int32_t previous_used[REACH_SURFACE_NATIVE_OVERLAY_CAPACITY] = {};
    for (size_t index = 0; index < REACH_SURFACE_NATIVE_OVERLAY_CAPACITY; ++index)
    {
        previous_ids[index] = desc->native_overlay_ids[index];
        previous_sources[index] = desc->native_overlay_sources[index];
        previous_planes[index] = desc->native_overlay_planes[index];
        desc->native_overlay_ids[index] = REACH_WINDOW_THUMBNAIL_NONE;
        desc->native_overlay_sources[index] = 0;
        desc->native_overlay_planes[index] = REACH_WINDOW_THUMBNAIL_PLANE_TARGET;
    }

    size_t count = ops->count(desc->capsule);
    if (count > REACH_SURFACE_NATIVE_OVERLAY_CAPACITY)
    {
        count = REACH_SURFACE_NATIVE_OVERLAY_CAPACITY;
    }
    const reach_theme *theme = host->theme != nullptr ? host->theme : reach_theme_default();
    for (size_t index = 0; index < count; ++index)
    {
        reach_feature_native_overlay_item item = {};
        if (ops->item(desc->capsule, index, theme, &item) != REACH_OK)
        {
            continue;
        }
        desc->native_overlay_sources[index] = item.source;
        desc->native_overlay_planes[index] = item.plane;
        for (size_t previous = 0; previous < REACH_SURFACE_NATIVE_OVERLAY_CAPACITY; ++previous)
        {
            if (!previous_used[previous] && previous_ids[previous] != REACH_WINDOW_THUMBNAIL_NONE &&
                previous_sources[previous] == item.source &&
                previous_planes[previous] == item.plane)
            {
                desc->native_overlay_ids[index] = previous_ids[previous];
                previous_used[previous] = 1;
                break;
            }
        }
    }

    for (size_t index = 0; index < REACH_SURFACE_NATIVE_OVERLAY_CAPACITY; ++index)
    {
        if (!previous_used[index] && previous_ids[index] != REACH_WINDOW_THUMBNAIL_NONE)
        {
            (void)host->window_thumbnails.ops.release(host->window_thumbnails.thumbnails,
                                                      previous_ids[index]);
        }
    }

    for (size_t index = count; index > 0; --index)
    {
        size_t item_index = index - 1;
        if (desc->native_overlay_sources[item_index] == 0 ||
            desc->native_overlay_ids[item_index] != REACH_WINDOW_THUMBNAIL_NONE)
        {
            continue;
        }
        reach_window_thumbnail_id id = REACH_WINDOW_THUMBNAIL_NONE;
        if (host->window_thumbnails.ops.create(
                host->window_thumbnails.thumbnails, desc->native_overlay_sources[item_index],
                desc->native_overlay_planes[item_index], &id) == REACH_OK)
        {
            desc->native_overlay_ids[item_index] = id;
        }
    }
    desc->native_overlay_target = target;
    desc->native_overlay_generation = ops->generation(desc->capsule);
    desc->native_overlay_registered = 1;
}

static reach_result reach_host_sync_native_overlay(reach_host *host, reach_feature_runtime *desc,
                                                   reach_rect_f32 visible_bounds)
{
    const reach_feature_native_overlay_ops *ops = desc->definition->surface_ops->native_overlay;
    if (ops == nullptr || ops->generation == nullptr || ops->count == nullptr ||
        ops->item == nullptr || host->window_thumbnails.ops.set_placement == nullptr)
    {
        return REACH_ERROR;
    }
    if (!desc->native_overlay_registered ||
        desc->native_overlay_generation != ops->generation(desc->capsule))
    {
        reach_host_reconcile_native_overlay(host, desc, ops);
    }

    size_t count = ops->count(desc->capsule);
    if (count > REACH_SURFACE_NATIVE_OVERLAY_CAPACITY)
    {
        count = REACH_SURFACE_NATIVE_OVERLAY_CAPACITY;
    }
    reach_result placement_result = REACH_OK;
    for (size_t index = 0; index < count; ++index)
    {
        reach_window_thumbnail_id id = desc->native_overlay_ids[index];
        reach_feature_native_overlay_item item = {};
        const reach_theme *theme = host->theme != nullptr ? host->theme : reach_theme_default();
        if (ops->item(desc->capsule, index, theme, &item) != REACH_OK)
        {
            continue;
        }
        if (id == REACH_WINDOW_THUMBNAIL_NONE)
        {
            if (item.placement.visible || item.placement.background_visible)
            {
                reach_window_thumbnail_id created = REACH_WINDOW_THUMBNAIL_NONE;
                if (host->window_thumbnails.ops.create != nullptr &&
                    host->window_thumbnails.ops.create(host->window_thumbnails.thumbnails,
                                                       item.source, item.plane,
                                                       &created) == REACH_OK)
                {
                    desc->native_overlay_ids[index] = created;
                    id = created;
                    desc->native_overlay_registered = 1;
                }
            }
            if (id == REACH_WINDOW_THUMBNAIL_NONE)
            {
                continue;
            }
        }
        item.placement.destination.x -= visible_bounds.x;
        item.placement.destination.y -= visible_bounds.y;
        reach_result result = host->window_thumbnails.ops.set_placement(
            host->window_thumbnails.thumbnails, id, &item.placement);
        if (result != REACH_OK)
        {
            placement_result = result;
        }
    }
    return placement_result;
}

reach_result reach_host_redraw_registered_surface(reach_host *host, reach_surface_id id)
{
    if (host == nullptr || id >= REACH_HOST_SURFACE_COUNT)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_feature_runtime *desc = &host->feature_runtimes[id];
    if (desc->surface == nullptr || desc->definition->surface_ops == nullptr ||
        desc->definition->surface_ops->append_render_commands == nullptr ||
        desc->definition->capsule_ops == nullptr ||
        desc->definition->capsule_ops->surface_geometry == nullptr)
    {
        return REACH_ERROR;
    }

    reach_feature_surface_geometry geometry = {};
    desc->definition->capsule_ops->surface_geometry(desc->capsule, &geometry);
    reach_feature_surface_context surface_ctx = {};
    reach_host_fill_surface_context(host, desc, nullptr, &surface_ctx);
    surface_ctx.visible_bounds = geometry.visible_bounds;
    surface_ctx.render_bounds =
        desc->surface->bounds_valid ? desc->surface->last_bounds : geometry.visible_bounds;
    return reach_host_execute_registered_surface(host, desc, &surface_ctx, &geometry);
}

reach_result reach_host_frame_registered_surface(reach_host *host, reach_feature_runtime *desc,
                                                 const reach_host_frame_context *ctx)
{
    REACH_ASSERT(host != nullptr);
    REACH_ASSERT(desc != nullptr);
    REACH_ASSERT(ctx != nullptr);
    if (host == nullptr || desc == nullptr || ctx == nullptr || desc->definition == nullptr ||
        desc->surface == nullptr || desc->surface->window.ops.set_bounds == nullptr)
    {
        return REACH_OK;
    }
    if (desc->definition->surface_ops == nullptr ||
        desc->definition->surface_ops->arrange == nullptr ||
        desc->definition->surface_ops->append_render_commands == nullptr ||
        desc->definition->capsule_ops == nullptr ||
        desc->definition->capsule_ops->surface_geometry == nullptr)
    {
        return REACH_ERROR;
    }

    int32_t needs_frame = reach_host_surface_needs_frame(desc);
    int32_t active = reach_host_surface_presented(desc);
    int32_t visible = active;
    int32_t frame_active = active;
    if (!visible)
    {
        reach_host_set_surface_visible(host, desc->definition->id, 0);
        reach_feature_surface_geometry hidden_geometry = {};
        desc->definition->capsule_ops->surface_geometry(desc->capsule, &hidden_geometry);
        hidden_geometry.reserve_monitor_work_area = 0;
        reach_host_sync_work_area_reservation(host, desc, &hidden_geometry, ctx->monitor_bounds);
        reach_host_release_native_overlay(host, desc);
        return REACH_OK;
    }
    reach_host_set_surface_visible(host, desc->definition->id, visible);

    reach_feature_surface_context surface_ctx = {};
    reach_host_fill_surface_context(host, desc, ctx, &surface_ctx);
    surface_ctx.transition_visible = visible;
    const reach_feature_definition *definition = desc->definition;
    reach_feature_layout_anchor layout_anchor = {};
    layout_anchor.surface = definition->layout.anchor;
    layout_anchor.slot = definition->layout.anchor_slot;
    if (desc->definition->surface_ops->layout_anchor != nullptr)
    {
        (void)desc->definition->surface_ops->layout_anchor(desc->capsule, &layout_anchor);
    }
    reach_surface_id anchor_id = layout_anchor.surface;
    if (anchor_id < REACH_HOST_SURFACE_COUNT)
    {
        const reach_feature_runtime *anchor = &host->feature_runtimes[anchor_id];
        if (anchor->resolved_bounds_valid)
        {
            surface_ctx.anchor_bounds = anchor->resolved_bounds;
        }
        if (anchor->definition != nullptr && anchor->definition->resolve_anchor != nullptr &&
            definition != nullptr)
        {
            reach_feature_anchor resolved = {};
            if (anchor->definition->resolve_anchor(anchor->capsule, layout_anchor.slot,
                                                   layout_anchor.index, &resolved))
            {
                surface_ctx.anchor_button = resolved.button;
                surface_ctx.anchor_bar_edge_y = resolved.bar_edge_y;
                surface_ctx.anchor_bar_height = resolved.bar_height;
                surface_ctx.anchor_direction = resolved.direction;
                surface_ctx.anchor_valid = 1;
            }
        }
    }

    int32_t layout_changed = desc->definition->surface_ops->arrange(desc->capsule, &surface_ctx);
    if (needs_frame)
    {
        desc->surface->dirty_flags = 1;
        reach_host_request_update(host);
    }
    reach_feature_surface_geometry geometry = {};
    desc->definition->capsule_ops->surface_geometry(desc->capsule, &geometry);
    desc->yield_topmost_to_foreground_fullscreen = geometry.yield_topmost_to_foreground_fullscreen;
    (void)reach_layout_set_layer_ceiling(&host->layout_manager,
                                         host->surface_participants[desc->definition->id],
                                         REACH_LAYOUT_CONDITION_FOREGROUND_FULLSCREEN,
                                         geometry.yield_topmost_to_foreground_fullscreen, 0);
    int32_t geometry_changed = !desc->resolved_bounds_valid ||
                               !reach_rect_equal(desc->resolved_bounds, geometry.visible_bounds);
    desc->resolved_bounds = geometry.visible_bounds;
    desc->resolved_bounds_valid = 1;
    surface_ctx.visible_bounds = geometry.visible_bounds;
    reach_host_sync_work_area_reservation(host, desc, &geometry, ctx->monitor_bounds);

    reach_rect_f32 bounds = geometry.visible_bounds;
    if (desc->definition->surface.bar_reveal.ops != nullptr)
    {
        bounds = reach_host_reconcile_bar_visibility(
            host, desc->definition->id, geometry.visible_bounds, ctx->monitor_bounds, &geometry);
    }
    reach_shadow_pad shadow_pad = reach_host_surface_shadow_pad(host, desc->definition->id);
    float applied_scale = 1.0f;
    int32_t scale_changed = 0;
    int32_t transition_frame_active = 0;
    if (geometry.presentation.managed && geometry.presentation.max_scale > 1.0f)
    {
        float shadow_scale = geometry.presentation.max_scale;
        shadow_pad.left *= shadow_scale;
        shadow_pad.top *= shadow_scale;
        shadow_pad.right *= shadow_scale;
        shadow_pad.bottom *= shadow_scale;
        reach_host_surface_presentation_frame frame = reach_host_surface_presentation_frame_compute(
            geometry.visible_bounds, geometry.envelope_bounds, shadow_pad,
            geometry.presentation.y_offset, geometry.presentation.scale,
            geometry.presentation.max_scale);
        bounds = frame.window_bounds;
        surface_ctx.content_rect = frame.content_rect;
        surface_ctx.render_transform = frame.render_transform;
        surface_ctx.content_transform_active = frame.scale_envelope_active;
        applied_scale = frame.scale;
        scale_changed = !desc->surface->transition_scale_valid ||
                        !reach_host_scalar_equal(desc->surface->last_transition_scale, frame.scale);
        transition_frame_active = needs_frame;
        if (desc->definition->surface_ops->set_pointer_transform != nullptr)
        {
            desc->definition->surface_ops->set_pointer_transform(desc->capsule,
                                                                 frame.pointer_transform);
        }
        if (desc->surface->window.ops.set_input_regions != nullptr)
        {
            if (frame.scale_envelope_active)
            {
                (void)desc->surface->window.ops.set_input_regions(desc->surface->window.window,
                                                                  &frame.content_rect, 1);
                desc->surface->transition_input_region_active = 1;
            }
            else if (desc->surface->transition_input_region_active)
            {
                (void)desc->surface->window.ops.set_input_regions(desc->surface->window.window,
                                                                  nullptr, 0);
                desc->surface->transition_input_region_active = 0;
            }
        }
    }
    else if (geometry.presentation.managed)
    {
        bounds.y += geometry.presentation.y_offset;
        transition_frame_active = needs_frame;
    }
    surface_ctx.render_bounds = bounds;

    int32_t window_changed = 0;
    reach_result result = reach_host_apply_window_state(
        &desc->surface->window, bounds, shadow_pad, &desc->surface->last_bounds,
        &desc->surface->bounds_valid, &window_changed);
    int32_t position_only = reach_host_bar_position_only(desc);
    int32_t render_needed = host->dirty.render || desc->surface->dirty_flags || layout_changed ||
                            geometry_changed || scale_changed || transition_frame_active ||
                            (window_changed && !position_only);
    reach_result native_overlay_result = REACH_OK;
    if (result == REACH_OK && active && desc->definition->surface_ops->native_overlay != nullptr)
    {
        native_overlay_result = reach_host_sync_native_overlay(host, desc, geometry.visible_bounds);
    }
    if (result != REACH_OK ||
        (native_overlay_result != REACH_OK && geometry.synchronize_presentation))
    {
        reach_result frame_result = result != REACH_OK ? result : native_overlay_result;
        if (desc->definition->capsule_ops->presentation_committed != nullptr)
        {
            reach_feature_tick_result tick = {};
            desc->definition->capsule_ops->presentation_committed(desc->capsule, frame_result,
                                                                  &tick);
            reach_host_apply_feature_tick_result(host, desc, &tick);
        }
        return frame_result;
    }
    if (!frame_active || !render_needed)
    {
        return result;
    }
    result = reach_host_execute_registered_surface(host, desc, &surface_ctx, &geometry);
    if (geometry.synchronize_presentation)
    {
        if (result == REACH_OK)
        {
            result = desc->surface->renderer.ops.synchronize != nullptr
                         ? desc->surface->renderer.ops.synchronize(desc->surface->renderer.backend)
                         : REACH_ERROR;
        }
        if (desc->definition->capsule_ops->presentation_committed != nullptr)
        {
            reach_feature_tick_result tick = {};
            desc->definition->capsule_ops->presentation_committed(desc->capsule, result, &tick);
            reach_host_apply_feature_tick_result(host, desc, &tick);
        }
    }
    if (result == REACH_OK && geometry.presentation.managed &&
        geometry.presentation.max_scale > 1.0f)
    {
        desc->surface->last_transition_scale = applied_scale;
        desc->surface->transition_scale_valid = 1;
    }
    return result;
}
