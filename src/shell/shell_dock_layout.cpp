#include "shell_internal.h"

#include <math.h>

static int32_t reach_shell_float_animation_active(const reach_float_animation *animation)
{
    return animation != nullptr && animation->elapsed_seconds < animation->duration_seconds;
}

static int32_t reach_shell_dock_key_equal(int32_t a_pinned, uint32_t a_pin_id, uintptr_t a_window,
                                          int32_t b_pinned, uint32_t b_pin_id, uintptr_t b_window)
{
    reach_dock_order_key a = {a_pinned, a_pin_id, a_window};
    reach_dock_order_key b = {b_pinned, b_pin_id, b_window};
    return reach_dock_key_equal(&a, &b);
}

static int32_t reach_shell_popup_blocks_dock_autohide(const reach_shell *shell)
{
    return shell != nullptr && (shell->tray_state.popup_open || shell->quick_settings_open ||
                                shell->context_menu_state.open);
}

int32_t reach_shell_should_auto_hide_dock(const reach_shell *shell)
{
    REACH_ASSERT(shell != nullptr);
    if (shell == nullptr)
    {
        return 0;
    }
    if (shell->window_manager.ops.foreground_is_maximized != nullptr)
    {
        return shell->window_manager.ops.foreground_is_maximized(shell->window_manager.manager);
    }
    return 0;
}

static int32_t reach_shell_get_cursor_position(reach_shell *shell, reach_point_i32 *out_cursor)
{
    if (shell == nullptr || shell->input_source.ops.get_pointer_position == nullptr ||
        out_cursor == nullptr)
    {
        return 0;
    }
    return shell->input_source.ops.get_pointer_position(shell->input_source.source, out_cursor) ==
           REACH_OK;
}

static int32_t reach_shell_point_in_rect(reach_point_i32 point, reach_rect_f32 rect)
{
    return (float)point.x >= rect.x && (float)point.x < rect.x + rect.width &&
           (float)point.y >= rect.y && (float)point.y < rect.y + rect.height;
}

static reach_rect_f32 reach_shell_dock_reveal_bounds(reach_rect_f32 shown_bounds,
                                                     reach_rect_f32 monitor_bounds)
{
    float monitor_bottom = monitor_bounds.y + monitor_bounds.height;
    reach_rect_f32 bounds = shown_bounds;
    bounds.height = monitor_bottom - shown_bounds.y;
    if (bounds.height < shown_bounds.height)
    {
        bounds.height = shown_bounds.height;
    }
    return bounds;
}

static int32_t reach_shell_cursor_in_dock_reveal_bounds(reach_shell *shell,
                                                        reach_rect_f32 shown_bounds,
                                                        reach_rect_f32 monitor_bounds)
{
    reach_point_i32 cursor = {};
    if (!reach_shell_get_cursor_position(shell, &cursor))
    {
        return 0;
    }
    return reach_shell_point_in_rect(cursor,
                                     reach_shell_dock_reveal_bounds(shown_bounds, monitor_bounds));
}

static int32_t reach_shell_popup_blocks_dock_reveal_edge(const reach_shell *shell)
{
    if (shell == nullptr)
    {
        return 1;
    }

    return shell->ui.launcher.open || shell->switcher_state.open || shell->tray_state.popup_open ||
           shell->quick_settings_open || shell->context_menu_state.open;
}

static int32_t reach_shell_reveal_edge_rect_equal(reach_rect_f32 a, reach_rect_f32 b)
{
    return fabsf(a.x - b.x) < 0.5f && fabsf(a.y - b.y) < 0.5f && fabsf(a.width - b.width) < 0.5f &&
           fabsf(a.height - b.height) < 0.5f;
}

void reach_shell_sync_dock_reveal_edge(reach_shell *shell, reach_rect_f32 shown_dock_bounds,
                                       reach_rect_f32 monitor_bounds)
{
    if (shell == nullptr || shell->dock_reveal_edge.edge == nullptr)
    {
        return;
    }

    int32_t should_auto_hide = shell->ui.dock.auto_hide && reach_shell_should_auto_hide_dock(shell);
    int32_t popup_blocks = reach_shell_popup_blocks_dock_reveal_edge(shell);
    int32_t should_show = should_auto_hide && !popup_blocks &&
                          (shell->dock_reveal.target_hidden || shell->dock_reveal.active) &&
                          !shell->dock_animation.animating;

    float monitor_bottom = monitor_bounds.y + monitor_bounds.height;

    reach_rect_f32 edge_bounds = {};
    edge_bounds.x = shown_dock_bounds.x;
    edge_bounds.y = monitor_bottom - 2.0f;
    edge_bounds.width = shown_dock_bounds.width;
    edge_bounds.height = 3.0f;

    if (should_show)
    {
        if (!shell->dock_reveal.edge_bounds_valid ||
            !reach_shell_reveal_edge_rect_equal(shell->dock_reveal.edge_bounds, edge_bounds))
        {
            if (shell->dock_reveal_edge.ops.set_bounds != nullptr &&
                shell->dock_reveal_edge.ops.set_bounds(shell->dock_reveal_edge.edge, edge_bounds) ==
                    REACH_OK)
            {
                shell->dock_reveal.edge_bounds = edge_bounds;
                shell->dock_reveal.edge_bounds_valid = 1;
            }
        }

        if (!shell->dock_reveal.edge_visible && shell->dock_reveal_edge.ops.show != nullptr &&
            shell->dock_reveal_edge.ops.show(shell->dock_reveal_edge.edge) == REACH_OK)
        {
            shell->dock_reveal.edge_visible = 1;
        }
    }
    else if (shell->dock_reveal.edge_visible)
    {
        if (shell->dock_reveal_edge.ops.hide != nullptr &&
            shell->dock_reveal_edge.ops.hide(shell->dock_reveal_edge.edge) == REACH_OK)
        {
            shell->dock_reveal.edge_visible = 0;
        }
    }
}

reach_rect_f32 reach_shell_apply_dock_animation(reach_shell *shell, reach_rect_f32 shown_bounds,
                                                reach_rect_f32 monitor_bounds, double delta_seconds)
{
    REACH_ASSERT(shell != nullptr);
    float hidden_y = monitor_bounds.y + monitor_bounds.height + 4.0f;
    int32_t popup_blocks_autohide = reach_shell_popup_blocks_dock_autohide(shell);
    int32_t should_auto_hide = shell->ui.dock.auto_hide && reach_shell_should_auto_hide_dock(shell);

    int32_t base_hidden = should_auto_hide && !popup_blocks_autohide;
    if (!should_auto_hide)
    {
        shell->dock_reveal.active = 0;
        shell->dock_reveal.requested = 0;
    }
    else if (popup_blocks_autohide)
    {
        if (reach_shell_cursor_in_dock_reveal_bounds(shell, shown_bounds, monitor_bounds))
        {
            shell->dock_reveal.active = 1;
        }
    }
    else if (!popup_blocks_autohide && shell->dock_reveal.requested)
    {
        shell->dock_reveal.active =
            reach_shell_cursor_in_dock_reveal_bounds(shell, shown_bounds, monitor_bounds);
        shell->dock_reveal.requested = 0;
    }
    else if (shell->dock_reveal.active && shell->dock_reveal.check_dirty &&
             !reach_shell_cursor_in_dock_reveal_bounds(shell, shown_bounds, monitor_bounds))
    {
        shell->dock_reveal.active = 0;
    }

    int32_t target_hidden = base_hidden && !shell->dock_reveal.active;
    float target_y = target_hidden ? hidden_y : shown_bounds.y;
    if (target_hidden && shell->tray_state.popup_open)
    {
        reach_shell_set_tray_popup_open(shell, 0);
    }
    if (target_hidden && shell->quick_settings_open)
    {
        reach_shell_set_quick_settings_open(shell, 0);
    }
    if (target_hidden && shell->context_menu_state.open)
    {
        reach_shell_close_context_menu(shell);
        reach_shell_clear_sticky_dock_feedback(shell);
    }
    else if (target_hidden && shell->feedback.dock_sticky)
    {
        reach_shell_clear_sticky_dock_feedback(shell);
    }

    if (!shell->dock_animation.initialized)
    {
        shell->dock_animation.initialized = 1;
        shell->dock_reveal.target_hidden = target_hidden;
        shell->dock_animation.y = {};
        shell->dock_animation.y.from = target_y;
        shell->dock_animation.y.to = target_y;
        shell->dock_animation.y.value = target_y;
        shell->ui.dock.visible = target_hidden ? 0 : 1;
    }

    if (shell->dock_reveal.target_hidden != target_hidden)
    {
        shell->dock_reveal.target_hidden = target_hidden;
        shell->ui.dock.visible = target_hidden ? 0 : 1;
        reach_float_animation_start(&shell->dock_animation.y, shell->dock_animation.y.value,
                                    target_y, 0.18);
        shell->dock_animation.animating = 1;
    }

    int32_t was_animating = shell->dock_animation.animating;
    if (shell->dock_animation.animating)
    {
        reach_float_animation_update(&shell->dock_animation.y, delta_seconds);
        shell->dock_animation.animating =
            reach_shell_float_animation_active(&shell->dock_animation.y);
    }
    if (was_animating && !shell->dock_animation.animating && !target_hidden && should_auto_hide &&
        !popup_blocks_autohide && shell->dock_reveal.active &&
        !reach_shell_cursor_in_dock_reveal_bounds(shell, shown_bounds, monitor_bounds))
    {
        shell->dock_reveal.active = 0;
        shell->dock_reveal.target_hidden = 1;
        shell->ui.dock.visible = 0;
        reach_float_animation_start(&shell->dock_animation.y, shell->dock_animation.y.value,
                                    hidden_y, 0.18);
        shell->dock_animation.animating = 1;
    }
    reach_rect_f32 animated = shown_bounds;
    animated.y = shell->dock_animation.y.value;
    return animated;
}

void reach_shell_apply_dock_width_animation(reach_shell *shell, reach_dock_layout *layout,
                                            double delta_seconds)
{
    if (shell == nullptr || layout == nullptr)
    {
        return;
    }

    float target_width = layout->bounds.width;
    size_t target_count = layout->app_slot_count;
    if (!shell->dock_width.initialized)
    {
        shell->dock_width.initialized = 1;
        shell->dock_width.item_count = target_count;
        shell->dock_width.animation = {};
        shell->dock_width.animation.from = target_width;
        shell->dock_width.animation.to = target_width;
        shell->dock_width.animation.value = target_width;
    }

    if (shell->dock_width.item_count != target_count &&
        fabsf(shell->dock_width.animation.to - target_width) >= 0.5f)
    {
        float from = shell->dock_width.animation.value > 0.0f ? shell->dock_width.animation.value
                                                              : target_width;
        reach_float_animation_start(&shell->dock_width.animation, from, target_width, 0.18);
        shell->dock_width.animating = 1;
        shell->dock_width.item_count = target_count;
        shell->dock.dirty_flags = 1;
    }
    else if (!shell->dock_width.animating &&
             fabsf(shell->dock_width.animation.value - target_width) >= 0.5f)
    {
        shell->dock_width.animation.from = target_width;
        shell->dock_width.animation.to = target_width;
        shell->dock_width.animation.value = target_width;
        shell->dock_width.item_count = target_count;
    }

    if (shell->dock_width.animating)
    {
        reach_float_animation_update(&shell->dock_width.animation, delta_seconds);
        shell->dock_width.animating =
            reach_shell_float_animation_active(&shell->dock_width.animation);
        shell->dock.dirty_flags = 1;
    }

    float animated_width = shell->dock_width.animation.value;
    if (animated_width <= 0.0f)
    {
        animated_width = target_width;
    }
    if (fabsf(animated_width - target_width) < 0.5f)
    {
        animated_width = target_width;
    }

    float target_x = layout->bounds.x;
    float center = layout->bounds.x + layout->bounds.width * 0.5f;
    layout->bounds.x = center - animated_width * 0.5f;
    layout->bounds.width = animated_width;
    float x_delta = layout->bounds.x - target_x;
    for (size_t index = 0; index < layout->app_slot_count; ++index)
    {
        layout->app_slots[index].x += x_delta;
    }
    const reach_theme *theme = shell->theme != nullptr ? shell->theme : reach_theme_default();
    float gap = shell->ui.dock.gap;
    layout->power_button.x =
        layout->bounds.x + layout->bounds.width - layout->power_button.width - gap;
    layout->clock.x = layout->power_button.x - gap - theme->dock_clock_width;
    layout->system_separator.x = layout->clock.x - gap - theme->dock_system_separator_width;
    layout->quick_settings_button.x =
        layout->system_separator.x - gap - layout->quick_settings_button.width;
    layout->tray_button.x = layout->quick_settings_button.x - layout->tray_button.width;
}

reach_result reach_shell_refresh_monitor_layout(reach_shell *shell)
{
    if (shell == nullptr || !shell->dirty.monitors || shell->monitors.list == nullptr ||
        shell->wallpaper_surface.ops.set_bounds == nullptr)
    {
        return REACH_OK;
    }

    if (shell->monitors.list == nullptr || shell->monitors.ops.refresh == nullptr ||
        shell->monitors.ops.count == nullptr || shell->monitors.ops.get == nullptr)
    {
        return REACH_OK;
    }

    (void)shell->monitors.ops.refresh(shell->monitors.list);
    size_t monitor_count = shell->monitors.ops.count(shell->monitors.list);
    if (monitor_count == 0)
    {
        shell->dirty.monitors = 0;
        return REACH_OK;
    }

    const reach_monitor_info *monitor = shell->monitors.ops.get(shell->monitors.list, 0);
    if (monitor == nullptr)
    {
        shell->dirty.monitors = 0;
        return REACH_OK;
    }

    int32_t left = monitor->bounds.left;
    int32_t top = monitor->bounds.top;
    int32_t right = monitor->bounds.right;
    int32_t bottom = monitor->bounds.bottom;
    for (size_t index = 1; index < monitor_count; ++index)
    {
        monitor = shell->monitors.ops.get(shell->monitors.list, index);
        if (monitor == nullptr)
        {
            continue;
        }
        if (monitor->bounds.left < left)
            left = monitor->bounds.left;
        if (monitor->bounds.top < top)
            top = monitor->bounds.top;
        if (monitor->bounds.right > right)
            right = monitor->bounds.right;
        if (monitor->bounds.bottom > bottom)
            bottom = monitor->bounds.bottom;
    }

    reach_rect_f32 wallpaper_bounds = {};
    wallpaper_bounds.x = (float)left;
    wallpaper_bounds.y = (float)top;
    wallpaper_bounds.width = (float)(right - left);
    wallpaper_bounds.height = (float)(bottom - top);
    if (!shell->wallpaper_state.bounds_valid ||
        !reach_shell_rect_equal(shell->wallpaper_state.bounds, wallpaper_bounds))
    {
        reach_result result = shell->wallpaper_surface.ops.set_bounds(
            shell->wallpaper_surface.surface, wallpaper_bounds);
        if (result != REACH_OK)
        {
            return result;
        }
        shell->wallpaper_state.bounds = wallpaper_bounds;
        shell->wallpaper_state.bounds_valid = 1;
    }

    shell->dirty.monitors = 0;
    return REACH_OK;
}

float reach_shell_dock_slot_box_x(const reach_shell *shell, const reach_dock_layout *layout,
                                  size_t index)
{
    const reach_theme *theme =
        shell != nullptr && shell->theme != nullptr ? shell->theme : reach_theme_default();
    return reach_dock_slot_box_x(theme, layout, index);
}

float reach_shell_dock_drag_clamped_x(const reach_shell *shell, const reach_dock_layout *layout,
                                      int32_t cursor_x)
{
    const reach_theme *theme =
        shell != nullptr && shell->theme != nullptr ? shell->theme : reach_theme_default();
    return reach_dock_drag_clamped_x(theme, layout, cursor_x,
                                     shell != nullptr ? shell->dock_drag.grab_offset_x : 0.0f);
}

size_t reach_shell_find_dock_item_key(const reach_shell *shell, int32_t pinned, uint32_t pin_id,
                                      uintptr_t window)
{
    if (shell == nullptr)
    {
        return REACH_MAX_PINNED_APPS;
    }
    for (size_t index = 0; index < shell->dock_model.item_count; ++index)
    {
        if (reach_shell_dock_item_matches_key(shell, index, pinned, pin_id, window))
        {
            return index;
        }
    }
    return REACH_MAX_PINNED_APPS;
}

size_t reach_shell_dock_reorder_target(const reach_shell *shell, size_t current_index,
                                       float dragged_box_x)
{
    return shell != nullptr && shell->has_layout
               ? reach_dock_reorder_target(&shell->dock_model, &shell->layout.dock, current_index,
                                           dragged_box_x)
               : REACH_MAX_PINNED_APPS;
}

size_t reach_shell_pinned_order_index(const reach_shell *shell, uint32_t pin_id)
{
    return shell != nullptr
               ? reach_dock_feature_model_pinned_order_index(&shell->dock_model, pin_id)
               : REACH_MAX_PINNED_APPS;
}

float reach_shell_dock_item_current_x(const reach_shell *shell, const reach_dock_layout *layout,
                                      size_t index)
{
    if (shell == nullptr || layout == nullptr || index >= shell->dock_model.item_count ||
        index >= layout->app_slot_count)
    {
        return 0.0f;
    }
    if ((shell->dock_drag.active || shell->dock_drag.snapping) &&
        reach_shell_dock_item_matches_key(shell, index, shell->dock_drag.pinned,
                                          shell->dock_drag.pin_id, shell->dock_drag.window))
    {
        return shell->dock_drag.snapping ? shell->dock_drag.snap_animation.value
                                         : shell->dock_drag.x;
    }
    if ((shell->dock_item_x_animating[index] || shell->dock_item_x_valid[index]) &&
        reach_shell_dock_key_equal(
            shell->dock_item_x_pinned[index], shell->dock_item_x_pin_ids[index],
            shell->dock_item_x_windows[index], shell->dock_model.items[index].pinned,
            reach_shell_dock_item_pin_id(shell, index), shell->dock_model.items[index].window))
    {
        return shell->dock_item_x_animations[index].value;
    }
    return reach_shell_dock_slot_box_x(shell, layout, index);
}

static void reach_shell_start_dock_item_x_animation(reach_shell *shell, size_t index, float from,
                                                    float to)
{
    if (shell == nullptr || index >= REACH_MAX_PINNED_APPS)
    {
        return;
    }
    if (fabsf(from - to) < 0.5f)
    {
        shell->dock_item_x_animations[index] = {};
        shell->dock_item_x_animations[index].from = to;
        shell->dock_item_x_animations[index].to = to;
        shell->dock_item_x_animations[index].value = to;
        shell->dock_item_x_valid[index] = 1;
        shell->dock_item_x_animating[index] = 0;
        return;
    }
    reach_float_animation_start(&shell->dock_item_x_animations[index], from, to, 0.12);
    shell->dock_item_x_valid[index] = 1;
    shell->dock_item_x_animating[index] = 1;
}

void reach_shell_rebuild_dock_items_with_animations(reach_shell *shell, reach_dock_layout *layout)
{
    if (shell == nullptr || layout == nullptr)
    {
        return;
    }

    int32_t old_pinned[REACH_MAX_PINNED_APPS] = {};
    uint32_t old_pin_ids[REACH_MAX_PINNED_APPS] = {};
    uintptr_t old_windows[REACH_MAX_PINNED_APPS] = {};
    float old_x[REACH_MAX_PINNED_APPS] = {};
    size_t old_count = shell->dock_model.item_count;
    const reach_dock_layout *old_layout = shell->has_layout ? &shell->layout.dock : layout;
    for (size_t index = 0; index < old_count; ++index)
    {
        old_pinned[index] = shell->dock_model.items[index].pinned;
        old_pin_ids[index] = reach_shell_dock_item_pin_id(shell, index);
        old_windows[index] = shell->dock_model.items[index].window;
        old_x[index] = reach_shell_dock_item_current_x(shell, old_layout, index);
    }

    reach_shell_build_dock_items(shell, layout);

    for (size_t index = 0; index < shell->dock_model.item_count; ++index)
    {
        uint32_t pin_id = reach_shell_dock_item_pin_id(shell, index);
        float target_x = reach_shell_dock_slot_box_x(shell, layout, index);
        float from_x = target_x;
        for (size_t old_index = 0; old_index < old_count; ++old_index)
        {
            if (reach_shell_dock_key_equal(old_pinned[old_index], old_pin_ids[old_index],
                                           old_windows[old_index],
                                           shell->dock_model.items[index].pinned, pin_id,
                                           shell->dock_model.items[index].window))
            {
                from_x = old_x[old_index];
                break;
            }
        }
        shell->dock_item_x_pinned[index] = shell->dock_model.items[index].pinned;
        shell->dock_item_x_pin_ids[index] = pin_id;
        shell->dock_item_x_windows[index] = shell->dock_model.items[index].window;
        if (reach_shell_dock_item_matches_key(shell, index, shell->dock_drag.pinned,
                                              shell->dock_drag.pin_id, shell->dock_drag.window) &&
            (shell->dock_drag.active || shell->dock_drag.snapping))
        {
            reach_shell_start_dock_item_x_animation(shell, index, target_x, target_x);
        }
        else
        {
            reach_shell_start_dock_item_x_animation(shell, index, from_x, target_x);
        }
    }
    for (size_t index = shell->dock_model.item_count; index < REACH_MAX_PINNED_APPS; ++index)
    {
        shell->dock_item_x_valid[index] = 0;
        shell->dock_item_x_animating[index] = 0;
        shell->dock_item_x_pinned[index] = 0;
        shell->dock_item_x_pin_ids[index] = 0;
        shell->dock_item_x_windows[index] = 0;
    }
}

int32_t reach_shell_can_move_dock_without_redraw(const reach_shell *shell)
{
    if (shell == nullptr)
    {
        return 0;
    }

    return shell->dock_animation.animating && shell->has_layout && !shell->dirty.layout &&
           !shell->dirty.render && !shell->dock.dirty_flags && !shell->launcher.dirty_flags &&
           !shell->tray.dirty_flags && !shell->switcher.dirty_flags &&
           !shell->context_menu.dirty_flags && !shell->quick_settings.dirty_flags &&
           !shell->dock_width.animating && !shell->dock_drag.active && !shell->dock_drag.snapping &&
           !shell->feedback.dock_animating && !shell->feedback.tray_animating &&
           !reach_shell_popup_bounds_animation_active(&shell->quick_settings_bounds_animation);
}

reach_result reach_shell_move_dock_animation_frame(reach_shell *shell, double delta_seconds)
{
    if (shell == nullptr || !shell->has_layout)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_float_animation_update(&shell->dock_animation.y, delta_seconds);
    shell->dock_animation.animating = reach_shell_float_animation_active(&shell->dock_animation.y);

    reach_rect_f32 dock_bounds = shell->layout.dock.bounds;
    dock_bounds.y = shell->dock_animation.y.value;

    int32_t dock_window_changed = 0;
    reach_result result = reach_shell_apply_window_state(
        &shell->dock.window, dock_bounds, 1.0f, &shell->dock.last_bounds, &shell->dock.last_opacity,
        &shell->dock.bounds_valid, &shell->dock.opacity_valid, &dock_window_changed);
    if (result != REACH_OK)
    {
        return result;
    }

    shell->layout.dock.bounds = dock_bounds;

    if (!shell->dock_animation.animating)
    {
        shell->dock_reveal.check_dirty = 1;
    }

    shell->dirty.update_requested = 0;
    return REACH_OK;
}
