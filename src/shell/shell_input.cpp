#include "shell_internal.h"

#include <windows.h>
#include <dwmapi.h>

#include <math.h>

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

static float reach_shell_clamp_float(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static void reach_shell_handle_global_mouse_down(reach_shell *shell, POINT point);

static void reach_shell_on_popup_mouse_down(void *user, int32_t x, int32_t y)
{
    reach_shell *shell = static_cast<reach_shell *>(user);
    if (shell == nullptr) {
        return;
    }
    POINT point = { x, y };
    reach_shell_handle_global_mouse_down(shell, point);
}

void reach_shell_raise_launcher(reach_shell *shell)
{
    if (shell == nullptr || shell->launcher.window.ops.native_handle == nullptr) {
        return;
    }

    HWND launcher_hwnd = (HWND)shell->launcher.window.ops.native_handle(shell->launcher.window.window);
    if (launcher_hwnd != nullptr) {
        SetWindowPos(launcher_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    }
}


static void reach_shell_capture_tray_input(reach_shell *shell)
{
    if (shell == nullptr ||
        shell->popup_capture.begin_capture == nullptr ||
        shell->tray.window.ops.native_handle == nullptr) {
        return;
    }

    void *tray_hwnd = shell->tray.window.ops.native_handle(shell->tray.window.window);
    if (tray_hwnd != nullptr) {
        (void)shell->popup_capture.begin_capture(
            shell->popup_capture.userdata,
            tray_hwnd);
    }
}

static void reach_shell_release_tray_input(reach_shell *shell)
{
    if (shell == nullptr ||
        shell->popup_capture.end_capture == nullptr ||
        shell->tray.window.ops.native_handle == nullptr) {
        return;
    }

    void *tray_hwnd = shell->tray.window.ops.native_handle(shell->tray.window.window);
    if (tray_hwnd != nullptr) {
        shell->popup_capture.end_capture(
            shell->popup_capture.userdata,
            tray_hwnd);
    }
}

static void reach_shell_capture_dock_input(reach_shell *shell)
{
    if (shell == nullptr ||
        shell->popup_capture.begin_capture == nullptr ||
        shell->dock.window.ops.native_handle == nullptr) {
        return;
    }

    void *dock_hwnd = shell->dock.window.ops.native_handle(shell->dock.window.window);
    if (dock_hwnd != nullptr) {
        (void)shell->popup_capture.begin_capture(
            shell->popup_capture.userdata,
            dock_hwnd);
    }
}

static void reach_shell_release_dock_input(reach_shell *shell)
{
    if (shell == nullptr ||
        shell->popup_capture.end_capture == nullptr ||
        shell->dock.window.ops.native_handle == nullptr) {
        return;
    }

    void *dock_hwnd = shell->dock.window.ops.native_handle(shell->dock.window.window);
    if (dock_hwnd != nullptr) {
        shell->popup_capture.end_capture(
            shell->popup_capture.userdata,
            dock_hwnd);
    }
}

static void reach_shell_capture_context_menu_input(reach_shell *shell)
{
    if (shell == nullptr ||
        shell->popup_capture.begin_capture == nullptr ||
        shell->context_menu.window.ops.native_handle == nullptr) {
        return;
    }

    void *hwnd = shell->context_menu.window.ops.native_handle(shell->context_menu.window.window);
    if (hwnd != nullptr) {
        (void)shell->popup_capture.begin_capture(
            shell->popup_capture.userdata,
            hwnd);
    }
}

static void reach_shell_release_context_menu_input(reach_shell *shell)
{
    if (shell == nullptr ||
        shell->popup_capture.end_capture == nullptr ||
        shell->context_menu.window.ops.native_handle == nullptr) {
        return;
    }

    void *hwnd = shell->context_menu.window.ops.native_handle(shell->context_menu.window.window);
    if (hwnd != nullptr) {
        shell->popup_capture.end_capture(
            shell->popup_capture.userdata,
            hwnd);
    }
}

static void reach_shell_handle_global_mouse_down(reach_shell *shell, POINT point)
{
    if (shell == nullptr) {
        return;
    }

    int32_t on_tray = shell->tray_popup_open &&
        (float)point.x >= shell->tray.last_bounds.x &&
        (float)point.x <= shell->tray.last_bounds.x + shell->tray.last_bounds.width &&
        (float)point.y >= shell->tray.last_bounds.y &&
        (float)point.y <= shell->tray.last_bounds.y + shell->tray.last_bounds.height;
    int32_t on_context = shell->context_menu_open &&
        (float)point.x >= shell->context_menu_bounds.x &&
        (float)point.x <= shell->context_menu_bounds.x + shell->context_menu_bounds.width &&
        (float)point.y >= shell->context_menu_bounds.y &&
        (float)point.y <= shell->context_menu_bounds.y + shell->context_menu_bounds.height;
    reach_dock_hit_result dock_hit = shell->has_layout ? reach_dock_hit_test(&shell->layout.dock, point.x, point.y) : reach_dock_hit_result {};
    int32_t on_tray_button = shell->tray_popup_open && dock_hit.type == REACH_DOCK_HIT_TRAY_BUTTON;

    if (shell->tray_popup_open && !on_tray && !on_tray_button) {
        reach_shell_set_tray_popup_open(shell, 0);
    }
    if (shell->context_menu_open && !on_context) {
        reach_shell_close_context_menu(shell);
        reach_shell_clear_sticky_dock_feedback(shell);
    }
}

void reach_shell_sync_popup_mouse_hook(reach_shell *shell)
{
    if (shell == nullptr || shell->popup_capture.sync_mouse_hook == nullptr) {
        return;
    }

    int32_t should_hook = shell->tray_popup_open || shell->context_menu_open;
    (void)shell->popup_capture.sync_mouse_hook(
        shell->popup_capture.userdata,
        should_hook,
        reach_shell_on_popup_mouse_down,
        shell);
}

void reach_shell_set_tray_popup_open(reach_shell *shell, int32_t open)
{
    if (shell == nullptr) {
        return;
    }

    int32_t next_open = open ? 1 : 0;
    if (shell->tray_popup_open == next_open) {
        return;
    }

    shell->tray_popup_open = next_open;
    if (shell->tray_popup_open) {
        (void)reach_shell_refresh_tray_items(shell);
        reach_shell_capture_tray_input(shell);
    } else {
        reach_shell_release_tray_input(shell);
    }
    reach_shell_sync_popup_mouse_hook(shell);
    shell->dock.dirty_flags = 1;
    shell->tray.dirty_flags = 1;
}

void reach_shell_toggle_tray_popup(reach_shell *shell)
{
    if (shell != nullptr) {
        reach_shell_set_tray_popup_open(shell, !shell->tray_popup_open);
    }
}

reach_result reach_shell_refresh_tray_items(reach_shell *shell)
{
    return shell != nullptr ? reach_tray_model_refresh(&shell->tray_model, &shell->tray_provider) : REACH_OK;
}

void reach_shell_compute_tray_popup_layout(
    reach_shell *shell,
    const reach_dock_layout *dock_layout,
    reach_rect_f32 *out_bounds,
    reach_rect_f32 *out_slots)
{
    if (shell == nullptr || dock_layout == nullptr || out_bounds == nullptr) {
        return;
    }

    (void)out_slots;
    const reach_theme *theme = shell->theme != nullptr ? shell->theme : reach_theme_default();
    reach_tray_compute_popup_layout(&shell->tray_model, theme, dock_layout, out_bounds);
}

static void reach_shell_start_dock_click_feedback(reach_shell *shell, size_t index, float target_opacity)
{
    if (shell == nullptr || index > REACH_SHELL_DOCK_FEEDBACK_TRAY_BUTTON) {
        return;
    }
    shell->dock_click_feedback_index = index;
    reach_float_animation_start(&shell->dock_click_feedback_opacity, shell->dock_click_feedback_opacity.value, target_opacity, 0.055);
    shell->dock_click_feedback_animating = 1;
    shell->dock.dirty_flags = 1;
}

static void reach_shell_press_dock_item(reach_shell *shell, size_t index)
{
    if (shell == nullptr) {
        return;
    }
    shell->dock_click_feedback_pressed = 1;
    shell->dock_click_feedback_sticky = 0;
    reach_shell_start_dock_click_feedback(shell, index, 0.50f);
}

static void reach_shell_set_dock_click_feedback_immediate(reach_shell *shell, size_t index, float opacity)
{
    if (shell == nullptr || index > REACH_SHELL_DOCK_FEEDBACK_TRAY_BUTTON) {
        return;
    }
    shell->dock_click_feedback_index = index;
    shell->dock_click_feedback_opacity = {};
    shell->dock_click_feedback_opacity.from = opacity;
    shell->dock_click_feedback_opacity.to = opacity;
    shell->dock_click_feedback_opacity.value = opacity;
    shell->dock_click_feedback_animating = 0;
    shell->dock.dirty_flags = 1;
}

static void reach_shell_stick_dock_item(reach_shell *shell)
{
    if (shell == nullptr || (!shell->dock_click_feedback_pressed && shell->dock_click_feedback_index == REACH_SHELL_DOCK_FEEDBACK_NONE)) {
        return;
    }
    shell->dock_click_feedback_pressed = 0;
    shell->dock_click_feedback_sticky = shell->dock_click_feedback_index != REACH_SHELL_DOCK_FEEDBACK_NONE;
    if (shell->dock_click_feedback_sticky) {
        reach_shell_set_dock_click_feedback_immediate(shell, shell->dock_click_feedback_index, 0.50f);
        reach_shell_capture_dock_input(shell);
    }
}

static void reach_shell_release_dock_item(reach_shell *shell)
{
    if (shell == nullptr || (!shell->dock_click_feedback_pressed && shell->dock_click_feedback_index == REACH_SHELL_DOCK_FEEDBACK_NONE)) {
        return;
    }
    shell->dock_click_feedback_pressed = 0;
    shell->dock_click_feedback_sticky = 0;
    reach_shell_release_dock_input(shell);
    if (shell->dock_click_feedback_index != REACH_SHELL_DOCK_FEEDBACK_NONE) {
        reach_shell_start_dock_click_feedback(shell, shell->dock_click_feedback_index, 0.0f);
    }
}

void reach_shell_clear_sticky_dock_feedback(reach_shell *shell)
{
    if (shell != nullptr && shell->dock_click_feedback_sticky) {
        reach_shell_release_dock_item(shell);
    }
}

static void reach_shell_press_tray_button(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }
    shell->dock_click_feedback_pressed = 1;
    shell->dock_click_feedback_sticky = 0;
    reach_shell_start_dock_click_feedback(shell, REACH_SHELL_DOCK_FEEDBACK_TRAY_BUTTON, 0.50f);
}

static void reach_shell_start_tray_click_feedback(reach_shell *shell, size_t index, float target_opacity)
{
    if (shell == nullptr || index >= REACH_MAX_TRAY_ITEMS) {
        return;
    }
    shell->tray_click_feedback_index = index;
    reach_float_animation_start(&shell->tray_click_feedback_opacity, shell->tray_click_feedback_opacity.value, target_opacity, 0.055);
    shell->tray_click_feedback_animating = 1;
    shell->tray.dirty_flags = 1;
}

static void reach_shell_press_tray_item(reach_shell *shell, size_t index)
{
    if (shell == nullptr) {
        return;
    }
    shell->tray_click_feedback_pressed = 1;
    reach_shell_start_tray_click_feedback(shell, index, 0.50f);
}

static void reach_shell_release_tray_item(reach_shell *shell)
{
    if (shell == nullptr || (!shell->tray_click_feedback_pressed && shell->tray_click_feedback_index == REACH_MAX_TRAY_ITEMS)) {
        return;
    }
    shell->tray_click_feedback_pressed = 0;
    if (shell->tray_click_feedback_index != REACH_MAX_TRAY_ITEMS) {
        reach_shell_start_tray_click_feedback(shell, shell->tray_click_feedback_index, 0.0f);
    }
}

static void reach_shell_mark_dirty_for_event(reach_shell *shell, const reach_ui_event *event)
{
    REACH_ASSERT(shell != nullptr);
    REACH_ASSERT(event != nullptr);
    if (shell == nullptr || event == nullptr) {
        return;
    }

    switch (event->type) {
    case REACH_UI_EVENT_WINDOWS_KEY:
    case REACH_UI_EVENT_ESCAPE:
    case REACH_UI_EVENT_TEXT:
    case REACH_UI_EVENT_BACKSPACE:
        shell->layout_dirty = 1;
        shell->launcher.dirty_flags = 1;
        break;
    case REACH_UI_EVENT_DOCK_APP_CLICK:
    case REACH_UI_EVENT_TRAY_BUTTON_CLICK:
    case REACH_UI_EVENT_POINTER_UP:
    case REACH_UI_EVENT_POINTER_MOVE:
    case REACH_UI_EVENT_POINTER_LEAVE:
    case REACH_UI_EVENT_POINTER_MIDDLE:
    case REACH_UI_EVENT_POINTER_DOWN:
        break;
    case REACH_UI_EVENT_NONE:
    default:
        break;
    }
}

static int32_t reach_rect_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return (float)x >= rect.x && (float)x <= rect.x + rect.width && (float)y >= rect.y && (float)y <= rect.y + rect.height;
}

static int32_t reach_utf16_starts_with_ascii_case_insensitive(const uint16_t *text, const char *prefix)
{
    if (text == nullptr || prefix == nullptr) {
        return 0;
    }

    size_t index = 0;
    while (prefix[index] != 0) {
        uint16_t current = text[index];
        char expected = prefix[index];
        if (current >= 'A' && current <= 'Z') {
            current = (uint16_t)(current - 'A' + 'a');
        }
        if (expected >= 'A' && expected <= 'Z') {
            expected = (char)(expected - 'A' + 'a');
        }
        if (current != (uint16_t)expected) {
            return 0;
        }
        ++index;
    }
    return 1;
}

static reach_result reach_shell_open_launcher_result(reach_shell *shell)
{
    REACH_ASSERT(shell != nullptr);
    if (shell == nullptr || shell->explorer_service.service == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    const uint16_t *query = shell->ui.launcher.query;
    if (query[0] == 0) {
        if (shell->explorer_service.ops.open_default != nullptr) {
            return shell->explorer_service.ops.open_default(shell->explorer_service.service);
        }
        return REACH_OK;
    }

    if (reach_utf16_starts_with_ascii_case_insensitive(query, "shell:") &&
        shell->explorer_service.ops.open_shell_location != nullptr) {
        return shell->explorer_service.ops.open_shell_location(shell->explorer_service.service, query);
    }

    DWORD attributes = GetFileAttributesW(reinterpret_cast<const wchar_t *>(query));
    if (attributes != INVALID_FILE_ATTRIBUTES && shell->explorer_service.ops.open_path != nullptr) {
        return shell->explorer_service.ops.open_path(shell->explorer_service.service, query);
    }

    if (shell->explorer_service.ops.open_default != nullptr) {
        return shell->explorer_service.ops.open_default(shell->explorer_service.service);
    }
    return REACH_OK;
}

static HHOOK g_reach_context_menu_hook;

static HFONT reach_shell_create_context_menu_font()
{
    NONCLIENTMETRICSW metrics = {};
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0)) {
        if (metrics.lfMenuFont.lfHeight < 0) {
            metrics.lfMenuFont.lfHeight += 2;
        } else if (metrics.lfMenuFont.lfHeight > 2) {
            metrics.lfMenuFont.lfHeight -= 2;
        }
        return CreateFontIndirectW(&metrics.lfMenuFont);
    }
    return static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
}

static void reach_shell_delete_context_menu_font(HFONT font)
{
    if (font != nullptr && font != GetStockObject(DEFAULT_GUI_FONT)) {
        DeleteObject(font);
    }
}

static void reach_shell_apply_context_menu_corners(HWND hwnd)
{
    if (hwnd == nullptr) {
        return;
    }
    RECT rect = {};
    if (!GetWindowRect(hwnd, &rect)) {
        return;
    }
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
        return;
    }
    int preference = DWMWCP_ROUND;
    (void)DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));
    SetWindowPos(
        hwnd,
        nullptr,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

static LRESULT CALLBACK reach_shell_context_menu_hook_proc(int code, WPARAM wparam, LPARAM lparam)
{
    if (code == HCBT_CREATEWND || code == HCBT_ACTIVATE) {
        HWND hwnd = reinterpret_cast<HWND>(wparam);
        wchar_t class_name[32] = {};
        if (hwnd != nullptr &&
            GetClassNameW(hwnd, class_name, 32) != 0 &&
            lstrcmpiW(class_name, L"#32768") == 0) {
            reach_shell_apply_context_menu_corners(hwnd);
        }
    }
    return CallNextHookEx(g_reach_context_menu_hook, code, wparam, lparam);
}

static void reach_shell_append_context_menu_item(HMENU menu, UINT id, const wchar_t *text)
{
    AppendMenuW(menu, MF_OWNERDRAW, id, reinterpret_cast<LPCWSTR>(text));
}

static const uint16_t *reach_shell_dock_item_path(const reach_shell *shell, size_t item_index)
{
    if (shell == nullptr || item_index >= shell->dock_model.item_count) {
        return nullptr;
    }
    if (shell->dock_model.items[item_index].pinned) {
        size_t pinned_index = shell->dock_model.items[item_index].pinned_index;
        return pinned_index < shell->ui.pinned_app_count ? shell->ui.pinned_apps[pinned_index].path : nullptr;
    }

    size_t open_index = shell->dock_model.items[item_index].open_index;
    return open_index < shell->open_window_count ? shell->open_windows[open_index].path : nullptr;
}

static int32_t reach_shell_context_menu_item_width(HDC dc, HFONT font, const wchar_t *text)
{
    if (dc == nullptr || text == nullptr) {
        return 0;
    }
    SIZE size = {};
    HGDIOBJ old_font = SelectObject(dc, font);
    GetTextExtentPoint32W(dc, text, (int)wcslen(text), &size);
    SelectObject(dc, old_font);
    return size.cx + 28;
}

static int32_t reach_shell_estimate_context_menu_width(const reach_shell *shell, size_t item_index, HWND owner)
{
    int32_t width = 160;
    HDC dc = GetDC(owner != nullptr ? owner : GetDesktopWindow());
    HFONT font = reach_shell_create_context_menu_font();
    if (dc == nullptr || font == nullptr) {
        reach_shell_delete_context_menu_font(font);
        if (dc != nullptr) {
            ReleaseDC(owner != nullptr ? owner : GetDesktopWindow(), dc);
        }
        return width;
    }

    int32_t candidate = reach_shell_context_menu_item_width(dc, font, reinterpret_cast<const wchar_t *>(reach_context_menu_command_text(REACH_CONTEXT_MENU_COMMAND_OPEN_NEW)));
    if (candidate > width) {
        width = candidate;
    }
    if (shell != nullptr && item_index < shell->dock_model.item_count && shell->dock_model.items[item_index].pinned) {
        candidate = reach_shell_context_menu_item_width(dc, font, reinterpret_cast<const wchar_t *>(reach_context_menu_command_text(REACH_CONTEXT_MENU_COMMAND_UNPIN)));
    } else {
        candidate = reach_shell_context_menu_item_width(dc, font, reinterpret_cast<const wchar_t *>(reach_context_menu_command_text(REACH_CONTEXT_MENU_COMMAND_PIN)));
    }
    if (candidate > width) {
        width = candidate;
    }
    if (shell != nullptr && item_index < shell->dock_model.item_count && shell->dock_model.items[item_index].window != 0) {
        candidate = reach_shell_context_menu_item_width(dc, font, reinterpret_cast<const wchar_t *>(reach_context_menu_command_text(REACH_CONTEXT_MENU_COMMAND_CLOSE)));
        if (candidate > width) {
            width = candidate;
        }
    }

    reach_shell_delete_context_menu_font(font);
    ReleaseDC(owner != nullptr ? owner : GetDesktopWindow(), dc);
    return width;
}

static reach_result reach_shell_launch_dock_item(reach_shell *shell, size_t item_index, int32_t force_new_instance)
{
    if (shell == nullptr || item_index >= shell->dock_model.item_count || shell->app_launcher.ops.launch == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    const uint16_t *path = reach_shell_dock_item_path(shell, item_index);
    if (path == nullptr || path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_app_launch_request request = {};
    reach_copy_utf16(request.path, 260, path);
    request.force_new_instance = force_new_instance;
    return shell->app_launcher.ops.launch(shell->app_launcher.launcher, &request);
}

void reach_shell_close_context_menu(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }
    shell->context_menu_open = 0;
    shell->context_menu_target_index = REACH_MAX_PINNED_APPS;
    shell->context_menu_hovered_index = REACH_MAX_PINNED_APPS;
    shell->context_menu_item_count = 0;
    shell->context_menu.dirty_flags = 1;
    reach_shell_release_context_menu_input(shell);
    if (shell->context_menu.window.ops.hide != nullptr) {
        (void)shell->context_menu.window.ops.hide(shell->context_menu.window.window);
    }
    reach_shell_sync_popup_mouse_hook(shell);
}

static reach_result reach_shell_execute_context_command(reach_shell *shell, uint32_t command)
{
    if (shell == nullptr || shell->context_menu_target_index >= shell->dock_model.item_count) {
        return REACH_OK;
    }
    size_t item_index = shell->context_menu_target_index;
    uint16_t item_path[260] = {};
    const uint16_t *path = reach_shell_dock_item_path(shell, item_index);
    if (path != nullptr) {
        (void)reach_copy_utf16(item_path, 260, path);
    }
    size_t pinned_index = shell->dock_model.items[item_index].pinned_index;
    uint32_t pin_id = pinned_index < shell->ui.pinned_app_count ? shell->ui.pinned_apps[pinned_index].id : 0;
    uintptr_t window_id = shell->dock_model.items[item_index].window;
    reach_shell_close_context_menu(shell);

    if (command == REACH_CONTEXT_MENU_COMMAND_OPEN_NEW) {
        if (item_path[0] == 0 || shell->app_launcher.ops.launch == nullptr) {
            return REACH_INVALID_ARGUMENT;
        }
        reach_app_launch_request request = {};
        reach_copy_utf16(request.path, 260, item_path);
        request.force_new_instance = 1;
        return shell->app_launcher.ops.launch(shell->app_launcher.launcher, &request);
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_UNPIN) {
        if (pin_id != 0 && reach_pin_config_unpin_id(&shell->config_store, pin_id) == REACH_OK) {
            return reach_shell_reload_pins(shell);
        }
        return REACH_ERROR;
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_PIN) {
        if (item_path[0] != 0 && reach_pin_config_pin_path(&shell->config_store, item_path) == REACH_OK) {
            return reach_shell_reload_pins(shell);
        }
        return REACH_ERROR;
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_CLOSE && shell->window_manager.ops.close != nullptr) {
        return shell->window_manager.ops.close(shell->window_manager.manager, window_id);
    }

    return REACH_OK;
}

static reach_result reach_shell_show_dock_app_context_menu(reach_shell *shell, size_t item_index, int32_t x, int32_t y)
{
    (void)x;
    (void)y;
    if (shell == nullptr || item_index >= shell->dock_model.item_count) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_context_menu_build_dock_item_commands(
        shell->dock_model.items[item_index].pinned,
        reach_shell_dock_item_path(shell, item_index) != nullptr,
        shell->dock_model.items[item_index].window != 0,
        shell->context_menu_item_commands,
        &shell->context_menu_item_count);

    float popup_width = 208.0f;
    float item_height = 34.0f;
    float padding = 8.0f;
    float notch_height = 8.0f;
    float anchor_ratio = 0.30f;
    float popup_body_height = padding * 2.0f + item_height * (float)shell->context_menu_item_count;
    float popup_height = popup_body_height + notch_height;
    float popup_x = (float)x - popup_width * anchor_ratio;
    float popup_y = (float)y - popup_height;
    if (shell->has_layout && item_index < shell->layout.dock.app_slot_count) {
        reach_rect_f32 slot = shell->layout.dock.app_slots[item_index];
        popup_x = slot.x + slot.width * 0.5f - popup_width * anchor_ratio;
        popup_y = shell->layout.dock.bounds.y - popup_height - 8.0f;
        reach_rect_f32 monitor = {};
        const reach_monitor_info *primary_monitor = shell->monitors != nullptr ? reach_monitor_primary(shell->monitors) : nullptr;
        if (primary_monitor != nullptr) {
            monitor.x = (float)primary_monitor->bounds.left;
            monitor.y = (float)primary_monitor->bounds.top;
            monitor.width = (float)(primary_monitor->bounds.right - primary_monitor->bounds.left);
            monitor.height = (float)(primary_monitor->bounds.bottom - primary_monitor->bounds.top);
        } else {
            monitor = shell->layout.dock.bounds;
        }
        if (popup_x < monitor.x + 8.0f) {
            popup_x = monitor.x + 8.0f;
        }
        float max_x = monitor.x + monitor.width - popup_width - 8.0f;
        if (popup_x > max_x) {
            popup_x = max_x;
        }
        if (popup_y < monitor.y + 8.0f) {
            popup_y = monitor.y + 8.0f;
        }
    }

    shell->context_menu_bounds = { popup_x, popup_y, popup_width, popup_height };
    for (size_t index = 0; index < shell->context_menu_item_count; ++index) {
        shell->context_menu_item_slots[index] = {
            popup_x + padding,
            popup_y + padding + item_height * (float)index,
            popup_width - padding * 2.0f,
            item_height
        };
    }
    shell->context_menu_target_index = item_index;
    shell->context_menu_hovered_index = REACH_MAX_PINNED_APPS;
    shell->context_menu_open = 1;
    shell->context_menu.dirty_flags = 1;
    reach_shell_capture_context_menu_input(shell);
    reach_shell_sync_popup_mouse_hook(shell);
    return REACH_OK;
}

static reach_result reach_shell_execute_dock_item_action(reach_shell *shell, reach_dock_item_action action)
{
    if (shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }
    if (action.type == REACH_DOCK_ITEM_ACTION_FOCUS_WINDOW) {
        uintptr_t window_id = action.window;
        if (window_id == 0) {
            return REACH_OK;
        }
        if (shell->window_manager.ops.refresh != nullptr) {
            (void)shell->window_manager.ops.refresh(shell->window_manager.manager);
            (void)reach_shell_refresh_open_windows(shell);
        }
        uintptr_t foreground = shell->window_manager.ops.foreground != nullptr
            ? shell->window_manager.ops.foreground(shell->window_manager.manager)
            : 0;
        int32_t minimized = reach_shell_window_is_minimized(shell, window_id);
        reach_result result = REACH_OK;
        if (foreground == window_id && !minimized && shell->window_manager.ops.minimize != nullptr) {
            result = shell->window_manager.ops.minimize(shell->window_manager.manager, window_id);
        } else if (shell->window_manager.ops.activate != nullptr) {
            result = shell->window_manager.ops.activate(shell->window_manager.manager, window_id);
        }
        if (shell->window_manager.ops.refresh != nullptr) {
            (void)shell->window_manager.ops.refresh(shell->window_manager.manager);
            (void)reach_shell_refresh_open_windows(shell);
        }
        shell->dock.dirty_flags = 1;
        return result;
    }
    if (action.type == REACH_DOCK_ITEM_ACTION_LAUNCH_PINNED) {
        if (action.pinned_index >= shell->ui.pinned_app_count) {
            return REACH_OK;
        }
        reach_ui_event routed = {};
        routed.type = REACH_UI_EVENT_DOCK_APP_CLICK;
        routed.id = shell->ui.pinned_apps[action.pinned_index].id;
        return reach_shell_handle_event(shell, &routed);
    }
    return REACH_OK;
}

static reach_result reach_shell_execute_tray_action(reach_shell *shell, reach_tray_feature_action action)
{
    if (shell == nullptr || action.type != REACH_TRAY_FEATURE_ACTION_ACTIVATE) {
        return REACH_OK;
    }
    if (shell->tray_provider.ops.activate == nullptr) {
        return REACH_OK;
    }

    reach_result result = shell->tray_provider.ops.activate(
        shell->tray_provider.provider,
        action.item_id,
        action.provider_action);
    reach_shell_release_tray_item(shell);
    if (shell->tray_popup_open) {
        reach_shell_capture_tray_input(shell);
    }
    return result;
}

static reach_result reach_shell_handle_pointer_up(reach_shell *shell, const reach_ui_event *event)
{
    if (!shell->has_layout) {
        return REACH_OK;
    }

    if (shell->context_menu_open) {
        reach_context_menu_hit_result context_hit = reach_context_menu_hit_test_items(
            shell->context_menu_item_slots,
            shell->context_menu_item_count,
            event->x,
            event->y);
        reach_context_menu_action context_action = reach_context_menu_action_for_hit(
            shell->context_menu_item_commands,
            shell->context_menu_item_count,
            context_hit);
        if (context_action.command != 0) {
            return reach_shell_execute_context_command(shell, context_action.command);
        }
        reach_shell_close_context_menu(shell);
        return REACH_OK;
    }

    if (shell->dock_drag_active) {
        uint32_t pin_id = shell->dock_drag_pin_id;
        int32_t dragged_pinned = shell->dock_drag_pinned;
        int32_t moved = shell->dock_drag_moved;
        size_t target_pinned_index = dragged_pinned ? reach_shell_pinned_order_index(shell, pin_id) : REACH_MAX_PINNED_APPS;
        size_t target_index = reach_shell_find_dock_item_key(shell, shell->dock_drag_pinned, shell->dock_drag_pin_id, shell->dock_drag_window);
        shell->dock_drag_active = 0;
        shell->dock_drag_moved = 0;
        shell->dock.dirty_flags = 1;
        reach_shell_release_dock_item(shell);
        if (moved && target_index < shell->layout.dock.app_slot_count) {
            float target_x = reach_shell_dock_slot_box_x(shell, &shell->layout.dock, target_index);
            reach_float_animation_start(&shell->dock_drag_snap_animation, shell->dock_drag_x, target_x, 0.12);
            shell->dock_drag_snapping = 1;
        } else {
            shell->dock_drag_source_index = REACH_MAX_PINNED_APPS;
            shell->dock_drag_target_index = REACH_MAX_PINNED_APPS;
            shell->dock_drag_pinned = 0;
            shell->dock_drag_pin_id = 0;
            shell->dock_drag_window = 0;
            shell->dock_drag_snapping = 0;
        }
        if (moved && dragged_pinned && target_pinned_index != REACH_MAX_PINNED_APPS) {
            reach_result result = reach_pin_config_move_id(&shell->config_store, pin_id, target_pinned_index);
            if (result != REACH_OK) {
                return result;
            }
            shell->dock_reload_pins_after_snap = shell->dock_drag_snapping;
            if (!shell->dock_drag_snapping) {
                return reach_shell_reload_pins(shell);
            }
        }
        if (moved) {
            return REACH_OK;
        }
    }

    reach_shell_release_dock_item(shell);

    if (shell->ui.launcher.open) {
        reach_launcher_hit_result launcher_hit = reach_launcher_hit_test(&shell->ui, &shell->layout.launcher, event->x, event->y);
        reach_launcher_action launcher_action = reach_launcher_action_for_hit(&shell->ui, launcher_hit);
        if (launcher_action.type == REACH_LAUNCHER_ACTION_LAUNCH_PINNED) {
            reach_ui_event routed = {};
            routed.type = REACH_UI_EVENT_DOCK_APP_CLICK;
            routed.id = launcher_action.pin_id;
            return reach_shell_handle_event(shell, &routed);
        }
        if (launcher_action.type == REACH_LAUNCHER_ACTION_OPEN_RESULT) {
            (void)reach_shell_open_launcher_result(shell);
            (void)reach_ui_state_close_launcher(&shell->ui);
            shell->layout_dirty = 1;
            shell->launcher.dirty_flags = 1;
            return shell->launcher.window.ops.hide != nullptr
                ? shell->launcher.window.ops.hide(shell->launcher.window.window)
                : REACH_OK;
        }
    }

    if (shell->tray_popup_open && shell->tray_provider.ops.activate != nullptr) {
        reach_tray_hit_result tray_hit = reach_tray_hit_test_popup(&shell->tray_model, shell->tray.last_bounds, event->x, event->y);
        reach_tray_feature_action tray_action = reach_tray_action_for_hit(&shell->tray_model, tray_hit, REACH_TRAY_ACTION_LEFT_CLICK);
        if (tray_action.type != REACH_TRAY_FEATURE_ACTION_NONE) {
            return reach_shell_execute_tray_action(shell, tray_action);
        }
    }

    reach_dock_hit_result dock_hit = reach_dock_hit_test(&shell->layout.dock, event->x, event->y);
    if (dock_hit.type == REACH_DOCK_HIT_TRAY_BUTTON) {
        reach_shell_toggle_tray_popup(shell);
        if (!shell->tray_popup_open && shell->tray.window.ops.hide != nullptr) {
            return shell->tray.window.ops.hide(shell->tray.window.window);
        }
        return REACH_OK;
    }

    if (shell->tray_popup_open && !reach_rect_contains(shell->tray.last_bounds, event->x, event->y)) {
        reach_shell_set_tray_popup_open(shell, 0);
        if (shell->tray.window.ops.hide != nullptr) {
            (void)shell->tray.window.ops.hide(shell->tray.window.window);
        }
    }

    if (dock_hit.type == REACH_DOCK_HIT_ITEM) {
        return reach_shell_execute_dock_item_action(shell, reach_dock_item_action_for_index(&shell->dock_model, dock_hit.index));
    }

    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_down(reach_shell *shell, const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->has_layout) {
        return REACH_OK;
    }

    reach_shell_clear_sticky_dock_feedback(shell);

    if (shell->context_menu_open) {
        if (!reach_rect_contains(shell->context_menu_bounds, event->x, event->y)) {
            reach_shell_close_context_menu(shell);
        } else {
            reach_shell_capture_context_menu_input(shell);
        }
        return REACH_OK;
    }

    reach_dock_hit_result dock_hit = reach_dock_hit_test(&shell->layout.dock, event->x, event->y);
    if (dock_hit.type == REACH_DOCK_HIT_TRAY_BUTTON) {
        reach_shell_press_tray_button(shell);
        return REACH_OK;
    }

    if (shell->tray_popup_open) {
        reach_tray_hit_result tray_hit = reach_tray_hit_test_popup(&shell->tray_model, shell->tray.last_bounds, event->x, event->y);
        if (tray_hit.type == REACH_TRAY_HIT_ITEM) {
            reach_shell_press_tray_item(shell, tray_hit.index);
            return REACH_OK;
        }
        if (tray_hit.type == REACH_TRAY_HIT_NONE) {
            reach_shell_capture_tray_input(shell);
            return REACH_OK;
        }
    }

    if (dock_hit.type == REACH_DOCK_HIT_ITEM) {
        size_t index = dock_hit.index;
        reach_shell_press_dock_item(shell, index);
        if (index < shell->dock_model.item_count) {
            shell->dock_drag_active = 1;
            shell->dock_drag_moved = 0;
            shell->dock_drag_source_index = index;
            shell->dock_drag_target_index = index;
            shell->dock_drag_pinned = shell->dock_model.items[index].pinned;
            shell->dock_drag_pin_id = 0;
            if (shell->dock_model.items[index].pinned && shell->dock_model.items[index].pinned_index < shell->ui.pinned_app_count) {
                shell->dock_drag_pin_id = shell->ui.pinned_apps[shell->dock_model.items[index].pinned_index].id;
            }
            shell->dock_drag_window = shell->dock_model.items[index].window;
            shell->dock_drag_start_x = event->x;
            shell->dock_drag_start_y = event->y;
            float box_x = reach_shell_dock_slot_box_x(shell, &shell->layout.dock, index);
            shell->dock_drag_grab_offset_x = (float)event->x - (shell->layout.dock.bounds.x + box_x);
            shell->dock_drag_x = box_x;
            shell->dock_drag_snapping = 0;
            shell->dock_reload_pins_after_snap = 0;
        }
        return REACH_OK;
    }

    reach_shell_release_tray_item(shell);
    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_move(reach_shell *shell, const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->has_layout) {
        return REACH_OK;
    }

    if (shell->context_menu_open) {
        reach_context_menu_hit_result context_hit = reach_context_menu_hit_test_items(
            shell->context_menu_item_slots,
            shell->context_menu_item_count,
            event->x,
            event->y);
        size_t hovered_context = context_hit.hit ? context_hit.index : REACH_MAX_PINNED_APPS;
        if (shell->context_menu_hovered_index != hovered_context) {
            shell->context_menu_hovered_index = hovered_context;
            shell->context_menu.dirty_flags = 1;
        }
        return REACH_OK;
    }

    if (shell->dock_drag_active) {
        int32_t dx = event->x - shell->dock_drag_start_x;
        int32_t dy = event->y - shell->dock_drag_start_y;
        if (!shell->dock_drag_moved && (dx * dx + dy * dy) >= 36) {
            shell->dock_drag_moved = 1;
        }
        if (shell->dock_drag_moved) {
            float next_drag_x = reach_shell_dock_drag_clamped_x(shell, &shell->layout.dock, event->x);
            if (fabsf(next_drag_x - shell->dock_drag_x) >= 0.5f) {
                shell->dock_drag_x = next_drag_x;
                shell->dock.dirty_flags = 1;
            }
            float dragged_box_x = shell->layout.dock.bounds.x + shell->dock_drag_x;
            size_t current = reach_shell_find_dock_order_key(
                shell,
                shell->dock_drag_pinned,
                shell->dock_drag_pin_id,
                shell->dock_drag_window);
            size_t target = reach_shell_dock_reorder_target(shell, current, dragged_box_x);
            if (target != REACH_MAX_PINNED_APPS && target != shell->dock_drag_target_index) {
                if (current != REACH_MAX_PINNED_APPS) {
                    reach_shell_move_dock_order(shell, current, target);
                    reach_shell_rebuild_dock_items_with_animations(shell, &shell->layout.dock);
                }
                shell->dock_drag_target_index = target;
                shell->dock_click_feedback_index = target;
                shell->dock.dirty_flags = 1;
            } else {
                size_t current = reach_shell_find_dock_order_key(
                    shell,
                    shell->dock_drag_pinned,
                    shell->dock_drag_pin_id,
                    shell->dock_drag_window);
                if (current != REACH_MAX_PINNED_APPS && shell->dock_click_feedback_index != current) {
                    shell->dock_click_feedback_index = current;
                    shell->dock.dirty_flags = 1;
                }
            }
        }
    }

    // size_t hovered = REACH_MAX_PINNED_APPS;
    // for (size_t index = 0; index < shell->layout.dock.app_slot_count; ++index) {
    //     if (reach_rect_contains(shell->layout.dock.app_slots[index], event->x, event->y)) {
    //         hovered = index;
    //         break;
    //     }
    // }

    // if (shell->hovered_dock_index != hovered) {
    //     shell->hovered_dock_index = hovered;
    //     shell->dock.dirty_flags = 1;
    // }
    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_middle(reach_shell *shell, const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->has_layout) {
        return REACH_OK;
    }

    reach_shell_release_dock_item(shell);
    reach_shell_release_tray_item(shell);

    reach_dock_hit_result dock_hit = reach_dock_hit_test(&shell->layout.dock, event->x, event->y);
    if (dock_hit.type == REACH_DOCK_HIT_ITEM) {
        return reach_shell_launch_dock_item(shell, dock_hit.index, 1);
    }
    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_leave(reach_shell *shell)
{
    // if (shell != nullptr && shell->hovered_dock_index != REACH_MAX_PINNED_APPS) {
    //     shell->hovered_dock_index = REACH_MAX_PINNED_APPS;
    //     shell->dock.dirty_flags = 1;
    // }
    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_context(reach_shell *shell, const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->has_layout) {
        return REACH_OK;
    }

    reach_shell_clear_sticky_dock_feedback(shell);
    if (shell->context_menu_open) {
        reach_shell_close_context_menu(shell);
    }

    if (shell->tray_popup_open && shell->tray_provider.ops.activate != nullptr) {
        reach_tray_hit_result tray_hit = reach_tray_hit_test_popup(&shell->tray_model, shell->tray.last_bounds, event->x, event->y);
        reach_tray_feature_action tray_action = reach_tray_action_for_hit(&shell->tray_model, tray_hit, REACH_TRAY_ACTION_RIGHT_CLICK);
        if (tray_action.type != REACH_TRAY_FEATURE_ACTION_NONE) {
            reach_shell_press_tray_item(shell, tray_action.item_index);
            return reach_shell_execute_tray_action(shell, tray_action);
        }
    }

    reach_dock_hit_result dock_hit = reach_dock_hit_test(&shell->layout.dock, event->x, event->y);
    if (dock_hit.type == REACH_DOCK_HIT_ITEM) {
        size_t index = dock_hit.index;
        shell->dock_click_feedback_pressed = 1;
        shell->dock_click_feedback_sticky = 0;
        reach_shell_set_dock_click_feedback_immediate(shell, index, 0.50f);
        (void)reach_shell_render_dock_surface(shell, &shell->layout.dock);
        reach_result result = reach_shell_show_dock_app_context_menu(shell, index, event->x, event->y);
        reach_shell_stick_dock_item(shell);
        (void)reach_shell_render_dock_surface(shell, &shell->layout.dock);
        return result;
    }

    return REACH_OK;
}

void reach_shell_on_window_event(void *user, const reach_ui_event *event)
{
    reach_shell *shell = static_cast<reach_shell *>(user);
    if (shell != nullptr && event != nullptr) {
        (void)reach_shell_handle_event(shell, event);
    }
}

static size_t reach_shell_foreground_open_window_index(reach_shell *shell)
{
    if (shell == nullptr || shell->window_manager.ops.foreground == nullptr) {
        return 0;
    }
    uintptr_t foreground = shell->window_manager.ops.foreground(shell->window_manager.manager);
    for (size_t index = 0; index < shell->open_window_count; ++index) {
        if (shell->open_windows[index].id == foreground) {
            return index;
        }
    }
    return 0;
}

static reach_result reach_shell_handle_switcher_event(reach_shell *shell, const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    if (event->type == REACH_UI_EVENT_ALT_TAB_BEGIN) {
        if (shell->window_manager.ops.refresh != nullptr) {
            (void)shell->window_manager.ops.refresh(shell->window_manager.manager);
            (void)reach_shell_refresh_open_windows(shell);
        }
        shell->switcher_open = shell->open_window_count > 0 ? 1 : 0;
        shell->switcher_selected_index = reach_shell_foreground_open_window_index(shell);
        reach_shell_update_switcher_visible_start(shell);
        shell->switcher.dirty_flags = 1;
        return REACH_OK;
    }
    if (!shell->switcher_open) {
        return REACH_OK;
    }
    if (event->type == REACH_UI_EVENT_ALT_TAB_NEXT && shell->open_window_count > 0) {
        shell->switcher_selected_index = (shell->switcher_selected_index + 1) % shell->open_window_count;
        reach_shell_update_switcher_visible_start(shell);
        shell->switcher.dirty_flags = 1;
        return REACH_OK;
    }
    if (event->type == REACH_UI_EVENT_ALT_TAB_PREVIOUS && shell->open_window_count > 0) {
        shell->switcher_selected_index = shell->switcher_selected_index == 0
            ? shell->open_window_count - 1
            : shell->switcher_selected_index - 1;
        reach_shell_update_switcher_visible_start(shell);
        shell->switcher.dirty_flags = 1;
        return REACH_OK;
    }
    if (event->type == REACH_UI_EVENT_ALT_TAB_CANCEL) {
        shell->switcher_open = 0;
        shell->switcher.dirty_flags = 1;
        if (shell->switcher.window.ops.hide != nullptr) {
            (void)shell->switcher.window.ops.hide(shell->switcher.window.window);
        }
        return REACH_OK;
    }
    if (event->type == REACH_UI_EVENT_ALT_TAB_COMMIT) {
        uintptr_t selected = shell->switcher_selected_index < shell->open_window_count
            ? shell->open_windows[shell->switcher_selected_index].id
            : 0;
        shell->switcher_open = 0;
        shell->switcher.dirty_flags = 1;
        if (shell->switcher.window.ops.hide != nullptr) {
            (void)shell->switcher.window.ops.hide(shell->switcher.window.window);
        }
        if (selected != 0 && shell->window_manager.ops.activate != nullptr) {
            return shell->window_manager.ops.activate(shell->window_manager.manager, selected);
        }
        return REACH_OK;
    }

    return REACH_OK;
}

reach_result reach_shell_handle_event(reach_shell *shell, const reach_ui_event *event)
{
    REACH_ASSERT(shell != nullptr);
    REACH_ASSERT(event != nullptr);
    if (shell == nullptr || event == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_ui_intent intent = {};
    if (event->type == REACH_UI_EVENT_ESCAPE && shell->context_menu_open) {
        reach_shell_close_context_menu(shell);
        return REACH_OK;
    }
    if (event->type == REACH_UI_EVENT_POINTER_UP) {
        return reach_shell_handle_pointer_up(shell, event);
    }
    if (event->type == REACH_UI_EVENT_POINTER_DOWN) {
        return reach_shell_handle_pointer_down(shell, event);
    }
    if (event->type == REACH_UI_EVENT_POINTER_MOVE) {
        return reach_shell_handle_pointer_move(shell, event);
    }
    if (event->type == REACH_UI_EVENT_POINTER_MIDDLE) {
        return reach_shell_handle_pointer_middle(shell, event);
    }
    if (event->type == REACH_UI_EVENT_POINTER_LEAVE) {
        return reach_shell_handle_pointer_leave(shell);
    }
    if (event->type == REACH_UI_EVENT_POINTER_CONTEXT) {
        return reach_shell_handle_pointer_context(shell, event);
    }
    if (event->type == REACH_UI_EVENT_WALLPAPER_CHANGED) {
        reach_shell_reload_wallpaper(shell, 1);
        return REACH_OK;
    }
    if (event->type == REACH_UI_EVENT_ALT_TAB_BEGIN ||
        event->type == REACH_UI_EVENT_ALT_TAB_NEXT ||
        event->type == REACH_UI_EVENT_ALT_TAB_PREVIOUS ||
        event->type == REACH_UI_EVENT_ALT_TAB_COMMIT ||
        event->type == REACH_UI_EVENT_ALT_TAB_CANCEL) {
        return reach_shell_handle_switcher_event(shell, event);
    }

    reach_result result = reach_ui_handle_event(&shell->ui, event, &intent);
    if (result != REACH_OK) {
        return result;
    }

    reach_shell_mark_dirty_for_event(shell, event);

    if (intent.type == REACH_UI_INTENT_OPEN_TRAY_MENU) {
        reach_shell_toggle_tray_popup(shell);
    } else if (intent.type == REACH_UI_INTENT_LAUNCH_APP) {
        for (size_t index = 0; index < shell->ui.pinned_app_count; ++index) {
            if (shell->ui.pinned_apps[index].id == intent.id && shell->app_launcher.ops.launch != nullptr) {
                reach_app_launch_request request = {};
                reach_copy_utf16(request.path, 260, shell->ui.pinned_apps[index].path);
                (void)shell->app_launcher.ops.launch(shell->app_launcher.launcher, &request);
            }
        }
    } else if (intent.type == REACH_UI_INTENT_RUN_SEARCH && shell->search_provider.ops.query != nullptr) {
        (void)shell->search_provider.ops.query(shell->search_provider.provider, shell->ui.launcher.query);
    }

    if (shell->launcher.window.ops.show != nullptr && shell->launcher.window.ops.hide != nullptr) {
        if (shell->ui.launcher.open) {
            reach_result show_result = shell->launcher.window.ops.show(shell->launcher.window.window);
            if (show_result == REACH_OK) {
                reach_shell_raise_launcher(shell);
            }
            return show_result;
        }
        return shell->launcher.window.ops.hide(shell->launcher.window.window);
    }

    return REACH_OK;
}
