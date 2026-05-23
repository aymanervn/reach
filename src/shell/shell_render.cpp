#include "shell_internal.h"

#include <dwrite.h>
#include <shlwapi.h>

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

    size_t dragged_render_index = (shell->dock_drag_active || shell->dock_drag_snapping)
        ? reach_shell_find_dock_item_key(shell, shell->dock_drag_pinned, shell->dock_drag_pin_id, shell->dock_drag_window)
        : REACH_MAX_PINNED_APPS;
    float dragged_x = shell->dock_drag_snapping
        ? shell->dock_drag_snap_animation.value
        : shell->dock_drag_x;
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
    input.click_feedback_index = shell->dock_click_feedback_index;
    input.click_feedback_opacity = shell->dock_click_feedback_opacity.value;
    input.tray_feedback_index = REACH_SHELL_DOCK_FEEDBACK_TRAY_BUTTON;
    input.text_alignment_center = DWRITE_TEXT_ALIGNMENT_CENTER;
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
    input.model = &shell->tray_model;
    input.bounds = bounds;
    input.dock_height = shell->layout.dock.bounds.height;
    input.click_feedback_index = shell->tray_click_feedback_index;
    input.click_feedback_opacity = shell->tray_click_feedback_opacity.value;
    input.text_alignment_center = DWRITE_TEXT_ALIGNMENT_CENTER;
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
    model.selected_index = shell->switcher_selected_index;
    model.visible_start = shell->switcher_visible_start;
    reach_switcher_update_visible_start(&model);
    shell->switcher_visible_start = model.visible_start;
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
    model.selected_index = shell->switcher_selected_index;
    model.visible_start = shell->switcher_visible_start;

    reach_switcher_render_item items[REACH_MAX_PINNED_APPS] = {};
    for (size_t index = 0; index < shell->open_window_count && index < REACH_MAX_PINNED_APPS; ++index) {
        items[index].icon = shell->dock_icons.open_window_icons[index];
        const wchar_t *path = reinterpret_cast<const wchar_t *>(shell->open_windows[index].path);
        const wchar_t *name = PathFindFileNameW(path != nullptr ? path : L"");
        // Strip .exe extension
        wchar_t name_buf[260];
        wcsncpy_s(name_buf, name, _TRUNCATE);
        wchar_t *dot = wcsrchr(name_buf, L'.');
        if (dot != nullptr) {
            *dot = L'\0';
        }
        reach_copy_utf16(items[index].label, 260, reinterpret_cast<const uint16_t *>(name_buf[0] != L'\0' ? name_buf : L"App"));
    }

    reach_render_command_buffer commands = {};
    reach_switcher_render_input input = {};
    input.theme = theme;
    input.bounds = bounds;
    input.model = &model;
    input.items = items;
    input.item_count = shell->open_window_count;
    input.text_alignment_center = DWRITE_TEXT_ALIGNMENT_CENTER;
    input.text_weight_demi_bold = DWRITE_FONT_WEIGHT_DEMI_BOLD;
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
    input.state = &shell->ui;
    input.layout = layout;
    input.text_alignment_leading = DWRITE_TEXT_ALIGNMENT_LEADING;
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
    input.bounds = shell->context_menu_bounds;
    input.item_slots = shell->context_menu_item_slots;
    input.item_commands = shell->context_menu_item_commands;
    input.item_count = shell->context_menu_item_count;
    input.hovered_index = shell->context_menu_hovered_index;
    input.target_index = shell->context_menu_target_index;
    input.dock_layout = &shell->layout.dock;
    input.has_layout = shell->has_layout;
    input.text_alignment_leading = DWRITE_TEXT_ALIGNMENT_LEADING;
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
