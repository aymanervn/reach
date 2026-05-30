#include "shell_internal.h"

reach_result reach_shell_render_dock_surface(reach_shell *shell, const reach_dock_layout *layout)
{
    REACH_ASSERT(shell != nullptr);
    REACH_ASSERT(layout != nullptr);
    if (shell == nullptr || layout == nullptr || shell->dock.renderer.ops.begin_frame == nullptr) {
        return REACH_OK;
    }

    float item_box_x[REACH_MAX_PINNED_APPS] = {};
    for (size_t index = 0; index < layout->app_slot_count && index < REACH_MAX_PINNED_APPS; ++index) {
        item_box_x[index] = reach_shell_dock_item_current_x(shell, layout, index);
    }

    size_t dragged_render_index = (shell->dock_drag.active || shell->dock_drag.snapping)
        ? reach_shell_find_dock_item_key(shell, shell->dock_drag.pinned, shell->dock_drag.pin_id, shell->dock_drag.window)
        : REACH_MAX_PINNED_APPS;
    float dragged_x = shell->dock_drag.snapping
        ? shell->dock_drag.snap_animation.value
        : shell->dock_drag.x;
    uintptr_t focused_window = shell->window_manager.ops.foreground != nullptr
        ? shell->window_manager.ops.foreground(shell->window_manager.manager)
        : 0;

    reach_render_command_buffer commands = {};
    reach_dock_render_input input = {};
    input.theme = shell->theme != nullptr ? shell->theme : reach_theme_default();
    input.layout = layout;
    input.model = &shell->dock_model;
    input.icons = &shell->dock_icons;
    input.item_box_x = item_box_x;
    input.item_box_x_count = REACH_MAX_PINNED_APPS;
    input.focused_window = focused_window;
    input.dragged_render_index = dragged_render_index;
    input.dragged_box_x = dragged_x;
    input.click_feedback_index = shell->feedback.dock_index;
    input.click_feedback_opacity = shell->feedback.dock_opacity.value;
    input.tray_feedback_index = REACH_SHELL_DOCK_FEEDBACK_TRAY_BUTTON;
    input.quick_settings_feedback_index = REACH_SHELL_DOCK_FEEDBACK_QUICK_SETTINGS_BUTTON;
    input.power_feedback_index = REACH_SHELL_DOCK_FEEDBACK_POWER_BUTTON;
    input.time_text = shell->dock_time_text;
    input.date_text = shell->dock_date_text;
    input.text_alignment_center = REACH_TEXT_ALIGNMENT_CENTER;

    reach_result result = reach_dock_build_render_commands(&input, &commands);
    if (result != REACH_OK) {
        return result;
    }

    if (shell->dock.renderer.ops.begin_frame(shell->dock.renderer.backend) != REACH_OK) {
        return REACH_ERROR;
    }

    result = shell->dock.renderer.ops.execute(shell->dock.renderer.backend, &commands);
    if (result != REACH_OK) {
        return result;
    }

    return shell->dock.renderer.ops.end_frame(shell->dock.renderer.backend);
}

reach_result reach_shell_render_tray_surface(reach_shell *shell, reach_rect_f32 bounds)
{
    if (shell == nullptr || shell->tray.renderer.ops.begin_frame == nullptr) {
        return REACH_OK;
    }

    reach_render_command_buffer commands = {};
    reach_tray_render_input input = {};
    input.theme = shell->theme != nullptr ? shell->theme : reach_theme_default();
    input.model = &shell->tray_state.model;
    input.bounds = bounds;
    input.dock_height = shell->layout.dock.bounds.height;
    input.click_feedback_index = shell->feedback.tray_index;
    input.click_feedback_opacity = shell->feedback.tray_opacity.value;
    input.text_alignment_center = REACH_TEXT_ALIGNMENT_CENTER;

    reach_result result = reach_tray_build_render_commands(&input, &commands);
    if (result != REACH_OK) {
        return result;
    }

    if (shell->tray.renderer.ops.begin_frame(shell->tray.renderer.backend) != REACH_OK) {
        return REACH_ERROR;
    }

    (void)shell->tray.renderer.ops.execute(shell->tray.renderer.backend, &commands);
    return shell->tray.renderer.ops.end_frame(shell->tray.renderer.backend);
}

reach_result reach_shell_render_quick_settings_surface(reach_shell *shell)
{
    if (shell == nullptr || shell->quick_settings.renderer.ops.begin_frame == nullptr) {
        return REACH_OK;
    }

    reach_quick_settings_render_input input = {};
    input.theme = shell->theme != nullptr ? *shell->theme : *reach_theme_default();
    input.model = shell->quick_settings_model;
    input.layout = shell->quick_settings_layout;

    reach_render_command_buffer commands = {};
    reach_result result = reach_quick_settings_build_render_commands(&input, &commands);
    if (result != REACH_OK) {
        return result;
    }

    return reach_shell_render_popup_surface(
        shell,
        &shell->quick_settings,
        shell->quick_settings_bounds,
        shell->quick_settings_notch_anchor_x,
        &commands);
}

size_t reach_shell_switcher_visible_count(const reach_shell *shell)
{
    if (shell == nullptr) {
        return 0;
    }
    return reach_switcher_visible_count(shell->open_window_count);
}

void reach_shell_update_switcher_visible_start(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }

    reach_switcher_model model = {};
    model.window_count = shell->open_window_count;
    model.selected_index = shell->switcher_state.selected_index;
    model.visible_start = shell->switcher_state.visible_start;
    reach_switcher_update_visible_start(&model);
    shell->switcher_state.visible_start = model.visible_start;
}

static void reach_shell_label_from_path(uint16_t *out_label, size_t out_count, const uint16_t *path)
{
    if (out_label == nullptr || out_count == 0) {
        return;
    }

    out_label[0] = 0;
    const uint16_t fallback[] = { 'A','p','p',0 };
    if (path == nullptr || path[0] == 0) {
        (void)reach_copy_utf16(out_label, out_count, fallback);
        return;
    }

    const uint16_t *name = path;
    for (const uint16_t *cursor = path; *cursor != 0; ++cursor) {
        if (*cursor == '\\' || *cursor == '/') {
            name = cursor + 1;
        }
    }

    size_t name_length = 0;
    while (name[name_length] != 0) {
        ++name_length;
    }

    size_t end = name_length;
    for (size_t index = name_length; index > 0; --index) {
        if (name[index - 1] == '.') {
            end = index - 1;
            break;
        }
    }
    if (end == 0) {
        end = name_length;
    }

    size_t write = 0;
    while (write + 1 < out_count && write < end) {
        out_label[write] = name[write];
        ++write;
    }
    out_label[write] = 0;
    if (out_label[0] == 0) {
        (void)reach_copy_utf16(out_label, out_count, fallback);
    }
}

reach_result reach_shell_render_switcher_surface(reach_shell *shell, reach_rect_f32 bounds)
{
    if (shell == nullptr || shell->switcher.renderer.ops.begin_frame == nullptr) {
        return REACH_OK;
    }

    const reach_theme *theme = shell->theme != nullptr ? shell->theme : reach_theme_default();
    reach_shell_update_switcher_visible_start(shell);

    reach_switcher_model model = {};
    model.window_count = shell->open_window_count;
    model.selected_index = shell->switcher_state.selected_index;
    model.visible_start = shell->switcher_state.visible_start;

    reach_switcher_render_item items[REACH_MAX_PINNED_APPS] = {};
    for (size_t index = 0; index < shell->open_window_count && index < REACH_MAX_PINNED_APPS; ++index) {
        items[index].icon = shell->dock_icons.open_window_icons[index];
        reach_shell_label_from_path(items[index].label, 260, shell->open_windows[index].path);
    }

    reach_render_command_buffer commands = {};
    reach_switcher_render_input input = {};
    input.theme = theme;
    input.bounds = bounds;
    input.model = &model;
    input.items = items;
    input.item_count = shell->open_window_count;
    input.text_alignment_center = REACH_TEXT_ALIGNMENT_CENTER;
    input.text_weight_demi_bold = REACH_TEXT_WEIGHT_DEMIBOLD;

    reach_result build_result = reach_switcher_build_render_commands(&input, &commands);
    if (build_result != REACH_OK) {
        return build_result;
    }

    if (shell->switcher.renderer.ops.begin_frame(shell->switcher.renderer.backend) != REACH_OK) {
        return REACH_ERROR;
    }

    (void)shell->switcher.renderer.ops.execute(shell->switcher.renderer.backend, &commands);
    return shell->switcher.renderer.ops.end_frame(shell->switcher.renderer.backend);
}

reach_result reach_shell_render_launcher_surface(reach_shell *shell, const reach_launcher_layout *layout)
{
    REACH_ASSERT(shell != nullptr);
    REACH_ASSERT(layout != nullptr);
    if (shell == nullptr || layout == nullptr || shell->launcher.renderer.ops.begin_frame == nullptr) {
        return REACH_OK;
    }

    reach_launcher_render_input input = {};
    input.theme = shell->theme != nullptr ? shell->theme : reach_theme_default();
    input.state = &shell->ui;
    input.layout = layout;
    input.text_alignment_leading = REACH_TEXT_ALIGNMENT_LEADING;

    reach_render_command_buffer commands = {};
    reach_result build_result = reach_launcher_build_render_commands(&input, &commands);
    if (build_result != REACH_OK) {
        return build_result;
    }

    if (shell->launcher.renderer.ops.begin_frame(shell->launcher.renderer.backend) != REACH_OK) {
        return REACH_ERROR;
    }

    (void)shell->launcher.renderer.ops.execute(shell->launcher.renderer.backend, &commands);
    return shell->launcher.renderer.ops.end_frame(shell->launcher.renderer.backend);
}

reach_result reach_shell_render_context_menu_surface(reach_shell *shell)
{
    if (shell == nullptr || shell->context_menu.renderer.ops.begin_frame == nullptr) {
        return REACH_OK;
    }

    reach_context_menu_render_input input = {};
    input.theme = shell->theme != nullptr ? shell->theme : reach_theme_default();
    input.bounds = shell->context_menu_state.bounds;
    input.item_slots = shell->context_menu_state.item_slots;
    input.item_commands = shell->context_menu_state.item_commands;
    input.item_icon_ids = shell->context_menu_state.item_icon_ids;
    input.item_count = shell->context_menu_state.item_count;
    input.hovered_index = shell->context_menu_state.hovered_index;
    input.target_index = shell->context_menu_state.target_index;
    input.dock_layout = &shell->layout.dock;
    input.has_layout = shell->has_layout;
    input.use_anchor_x = shell->context_menu_state.power_open && shell->has_layout;
    input.anchor_x = shell->layout.dock.power_button.x + shell->layout.dock.power_button.width * 0.5f;
    input.text_alignment_leading = REACH_TEXT_ALIGNMENT_LEADING;

    reach_render_command_buffer commands = {};
    reach_result build_result = reach_context_menu_build_render_commands(&input, &commands);
    if (build_result != REACH_OK) {
        return build_result;
    }

    if (shell->context_menu.renderer.ops.begin_frame(shell->context_menu.renderer.backend) != REACH_OK) {
        return REACH_ERROR;
    }

    reach_result result = shell->context_menu.renderer.ops.execute(shell->context_menu.renderer.backend, &commands);
    if (result != REACH_OK) {
        return result;
    }

    return shell->context_menu.renderer.ops.end_frame(shell->context_menu.renderer.backend);
}
