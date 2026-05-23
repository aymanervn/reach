#include "shell_internal.h"

#include <dwrite.h>
#include <shlwapi.h>

static size_t reach_shell_min_size(size_t a, size_t b)
{
    return a < b ? a : b;
}

static reach_color reach_shell_rgb(uint8_t r, uint8_t g, uint8_t b, float a)
{
    reach_color color = {};
    color.r = (float)r / 255.0f;
    color.g = (float)g / 255.0f;
    color.b = (float)b / 255.0f;
    color.a = a;
    return color;
}

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
static reach_rect_f32 reach_shell_switcher_bounds(reach_rect_f32 monitor_bounds)
{
    reach_rect_f32 bounds = {};
    bounds.width = monitor_bounds.width < 320.0f ? monitor_bounds.width : 320.0f;
    bounds.height = 168.0f;
    bounds.x = monitor_bounds.x + (monitor_bounds.width - bounds.width) * 0.5f;
    bounds.y = monitor_bounds.y + monitor_bounds.height * 0.20f;
    return bounds;
}

size_t reach_shell_switcher_visible_count(const reach_shell *shell)
{
    if (shell == nullptr) {
        return 0;
    }
    return reach_shell_min_size(shell->open_window_count, REACH_SHELL_SWITCHER_VISIBLE_MAX);
}

reach_rect_f32 reach_shell_switcher_bounds_for_count(reach_rect_f32 monitor_bounds, size_t visible_count)
{
    float padding = 24.0f;
    float item_size = 112.0f;
    float gap = 14.0f;
    reach_rect_f32 bounds = {};
    size_t count = visible_count > 0 ? visible_count : 1;
    bounds.width = padding * 2.0f + (float)count * item_size + (float)(count - 1) * gap;
    float max_width = monitor_bounds.width - 48.0f;
    if (bounds.width > max_width) {
        bounds.width = max_width;
    }
    if (bounds.width < 280.0f) {
        bounds.width = monitor_bounds.width < 280.0f ? monitor_bounds.width : 280.0f;
    }
    bounds.height = 184.0f;
    bounds.x = monitor_bounds.x + (monitor_bounds.width - bounds.width) * 0.5f;
    bounds.y = monitor_bounds.y + (monitor_bounds.height - bounds.height) * 0.5f;
    return bounds;
}

void reach_shell_update_switcher_visible_start(reach_shell *shell)
{
    if (shell == nullptr || shell->open_window_count == 0) {
        if (shell != nullptr) {
            shell->switcher_visible_start = 0;
        }
        return;
    }
    size_t visible_count = reach_shell_switcher_visible_count(shell);
    if (visible_count == 0 || visible_count >= shell->open_window_count) {
        shell->switcher_visible_start = 0;
        return;
    }
    if (shell->switcher_selected_index < shell->switcher_visible_start) {
        shell->switcher_visible_start = shell->switcher_selected_index;
    } else if (shell->switcher_selected_index >= shell->switcher_visible_start + visible_count) {
        shell->switcher_visible_start = shell->switcher_selected_index - visible_count + 1;
    }
    size_t max_start = shell->open_window_count - visible_count;
    if (shell->switcher_visible_start > max_start) {
        shell->switcher_visible_start = max_start;
    }
}

reach_result reach_shell_render_switcher_surface(reach_shell *shell, reach_rect_f32 bounds)
{
    if (shell == nullptr || shell->switcher.renderer.ops.begin_frame == nullptr) {
        return REACH_OK;
    }

    reach_render_command_buffer commands = {};
    reach_render_command command = {};
    float radius = 20.0f;
    float padding = 24.0f;
    float item_size = 112.0f;
    float icon_box_size = 88.0f;
    float gap = 14.0f;
    const reach_theme *theme = shell->theme != nullptr ? shell->theme : reach_theme_default();
    float icon_box_radius = reach_theme_icon_box_corner_radius(theme, icon_box_size);
    size_t visible_count = reach_shell_switcher_visible_count(shell);
    reach_shell_update_switcher_visible_start(shell);

    command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = 0.5f;
    command.rect.y = 0.5f;
    command.rect.width = bounds.width - 1.0f;
    command.rect.height = bounds.height - 1.0f;
    command.color = theme->switcher_background;
    command.radius = radius;
    reach_render_command_buffer_push(&commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE;
    command.rect.x = 0.5f;
    command.rect.y = 0.5f;
    command.rect.width = bounds.width - 1.0f;
    command.rect.height = bounds.height - 1.0f;
    command.color = shell->theme != nullptr ? shell->theme->dock_border : reach_theme_default()->dock_border;
    command.radius = radius;
    command.stroke_width = 1.0f;
    reach_render_command_buffer_push(&commands, &command);

    if (visible_count > 0) {
        float total_width = (float)visible_count * item_size + (float)(visible_count - 1) * gap;
        float x = (bounds.width - total_width) * 0.5f;
        if (x < padding) {
            x = padding;
        }
        float y = (bounds.height - item_size) * 0.5f;
        for (size_t visible_index = 0; visible_index < visible_count; ++visible_index) {
            size_t index = shell->switcher_visible_start + visible_index;
            if (index >= shell->open_window_count) {
                break;
            }
            reach_rect_f32 item = { x + (float)visible_index * (item_size + gap), y, item_size, item_size };
            int32_t selected = index == shell->switcher_selected_index;
            float box_x = item.x + (item.width - icon_box_size) * 0.5f;
            float box_y = item.y + 4.0f;
            reach_icon_handle icon = index < shell->open_window_count ? shell->dock_icons.open_window_icons[index] : reach_icon_handle {};

            if (selected) {
                command = {};
                command.type = REACH_RENDER_COMMAND_RECT;
                command.rect.x = box_x - 5.0f;
                command.rect.y = box_y - 5.0f;
                command.rect.width = icon_box_size + 10.0f;
                command.rect.height = icon_box_size + 10.0f;
                command.color = reach_shell_rgb(255, 255, 255, 0.34f);
                command.radius = icon_box_radius + 5.0f;
                reach_render_command_buffer_push(&commands, &command);
            }

            if (icon.id != 0 && icon.wants_backplate) {
                command = {};
                command.type = REACH_RENDER_COMMAND_RECT;
                command.rect.x = box_x;
                command.rect.y = box_y;
                command.rect.width = icon_box_size;
                command.rect.height = icon_box_size;
                command.color = theme->icon_backplate_background;
                command.radius = icon_box_radius;
                reach_render_command_buffer_push(&commands, &command);
            }

            command = {};
            command.type = REACH_RENDER_COMMAND_ICON;
            if (icon.id != 0 && icon.wants_backplate) {
                float actual_icon_size = icon_box_size * theme->icon_backplate_scale;
                command.rect.x = box_x + (icon_box_size - actual_icon_size) * 0.5f;
                command.rect.y = box_y + (icon_box_size - actual_icon_size) * 0.5f;
                command.rect.width = actual_icon_size;
                command.rect.height = actual_icon_size;
                command.radius = 0.0f;
            } else {
                command.rect.x = box_x;
                command.rect.y = box_y;
                command.rect.width = icon_box_size;
                command.rect.height = icon_box_size;
                command.radius = icon_box_radius;
            }
            command.color.a = 1.0f;
            command.icon_id = icon.id;
            reach_render_command_buffer_push(&commands, &command);

            if (icon.id != 0 && icon.wants_backplate) {
                command = {};
                command.type = REACH_RENDER_COMMAND_BACKPLATE_EDGE;
                command.rect.x = box_x;
                command.rect.y = box_y;
                command.rect.width = icon_box_size;
                command.rect.height = icon_box_size;
                command.color = theme->icon_backplate_edge;
                command.radius = icon_box_radius;
                command.stroke_width = 0.55f;
                reach_render_command_buffer_push(&commands, &command);
            }

            if (selected) {
                const wchar_t *path = reinterpret_cast<const wchar_t *>(shell->open_windows[index].path);
                const wchar_t *name = PathFindFileNameW(path != nullptr ? path : L"");
                // Strip .exe extension
                wchar_t name_buf[260];
                wcsncpy_s(name_buf, name, _TRUNCATE);
                wchar_t *dot = wcsrchr(name_buf, L'.');
                if (dot != nullptr) {
                    *dot = L'\0';
                }
                command = {};
                command.type = REACH_RENDER_COMMAND_TEXT;
                command.rect.x = item.x - gap * 0.5f;
                command.rect.y = item.y + 104.0f;
                command.rect.width = item.width + gap;
                command.rect.height = 20.0f;
                command.color = reach_shell_rgb(242, 240, 236, 0.96f);
                command.text_weight = DWRITE_FONT_WEIGHT_DEMI_BOLD;
                command.text_alignment = DWRITE_TEXT_ALIGNMENT_CENTER;
                command.text_size = 13.0f;
                command.text_ellipsis = 1;
                reach_copy_utf16(command.text, 260, reinterpret_cast<const uint16_t *>(name_buf[0] != L'\0' ? name_buf : L"App"));
                reach_render_command_buffer_push(&commands, &command);
            }
        }
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
