#include "reach/shell.h"

#include "reach/core/render_commands.h"
#include "reach/core/ui_events.h"
#include "reach/core/ui_layout.h"
#include "reach/animation.h"
#include "reach/features/dock.h"
#include "reach/features/tray.h"
#include "reach/hotkeys.h"
#include "reach/monitor.h"
#include "reach/platform/windows_adapters.h"
#include "reach/pin_config.h"
#include "reach/shell/surface_runtime.h"
#include "reach/theme.h"

#include <windows.h>
#include <dwmapi.h>
#include <shlwapi.h>
#include <dwrite.h>

#include <math.h>
#include <new>

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

struct reach_shell {
    reach_hotkeys *hotkeys;
    reach_monitor_list *monitors;
    reach_ui_state ui;
    reach_surface_runtime launcher;
    reach_surface_runtime dock;
    reach_surface_runtime tray;
    reach_surface_runtime switcher;
    reach_surface_runtime context_menu;
    reach_input_source_port input_source;
    reach_window_manager_port window_manager;
    reach_config_store_port config_store;
    reach_tray_provider_port tray_provider;
    reach_search_provider_port search_provider;
    reach_app_launcher_port app_launcher;
    reach_icon_provider_port icon_provider;
    reach_explorer_service_port explorer_service;
    reach_wallpaper_service_port wallpaper_service;
    reach_wallpaper_surface_port wallpaper_surface;
    const reach_theme *theme;
    reach_window_snapshot open_windows[REACH_MAX_PINNED_APPS];
    size_t open_window_count;
    reach_dock_feature_model dock_model;
    reach_dock_icon_cache dock_icons;
    reach_float_animation dock_item_x_animations[REACH_MAX_PINNED_APPS];
    int32_t dock_item_x_animating[REACH_MAX_PINNED_APPS];
    int32_t dock_item_x_valid[REACH_MAX_PINNED_APPS];
    int32_t dock_item_x_pinned[REACH_MAX_PINNED_APPS];
    uint32_t dock_item_x_pin_ids[REACH_MAX_PINNED_APPS];
    uintptr_t dock_item_x_windows[REACH_MAX_PINNED_APPS];
    int32_t dock_items_changed;
    reach_ui_layout layout;
    int32_t has_layout;
    int32_t layout_dirty;
    int32_t render_dirty;
    size_t hovered_dock_index;
    int32_t dock_animation_initialized;
    int32_t dock_animating;
    int32_t dock_target_hidden;
    int32_t dock_reveal_active;
    int32_t dock_width_animation_initialized;
    int32_t dock_width_animating;
    size_t dock_width_item_count;
    reach_float_animation dock_width_animation;
    size_t dock_click_feedback_index;
    int32_t dock_click_feedback_pressed;
    int32_t dock_click_feedback_sticky;
    int32_t dock_click_feedback_animating;
    reach_float_animation dock_click_feedback_opacity;
    size_t tray_click_feedback_index;
    int32_t tray_click_feedback_pressed;
    int32_t tray_click_feedback_animating;
    reach_float_animation tray_click_feedback_opacity;
    int32_t dock_drag_active;
    int32_t dock_drag_moved;
    size_t dock_drag_source_index;
    size_t dock_drag_target_index;
    int32_t dock_drag_pinned;
    uint32_t dock_drag_pin_id;
    uintptr_t dock_drag_window;
    int32_t dock_drag_start_x;
    int32_t dock_drag_start_y;
    float dock_drag_grab_offset_x;
    float dock_drag_x;
    int32_t dock_drag_snapping;
    int32_t dock_reload_pins_after_snap;
    reach_float_animation dock_drag_snap_animation;
    reach_float_animation dock_y_animation;
    double window_manager_refresh_elapsed;
    int32_t tray_popup_open;
    reach_tray_model tray_model;
    int32_t switcher_open;
    size_t switcher_selected_index;
    size_t switcher_visible_start;
    int32_t context_menu_open;
    size_t context_menu_target_index;
    reach_rect_f32 context_menu_bounds;
    reach_rect_f32 context_menu_item_slots[4];
    uint32_t context_menu_item_commands[4];
    size_t context_menu_item_count;
    size_t context_menu_hovered_index;
    int32_t running;
    uint16_t wallpaper_path[260];
    HHOOK popup_mouse_hook;
};

static const size_t REACH_SHELL_DOCK_FEEDBACK_TRAY_BUTTON = REACH_MAX_PINNED_APPS;
static const size_t REACH_SHELL_DOCK_FEEDBACK_NONE = REACH_MAX_PINNED_APPS + 1;
static const size_t REACH_SHELL_SWITCHER_VISIBLE_MAX = 12;

static int32_t reach_shell_rect_equal(reach_rect_f32 a, reach_rect_f32 b)
{
    return fabsf(a.x - b.x) < 0.5f &&
           fabsf(a.y - b.y) < 0.5f &&
           fabsf(a.width - b.width) < 0.5f &&
           fabsf(a.height - b.height) < 0.5f;
}

static int32_t reach_shell_opacity_equal(float a, float b)
{
    return fabsf(a - b) < 0.001f;
}

static int32_t reach_shell_float_animation_active(const reach_float_animation *animation)
{
    return animation != nullptr && animation->elapsed_seconds < animation->duration_seconds;
}

static size_t reach_shell_min_size(size_t a, size_t b)
{
    return a < b ? a : b;
}

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

static reach_shell *g_reach_popup_mouse_shell;

static HWND reach_shell_native_window(const reach_platform_window_port *window)
{
    if (window == nullptr || window->ops.native_handle == nullptr) {
        return nullptr;
    }
    return (HWND)window->ops.native_handle(window->window);
}

static int32_t reach_shell_point_on_window(HWND hwnd, POINT point)
{
    if (hwnd == nullptr || !IsWindow(hwnd)) {
        return 0;
    }
    HWND hit = WindowFromPoint(point);
    while (hit != nullptr) {
        if (hit == hwnd) {
            return 1;
        }
        hit = GetAncestor(hit, GA_PARENT);
    }
    return 0;
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

static int32_t reach_shell_dock_key_equal(int32_t a_pinned, uint32_t a_pin_id, uintptr_t a_window, int32_t b_pinned, uint32_t b_pin_id, uintptr_t b_window)
{
    reach_dock_order_key a = { a_pinned, a_pin_id, a_window };
    reach_dock_order_key b = { b_pinned, b_pin_id, b_window };
    return reach_dock_key_equal(&a, &b);
}

static size_t reach_shell_find_dock_order_key(const reach_shell *shell, int32_t pinned, uint32_t pin_id, uintptr_t window)
{
    if (shell == nullptr) {
        return REACH_MAX_PINNED_APPS;
    }
    reach_dock_order_key key = { pinned, pin_id, window };
    return reach_dock_feature_model_find_order_key(&shell->dock_model, key);
}

static void reach_shell_move_dock_order(reach_shell *shell, size_t source, size_t target)
{
    if (shell != nullptr) {
        reach_dock_feature_model_move_order(&shell->dock_model, source, target);
    }
}

static uint32_t reach_shell_dock_item_pin_id(const reach_shell *shell, size_t index)
{
    return shell != nullptr ? reach_dock_feature_model_item_pin_id(&shell->dock_model, index) : 0;
}

static int32_t reach_shell_dock_item_matches_key(const reach_shell *shell, size_t index, int32_t pinned, uint32_t pin_id, uintptr_t window)
{
    if (shell == nullptr) {
        return 0;
    }
    reach_dock_order_key key = { pinned, pin_id, window };
    return reach_dock_feature_model_item_matches_key(&shell->dock_model, index, key);
}

static void reach_shell_raise_launcher(reach_shell *shell)
{
    if (shell == nullptr || shell->launcher.window.ops.native_handle == nullptr) {
        return;
    }

    HWND launcher_hwnd = (HWND)shell->launcher.window.ops.native_handle(shell->launcher.window.window);
    if (launcher_hwnd != nullptr) {
        SetWindowPos(launcher_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    }
}

static reach_result reach_shell_refresh_tray_items(reach_shell *shell);
static void reach_shell_set_tray_popup_open(reach_shell *shell, int32_t open);
static void reach_shell_close_context_menu(reach_shell *shell);
static void reach_shell_sync_popup_mouse_hook(reach_shell *shell);
static void reach_shell_clear_sticky_dock_feedback(reach_shell *shell);

static void reach_shell_capture_tray_input(reach_shell *shell)
{
    if (shell == nullptr || shell->tray.window.ops.native_handle == nullptr) {
        return;
    }

    HWND tray_hwnd = (HWND)shell->tray.window.ops.native_handle(shell->tray.window.window);
    if (tray_hwnd != nullptr) {
        SetCapture(tray_hwnd);
    }
}

static void reach_shell_release_tray_input(reach_shell *shell)
{
    if (shell == nullptr || shell->tray.window.ops.native_handle == nullptr) {
        return;
    }

    HWND tray_hwnd = (HWND)shell->tray.window.ops.native_handle(shell->tray.window.window);
    if (tray_hwnd != nullptr && GetCapture() == tray_hwnd) {
        ReleaseCapture();
    }
}

static void reach_shell_capture_dock_input(reach_shell *shell)
{
    if (shell == nullptr || shell->dock.window.ops.native_handle == nullptr) {
        return;
    }

    HWND dock_hwnd = (HWND)shell->dock.window.ops.native_handle(shell->dock.window.window);
    if (dock_hwnd != nullptr) {
        SetCapture(dock_hwnd);
    }
}

static void reach_shell_release_dock_input(reach_shell *shell)
{
    if (shell == nullptr || shell->dock.window.ops.native_handle == nullptr) {
        return;
    }

    HWND dock_hwnd = (HWND)shell->dock.window.ops.native_handle(shell->dock.window.window);
    if (dock_hwnd != nullptr && GetCapture() == dock_hwnd) {
        ReleaseCapture();
    }
}

static void reach_shell_capture_context_menu_input(reach_shell *shell)
{
    if (shell == nullptr || shell->context_menu.window.ops.native_handle == nullptr) {
        return;
    }

    HWND hwnd = (HWND)shell->context_menu.window.ops.native_handle(shell->context_menu.window.window);
    if (hwnd != nullptr) {
        SetCapture(hwnd);
    }
}

static void reach_shell_release_context_menu_input(reach_shell *shell)
{
    if (shell == nullptr || shell->context_menu.window.ops.native_handle == nullptr) {
        return;
    }

    HWND hwnd = (HWND)shell->context_menu.window.ops.native_handle(shell->context_menu.window.window);
    if (hwnd != nullptr && GetCapture() == hwnd) {
        ReleaseCapture();
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

static LRESULT CALLBACK reach_shell_popup_mouse_hook_proc(int code, WPARAM wparam, LPARAM lparam)
{
    if (code >= 0 &&
        (wparam == WM_LBUTTONDOWN || wparam == WM_RBUTTONDOWN || wparam == WM_MBUTTONDOWN || wparam == WM_XBUTTONDOWN)) {
        MSLLHOOKSTRUCT *mouse = reinterpret_cast<MSLLHOOKSTRUCT *>(lparam);
        if (mouse != nullptr) {
            reach_shell_handle_global_mouse_down(g_reach_popup_mouse_shell, mouse->pt);
        }
    }
    HHOOK hook = g_reach_popup_mouse_shell != nullptr ? g_reach_popup_mouse_shell->popup_mouse_hook : nullptr;
    return CallNextHookEx(hook, code, wparam, lparam);
}

static void reach_shell_sync_popup_mouse_hook(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }

    int32_t should_hook = shell->tray_popup_open || shell->context_menu_open;
    if (should_hook && shell->popup_mouse_hook == nullptr) {
        shell->popup_mouse_hook = SetWindowsHookExW(WH_MOUSE_LL, reach_shell_popup_mouse_hook_proc, GetModuleHandleW(nullptr), 0);
        if (shell->popup_mouse_hook != nullptr) {
            g_reach_popup_mouse_shell = shell;
        }
    } else if (!should_hook && shell->popup_mouse_hook != nullptr) {
        UnhookWindowsHookEx(shell->popup_mouse_hook);
        shell->popup_mouse_hook = nullptr;
        if (g_reach_popup_mouse_shell == shell) {
            g_reach_popup_mouse_shell = nullptr;
        }
    }
}

static void reach_shell_set_tray_popup_open(reach_shell *shell, int32_t open)
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

static void reach_shell_toggle_tray_popup(reach_shell *shell)
{
    if (shell != nullptr) {
        reach_shell_set_tray_popup_open(shell, !shell->tray_popup_open);
    }
}

static reach_result reach_shell_refresh_tray_items(reach_shell *shell)
{
    return shell != nullptr ? reach_tray_model_refresh(&shell->tray_model, &shell->tray_provider) : REACH_OK;
}

static void reach_shell_compute_tray_popup_layout(
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

static reach_result reach_shell_apply_window_state(
    reach_platform_window_port *window,
    reach_rect_f32 bounds,
    float opacity,
    reach_rect_f32 *last_bounds,
    float *last_opacity,
    int32_t *bounds_valid,
    int32_t *opacity_valid,
    int32_t *out_changed)
{
    REACH_ASSERT(window != nullptr);
    REACH_ASSERT(last_bounds != nullptr);
    REACH_ASSERT(last_opacity != nullptr);
    REACH_ASSERT(bounds_valid != nullptr);
    REACH_ASSERT(opacity_valid != nullptr);
    REACH_ASSERT(out_changed != nullptr);
    if (window == nullptr || last_bounds == nullptr || last_opacity == nullptr ||
        bounds_valid == nullptr || opacity_valid == nullptr || out_changed == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_changed = 0;
    if (window->ops.set_bounds != nullptr && (!*bounds_valid || !reach_shell_rect_equal(*last_bounds, bounds))) {
        reach_result result = window->ops.set_bounds(window->window, bounds);
        if (result != REACH_OK) {
            return result;
        }
        *last_bounds = bounds;
        *bounds_valid = 1;
        *out_changed = 1;
    }

    if (window->ops.set_opacity != nullptr && (!*opacity_valid || !reach_shell_opacity_equal(*last_opacity, opacity))) {
        reach_result result = window->ops.set_opacity(window->window, opacity);
        if (result != REACH_OK) {
            return result;
        }
        *last_opacity = opacity;
        *opacity_valid = 1;
        *out_changed = 1;
    }

    return REACH_OK;
}

static int32_t reach_shell_dock_icon_size_px(const reach_shell *shell)
{
    const reach_theme *theme = shell != nullptr && shell->theme != nullptr ? shell->theme : reach_theme_default();
    float dock_height = shell != nullptr ? shell->ui.dock.height : 48.0f;
    return (int32_t)(reach_theme_icon_box_size(theme, dock_height) * 4.0f);
}

static reach_result reach_shell_load_pinned_icons(reach_shell *shell)
{
    REACH_ASSERT(shell != nullptr);
    if (shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }
    if (shell->icon_provider.ops.load == nullptr) {
        return REACH_OK;
    }
    return reach_dock_load_pinned_icons(&shell->dock_icons, &shell->icon_provider, shell->ui.pinned_apps, shell->ui.pinned_app_count, reach_shell_dock_icon_size_px(shell));
}

static int32_t reach_shell_path_equals(const uint16_t *a, const uint16_t *b)
{
    return a != nullptr && b != nullptr &&
        CompareStringOrdinal(reinterpret_cast<const wchar_t *>(a), -1, reinterpret_cast<const wchar_t *>(b), -1, TRUE) == CSTR_EQUAL;
}

static int32_t reach_shell_dock_path_matches_pinned(void *user, const reach_pinned_app_model *pinned_app, const uint16_t *path)
{
    (void)user;
    if (pinned_app == nullptr || path == nullptr || path[0] == 0) {
        return 0;
    }
    if (reach_shell_path_equals(pinned_app->path, path) ||
        reach_shell_path_equals(pinned_app->icon_ref, path)) {
        return 1;
    }
    const wchar_t *pinned_name = PathFindFileNameW(reinterpret_cast<const wchar_t *>(pinned_app->path));
    const wchar_t *window_name = PathFindFileNameW(reinterpret_cast<const wchar_t *>(path));
    return pinned_name != nullptr && window_name != nullptr && lstrcmpiW(pinned_name, window_name) == 0;
}

static reach_result reach_shell_refresh_open_windows(reach_shell *shell)
{
    if (shell == nullptr || shell->window_manager.ops.window_count == nullptr || shell->window_manager.ops.window_at == nullptr) {
        return REACH_OK;
    }

    uintptr_t old_windows[REACH_MAX_PINNED_APPS] = {};
    size_t old_count = shell->open_window_count;
    for (size_t index = 0; index < old_count; ++index) {
        old_windows[index] = shell->open_windows[index].id;
    }

    reach_dock_release_open_window_icons(&shell->dock_icons, &shell->icon_provider, shell->open_window_count);
    shell->open_window_count = 0;
    size_t count = shell->window_manager.ops.window_count(shell->window_manager.manager);
    for (size_t index = 0; index < count && shell->open_window_count < REACH_MAX_PINNED_APPS; ++index) {
        reach_window_snapshot snapshot = {};
        if (shell->window_manager.ops.window_at(shell->window_manager.manager, index, &snapshot) != REACH_OK ||
            snapshot.path[0] == 0) {
            continue;
        }
        size_t out_index = shell->open_window_count++;
        shell->open_windows[out_index] = snapshot;
    }
    (void)reach_dock_load_open_window_icons(&shell->dock_icons, &shell->icon_provider, shell->open_windows, shell->open_window_count, reach_shell_dock_icon_size_px(shell));

    if (old_count != shell->open_window_count) {
        shell->dock_items_changed = 1;
    } else {
        for (size_t index = 0; index < shell->open_window_count; ++index) {
            if (old_windows[index] != shell->open_windows[index].id) {
                shell->dock_items_changed = 1;
                break;
            }
        }
    }
    return REACH_OK;
}

static int32_t reach_shell_window_is_minimized(const reach_shell *shell, uintptr_t window_id)
{
    if (shell == nullptr || window_id == 0) {
        return 0;
    }
    for (size_t index = 0; index < shell->open_window_count; ++index) {
        if (shell->open_windows[index].id == window_id) {
            return shell->open_windows[index].minimized;
        }
    }
    return 0;
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

static void reach_shell_clear_sticky_dock_feedback(reach_shell *shell)
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

static void reach_shell_build_dock_items(reach_shell *shell, reach_dock_layout *layout)
{
    if (shell == nullptr || layout == nullptr) {
        return;
    }

    reach_dock_feature_model_build_items(
        &shell->dock_model,
        shell->ui.pinned_apps,
        shell->ui.pinned_app_count,
        shell->open_windows,
        shell->open_window_count,
        reach_shell_dock_path_matches_pinned,
        shell);

    layout->app_slot_count = shell->dock_model.item_count;
    float icon_size = shell->ui.dock.icon_size;
    float gap = shell->ui.dock.gap;
    size_t count = shell->dock_model.item_count;
    float dock_width = ceilf(icon_size * (float)(count + 1) + gap * (float)(count + 2));
    if (count == 0) {
        dock_width = ceilf(icon_size + gap * 2.0f);
    }
    float old_width = layout->bounds.width;
    if (dock_width != old_width) {
        layout->bounds.x += (old_width - dock_width) * 0.5f;
        layout->bounds.width = dock_width;
    }

    float left = layout->bounds.x + gap;
    float top = layout->bounds.y + (layout->bounds.height - icon_size) * 0.5f;
    for (size_t index = 0; index < layout->app_slot_count; ++index) {
        layout->app_slots[index].x = left + (icon_size + gap) * (float)index;
        layout->app_slots[index].y = top;
        layout->app_slots[index].width = icon_size;
        layout->app_slots[index].height = icon_size;
    }

    layout->tray_button.x = layout->bounds.x + dock_width - icon_size - gap;
    layout->tray_button.y = top;
}

static reach_result reach_shell_reload_pins(reach_shell *shell)
{
    if (shell == nullptr || shell->config_store.ops.load == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    int32_t old_order_pinned[REACH_MAX_PINNED_APPS] = {};
    uint32_t old_order_pin_ids[REACH_MAX_PINNED_APPS] = {};
    uintptr_t old_order_windows[REACH_MAX_PINNED_APPS] = {};
    uint16_t old_order_paths[REACH_MAX_PINNED_APPS][260] = {};
    size_t old_order_count = shell->dock_model.order_count;
    for (size_t order_index = 0; order_index < old_order_count; ++order_index) {
        old_order_pinned[order_index] = shell->dock_model.order[order_index].pinned;
        old_order_pin_ids[order_index] = shell->dock_model.order[order_index].pin_id;
        old_order_windows[order_index] = shell->dock_model.order[order_index].window;
        if (old_order_pinned[order_index]) {
            for (size_t pin_index = 0; pin_index < shell->ui.pinned_app_count; ++pin_index) {
                if (shell->ui.pinned_apps[pin_index].id == old_order_pin_ids[order_index]) {
                    reach_copy_utf16(old_order_paths[order_index], 260, shell->ui.pinned_apps[pin_index].path);
                    break;
                }
            }
        }
    }

    reach_config_snapshot snapshot = {};
    reach_result result = shell->config_store.ops.load(shell->config_store.store, &snapshot);
    if (result != REACH_OK) {
        return result;
    }
    result = reach_ui_state_set_pinned_apps(&shell->ui, snapshot.pinned_apps, snapshot.pinned_app_count);
    if (result != REACH_OK) {
        return result;
    }
    result = reach_shell_load_pinned_icons(shell);
    shell->dock_model.order_count = old_order_count;
    for (size_t order_index = 0; order_index < shell->dock_model.order_count; ++order_index) {
        shell->dock_model.order[order_index].pinned = old_order_pinned[order_index];
        shell->dock_model.order[order_index].pin_id = old_order_pin_ids[order_index];
        shell->dock_model.order[order_index].window = old_order_windows[order_index];
        if (old_order_pinned[order_index] && old_order_paths[order_index][0] != 0) {
            for (size_t pin_index = 0; pin_index < shell->ui.pinned_app_count; ++pin_index) {
                if (reach_shell_path_equals(shell->ui.pinned_apps[pin_index].path, old_order_paths[order_index])) {
                    shell->dock_model.order[order_index].pin_id = shell->ui.pinned_apps[pin_index].id;
                    break;
                }
            }
        }
    }
    shell->layout_dirty = 1;
    shell->render_dirty = 1;
    shell->dock.dirty_flags = 1;
    shell->launcher.dirty_flags = 1;
    shell->dock_items_changed = 1;
    return result;
}

static void reach_shell_seed_or_apply_wallpaper(reach_shell *shell, reach_config_snapshot *snapshot)
{
    if (shell == nullptr || snapshot == nullptr || shell->wallpaper_service.service == nullptr) {
        return;
    }
    shell->wallpaper_path[0] = 0;
    if (snapshot->wallpaper_path[0] != 0) {
        reach_copy_utf16(shell->wallpaper_path, 260, snapshot->wallpaper_path);
        if (shell->wallpaper_service.ops.set_wallpaper != nullptr) {
            (void)shell->wallpaper_service.ops.set_wallpaper(shell->wallpaper_service.service, snapshot->wallpaper_path);
        }
        if (shell->wallpaper_surface.ops.set_wallpaper != nullptr) {
            (void)shell->wallpaper_surface.ops.set_wallpaper(shell->wallpaper_surface.surface, snapshot->wallpaper_path);
        }
        if (shell->wallpaper_surface.ops.set_monitor_wallpaper != nullptr) {
            for (size_t index = 0; index < REACH_MAX_WALLPAPER_MONITORS; ++index) {
                if (snapshot->monitor_wallpaper_paths[index][0] != 0) {
                    (void)shell->wallpaper_surface.ops.set_monitor_wallpaper(
                        shell->wallpaper_surface.surface,
                        index,
                        snapshot->monitor_wallpaper_paths[index]);
                }
            }
        }
        return;
    }
    if (shell->wallpaper_service.ops.current_wallpaper == nullptr || shell->config_store.ops.save == nullptr) {
        return;
    }
    uint16_t current[260] = {};
    if (shell->wallpaper_service.ops.current_wallpaper(shell->wallpaper_service.service, current, 260) == REACH_OK &&
        current[0] != 0) {
        reach_copy_utf16(shell->wallpaper_path, 260, current);
        (void)reach_copy_utf16(snapshot->wallpaper_path, 260, current);
        (void)shell->config_store.ops.save(shell->config_store.store, snapshot);
        if (shell->wallpaper_surface.ops.set_wallpaper != nullptr) {
            (void)shell->wallpaper_surface.ops.set_wallpaper(shell->wallpaper_surface.surface, current);
        }
    }
    if (shell->wallpaper_surface.ops.set_monitor_wallpaper != nullptr) {
        for (size_t index = 0; index < REACH_MAX_WALLPAPER_MONITORS; ++index) {
            if (snapshot->monitor_wallpaper_paths[index][0] != 0) {
                (void)shell->wallpaper_surface.ops.set_monitor_wallpaper(
                    shell->wallpaper_surface.surface,
                    index,
                    snapshot->monitor_wallpaper_paths[index]);
            }
        }
    }
}

static void reach_shell_reload_wallpaper(reach_shell *shell, int32_t force)
{
    if (shell == nullptr || shell->config_store.ops.load == nullptr) {
        return;
    }

    reach_config_snapshot snapshot = {};
    if (shell->config_store.ops.load(shell->config_store.store, &snapshot) != REACH_OK) {
        return;
    }

    uint16_t new_path[260] = {};
    if (snapshot.wallpaper_path[0] != 0) {
        reach_copy_utf16(new_path, 260, snapshot.wallpaper_path);
    }

    if (!force && reach_shell_path_equals(shell->wallpaper_path, new_path)) {
        return;
    }

    reach_copy_utf16(shell->wallpaper_path, 260, new_path);
    if (new_path[0] != 0 && shell->wallpaper_surface.ops.set_wallpaper != nullptr) {
        (void)shell->wallpaper_surface.ops.set_wallpaper(shell->wallpaper_surface.surface, new_path);
    } else if (new_path[0] == 0 && shell->wallpaper_surface.ops.clear != nullptr) {
        (void)shell->wallpaper_surface.ops.clear(shell->wallpaper_surface.surface);
    }
    if (shell->wallpaper_surface.ops.set_monitor_wallpaper != nullptr &&
        shell->wallpaper_surface.ops.clear_monitor_wallpaper != nullptr) {
        for (size_t index = 0; index < REACH_MAX_WALLPAPER_MONITORS; ++index) {
            if (snapshot.monitor_wallpaper_paths[index][0] != 0) {
                (void)shell->wallpaper_surface.ops.set_monitor_wallpaper(
                    shell->wallpaper_surface.surface,
                    index,
                    snapshot.monitor_wallpaper_paths[index]);
            } else {
                (void)shell->wallpaper_surface.ops.clear_monitor_wallpaper(shell->wallpaper_surface.surface, index);
            }
        }
    }
}

static int32_t reach_shell_should_auto_hide_dock(const reach_shell *shell)
{
    REACH_ASSERT(shell != nullptr);
    if (shell == nullptr) {
        return 0;
    }
    if (shell->window_manager.ops.dock_should_auto_hide != nullptr) {
        return shell->window_manager.ops.dock_should_auto_hide(shell->window_manager.manager);
    }
    if (shell->window_manager.ops.foreground_is_maximized != nullptr) {
        return shell->window_manager.ops.foreground_is_maximized(shell->window_manager.manager);
    }
    return 0;
}

static int32_t reach_shell_get_cursor_position(POINT *out_cursor)
{
    if (out_cursor == nullptr) {
        return 0;
    }
    return GetCursorPos(out_cursor) ? 1 : 0;
}

static int32_t reach_shell_point_in_rect(POINT point, reach_rect_f32 rect)
{
    return (float)point.x >= rect.x &&
        (float)point.x < rect.x + rect.width &&
        (float)point.y >= rect.y &&
        (float)point.y < rect.y + rect.height;
}

static reach_rect_f32 reach_shell_dock_reveal_bounds(reach_rect_f32 shown_bounds, reach_rect_f32 monitor_bounds)
{
    float monitor_bottom = monitor_bounds.y + monitor_bounds.height;
    reach_rect_f32 bounds = shown_bounds;
    bounds.height = monitor_bottom - shown_bounds.y;
    if (bounds.height < shown_bounds.height) {
        bounds.height = shown_bounds.height;
    }
    return bounds;
}

static int32_t reach_shell_cursor_at_dock_reveal_edge(reach_rect_f32 shown_bounds, reach_rect_f32 monitor_bounds)
{
    POINT cursor = {};
    if (!reach_shell_get_cursor_position(&cursor)) {
        return 0;
    }

    float monitor_bottom = monitor_bounds.y + monitor_bounds.height;
    reach_rect_f32 reveal_bounds = reach_shell_dock_reveal_bounds(shown_bounds, monitor_bounds);
    reveal_bounds.y = monitor_bottom - 2.0f;
    reveal_bounds.height = 3.0f;
    return reach_shell_point_in_rect(cursor, reveal_bounds);
}

static int32_t reach_shell_cursor_in_dock_reveal_bounds(reach_rect_f32 shown_bounds, reach_rect_f32 monitor_bounds)
{
    POINT cursor = {};
    if (!reach_shell_get_cursor_position(&cursor)) {
        return 0;
    }
    return reach_shell_point_in_rect(cursor, reach_shell_dock_reveal_bounds(shown_bounds, monitor_bounds));
}

static reach_rect_f32 reach_shell_apply_dock_animation(reach_shell *shell, reach_rect_f32 shown_bounds, reach_rect_f32 monitor_bounds, double delta_seconds)
{
    REACH_ASSERT(shell != nullptr);
    float hidden_y = monitor_bounds.y + monitor_bounds.height + 4.0f;
    int32_t base_hidden = shell != nullptr &&
        shell->ui.dock.auto_hide &&
        reach_shell_should_auto_hide_dock(shell);
    if (!base_hidden) {
        shell->dock_reveal_active = 0;
    } else if (shell->dock_reveal_active) {
        shell->dock_reveal_active = reach_shell_cursor_in_dock_reveal_bounds(shown_bounds, monitor_bounds);
    } else if (reach_shell_cursor_at_dock_reveal_edge(shown_bounds, monitor_bounds)) {
        shell->dock_reveal_active = 1;
    }

    int32_t target_hidden = base_hidden && !shell->dock_reveal_active;
    float target_y = target_hidden ? hidden_y : shown_bounds.y;
    if (target_hidden && shell->tray_popup_open) {
        reach_shell_set_tray_popup_open(shell, 0);
    }
    if (target_hidden && shell->context_menu_open) {
        reach_shell_close_context_menu(shell);
        reach_shell_clear_sticky_dock_feedback(shell);
    } else if (target_hidden && shell->dock_click_feedback_sticky) {
        reach_shell_clear_sticky_dock_feedback(shell);
    }

    if (!shell->dock_animation_initialized) {
        shell->dock_animation_initialized = 1;
        shell->dock_target_hidden = target_hidden;
        shell->dock_y_animation = {};
        shell->dock_y_animation.from = target_y;
        shell->dock_y_animation.to = target_y;
        shell->dock_y_animation.value = target_y;
        shell->ui.dock.visible = target_hidden ? 0 : 1;
    }

    if (shell->dock_target_hidden != target_hidden) {
        shell->dock_target_hidden = target_hidden;
        shell->ui.dock.visible = target_hidden ? 0 : 1;
        reach_float_animation_start(&shell->dock_y_animation, shell->dock_y_animation.value, target_y, 0.18);
        shell->dock_animating = 1;
        shell->dock.dirty_flags = 1;
    }

    if (shell->dock_animating) {
        reach_float_animation_update(&shell->dock_y_animation, delta_seconds);
        shell->dock_animating = reach_shell_float_animation_active(&shell->dock_y_animation);
        shell->dock.dirty_flags = 1;
    }
    reach_rect_f32 animated = shown_bounds;
    animated.y = shell->dock_y_animation.value;
    return animated;
}

static void reach_shell_apply_dock_width_animation(reach_shell *shell, reach_dock_layout *layout, double delta_seconds)
{
    if (shell == nullptr || layout == nullptr) {
        return;
    }

    float target_width = layout->bounds.width;
    size_t target_count = layout->app_slot_count;
    if (!shell->dock_width_animation_initialized) {
        shell->dock_width_animation_initialized = 1;
        shell->dock_width_item_count = target_count;
        shell->dock_width_animation = {};
        shell->dock_width_animation.from = target_width;
        shell->dock_width_animation.to = target_width;
        shell->dock_width_animation.value = target_width;
    }

    if (shell->dock_width_item_count != target_count && fabsf(shell->dock_width_animation.to - target_width) >= 0.5f) {
        float from = shell->dock_width_animation.value > 0.0f ? shell->dock_width_animation.value : target_width;
        reach_float_animation_start(&shell->dock_width_animation, from, target_width, 0.18);
        shell->dock_width_animating = 1;
        shell->dock_width_item_count = target_count;
        shell->dock.dirty_flags = 1;
    } else if (!shell->dock_width_animating && fabsf(shell->dock_width_animation.value - target_width) >= 0.5f) {
        shell->dock_width_animation.from = target_width;
        shell->dock_width_animation.to = target_width;
        shell->dock_width_animation.value = target_width;
        shell->dock_width_item_count = target_count;
    }

    if (shell->dock_width_animating) {
        reach_float_animation_update(&shell->dock_width_animation, delta_seconds);
        shell->dock_width_animating = reach_shell_float_animation_active(&shell->dock_width_animation);
        shell->dock.dirty_flags = 1;
    }

    float animated_width = shell->dock_width_animation.value;
    if (animated_width <= 0.0f) {
        animated_width = target_width;
    }
    if (fabsf(animated_width - target_width) < 0.5f) {
        animated_width = target_width;
    }

    float target_x = layout->bounds.x;
    float center = layout->bounds.x + layout->bounds.width * 0.5f;
    layout->bounds.x = center - animated_width * 0.5f;
    layout->bounds.width = animated_width;
    float x_delta = layout->bounds.x - target_x;
    for (size_t index = 0; index < layout->app_slot_count; ++index) {
        layout->app_slots[index].x += x_delta;
    }
    layout->tray_button.x = layout->bounds.x + layout->bounds.width - layout->tray_button.width - shell->ui.dock.gap;
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

static void reach_shell_cleanup(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }

    reach_shell_set_tray_popup_open(shell, 0);
    reach_shell_close_context_menu(shell);
    reach_shell_sync_popup_mouse_hook(shell);
    reach_hotkeys_destroy(shell->hotkeys);
    reach_monitor_list_destroy(shell->monitors);
    if (shell->launcher.window.ops.destroy != nullptr) {
        shell->launcher.window.ops.destroy(shell->launcher.window.window);
    }
    if (shell->launcher.renderer.ops.destroy != nullptr) {
        shell->launcher.renderer.ops.destroy(shell->launcher.renderer.backend);
    }
    if (shell->dock.window.ops.destroy != nullptr) {
        shell->dock.window.ops.destroy(shell->dock.window.window);
    }
    if (shell->dock.renderer.ops.destroy != nullptr) {
        shell->dock.renderer.ops.destroy(shell->dock.renderer.backend);
    }
    if (shell->tray.window.ops.destroy != nullptr) {
        shell->tray.window.ops.destroy(shell->tray.window.window);
    }
    if (shell->tray.renderer.ops.destroy != nullptr) {
        shell->tray.renderer.ops.destroy(shell->tray.renderer.backend);
    }
    if (shell->switcher.window.ops.destroy != nullptr) {
        shell->switcher.window.ops.destroy(shell->switcher.window.window);
    }
    if (shell->switcher.renderer.ops.destroy != nullptr) {
        shell->switcher.renderer.ops.destroy(shell->switcher.renderer.backend);
    }
    if (shell->context_menu.window.ops.destroy != nullptr) {
        shell->context_menu.window.ops.destroy(shell->context_menu.window.window);
    }
    if (shell->context_menu.renderer.ops.destroy != nullptr) {
        shell->context_menu.renderer.ops.destroy(shell->context_menu.renderer.backend);
    }
    if (shell->input_source.ops.destroy != nullptr) {
        shell->input_source.ops.destroy(shell->input_source.source);
    }
    if (shell->window_manager.ops.destroy != nullptr) {
        shell->window_manager.ops.destroy(shell->window_manager.manager);
    }
    if (shell->config_store.ops.destroy != nullptr) {
        shell->config_store.ops.destroy(shell->config_store.store);
    }
    if (shell->tray_provider.ops.destroy != nullptr) {
        shell->tray_provider.ops.destroy(shell->tray_provider.provider);
    }
    if (shell->search_provider.ops.destroy != nullptr) {
        shell->search_provider.ops.destroy(shell->search_provider.provider);
    }
    reach_dock_release_all_icons(&shell->dock_icons, &shell->icon_provider, shell->open_window_count);
    if (shell->app_launcher.ops.destroy != nullptr) {
        shell->app_launcher.ops.destroy(shell->app_launcher.launcher);
    }
    if (shell->icon_provider.ops.destroy != nullptr) {
        shell->icon_provider.ops.destroy(shell->icon_provider.provider);
    }
    if (shell->explorer_service.ops.destroy != nullptr) {
        shell->explorer_service.ops.destroy(shell->explorer_service.service);
    }
    if (shell->wallpaper_service.ops.destroy != nullptr) {
        shell->wallpaper_service.ops.destroy(shell->wallpaper_service.service);
    }
    if (shell->wallpaper_surface.ops.destroy != nullptr) {
        shell->wallpaper_surface.ops.destroy(shell->wallpaper_surface.surface);
    }
    shell->hotkeys = nullptr;
    shell->monitors = nullptr;
    reach_surface_runtime_init(&shell->launcher);
    reach_surface_runtime_init(&shell->dock);
    reach_surface_runtime_init(&shell->tray);
    reach_surface_runtime_init(&shell->switcher);
    reach_surface_runtime_init(&shell->context_menu);
    shell->input_source = {};
    shell->window_manager = {};
    shell->config_store = {};
    shell->tray_provider = {};
    shell->search_provider = {};
    shell->app_launcher = {};
    shell->icon_provider = {};
    shell->explorer_service = {};
    shell->wallpaper_service = {};
    shell->wallpaper_surface = {};
    reach_dock_icon_cache_init(&shell->dock_icons);
    reach_tray_model_init(&shell->tray_model);
}

static float reach_shell_dock_item_current_x(const reach_shell *shell, const reach_dock_layout *layout, size_t index);
static size_t reach_shell_find_dock_item_key(const reach_shell *shell, int32_t pinned, uint32_t pin_id, uintptr_t window);

static reach_result reach_shell_render_dock_surface(reach_shell *shell, const reach_dock_layout *layout)
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
static reach_result reach_shell_render_tray_surface(reach_shell *shell, reach_rect_f32 bounds)
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

static size_t reach_shell_switcher_visible_count(const reach_shell *shell)
{
    if (shell == nullptr) {
        return 0;
    }
    return reach_shell_min_size(shell->open_window_count, REACH_SHELL_SWITCHER_VISIBLE_MAX);
}

static reach_rect_f32 reach_shell_switcher_bounds_for_count(reach_rect_f32 monitor_bounds, size_t visible_count)
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

static void reach_shell_update_switcher_visible_start(reach_shell *shell)
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

static reach_result reach_shell_render_switcher_surface(reach_shell *shell, reach_rect_f32 bounds)
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

static reach_result reach_shell_render_launcher_surface(reach_shell *shell, const reach_launcher_layout *layout)
{
    REACH_ASSERT(shell != nullptr);
    REACH_ASSERT(layout != nullptr);
    if (shell == nullptr || layout == nullptr || shell->launcher.renderer.ops.begin_frame == nullptr) {
        return REACH_OK;
    }

    reach_render_command_buffer commands = {};
    reach_render_command command = {};
    command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = layout->search_box.x - layout->bounds.x;
    command.rect.y = layout->search_box.y - layout->bounds.y;
    command.rect.width = layout->search_box.width;
    command.rect.height = layout->search_box.height;
    command.color.r = 0.08f;
    command.color.g = 0.08f;
    command.color.b = 0.09f;
    command.color.a = 0.95f;
    command.radius = 10.0f;
    reach_render_command_buffer_push(&commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect.x = layout->search_box.x - layout->bounds.x + 18.0f;
    command.rect.y = layout->search_box.y - layout->bounds.y;
    command.rect.width = layout->search_box.width - 36.0f;
    command.rect.height = layout->search_box.height;
    command.color.r = 1.0f;
    command.color.g = 1.0f;
    command.color.b = 1.0f;
    command.color.a = shell->ui.launcher.query_length > 0 ? 0.95f : 0.58f;
    command.text_size = 18.0f;
    command.text_alignment = DWRITE_TEXT_ALIGNMENT_LEADING;
    command.text_ellipsis = 1;
    reach_copy_utf16(command.text, 260, shell->ui.launcher.query_length > 0 ? shell->ui.launcher.query : (const uint16_t *)L"Search");
    reach_render_command_buffer_push(&commands, &command);

    if (shell->launcher.renderer.ops.begin_frame(shell->launcher.renderer.backend) != REACH_OK) {
        return REACH_ERROR;
    }
    (void)shell->launcher.renderer.ops.execute(shell->launcher.renderer.backend, &commands);
    return shell->launcher.renderer.ops.end_frame(shell->launcher.renderer.backend);
}

static int32_t reach_rect_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return (float)x >= rect.x && (float)x <= rect.x + rect.width && (float)y >= rect.y && (float)y <= rect.y + rect.height;
}

static float reach_shell_dock_slot_box_x(const reach_shell *shell, const reach_dock_layout *layout, size_t index)
{
    const reach_theme *theme = shell != nullptr && shell->theme != nullptr ? shell->theme : reach_theme_default();
    return reach_dock_slot_box_x(theme, layout, index);
}

static float reach_shell_dock_drag_clamped_x(const reach_shell *shell, const reach_dock_layout *layout, int32_t cursor_x)
{
    const reach_theme *theme = shell != nullptr && shell->theme != nullptr ? shell->theme : reach_theme_default();
    return reach_dock_drag_clamped_x(theme, layout, cursor_x, shell != nullptr ? shell->dock_drag_grab_offset_x : 0.0f);
}

static size_t reach_shell_find_dock_item_key(const reach_shell *shell, int32_t pinned, uint32_t pin_id, uintptr_t window)
{
    if (shell == nullptr) {
        return REACH_MAX_PINNED_APPS;
    }
    for (size_t index = 0; index < shell->dock_model.item_count; ++index) {
        if (reach_shell_dock_item_matches_key(shell, index, pinned, pin_id, window)) {
            return index;
        }
    }
    return REACH_MAX_PINNED_APPS;
}

static size_t reach_shell_dock_reorder_target(const reach_shell *shell, int32_t x)
{
    return shell != nullptr && shell->has_layout
        ? reach_dock_reorder_target(&shell->dock_model, &shell->layout.dock, x)
        : REACH_MAX_PINNED_APPS;
}

static size_t reach_shell_pinned_order_index(const reach_shell *shell, uint32_t pin_id)
{
    return shell != nullptr ? reach_dock_feature_model_pinned_order_index(&shell->dock_model, pin_id) : REACH_MAX_PINNED_APPS;
}

static float reach_shell_dock_item_current_x(const reach_shell *shell, const reach_dock_layout *layout, size_t index)
{
    if (shell == nullptr || layout == nullptr || index >= shell->dock_model.item_count || index >= layout->app_slot_count) {
        return 0.0f;
    }
    if ((shell->dock_drag_active || shell->dock_drag_snapping) &&
        reach_shell_dock_item_matches_key(shell, index, shell->dock_drag_pinned, shell->dock_drag_pin_id, shell->dock_drag_window)) {
        return shell->dock_drag_snapping ? shell->dock_drag_snap_animation.value : shell->dock_drag_x;
    }
    if ((shell->dock_item_x_animating[index] || shell->dock_item_x_valid[index]) &&
        reach_shell_dock_key_equal(
            shell->dock_item_x_pinned[index],
            shell->dock_item_x_pin_ids[index],
            shell->dock_item_x_windows[index],
            shell->dock_model.items[index].pinned,
            reach_shell_dock_item_pin_id(shell, index),
            shell->dock_model.items[index].window)) {
        return shell->dock_item_x_animations[index].value;
    }
    return reach_shell_dock_slot_box_x(shell, layout, index);
}

static void reach_shell_start_dock_item_x_animation(reach_shell *shell, size_t index, float from, float to)
{
    if (shell == nullptr || index >= REACH_MAX_PINNED_APPS) {
        return;
    }
    if (fabsf(from - to) < 0.5f) {
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

static void reach_shell_rebuild_dock_items_with_animations(reach_shell *shell, reach_dock_layout *layout)
{
    if (shell == nullptr || layout == nullptr) {
        return;
    }

    int32_t old_pinned[REACH_MAX_PINNED_APPS] = {};
    uint32_t old_pin_ids[REACH_MAX_PINNED_APPS] = {};
    uintptr_t old_windows[REACH_MAX_PINNED_APPS] = {};
    float old_x[REACH_MAX_PINNED_APPS] = {};
    size_t old_count = shell->dock_model.item_count;
    const reach_dock_layout *old_layout = shell->has_layout ? &shell->layout.dock : layout;
    for (size_t index = 0; index < old_count; ++index) {
        old_pinned[index] = shell->dock_model.items[index].pinned;
        old_pin_ids[index] = reach_shell_dock_item_pin_id(shell, index);
        old_windows[index] = shell->dock_model.items[index].window;
        old_x[index] = reach_shell_dock_item_current_x(shell, old_layout, index);
    }

    reach_shell_build_dock_items(shell, layout);

    for (size_t index = 0; index < shell->dock_model.item_count; ++index) {
        uint32_t pin_id = reach_shell_dock_item_pin_id(shell, index);
        float target_x = reach_shell_dock_slot_box_x(shell, layout, index);
        float from_x = target_x;
        for (size_t old_index = 0; old_index < old_count; ++old_index) {
            if (reach_shell_dock_key_equal(
                    old_pinned[old_index],
                    old_pin_ids[old_index],
                    old_windows[old_index],
                    shell->dock_model.items[index].pinned,
                    pin_id,
                    shell->dock_model.items[index].window)) {
                from_x = old_x[old_index];
                break;
            }
        }
        shell->dock_item_x_pinned[index] = shell->dock_model.items[index].pinned;
        shell->dock_item_x_pin_ids[index] = pin_id;
        shell->dock_item_x_windows[index] = shell->dock_model.items[index].window;
        if (reach_shell_dock_item_matches_key(shell, index, shell->dock_drag_pinned, shell->dock_drag_pin_id, shell->dock_drag_window) &&
            (shell->dock_drag_active || shell->dock_drag_snapping)) {
            reach_shell_start_dock_item_x_animation(shell, index, target_x, target_x);
        } else {
            reach_shell_start_dock_item_x_animation(shell, index, from_x, target_x);
        }
    }
    for (size_t index = shell->dock_model.item_count; index < REACH_MAX_PINNED_APPS; ++index) {
        shell->dock_item_x_valid[index] = 0;
        shell->dock_item_x_animating[index] = 0;
        shell->dock_item_x_pinned[index] = 0;
        shell->dock_item_x_pin_ids[index] = 0;
        shell->dock_item_x_windows[index] = 0;
    }
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

enum reach_shell_context_command {
    REACH_SHELL_CONTEXT_UNPIN = 100,
    REACH_SHELL_CONTEXT_CLOSE = 101,
    REACH_SHELL_CONTEXT_OPEN_NEW = 102,
    REACH_SHELL_CONTEXT_PIN = 103
};

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

    int32_t candidate = reach_shell_context_menu_item_width(dc, font, L"Open Another Instance");
    if (candidate > width) {
        width = candidate;
    }
    if (shell != nullptr && item_index < shell->dock_model.item_count && shell->dock_model.items[item_index].pinned) {
        candidate = reach_shell_context_menu_item_width(dc, font, L"Unpin app from dock");
    } else {
        candidate = reach_shell_context_menu_item_width(dc, font, L"Pin app to dock");
    }
    if (candidate > width) {
        width = candidate;
    }
    if (shell != nullptr && item_index < shell->dock_model.item_count && shell->dock_model.items[item_index].window != 0) {
        candidate = reach_shell_context_menu_item_width(dc, font, L"Close app");
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

static const wchar_t *reach_shell_context_command_text(uint32_t command)
{
    if (command == REACH_SHELL_CONTEXT_OPEN_NEW) {
        return L"Open Another Instance";
    }
    if (command == REACH_SHELL_CONTEXT_UNPIN) {
        return L"Unpin app from dock";
    }
    if (command == REACH_SHELL_CONTEXT_PIN) {
        return L"Pin app to dock";
    }
    if (command == REACH_SHELL_CONTEXT_CLOSE) {
        return L"Close app";
    }
    return L"";
}

static void reach_shell_close_context_menu(reach_shell *shell)
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

    if (command == REACH_SHELL_CONTEXT_OPEN_NEW) {
        if (item_path[0] == 0 || shell->app_launcher.ops.launch == nullptr) {
            return REACH_INVALID_ARGUMENT;
        }
        reach_app_launch_request request = {};
        reach_copy_utf16(request.path, 260, item_path);
        request.force_new_instance = 1;
        return shell->app_launcher.ops.launch(shell->app_launcher.launcher, &request);
    }
    if (command == REACH_SHELL_CONTEXT_UNPIN) {
        if (pin_id != 0 && reach_pin_config_unpin_id(&shell->config_store, pin_id) == REACH_OK) {
            return reach_shell_reload_pins(shell);
        }
        return REACH_ERROR;
    }
    if (command == REACH_SHELL_CONTEXT_PIN) {
        if (item_path[0] != 0 && reach_pin_config_pin_path(&shell->config_store, item_path) == REACH_OK) {
            return reach_shell_reload_pins(shell);
        }
        return REACH_ERROR;
    }
    if (command == REACH_SHELL_CONTEXT_CLOSE && shell->window_manager.ops.close != nullptr) {
        return shell->window_manager.ops.close(shell->window_manager.manager, window_id);
    }

    return REACH_OK;
}

static reach_result reach_shell_render_context_menu_surface(reach_shell *shell)
{
    if (shell == nullptr || shell->context_menu.renderer.ops.begin_frame == nullptr) {
        return REACH_OK;
    }

    reach_render_command_buffer commands = {};
    reach_render_command command = {};
    float width = shell->context_menu_bounds.width;
    float height = shell->context_menu_bounds.height;
    float radius = 14.0f;
    float notch_width = 18.0f;
    float notch_height = 8.0f;
    float body_height = height - notch_height;
    float notch_center = width * 0.30f;
    reach_color context_border = shell->theme != nullptr ? shell->theme->dock_border : reach_theme_default()->dock_border;
    context_border.a = 1.0f;
    if (shell->has_layout &&
        shell->context_menu_target_index < shell->layout.dock.app_slot_count) {
        reach_rect_f32 slot = shell->layout.dock.app_slots[shell->context_menu_target_index];
        notch_center = slot.x + slot.width * 0.5f - shell->context_menu_bounds.x;
        if (notch_center < radius + notch_width) {
            notch_center = radius + notch_width;
        }
        if (notch_center > width - radius - notch_width) {
            notch_center = width - radius - notch_width;
        }
    }

    command.type = REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT;
    command.rect.x = 0.5f;
    command.rect.y = 0.5f;
    command.rect.width = width - 1.0f;
    command.rect.height = height - 1.0f;
    command.color = reach_shell_rgb(32, 30, 28, 0.96f);
    command.radius = radius;
    command.notch_center_x = notch_center;
    command.notch_width = notch_width;
    command.notch_height = notch_height;
    reach_render_command_buffer_push(&commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT;
    command.rect.x = 0.5f;
    command.rect.y = 0.5f;
    command.rect.width = width - 1.0f;
    command.rect.height = height - 1.0f;
    command.color = context_border;
    command.radius = radius;
    command.stroke_width = 1.0f;
    command.notch_center_x = notch_center;
    command.notch_width = notch_width;
    command.notch_height = notch_height;
    reach_render_command_buffer_push(&commands, &command);

    for (size_t index = 0; index < shell->context_menu_item_count; ++index) {
        reach_rect_f32 item = shell->context_menu_item_slots[index];
        item.x -= shell->context_menu_bounds.x;
        item.y -= shell->context_menu_bounds.y;

        if (shell->context_menu_hovered_index == index) {
            command = {};
            command.type = REACH_RENDER_COMMAND_RECT;
            command.rect = item;
            command.color = reach_shell_rgb(255, 255, 255, 0.12f);
            command.radius = 8.0f;
            reach_render_command_buffer_push(&commands, &command);
        }

        command = {};
        command.type = REACH_RENDER_COMMAND_TEXT;
        command.rect.x = item.x + 14.0f;
        command.rect.y = item.y;
        command.rect.width = item.width - 28.0f;
        command.rect.height = item.height;
        command.color = reach_shell_rgb(232, 229, 224, 0.96f);
        command.text_size = 14.0f;
        command.text_alignment = DWRITE_TEXT_ALIGNMENT_LEADING;
        command.text_ellipsis = 1;
        reach_copy_utf16(command.text, 260, reinterpret_cast<const uint16_t *>(reach_shell_context_command_text(shell->context_menu_item_commands[index])));
        reach_render_command_buffer_push(&commands, &command);
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

static reach_result reach_shell_show_dock_app_context_menu(reach_shell *shell, size_t item_index, int32_t x, int32_t y)
{
    (void)x;
    (void)y;
    if (shell == nullptr || item_index >= shell->dock_model.item_count) {
        return REACH_INVALID_ARGUMENT;
    }

    shell->context_menu_item_count = 0;
    shell->context_menu_item_commands[shell->context_menu_item_count++] = REACH_SHELL_CONTEXT_OPEN_NEW;
    if (shell->dock_model.items[item_index].pinned) {
        shell->context_menu_item_commands[shell->context_menu_item_count++] = REACH_SHELL_CONTEXT_UNPIN;
    } else if (reach_shell_dock_item_path(shell, item_index) != nullptr) {
        shell->context_menu_item_commands[shell->context_menu_item_count++] = REACH_SHELL_CONTEXT_PIN;
    }
    if (shell->dock_model.items[item_index].window != 0) {
        shell->context_menu_item_commands[shell->context_menu_item_count++] = REACH_SHELL_CONTEXT_CLOSE;
    }

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
        for (size_t index = 0; index < shell->context_menu_item_count; ++index) {
            if (reach_rect_contains(shell->context_menu_item_slots[index], event->x, event->y)) {
                return reach_shell_execute_context_command(shell, shell->context_menu_item_commands[index]);
            }
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

    reach_ui_event routed = {};
    if (shell->ui.launcher.open) {
        for (size_t index = 0; index < shell->layout.launcher.pinned_app_slot_count; ++index) {
            if (reach_rect_contains(shell->layout.launcher.pinned_app_slots[index], event->x, event->y)) {
                routed.type = REACH_UI_EVENT_DOCK_APP_CLICK;
                routed.id = shell->ui.pinned_apps[index].id;
                return reach_shell_handle_event(shell, &routed);
            }
        }

        if (reach_rect_contains(shell->layout.launcher.search_results, event->x, event->y)) {
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
        size_t hovered_context = REACH_MAX_PINNED_APPS;
        for (size_t index = 0; index < shell->context_menu_item_count; ++index) {
            if (reach_rect_contains(shell->context_menu_item_slots[index], event->x, event->y)) {
                hovered_context = index;
                break;
            }
        }
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
            float dragged_center_x = shell->layout.dock.bounds.x + shell->dock_drag_x + shell->ui.dock.icon_size * 0.5f;
            size_t target = reach_shell_dock_reorder_target(shell, (int32_t)dragged_center_x);
            if (target != REACH_MAX_PINNED_APPS && target != shell->dock_drag_target_index) {
                size_t source = reach_shell_find_dock_order_key(
                    shell,
                    shell->dock_drag_pinned,
                    shell->dock_drag_pin_id,
                    shell->dock_drag_window);
                if (source != REACH_MAX_PINNED_APPS) {
                    reach_shell_move_dock_order(shell, source, target);
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

static void reach_shell_on_window_event(void *user, const reach_ui_event *event)
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

reach_result reach_shell_create(const reach_shell_desc *desc, reach_shell **out_shell)
{
    reach_shell_dependencies dependencies = {};
    reach_result result = reach_windows_create_wallpaper_surface(&dependencies.wallpaper_surface);
    if (result == REACH_OK) {
        result = reach_windows_create_platform_window(REACH_SURFACE_LAUNCHER, &dependencies.launcher_window);
    }
    if (result == REACH_OK) {
        void *native_window = dependencies.launcher_window.ops.native_handle(dependencies.launcher_window.window);
        result = reach_windows_create_d2d_render_backend(native_window, &dependencies.launcher_renderer);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_platform_window(REACH_SURFACE_DOCK, &dependencies.dock_window);
    }
    if (result == REACH_OK) {
        void *native_window = dependencies.dock_window.ops.native_handle(dependencies.dock_window.window);
        result = reach_windows_create_dcomp_render_backend(native_window, &dependencies.dock_renderer);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_platform_window(REACH_SURFACE_TRAY_MENU, &dependencies.tray_window);
    }
    if (result == REACH_OK) {
        void *native_window = dependencies.tray_window.ops.native_handle(dependencies.tray_window.window);
        result = reach_windows_create_dcomp_render_backend(native_window, &dependencies.tray_renderer);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_platform_window(REACH_SURFACE_SWITCHER, &dependencies.switcher_window);
    }
    if (result == REACH_OK) {
        void *native_window = dependencies.switcher_window.ops.native_handle(dependencies.switcher_window.window);
        result = reach_windows_create_dcomp_render_backend(native_window, &dependencies.switcher_renderer);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_platform_window(REACH_SURFACE_CONTEXT_MENU, &dependencies.context_menu_window);
    }
    if (result == REACH_OK) {
        void *native_window = dependencies.context_menu_window.ops.native_handle(dependencies.context_menu_window.window);
        result = reach_windows_create_dcomp_render_backend(native_window, &dependencies.context_menu_renderer);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_search_stub(&dependencies.search_provider);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_app_launcher(&dependencies.app_launcher);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_input_source(&dependencies.input_source);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_window_manager(&dependencies.window_manager);
    }
    if (result == REACH_OK) {
        const uint16_t default_path[] = { 'r','e','a','c','h','.','i','n','i',0 };
        result = reach_windows_create_config_store(desc != nullptr && desc->config_path != nullptr ? desc->config_path : default_path, &dependencies.config_store);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_tray_provider(&dependencies.tray_provider);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_icon_provider(&dependencies.icon_provider);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_explorer_service(&dependencies.explorer_service);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_wallpaper_service(&dependencies.wallpaper_service);
    }
    if (result != REACH_OK) {
        if (dependencies.launcher_window.ops.destroy != nullptr) {
            dependencies.launcher_window.ops.destroy(dependencies.launcher_window.window);
        }
        if (dependencies.launcher_renderer.ops.destroy != nullptr) {
            dependencies.launcher_renderer.ops.destroy(dependencies.launcher_renderer.backend);
        }
        if (dependencies.dock_window.ops.destroy != nullptr) {
            dependencies.dock_window.ops.destroy(dependencies.dock_window.window);
        }
        if (dependencies.dock_renderer.ops.destroy != nullptr) {
            dependencies.dock_renderer.ops.destroy(dependencies.dock_renderer.backend);
        }
        if (dependencies.tray_window.ops.destroy != nullptr) {
            dependencies.tray_window.ops.destroy(dependencies.tray_window.window);
        }
        if (dependencies.tray_renderer.ops.destroy != nullptr) {
            dependencies.tray_renderer.ops.destroy(dependencies.tray_renderer.backend);
        }
        if (dependencies.switcher_window.ops.destroy != nullptr) {
            dependencies.switcher_window.ops.destroy(dependencies.switcher_window.window);
        }
        if (dependencies.switcher_renderer.ops.destroy != nullptr) {
            dependencies.switcher_renderer.ops.destroy(dependencies.switcher_renderer.backend);
        }
        if (dependencies.context_menu_window.ops.destroy != nullptr) {
            dependencies.context_menu_window.ops.destroy(dependencies.context_menu_window.window);
        }
        if (dependencies.context_menu_renderer.ops.destroy != nullptr) {
            dependencies.context_menu_renderer.ops.destroy(dependencies.context_menu_renderer.backend);
        }
        if (dependencies.input_source.ops.destroy != nullptr) {
            dependencies.input_source.ops.destroy(dependencies.input_source.source);
        }
        if (dependencies.window_manager.ops.destroy != nullptr) {
            dependencies.window_manager.ops.destroy(dependencies.window_manager.manager);
        }
        if (dependencies.config_store.ops.destroy != nullptr) {
            dependencies.config_store.ops.destroy(dependencies.config_store.store);
        }
        if (dependencies.tray_provider.ops.destroy != nullptr) {
            dependencies.tray_provider.ops.destroy(dependencies.tray_provider.provider);
        }
        if (dependencies.search_provider.ops.destroy != nullptr) {
            dependencies.search_provider.ops.destroy(dependencies.search_provider.provider);
        }
        if (dependencies.app_launcher.ops.destroy != nullptr) {
            dependencies.app_launcher.ops.destroy(dependencies.app_launcher.launcher);
        }
        if (dependencies.icon_provider.ops.destroy != nullptr) {
            dependencies.icon_provider.ops.destroy(dependencies.icon_provider.provider);
        }
        if (dependencies.explorer_service.ops.destroy != nullptr) {
            dependencies.explorer_service.ops.destroy(dependencies.explorer_service.service);
        }
        if (dependencies.wallpaper_service.ops.destroy != nullptr) {
            dependencies.wallpaper_service.ops.destroy(dependencies.wallpaper_service.service);
        }
        if (dependencies.wallpaper_surface.ops.destroy != nullptr) {
            dependencies.wallpaper_surface.ops.destroy(dependencies.wallpaper_surface.surface);
        }
        return result;
    }

    result = reach_shell_create_with_dependencies(desc, &dependencies, out_shell);
    if (result != REACH_OK) {
        if (dependencies.launcher_window.ops.destroy != nullptr) {
            dependencies.launcher_window.ops.destroy(dependencies.launcher_window.window);
        }
        if (dependencies.launcher_renderer.ops.destroy != nullptr) {
            dependencies.launcher_renderer.ops.destroy(dependencies.launcher_renderer.backend);
        }
        if (dependencies.dock_window.ops.destroy != nullptr) {
            dependencies.dock_window.ops.destroy(dependencies.dock_window.window);
        }
        if (dependencies.dock_renderer.ops.destroy != nullptr) {
            dependencies.dock_renderer.ops.destroy(dependencies.dock_renderer.backend);
        }
        if (dependencies.tray_window.ops.destroy != nullptr) {
            dependencies.tray_window.ops.destroy(dependencies.tray_window.window);
        }
        if (dependencies.tray_renderer.ops.destroy != nullptr) {
            dependencies.tray_renderer.ops.destroy(dependencies.tray_renderer.backend);
        }
        if (dependencies.switcher_window.ops.destroy != nullptr) {
            dependencies.switcher_window.ops.destroy(dependencies.switcher_window.window);
        }
        if (dependencies.switcher_renderer.ops.destroy != nullptr) {
            dependencies.switcher_renderer.ops.destroy(dependencies.switcher_renderer.backend);
        }
        if (dependencies.context_menu_window.ops.destroy != nullptr) {
            dependencies.context_menu_window.ops.destroy(dependencies.context_menu_window.window);
        }
        if (dependencies.context_menu_renderer.ops.destroy != nullptr) {
            dependencies.context_menu_renderer.ops.destroy(dependencies.context_menu_renderer.backend);
        }
        if (dependencies.input_source.ops.destroy != nullptr) {
            dependencies.input_source.ops.destroy(dependencies.input_source.source);
        }
        if (dependencies.window_manager.ops.destroy != nullptr) {
            dependencies.window_manager.ops.destroy(dependencies.window_manager.manager);
        }
        if (dependencies.config_store.ops.destroy != nullptr) {
            dependencies.config_store.ops.destroy(dependencies.config_store.store);
        }
        if (dependencies.tray_provider.ops.destroy != nullptr) {
            dependencies.tray_provider.ops.destroy(dependencies.tray_provider.provider);
        }
        if (dependencies.search_provider.ops.destroy != nullptr) {
            dependencies.search_provider.ops.destroy(dependencies.search_provider.provider);
        }
        if (dependencies.app_launcher.ops.destroy != nullptr) {
            dependencies.app_launcher.ops.destroy(dependencies.app_launcher.launcher);
        }
        if (dependencies.icon_provider.ops.destroy != nullptr) {
            dependencies.icon_provider.ops.destroy(dependencies.icon_provider.provider);
        }
        if (dependencies.explorer_service.ops.destroy != nullptr) {
            dependencies.explorer_service.ops.destroy(dependencies.explorer_service.service);
        }
        if (dependencies.wallpaper_service.ops.destroy != nullptr) {
            dependencies.wallpaper_service.ops.destroy(dependencies.wallpaper_service.service);
        }
        if (dependencies.wallpaper_surface.ops.destroy != nullptr) {
            dependencies.wallpaper_surface.ops.destroy(dependencies.wallpaper_surface.surface);
        }
    }
    return result;
}

reach_result reach_shell_create_with_dependencies(const reach_shell_desc *desc, const reach_shell_dependencies *dependencies, reach_shell **out_shell)
{
    (void)desc;
    REACH_ASSERT(dependencies != nullptr);
    if (out_shell == nullptr || dependencies == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_shell *shell = new (std::nothrow) reach_shell();
    if (shell == nullptr) {
        *out_shell = nullptr;
        return REACH_ERROR;
    }

    reach_ui_state_init(&shell->ui);
    reach_dock_feature_model_init(&shell->dock_model);
    reach_dock_icon_cache_init(&shell->dock_icons);
    reach_tray_model_init(&shell->tray_model);
    reach_surface_runtime_init(&shell->launcher);
    reach_surface_runtime_init(&shell->dock);
    reach_surface_runtime_init(&shell->tray);
    reach_surface_runtime_init(&shell->switcher);
    reach_surface_runtime_init(&shell->context_menu);
    shell->dock_click_feedback_index = REACH_SHELL_DOCK_FEEDBACK_NONE;
    shell->dock_click_feedback_opacity = {};
    shell->tray_click_feedback_index = REACH_MAX_TRAY_ITEMS;
    shell->tray_click_feedback_opacity = {};
    shell->dock_drag_source_index = REACH_MAX_PINNED_APPS;
    shell->dock_drag_target_index = REACH_MAX_PINNED_APPS;
    shell->context_menu_target_index = REACH_MAX_PINNED_APPS;
    shell->context_menu_hovered_index = REACH_MAX_PINNED_APPS;

    reach_result result = reach_monitor_list_create(&shell->monitors);
    if (result == REACH_OK) {
        result = reach_hotkeys_create(&shell->hotkeys);
    }

    shell->launcher.window = dependencies->launcher_window;
    shell->launcher.renderer = dependencies->launcher_renderer;
    shell->dock.window = dependencies->dock_window;
    shell->dock.renderer = dependencies->dock_renderer;
    shell->tray.window = dependencies->tray_window;
    shell->tray.renderer = dependencies->tray_renderer;
    shell->switcher.window = dependencies->switcher_window;
    shell->switcher.renderer = dependencies->switcher_renderer;
    shell->context_menu.window = dependencies->context_menu_window;
    shell->context_menu.renderer = dependencies->context_menu_renderer;
    shell->input_source = dependencies->input_source;
    shell->window_manager = dependencies->window_manager;
    shell->config_store = dependencies->config_store;
    shell->tray_provider = dependencies->tray_provider;
    shell->search_provider = dependencies->search_provider;
    shell->app_launcher = dependencies->app_launcher;
    shell->icon_provider = dependencies->icon_provider;
    shell->explorer_service = dependencies->explorer_service;
    shell->wallpaper_service = dependencies->wallpaper_service;
    shell->wallpaper_surface = dependencies->wallpaper_surface;
    shell->theme = reach_theme_default();

    if (result == REACH_OK && shell->config_store.ops.load != nullptr) {
        (void)reach_pin_config_ensure_defaults(&shell->config_store);
        reach_config_snapshot snapshot = {};
        if (shell->config_store.ops.load(shell->config_store.store, &snapshot) == REACH_OK) {
            if (snapshot.dock_height > 0.0f) shell->ui.dock.height = snapshot.dock_height;
            if (snapshot.dock_width > 0.0f) shell->ui.dock.width = snapshot.dock_width;
            shell->ui.dock.icon_size = reach_theme_icon_box_size(shell->theme, shell->ui.dock.height);
            (void)reach_ui_state_set_pinned_apps(&shell->ui, snapshot.pinned_apps, snapshot.pinned_app_count);
            reach_shell_seed_or_apply_wallpaper(shell, &snapshot);
        }
    }
    if (result == REACH_OK) {
        result = reach_shell_load_pinned_icons(shell);
    }

    if (result != REACH_OK) {
        reach_shell_cleanup(shell);
        delete shell;
        *out_shell = nullptr;
        return result;
    }

    shell->layout_dirty = 1;
    shell->render_dirty = 1;
    shell->dock.dirty_flags = 1;
    shell->launcher.dirty_flags = 1;
    shell->switcher.dirty_flags = 1;
    shell->hovered_dock_index = REACH_MAX_PINNED_APPS;
    *out_shell = shell;
    return REACH_OK;
}

void reach_shell_destroy(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }

    reach_shell_cleanup(shell);
    delete shell;
}

reach_result reach_shell_start(reach_shell *shell)
{
    if (shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = REACH_OK;
    if (shell->window_manager.ops.start != nullptr) {
        result = shell->window_manager.ops.start(shell->window_manager.manager);
    }
    if (result != REACH_OK) {
        return result;
    }

    if (shell->dock.window.ops.set_event_callback != nullptr) {
        result = shell->dock.window.ops.set_event_callback(shell->dock.window.window, reach_shell_on_window_event, shell);
        if (result != REACH_OK) {
            return result;
        }
    }
    if (shell->launcher.window.ops.set_event_callback != nullptr) {
        result = shell->launcher.window.ops.set_event_callback(shell->launcher.window.window, reach_shell_on_window_event, shell);
        if (result != REACH_OK) {
            return result;
        }
    }
    if (shell->tray.window.ops.set_event_callback != nullptr) {
        result = shell->tray.window.ops.set_event_callback(shell->tray.window.window, reach_shell_on_window_event, shell);
        if (result != REACH_OK) {
            return result;
        }
    }
    if (shell->switcher.window.ops.set_event_callback != nullptr) {
        result = shell->switcher.window.ops.set_event_callback(shell->switcher.window.window, reach_shell_on_window_event, shell);
        if (result != REACH_OK) {
            return result;
        }
    }
    if (shell->context_menu.window.ops.set_event_callback != nullptr) {
        result = shell->context_menu.window.ops.set_event_callback(shell->context_menu.window.window, reach_shell_on_window_event, shell);
        if (result != REACH_OK) {
            return result;
        }
    }
    if (shell->launcher.window.ops.set_blur_enabled != nullptr) {
        result = shell->launcher.window.ops.set_blur_enabled(shell->launcher.window.window, 1);
        if (result != REACH_OK) {
            return result;
        }
    }

    if (shell->dock.window.ops.show != nullptr) {
        if (shell->wallpaper_surface.ops.show != nullptr) {
            result = shell->wallpaper_surface.ops.show(shell->wallpaper_surface.surface);
            if (result != REACH_OK) {
                return result;
            }
        }
        result = shell->dock.window.ops.show(shell->dock.window.window);
        if (result != REACH_OK) {
            return result;
        }
    }

    shell->running = 1;
    shell->layout_dirty = 1;
    shell->render_dirty = 1;
    shell->dock.dirty_flags = 1;
    shell->launcher.dirty_flags = 1;
    shell->tray.dirty_flags = 1;
    shell->switcher.dirty_flags = 1;
    shell->context_menu_open = 0;
    return REACH_OK;
}

reach_result reach_shell_stop(reach_shell *shell)
{
    if (shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    shell->running = 0;
    shell->switcher_open = 0;
    shell->context_menu_open = 0;
    reach_shell_set_tray_popup_open(shell, 0);
    if (shell->window_manager.ops.stop != nullptr) {
        (void)shell->window_manager.ops.stop(shell->window_manager.manager);
    }
    (void)reach_hotkeys_unregister_all(shell->hotkeys);
    if (shell->dock.window.ops.hide != nullptr) {
        (void)shell->dock.window.ops.hide(shell->dock.window.window);
    }
    if (shell->launcher.window.ops.hide != nullptr) {
        (void)shell->launcher.window.ops.hide(shell->launcher.window.window);
    }
    if (shell->tray.window.ops.hide != nullptr) {
        (void)shell->tray.window.ops.hide(shell->tray.window.window);
    }
    if (shell->switcher.window.ops.hide != nullptr) {
        (void)shell->switcher.window.ops.hide(shell->switcher.window.window);
    }
    if (shell->context_menu.window.ops.hide != nullptr) {
        (void)shell->context_menu.window.ops.hide(shell->context_menu.window.window);
    }
    if (shell->wallpaper_surface.ops.hide != nullptr) {
        (void)shell->wallpaper_surface.ops.hide(shell->wallpaper_surface.surface);
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

reach_result reach_shell_update(reach_shell *shell, double delta_seconds)
{
    if (shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    shell->window_manager_refresh_elapsed += delta_seconds;
    if (shell->dock_click_feedback_animating) {
        reach_float_animation_update(&shell->dock_click_feedback_opacity, delta_seconds);
        shell->dock_click_feedback_animating = reach_shell_float_animation_active(&shell->dock_click_feedback_opacity);
        if (!shell->dock_click_feedback_animating &&
            !shell->dock_click_feedback_pressed &&
            !shell->dock_click_feedback_sticky &&
            shell->dock_click_feedback_opacity.value <= 0.001f) {
            shell->dock_click_feedback_opacity.value = 0.0f;
            shell->dock_click_feedback_index = REACH_SHELL_DOCK_FEEDBACK_NONE;
        }
        shell->dock.dirty_flags = 1;
    }
    if (shell->tray_click_feedback_animating) {
        reach_float_animation_update(&shell->tray_click_feedback_opacity, delta_seconds);
        shell->tray_click_feedback_animating = reach_shell_float_animation_active(&shell->tray_click_feedback_opacity);
        if (!shell->tray_click_feedback_animating &&
            !shell->tray_click_feedback_pressed &&
            shell->tray_click_feedback_opacity.value <= 0.001f) {
            shell->tray_click_feedback_opacity.value = 0.0f;
            shell->tray_click_feedback_index = REACH_MAX_TRAY_ITEMS;
        }
        shell->tray.dirty_flags = 1;
    }
    for (size_t index = 0; index < shell->dock_model.item_count; ++index) {
        if (shell->dock_item_x_animating[index]) {
            reach_float_animation_update(&shell->dock_item_x_animations[index], delta_seconds);
            shell->dock_item_x_animating[index] = reach_shell_float_animation_active(&shell->dock_item_x_animations[index]);
            shell->dock.dirty_flags = 1;
        }
    }
    if (shell->dock_drag_snapping) {
        reach_float_animation_update(&shell->dock_drag_snap_animation, delta_seconds);
        shell->dock_drag_x = shell->dock_drag_snap_animation.value;
        shell->dock_drag_snapping = reach_shell_float_animation_active(&shell->dock_drag_snap_animation);
        shell->dock.dirty_flags = 1;
        if (!shell->dock_drag_snapping) {
            shell->dock_drag_source_index = REACH_MAX_PINNED_APPS;
            shell->dock_drag_target_index = REACH_MAX_PINNED_APPS;
            shell->dock_drag_pinned = 0;
            shell->dock_drag_pin_id = 0;
            shell->dock_drag_window = 0;
            if (shell->dock_reload_pins_after_snap) {
                shell->dock_reload_pins_after_snap = 0;
                (void)reach_shell_reload_pins(shell);
                shell->dock_model.item_count = 0;
                for (size_t index = 0; index < REACH_MAX_PINNED_APPS; ++index) {
                    shell->dock_item_x_valid[index] = 0;
                    shell->dock_item_x_animating[index] = 0;
                }
            }
        }
    }
    int32_t window_manager_dirty = shell->window_manager.ops.needs_refresh != nullptr &&
        shell->window_manager.ops.needs_refresh(shell->window_manager.manager);
    if ((window_manager_dirty || shell->window_manager_refresh_elapsed >= 0.25) && shell->window_manager.ops.refresh != nullptr) {
        (void)shell->window_manager.ops.refresh(shell->window_manager.manager);
        (void)reach_shell_refresh_open_windows(shell);
        shell->dock.dirty_flags = 1;
        shell->window_manager_refresh_elapsed = 0.0;
    }
    if (shell->tray_popup_open &&
        shell->tray_provider.ops.needs_refresh != nullptr &&
        shell->tray_provider.ops.needs_refresh(shell->tray_provider.provider)) {
        (void)reach_shell_refresh_tray_items(shell);
        shell->tray.dirty_flags = 1;
    }
    if (shell->monitors != nullptr && shell->wallpaper_surface.ops.set_bounds != nullptr) {
        (void)reach_monitor_refresh(shell->monitors);
        size_t monitor_count = reach_monitor_count(shell->monitors);
        if (monitor_count > 0) {
            const reach_monitor_info *monitor = reach_monitor_get(shell->monitors, 0);
            if (monitor != nullptr) {
                int32_t left = monitor->bounds.left;
                int32_t top = monitor->bounds.top;
                int32_t right = monitor->bounds.right;
                int32_t bottom = monitor->bounds.bottom;
                for (size_t index = 1; index < monitor_count; ++index) {
                    monitor = reach_monitor_get(shell->monitors, index);
                    if (monitor == nullptr) {
                        continue;
                    }
                    if (monitor->bounds.left < left) left = monitor->bounds.left;
                    if (monitor->bounds.top < top) top = monitor->bounds.top;
                    if (monitor->bounds.right > right) right = monitor->bounds.right;
                    if (monitor->bounds.bottom > bottom) bottom = monitor->bounds.bottom;
                }

                reach_rect_f32 wallpaper_bounds = {};
                wallpaper_bounds.x = (float)left;
                wallpaper_bounds.y = (float)top;
                wallpaper_bounds.width = (float)(right - left);
                wallpaper_bounds.height = (float)(bottom - top);
                (void)shell->wallpaper_surface.ops.set_bounds(shell->wallpaper_surface.surface, wallpaper_bounds);
            }
        }
    }
    if (shell->launcher.window.ops.set_bounds != nullptr && shell->monitors != nullptr && reach_monitor_count(shell->monitors) > 0) {
        const reach_monitor_info *monitor = reach_monitor_primary(shell->monitors);
        REACH_ASSERT(monitor != nullptr);
        REACH_ASSERT(monitor->primary || reach_monitor_count(shell->monitors) == 1);
        if (monitor == nullptr) {
            return REACH_ERROR;
        }
        reach_rect_f32 bounds = {};
        bounds.x = (float)monitor->bounds.left;
        bounds.y = (float)monitor->bounds.top;
        bounds.width = (float)(monitor->bounds.right - monitor->bounds.left);
        bounds.height = (float)(monitor->bounds.bottom - monitor->bounds.top);

        reach_result result = REACH_OK;
        int32_t launcher_window_changed = 0;
        if (shell->launcher.renderer.ops.begin_frame != nullptr) {
            reach_ui_layout_input input = {};
            input.monitor_bounds = bounds;
            input.work_area = bounds;
            input.dpi_scale = 1.0f;
            reach_ui_layout layout = {};
            reach_render_command_buffer commands = {};
            if (reach_ui_layout_compute(&shell->ui, &input, &layout) == REACH_OK &&
                reach_ui_build_render_commands(&shell->ui, &layout, &commands) == REACH_OK) {
                reach_rect_f32 shown_dock_bounds = layout.dock.bounds;
                reach_rect_f32 animated_dock_bounds = reach_shell_apply_dock_animation(shell, shown_dock_bounds, bounds, delta_seconds);
                float dock_y_offset = animated_dock_bounds.y - shown_dock_bounds.y;
                layout.dock.bounds = animated_dock_bounds;
                for (size_t index = 0; index < layout.dock.app_slot_count; ++index) {
                    layout.dock.app_slots[index].y += dock_y_offset;
                }
                layout.dock.tray_button.y += dock_y_offset;
                if (shell->dock_items_changed) {
                    reach_shell_rebuild_dock_items_with_animations(shell, &layout.dock);
                    shell->dock_items_changed = 0;
                } else {
                    reach_shell_build_dock_items(shell, &layout.dock);
                }
                reach_shell_apply_dock_width_animation(shell, &layout.dock, delta_seconds);
                float dock_left_offset = bounds.x - layout.dock.bounds.x;
                float dock_right_offset = bounds.x + bounds.width - (layout.dock.bounds.x + layout.dock.bounds.width);
                float dock_x_offset = 0.0f;
                if (dock_left_offset > 0.0f) {
                    dock_x_offset = dock_left_offset;
                } else if (dock_right_offset < 0.0f) {
                    dock_x_offset = dock_right_offset;
                }
                if (dock_x_offset != 0.0f) {
                    layout.dock.bounds.x += dock_x_offset;
                    for (size_t index = 0; index < layout.dock.app_slot_count; ++index) {
                        layout.dock.app_slots[index].x += dock_x_offset;
                    }
                    layout.dock.tray_button.x += dock_x_offset;
                }
                int32_t dock_layout_changed = !shell->has_layout || !reach_shell_rect_equal(shell->layout.dock.bounds, layout.dock.bounds);
                int32_t launcher_layout_changed = !shell->has_layout || !reach_shell_rect_equal(shell->layout.launcher.bounds, layout.launcher.bounds);
                shell->layout = layout;
                shell->has_layout = 1;
                result = reach_shell_apply_window_state(
                    &shell->launcher.window,
                    layout.launcher.bounds,
                    shell->ui.launcher.open ? 1.0f : 0.0f,
                    &shell->launcher.last_bounds,
                    &shell->launcher.last_opacity,
                    &shell->launcher.bounds_valid,
                    &shell->launcher.opacity_valid,
                    &launcher_window_changed);
                if (result != REACH_OK) {
                    return result;
                }
                if (shell->ui.launcher.open && (shell->render_dirty || shell->launcher.dirty_flags || launcher_window_changed || launcher_layout_changed)) {
                    (void)reach_shell_render_launcher_surface(shell, &layout.launcher);
                }
                if (shell->dock.window.ops.set_bounds != nullptr) {
                    int32_t dock_window_changed = 0;
                    float dock_radius = reach_theme_dock_corner_radius(shell->theme, layout.dock.bounds.height);
                    result = reach_shell_apply_window_state(
                        &shell->dock.window,
                        layout.dock.bounds,
                        1.0f,
                        &shell->dock.last_bounds,
                        &shell->dock.last_opacity,
                        &shell->dock.bounds_valid,
                        &shell->dock.opacity_valid,
                        &dock_window_changed);
                    if (result != REACH_OK) {
                        return result;
                    }
                    if (dock_window_changed && shell->dock.window.ops.apply_rounded_corners != nullptr) {
                        (void)shell->dock.window.ops.apply_rounded_corners(shell->dock.window.window, dock_radius);
                    }
                    if (shell->render_dirty || shell->dock.dirty_flags || dock_window_changed || dock_layout_changed) {
                        (void)reach_shell_render_dock_surface(shell, &layout.dock);
                    }
                }
                if (shell->tray.window.ops.set_bounds != nullptr) {
                    const reach_theme *theme = shell->theme != nullptr ? shell->theme : reach_theme_default();
                    reach_rect_f32 tray_bounds = {};
                    reach_shell_compute_tray_popup_layout(shell, &layout.dock, &tray_bounds, shell->tray_model.item_slots);
                    int32_t tray_window_changed = 0;
                    result = reach_shell_apply_window_state(
                        &shell->tray.window,
                        tray_bounds,
                        shell->tray_popup_open ? 1.0f : 0.0f,
                        &shell->tray.last_bounds,
                        &shell->tray.last_opacity,
                        &shell->tray.bounds_valid,
                        &shell->tray.opacity_valid,
                        &tray_window_changed);
                    if (result != REACH_OK) {
                        return result;
                    }
                    if (tray_window_changed && shell->tray.window.ops.apply_rounded_corners != nullptr) {
                        (void)shell->tray.window.ops.apply_rounded_corners(shell->tray.window.window, theme->tray_popup_corner_radius);
                    }
                    if (shell->tray_popup_open) {
                        if (shell->tray.window.ops.show != nullptr) {
                            (void)shell->tray.window.ops.show(shell->tray.window.window);
                        }
                        if (shell->render_dirty || shell->tray.dirty_flags || tray_window_changed) {
                            (void)reach_shell_render_tray_surface(shell, tray_bounds);
                        }
                    } else if (shell->tray.window.ops.hide != nullptr) {
                        (void)shell->tray.window.ops.hide(shell->tray.window.window);
                    }
                }
                if (shell->switcher.window.ops.set_bounds != nullptr) {
                    reach_rect_f32 switcher_bounds = reach_shell_switcher_bounds_for_count(bounds, reach_shell_switcher_visible_count(shell));
                    int32_t switcher_window_changed = 0;
                    result = reach_shell_apply_window_state(
                        &shell->switcher.window,
                        switcher_bounds,
                        shell->switcher_open ? 1.0f : 0.0f,
                        &shell->switcher.last_bounds,
                        &shell->switcher.last_opacity,
                        &shell->switcher.bounds_valid,
                        &shell->switcher.opacity_valid,
                        &switcher_window_changed);
                    if (result != REACH_OK) {
                        return result;
                    }
                    if (switcher_window_changed && shell->switcher.window.ops.apply_rounded_corners != nullptr) {
                        (void)shell->switcher.window.ops.apply_rounded_corners(shell->switcher.window.window, 16.0f);
                    }
                    if (shell->switcher_open) {
                        if (shell->switcher.window.ops.show != nullptr) {
                            (void)shell->switcher.window.ops.show(shell->switcher.window.window);
                        }
                        if (shell->render_dirty || shell->switcher.dirty_flags || switcher_window_changed) {
                            (void)reach_shell_render_switcher_surface(shell, switcher_bounds);
                        }
                    } else if (shell->switcher.window.ops.hide != nullptr) {
                        (void)shell->switcher.window.ops.hide(shell->switcher.window.window);
                    }
                }
                if (shell->context_menu.window.ops.set_bounds != nullptr) {
                    int32_t context_window_changed = 0;
                    result = reach_shell_apply_window_state(
                        &shell->context_menu.window,
                        shell->context_menu_bounds,
                        shell->context_menu_open ? 1.0f : 0.0f,
                        &shell->context_menu.last_bounds,
                        &shell->context_menu.last_opacity,
                        &shell->context_menu.bounds_valid,
                        &shell->context_menu.opacity_valid,
                        &context_window_changed);
                    if (result != REACH_OK) {
                        return result;
                    }
                    if (context_window_changed && shell->context_menu.window.ops.apply_rounded_corners != nullptr) {
                        (void)shell->context_menu.window.ops.apply_rounded_corners(shell->context_menu.window.window, 14.0f);
                    }
                    if (shell->context_menu_open) {
                        if (shell->context_menu.window.ops.show != nullptr) {
                            (void)shell->context_menu.window.ops.show(shell->context_menu.window.window);
                        }
                        if (shell->render_dirty || shell->context_menu.dirty_flags || context_window_changed) {
                            (void)reach_shell_render_context_menu_surface(shell);
                        }
                    } else if (shell->context_menu.window.ops.hide != nullptr) {
                        (void)shell->context_menu.window.ops.hide(shell->context_menu.window.window);
                    }
                }
                if (shell->ui.launcher.open) {
                    reach_shell_raise_launcher(shell);
                }
            }
        }
    }
    shell->layout_dirty = 0;
    shell->render_dirty = 0;
    shell->dock.dirty_flags = 0;
    shell->launcher.dirty_flags = 0;
    shell->tray.dirty_flags = 0;
    shell->switcher.dirty_flags = 0;
    shell->context_menu.dirty_flags = 0;

    return REACH_OK;
}

int32_t reach_shell_needs_frame(const reach_shell *shell)
{
    int32_t dock_item_animating = 0;
    if (shell != nullptr) {
        for (size_t index = 0; index < shell->dock_model.item_count; ++index) {
            if (shell->dock_item_x_animating[index]) {
                dock_item_animating = 1;
                break;
            }
        }
    }
    return shell != nullptr &&
        (shell->render_dirty ||
         shell->dock.dirty_flags ||
         shell->launcher.dirty_flags ||
         shell->tray.dirty_flags ||
         shell->switcher.dirty_flags ||
         shell->context_menu.dirty_flags ||
         shell->switcher_open ||
         shell->context_menu_open ||
         shell->dock_animating ||
         shell->dock_width_animating ||
         shell->dock_drag_active ||
         shell->dock_drag_snapping ||
         dock_item_animating ||
         shell->dock_click_feedback_animating ||
         shell->tray_click_feedback_animating ||
         (shell->ui.dock.auto_hide &&
             (shell->dock_target_hidden ||
              reach_shell_should_auto_hide_dock(shell))));
}
