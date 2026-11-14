#include "host_internal.h"
#include "reach/support/workload.h"

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

static int32_t reach_host_bar_auto_hide_active(const reach_host *host,
                                               const reach_feature_runtime *desc,
                                               const reach_feature_surface_geometry *geometry,
                                               reach_rect_f32 monitor_bounds)
{
    if (host == nullptr || desc == nullptr || geometry == nullptr || desc->definition == nullptr ||
        desc->definition->surface.bar_reveal.ops == nullptr)
    {
        return 0;
    }
    if (geometry->bar_reveal_policy == REACH_FEATURE_BAR_REVEAL_ALWAYS)
    {
        return 1;
    }
    if (geometry->bar_reveal_policy == REACH_FEATURE_BAR_REVEAL_FULLSCREEN)
    {
        return reach_window_tracking_any_non_game_fullscreen(host->window_tracking, monitor_bounds);
    }
    return 0;
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
    out->retained_supported =
        desc->surface->renderer.ops.retained_supported != nullptr &&
        desc->surface->renderer.ops.retained_supported(desc->surface->renderer.backend);
    out->preferred_front_source = desc->native_overlay_front_source;
}

static reach_result
reach_host_execute_registered_surface(reach_host *host, reach_feature_runtime *desc,
                                      const reach_feature_surface_context *ctx,
                                      const reach_feature_surface_geometry *geometry)
{
    reach_render_command_buffer *commands = &host->render_commands;
    reach_render_command_buffer_clear(commands);
    reach_workload_add(REACH_WORKLOAD_COMMAND_BUILDS, 1);
    reach_result result =
        desc->definition->surface_ops->append_render_commands(desc->capsule, ctx, commands);
    if (result != REACH_OK)
    {
        return result;
    }
    desc->surface->commands_require_content_sampling = commands->requires_content_sampling;
    const reach_feature_definition *definition = desc->definition;
    if (geometry->presentation.retained)
    {
        commands->has_presentation = 1;
        commands->presentation = geometry->presentation.group;
        if (ctx->content_transform_active)
        {
            reach_render_command_buffer_set_content_transform(commands, ctx->content_rect,
                                                              ctx->render_transform);
        }
        else if (desc->surface->retained_presentation)
        {
            reach_render_command_buffer_set_content_transform(
                commands, desc->surface->render_content_rect, desc->surface->render_transform);
        }
        else
        {
            reach_host_stamp_surface_content(host, desc->definition->id, commands);
        }
        commands->presentation.bounds = commands->content_rect;
        commands->presentation_center_x =
            geometry->presentation.center_x.generation
                ? reach_motion_affine(&geometry->presentation.center_x,
                                      commands->content_transform.scale_x,
                                      commands->content_transform.offset_x)
                : reach_motion{};
        commands->presentation_center_y =
            geometry->presentation.center_y.generation
                ? reach_motion_affine(&geometry->presentation.center_y,
                                      commands->content_transform.scale_y,
                                      commands->content_transform.offset_y)
                : reach_motion{};
    }
    desc->surface->fallback_animation_active =
        reach_render_backend_content_sampling_active(&desc->surface->renderer, commands);
    if (definition != nullptr && definition->surface.popup_chrome)
    {
        result = reach_host_render_popup_surface(host, desc->definition->id, desc->surface,
                                                 geometry->visible_bounds, geometry->notch_anchor_x,
                                                 geometry->notch_side, commands);
        desc->surface->fallback_animation_active =
            reach_render_backend_content_sampling_active(&desc->surface->renderer, commands);
        return result;
    }
    if (!geometry->presentation.retained)
    {
        reach_host_stamp_surface_content(host, desc->definition->id, commands);
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
    desc->surface->fallback_animation_active =
        reach_render_backend_content_sampling_active(&desc->surface->renderer, commands);
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
    desc->frame_context_valid = 0;
    desc->native_overlay_motion_pending = 0;
    desc->native_overlay_front_source = 0;
    desc->native_overlay_front_id = REACH_WINDOW_THUMBNAIL_NONE;
    if (!desc->native_overlay_registered)
    {
        return;
    }
    for (size_t index = 0; index < REACH_SURFACE_NATIVE_OVERLAY_CAPACITY; ++index)
    {
        if (desc->native_overlay_ids[index] != REACH_WINDOW_THUMBNAIL_NONE &&
            host->window_thumbnails.ops.release != nullptr)
        {
            (void)host->window_thumbnails.ops.release(host->window_thumbnails.thumbnails,
                                                      desc->native_overlay_ids[index]);
        }
        desc->native_overlay_ids[index] = REACH_WINDOW_THUMBNAIL_NONE;
        desc->native_overlay_sources[index] = 0;
        desc->native_overlay_planes[index] = REACH_WINDOW_THUMBNAIL_PLANE_TARGET;
    }
    desc->native_overlay_target = 0;
    desc->native_overlay_registered = 0;
}

static void reach_host_apply_native_overlay_front(reach_host *host, reach_feature_runtime *desc)
{
    if (desc->native_overlay_front_source == 0 ||
        host->window_thumbnails.ops.bring_to_front == nullptr)
    {
        return;
    }
    for (size_t index = 0; index < REACH_SURFACE_NATIVE_OVERLAY_CAPACITY; ++index)
    {
        reach_window_thumbnail_id id = desc->native_overlay_ids[index];
        if (id == REACH_WINDOW_THUMBNAIL_NONE || id == desc->native_overlay_front_id ||
            desc->native_overlay_sources[index] != desc->native_overlay_front_source ||
            desc->native_overlay_planes[index] != REACH_WINDOW_THUMBNAIL_PLANE_TARGET)
        {
            continue;
        }
        if (host->window_thumbnails.ops.bring_to_front(host->window_thumbnails.thumbnails, id) ==
            REACH_OK)
        {
            desc->native_overlay_front_id = id;
        }
        return;
    }
}

void reach_host_set_native_overlay_front_source(reach_host *host, reach_feature_runtime *desc,
                                                reach_window_id source)
{
    if (host == nullptr || desc == nullptr || source == 0 ||
        desc->native_overlay_front_source == source)
    {
        return;
    }
    desc->native_overlay_front_source = source;
    desc->native_overlay_front_id = REACH_WINDOW_THUMBNAIL_NONE;
    reach_host_apply_native_overlay_front(host, desc);
    if (desc->surface != nullptr)
    {
        desc->surface->dirty_flags = 1;
    }
    reach_host_request_update(host);
}

static void reach_host_reconcile_native_overlay(reach_host *host, reach_feature_runtime *desc,
                                                const reach_feature_native_overlay_ops *ops)
{
    if (host->window_thumbnails.ops.create == nullptr ||
        host->window_thumbnails.ops.release == nullptr ||
        desc->surface->window.ops.native_id == nullptr)
    {
        return;
    }

    reach_window_id target = desc->surface->window.ops.native_id(desc->surface->window.window);
    if (desc->native_overlay_registered && desc->native_overlay_target != target)
    {
        reach_window_id front_source = desc->native_overlay_front_source;
        reach_host_release_native_overlay(host, desc);
        desc->native_overlay_front_source = front_source;
    }
    if (target == 0)
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
        if (host->window_thumbnails.ops.create(host->window_thumbnails.thumbnails, target,
                                               desc->native_overlay_sources[item_index],
                                               desc->native_overlay_planes[item_index],
                                               &id) == REACH_OK)
        {
            desc->native_overlay_ids[item_index] = id;
        }
    }
    reach_host_apply_native_overlay_front(host, desc);
    desc->native_overlay_target = target;
    desc->native_overlay_generation = ops->generation(desc->capsule);
    desc->native_overlay_registered = 1;
}

reach_window_thumbnail_placement reach_host_native_overlay_placement(
    reach_window_thumbnail_placement placement, const reach_feature_surface_geometry *geometry,
    const reach_feature_surface_context *ctx, reach_shadow_pad shadow_pad, double now_seconds)
{
    reach_rect_f32 *rect = &placement.destination;
    rect->x -= geometry->visible_bounds.x;
    rect->y -= geometry->visible_bounds.y;
    reach_transform_f32 transform =
        ctx->content_transform_active
            ? ctx->render_transform
            : reach_transform_f32{1.0f, 1.0f, shadow_pad.left, shadow_pad.top};
    rect->x = rect->x * transform.scale_x + transform.offset_x;
    rect->y = rect->y * transform.scale_y + transform.offset_y;
    rect->width *= transform.scale_x;
    rect->height *= transform.scale_y;
    if (geometry->presentation.retained)
    {
        const reach_render_group *group = &geometry->presentation.group;
        reach_point_f32 scale = reach_render_group_scale_at(group, now_seconds);
        float center_x =
            geometry->presentation.center_x.generation
                ? reach_motion_value_at(&geometry->presentation.center_x, now_seconds) *
                          transform.scale_x +
                      transform.offset_x
                : ctx->content_rect.x + ctx->content_rect.width * .5f;
        float center_y =
            geometry->presentation.center_y.generation
                ? reach_motion_value_at(&geometry->presentation.center_y, now_seconds) *
                          transform.scale_y +
                      transform.offset_y
                : ctx->content_rect.y + ctx->content_rect.height * .5f;
        rect->x = center_x + (rect->x - center_x) * scale.x +
                  reach_motion_value_at(&group->x, now_seconds);
        rect->y = center_y + (rect->y - center_y) * scale.y +
                  reach_motion_value_at(&group->y, now_seconds);
        rect->width *= scale.x;
        rect->height *= scale.y;
        float opacity =
            group->opacity.generation ? reach_motion_value_at(&group->opacity, now_seconds) : 1.0f;
        placement.opacity *= opacity;
        placement.background.a *= opacity;
    }
    return placement;
}

static int32_t reach_host_native_presentation_active(const reach_feature_surface_geometry *geometry,
                                                     double now_seconds)
{
    if (!geometry->presentation.retained)
        return 0;
    const reach_render_group *group = &geometry->presentation.group;
    auto scale = reach_render_group_scale_at(group, now_seconds);
    return reach_motion_active_at(&group->x, now_seconds) ||
           reach_motion_active_at(&group->y, now_seconds) ||
           reach_motion_active_at(&group->scale, now_seconds) ||
           reach_motion_active_at(&group->scale_y, now_seconds) ||
           reach_motion_active_at(&group->opacity, now_seconds) ||
           (scale.x != 1 &&
            reach_motion_active_at(&geometry->presentation.center_x, now_seconds)) ||
           (scale.y != 1 && reach_motion_active_at(&geometry->presentation.center_y, now_seconds));
}

static reach_result reach_host_sync_native_overlay(reach_host *host, reach_feature_runtime *desc,
                                                   const reach_feature_surface_geometry *geometry,
                                                   const reach_feature_surface_context *ctx,
                                                   reach_shadow_pad shadow_pad)
{
    desc->native_overlay_motion_pending = 0;
    const reach_feature_native_overlay_ops *ops = desc->definition->surface_ops->native_overlay;
    if (ops == nullptr || ops->generation == nullptr || ops->count == nullptr ||
        ops->item == nullptr || host->window_thumbnails.ops.set_placement == nullptr)
    {
        return REACH_ERROR;
    }
    reach_window_id target = desc->surface->window.ops.native_id != nullptr
                                 ? desc->surface->window.ops.native_id(desc->surface->window.window)
                                 : 0;
    if (!desc->native_overlay_registered || desc->native_overlay_target != target ||
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
    double now_seconds = reach_motion_now_seconds();
    for (size_t index = 0; index < count; ++index)
    {
        reach_window_thumbnail_id id = desc->native_overlay_ids[index];
        reach_feature_native_overlay_item item = {};
        const reach_theme *theme = host->theme != nullptr ? host->theme : reach_theme_default();
        if (ops->item(desc->capsule, index, theme, &item) != REACH_OK)
        {
            continue;
        }
        if (item.source == 0)
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
                                                       desc->native_overlay_target, item.source,
                                                       item.plane, &created) == REACH_OK)
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
        int32_t destination_active = 0;
        if (item.has_destination_motion)
        {
            item.placement.destination =
                reach_rect_motion_value_at(&item.destination_motion, now_seconds);
            destination_active =
                reach_motion_active_at(&item.destination_motion.x, now_seconds) ||
                reach_motion_active_at(&item.destination_motion.y, now_seconds) ||
                reach_motion_active_at(&item.destination_motion.width, now_seconds) ||
                reach_motion_active_at(&item.destination_motion.height, now_seconds);
        }
        if ((item.placement.visible || item.placement.background_visible) &&
            (destination_active || reach_host_native_presentation_active(geometry, now_seconds)))
            desc->native_overlay_motion_pending = 1;
        item.placement = reach_host_native_overlay_placement(item.placement, geometry, ctx,
                                                             shadow_pad, now_seconds);
        reach_result result = host->window_thumbnails.ops.set_placement(
            host->window_thumbnails.thumbnails, id, &item.placement);
        if (result != REACH_OK)
        {
            placement_result = result;
        }
    }
    reach_host_apply_native_overlay_front(host, desc);
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

static void reach_host_sync_surface_pointer_observation(reach_host *host,
                                                        const reach_feature_runtime *desc)
{
    const reach_feature_surface_ops *ops = desc->definition->surface_ops;
    if (ops->pointer_observation != nullptr)
    {
        reach_rect_union shape = {};
        if (!ops->pointer_observation(desc->capsule, &shape))
        {
            shape.count = 0;
        }
        reach_host_set_pointer_observation(host, desc->definition->id, &shape);
    }
}

static int32_t reach_host_surface_layout_context_equal(const reach_feature_surface_context *a,
                                                       const reach_feature_surface_context *b)
{
    return a->theme == b->theme && reach_rect_equal(a->monitor_bounds, b->monitor_bounds) &&
           reach_rect_equal(a->available_bounds, b->available_bounds) &&
           reach_rect_equal(a->anchor_bounds, b->anchor_bounds) &&
           a->text_measure.context == b->text_measure.context &&
           a->text_measure.measure == b->text_measure.measure && a->dpi_scale == b->dpi_scale &&
           a->icon_size_px == b->icon_size_px && a->transition_visible == b->transition_visible &&
           a->bounds_valid == b->bounds_valid &&
           reach_rect_equal(a->anchor_button, b->anchor_button) &&
           a->anchor_bar_edge_y == b->anchor_bar_edge_y &&
           a->anchor_bar_height == b->anchor_bar_height &&
           a->anchor_direction == b->anchor_direction && a->anchor_valid == b->anchor_valid &&
           a->preferred_front_source == b->preferred_front_source;
}

static int32_t reach_host_surface_geometry_equal(const reach_feature_surface_geometry *a,
                                                 const reach_feature_surface_geometry *b)
{
    return reach_rect_equal(a->visible_bounds, b->visible_bounds) &&
           reach_rect_equal(a->envelope_bounds, b->envelope_bounds) &&
           a->has_input_bounds == b->has_input_bounds &&
           reach_motion_equal(&a->input_bounds.x, &b->input_bounds.x) &&
           reach_motion_equal(&a->input_bounds.y, &b->input_bounds.y) &&
           reach_motion_equal(&a->input_bounds.width, &b->input_bounds.width) &&
           reach_motion_equal(&a->input_bounds.height, &b->input_bounds.height) &&
           a->synchronize_presentation == b->synchronize_presentation &&
           a->manage_monitor_work_area == b->manage_monitor_work_area &&
           a->reserve_monitor_work_area == b->reserve_monitor_work_area &&
           a->work_area_clearance == b->work_area_clearance &&
           a->bar_reveal_policy == b->bar_reveal_policy && a->force_topmost == b->force_topmost &&
           a->notch_anchor_x == b->notch_anchor_x && a->notch_side == b->notch_side &&
           a->deadline_seconds == b->deadline_seconds &&
           a->wait_for_presentation == b->wait_for_presentation &&
           a->presentation.max_scale == b->presentation.max_scale &&
           reach_rect_equal(a->presentation.translation_bounds,
                            b->presentation.translation_bounds) &&
           a->presentation.retained == b->presentation.retained &&
           reach_motion_equal(&a->presentation.center_x, &b->presentation.center_x) &&
           reach_motion_equal(&a->presentation.center_y, &b->presentation.center_y) &&
           reach_render_group_equal(&a->presentation.group, &b->presentation.group);
}

static void reach_host_cache_surface_frame(reach_feature_runtime *desc,
                                           const reach_feature_surface_context *layout,
                                           const reach_feature_surface_context *native,
                                           const reach_feature_surface_geometry *geometry,
                                           reach_shadow_pad shadow_pad)
{
    desc->layout_context = *layout;
    desc->layout_context.last_bounds = desc->surface->last_bounds;
    desc->layout_context.bounds_valid = desc->surface->bounds_valid;
    desc->native_overlay_context = *native;
    desc->frame_geometry = *geometry;
    desc->native_overlay_shadow_pad = shadow_pad;
    desc->frame_retained_supported =
        desc->surface->renderer.ops.retained_supported != nullptr &&
        desc->surface->renderer.ops.retained_supported(desc->surface->renderer.backend);
    desc->frame_window = desc->surface->window.window;
    desc->frame_native_target =
        desc->surface->window.ops.native_id != nullptr
            ? desc->surface->window.ops.native_id(desc->surface->window.window)
            : 0;
    desc->frame_context_valid = 1;
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

    int32_t capsule_needs_frame = reach_host_surface_needs_frame(desc);
    int32_t needs_frame = capsule_needs_frame || desc->surface->fallback_animation_active;
    int32_t active = reach_host_surface_presented(desc);
    int32_t visible = active;
    int32_t frame_active = active;
    if (!visible)
    {
        desc->surface->fallback_animation_active = 0;
        desc->surface->commands_require_content_sampling = 0;
        desc->frame_context_valid = 0;
        desc->resolved_anchor_valid = 0;
        if (desc->surface->window.ops.set_input_presentation != nullptr)
            (void)desc->surface->window.ops.set_input_presentation(desc->surface->window.window,
                                                                   nullptr);
        reach_host_set_surface_visible(host, desc->definition->id, 0);
        reach_feature_surface_geometry hidden_geometry = {};
        desc->definition->capsule_ops->surface_geometry(desc->capsule, &hidden_geometry);
        hidden_geometry.reserve_monitor_work_area = 0;
        reach_host_sync_work_area_reservation(host, desc, &hidden_geometry, ctx->monitor_bounds);
        reach_host_release_native_overlay(host, desc);
        reach_host_sync_surface_pointer_observation(host, desc);
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
    if (!reach_host_layout_anchor_equal(&layout_anchor, &desc->resolved_anchor_owner))
        desc->resolved_anchor_valid = 0;
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
                                                   layout_anchor.index, layout_anchor.identity,
                                                   &resolved))
            {
                desc->resolved_anchor_owner = layout_anchor;
                desc->resolved_anchor = resolved;
                desc->resolved_anchor_bounds = surface_ctx.anchor_bounds;
                desc->resolved_anchor_valid = 1;
            }
            else if (layout_anchor.identity == 0)
                desc->resolved_anchor_valid = 0;
            if (desc->resolved_anchor_valid)
            {
                surface_ctx.anchor_bounds = desc->resolved_anchor_bounds;
                surface_ctx.anchor_button = desc->resolved_anchor.button;
                surface_ctx.anchor_bar_edge_y = desc->resolved_anchor.bar_edge_y;
                surface_ctx.anchor_bar_height = desc->resolved_anchor.bar_height;
                surface_ctx.anchor_direction = desc->resolved_anchor.direction;
                surface_ctx.anchor_valid = 1;
            }
        }
    }

    reach_feature_surface_geometry current_geometry = {};
    desc->definition->capsule_ops->surface_geometry(desc->capsule, &current_geometry);
    int32_t retained_supported =
        desc->surface->renderer.ops.retained_supported != nullptr &&
        desc->surface->renderer.ops.retained_supported(desc->surface->renderer.backend);
    reach_window_id native_target =
        desc->surface->window.ops.native_id != nullptr
            ? desc->surface->window.ops.native_id(desc->surface->window.window)
            : 0;
    int32_t render_context_changed =
        !desc->frame_context_valid || desc->frame_window != desc->surface->window.window ||
        desc->frame_native_target != native_target ||
        desc->frame_retained_supported != retained_supported ||
        !reach_host_surface_layout_context_equal(&surface_ctx, &desc->layout_context) ||
        !reach_host_surface_geometry_equal(&current_geometry, &desc->frame_geometry);
    int32_t frame_context_changed =
        render_context_changed ||
        !reach_rect_equal(surface_ctx.last_bounds, desc->layout_context.last_bounds);
    const auto &renderer = desc->surface->renderer;
    int32_t renderer_sampling = !capsule_needs_frame &&
                                !desc->surface->commands_require_content_sampling &&
                                renderer.ops.sample_content != nullptr &&
                                renderer.ops.content_sampling_required != nullptr &&
                                renderer.ops.content_sampling_required(renderer.backend);
    int32_t bar_visibility_pending =
        host->dirty.bar_visibility && desc->definition->surface.bar_reveal.ops != nullptr;
    if ((!needs_frame || renderer_sampling) && !host->dirty.layout && !host->dirty.render &&
        !desc->surface->dirty_flags && !current_geometry.synchronize_presentation &&
        !frame_context_changed && !bar_visibility_pending)
    {
        reach_result sampling_result = REACH_OK;
        if (renderer_sampling)
        {
            sampling_result = renderer.ops.sample_content(renderer.backend);
            desc->surface->fallback_animation_active =
                renderer.ops.content_sampling_required(renderer.backend);
        }
        const reach_feature_native_overlay_ops *native_ops =
            desc->definition->surface_ops->native_overlay;
        int32_t native_changed =
            native_ops != nullptr && native_ops->generation != nullptr &&
            (!desc->native_overlay_registered ||
             desc->native_overlay_generation != native_ops->generation(desc->capsule));
        if (desc->native_overlay_motion_pending || native_changed)
            (void)reach_host_sync_native_overlay(host, desc, &current_geometry,
                                                 &desc->native_overlay_context,
                                                 desc->native_overlay_shadow_pad);
        return sampling_result;
    }
    reach_feature_surface_context layout_context = surface_ctx;
    desc->frame_context_valid = 0;
    reach_workload_add(REACH_WORKLOAD_LAYOUTS, 1);
    int32_t layout_changed = desc->definition->surface_ops->arrange(desc->capsule, &surface_ctx);
    reach_host_sync_surface_pointer_observation(host, desc);
    if (needs_frame)
    {
        desc->surface->dirty_flags = 1;
        reach_host_request_update(host);
    }
    reach_feature_surface_geometry geometry = {};
    desc->definition->capsule_ops->surface_geometry(desc->capsule, &geometry);
    desc->bar_auto_hide_active =
        reach_host_bar_auto_hide_active(host, desc, &geometry, ctx->monitor_bounds);
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
    if (desc->surface->window.ops.set_input_passthrough != nullptr)
        (void)desc->surface->window.ops.set_input_passthrough(
            desc->surface->window.window, (desc->definition->surface.behavior_flags &
                                           REACH_SURFACE_BEHAVIOR_INPUT_PASSTHROUGH) != 0 ||
                                              geometry.wait_for_presentation);
    reach_shadow_pad shadow_pad = reach_host_surface_shadow_pad(host, desc->definition->id);
    if (geometry.presentation.retained)
    {
        reach_rect_f32 translation = geometry.presentation.translation_bounds;
        float maximum_scale = geometry.presentation.max_scale;
        reach_host_surface_presentation_extent(&geometry.presentation.group, &translation,
                                               &maximum_scale);
        shadow_pad.left *= maximum_scale;
        shadow_pad.top *= maximum_scale;
        shadow_pad.right *= maximum_scale;
        shadow_pad.bottom *= maximum_scale;
        reach_host_surface_presentation_frame frame = reach_host_surface_presentation_frame_compute(
            geometry.visible_bounds, geometry.envelope_bounds, shadow_pad, translation,
            maximum_scale);
        bounds = frame.window_bounds;
        surface_ctx.content_rect = frame.content_rect;
        surface_ctx.render_transform = frame.render_transform;
        surface_ctx.content_transform_active = 1;
        desc->surface->render_content_rect = surface_ctx.content_rect;
        desc->surface->render_transform = surface_ctx.render_transform;
        desc->surface->retained_presentation = 1;
        if (desc->surface->window.ops.set_input_regions != nullptr)
            (void)desc->surface->window.ops.set_input_regions(desc->surface->window.window,
                                                              &surface_ctx.content_rect, 1);
        if (desc->surface->window.ops.set_input_presentation != nullptr)
        {
            reach_platform_window_input_presentation input = {};
            input.bounds = surface_ctx.content_rect;
            input.x = geometry.presentation.group.x;
            input.y = geometry.presentation.group.y;
            input.scale = geometry.presentation.group.scale;
            input.scale_y = geometry.presentation.group.scale_y;
            if (geometry.presentation.center_x.generation)
                input.center_x = reach_motion_affine(&geometry.presentation.center_x,
                                                     surface_ctx.render_transform.scale_x,
                                                     surface_ctx.render_transform.offset_x);
            if (geometry.presentation.center_y.generation)
                input.center_y = reach_motion_affine(&geometry.presentation.center_y,
                                                     surface_ctx.render_transform.scale_y,
                                                     surface_ctx.render_transform.offset_y);
            input.has_motion_bounds = geometry.has_input_bounds;
            input.input_regions_at = desc->definition->capsule_ops->input_regions_at;
            input.input_regions_user = desc->capsule;
            input.input_regions_transform =
                surface_ctx.content_transform_active
                    ? surface_ctx.render_transform
                    : reach_transform_f32{1, 1, shadow_pad.left, shadow_pad.top};
            if (geometry.has_input_bounds)
            {
                reach_transform_f32 transform =
                    surface_ctx.content_transform_active
                        ? surface_ctx.render_transform
                        : reach_transform_f32{1, 1, shadow_pad.left, shadow_pad.top};
                input.motion_bounds =
                    reach_rect_motion_transform(&geometry.input_bounds, transform);
            }
            (void)desc->surface->window.ops.set_input_presentation(desc->surface->window.window,
                                                                   &input);
        }
    }
    if (!geometry.presentation.retained &&
        desc->surface->window.ops.set_input_presentation != nullptr)
    {
        reach_platform_window_input_presentation input = {};
        input.has_motion_bounds = geometry.has_input_bounds;
        input.input_regions_at = desc->definition->capsule_ops->input_regions_at;
        input.input_regions_user = desc->capsule;
        input.input_regions_transform =
            surface_ctx.content_transform_active
                ? surface_ctx.render_transform
                : reach_transform_f32{1, 1, shadow_pad.left, shadow_pad.top};
        if (geometry.has_input_bounds)
        {
            reach_transform_f32 transform =
                surface_ctx.content_transform_active
                    ? surface_ctx.render_transform
                    : reach_transform_f32{1, 1, shadow_pad.left, shadow_pad.top};
            input.motion_bounds = reach_rect_motion_transform(&geometry.input_bounds, transform);
            reach_rect_f32 region = {shadow_pad.left, shadow_pad.top, geometry.visible_bounds.width,
                                     geometry.visible_bounds.height};
            if (desc->surface->window.ops.set_input_regions != nullptr)
                (void)desc->surface->window.ops.set_input_regions(desc->surface->window.window,
                                                                  &region, 1);
        }
        (void)desc->surface->window.ops.set_input_presentation(
            desc->surface->window.window,
            geometry.has_input_bounds || input.input_regions_at != nullptr ? &input : nullptr);
    }
    surface_ctx.render_bounds = bounds;

    int32_t window_changed = 0;
    reach_result result = reach_host_apply_window_state(
        &desc->surface->window, bounds, shadow_pad, &desc->surface->last_bounds,
        &desc->surface->bounds_valid, &window_changed);
    int32_t position_only = reach_host_bar_position_only(desc);
    int32_t render_needed = host->dirty.render || desc->surface->dirty_flags || layout_changed ||
                            geometry_changed || render_context_changed ||
                            (window_changed && !position_only);
    reach_result native_overlay_result = REACH_OK;
    if (result == REACH_OK && active && desc->definition->surface_ops->native_overlay != nullptr)
    {
        native_overlay_result =
            reach_host_sync_native_overlay(host, desc, &geometry, &surface_ctx, shadow_pad);
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
        reach_host_cache_surface_frame(desc, &layout_context, &surface_ctx, &geometry, shadow_pad);
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
    if (result == REACH_OK)
        reach_host_cache_surface_frame(desc, &layout_context, &surface_ctx, &geometry, shadow_pad);
    return result;
}
