#include "shell_internal.h"

#include <math.h>

void reach_shell_begin_dock_drag(
    reach_shell *shell,
    size_t index,
    const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->has_layout) {
        return;
    }

    if (index >= shell->dock_model.item_count) {
        return;
    }

    shell->dock_drag.active = 1;

    if (shell->dock.window.ops.set_pointer_move_enabled != nullptr) {
        (void)shell->dock.window.ops.set_pointer_move_enabled(
            shell->dock.window.window,
            1);
    }

    shell->dock_drag.moved = 0;
    shell->dock_drag.source_index = index;
    shell->dock_drag.target_index = index;
    shell->dock_drag.pinned = shell->dock_model.items[index].pinned;
    shell->dock_drag.pin_id = 0;

    if (shell->dock_model.items[index].pinned &&
        shell->dock_model.items[index].pinned_index < shell->ui.pinned_app_count) {
        shell->dock_drag.pin_id =
            shell->ui.pinned_apps[shell->dock_model.items[index].pinned_index].id;
    }

    shell->dock_drag.window = shell->dock_model.items[index].window;
    shell->dock_drag.start_x = event->x;
    shell->dock_drag.start_y = event->y;

    float box_x = reach_shell_dock_slot_box_x(shell, &shell->layout.dock, index);
    shell->dock_drag.grab_offset_x =
        (float)event->x - (shell->layout.dock.bounds.x + box_x);

    shell->dock_drag.x = box_x;
    shell->dock_drag.snapping = 0;
    shell->dock_drag.reload_pins_after_snap = 0;
}

reach_result reach_shell_update_dock_drag(
    reach_shell *shell,
    const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->has_layout) {
        return REACH_OK;
    }

    if (!shell->dock_drag.active) {
        return REACH_OK;
    }

    reach_shell_request_update(shell);

    int32_t dx = event->x - shell->dock_drag.start_x;
    int32_t dy = event->y - shell->dock_drag.start_y;

    if (!shell->dock_drag.moved && (dx * dx + dy * dy) >= 36) {
        shell->dock_drag.moved = 1;
    }

    if (!shell->dock_drag.moved) {
        return REACH_OK;
    }

    float next_drag_x = reach_shell_dock_drag_clamped_x(
        shell,
        &shell->layout.dock,
        event->x);

    if (fabsf(next_drag_x - shell->dock_drag.x) >= 0.5f) {
        shell->dock_drag.x = next_drag_x;
        shell->dock.dirty_flags = 1;
    }

    float dragged_box_x = shell->layout.dock.bounds.x + shell->dock_drag.x;

    size_t current = reach_shell_find_dock_order_key(
        shell,
        shell->dock_drag.pinned,
        shell->dock_drag.pin_id,
        shell->dock_drag.window);

    size_t target = reach_shell_dock_reorder_target(
        shell,
        current,
        dragged_box_x);

    if (target != REACH_MAX_PINNED_APPS &&
        target != shell->dock_drag.target_index) {
        if (current != REACH_MAX_PINNED_APPS) {
            reach_shell_move_dock_order(shell, current, target);
            reach_shell_rebuild_dock_items_with_animations(
                shell,
                &shell->layout.dock);
        }

        shell->dock_drag.target_index = target;
        shell->dock_click_feedback_index = target;
        shell->dock.dirty_flags = 1;
        return REACH_OK;
    }

    current = reach_shell_find_dock_order_key(
        shell,
        shell->dock_drag.pinned,
        shell->dock_drag.pin_id,
        shell->dock_drag.window);

    if (current != REACH_MAX_PINNED_APPS &&
        shell->dock_click_feedback_index != current) {
        shell->dock_click_feedback_index = current;
        shell->dock.dirty_flags = 1;
    }

    return REACH_OK;
}

reach_result reach_shell_end_dock_drag(reach_shell *shell)
{
    if (shell == nullptr || !shell->has_layout) {
        return REACH_OK;
    }

    if (!shell->dock_drag.active) {
        return REACH_OK;
    }

    uint32_t pin_id = shell->dock_drag.pin_id;
    int32_t dragged_pinned = shell->dock_drag.pinned;
    int32_t moved = shell->dock_drag.moved;
    size_t previous_pressed_dock_index = shell->pressed_dock_index;

    size_t target_pinned_index = dragged_pinned
        ? reach_shell_pinned_order_index(shell, pin_id)
        : REACH_MAX_PINNED_APPS;

    size_t target_index = reach_shell_find_dock_item_key(
        shell,
        shell->dock_drag.pinned,
        shell->dock_drag.pin_id,
        shell->dock_drag.window);

    shell->dock_drag.active = 0;
    shell->dock_drag.moved = 0;
    shell->pressed_dock_index = moved
        ? REACH_MAX_PINNED_APPS
        : previous_pressed_dock_index;
    shell->dock.dirty_flags = 1;

    reach_shell_release_dock_item(shell);

    if (moved && target_index < shell->layout.dock.app_slot_count) {
        float target_x = reach_shell_dock_slot_box_x(
            shell,
            &shell->layout.dock,
            target_index);

        reach_float_animation_start(
            &shell->dock_drag.snap_animation,
            shell->dock_drag.x,
            target_x,
            0.12);

        shell->dock_drag.snapping = 1;
    } else {
        shell->dock_drag.source_index = REACH_MAX_PINNED_APPS;
        shell->dock_drag.target_index = REACH_MAX_PINNED_APPS;
        shell->dock_drag.pinned = 0;
        shell->dock_drag.pin_id = 0;
        shell->dock_drag.window = 0;
        shell->dock_drag.snapping = 0;
    }

    if (moved && dragged_pinned && target_pinned_index != REACH_MAX_PINNED_APPS) {
        reach_result result = reach_pin_config_move_id(
            &shell->config_store,
            pin_id,
            target_pinned_index);

        if (result != REACH_OK) {
            return result;
        }

        shell->dock_drag.reload_pins_after_snap = shell->dock_drag.snapping;

        if (!shell->dock_drag.snapping) {
            return reach_shell_reload_pins(shell);
        }
    }

    return REACH_OK;
}
