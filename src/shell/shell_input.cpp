#include "shell_internal.h"

#include <math.h>

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

static void reach_shell_handle_global_mouse_down(reach_shell *shell, reach_point_i32 point);
static void reach_shell_close_launcher(reach_shell *shell);

static void reach_shell_on_popup_mouse_down(void *user, int32_t x, int32_t y)
{
    reach_shell *shell = static_cast<reach_shell *>(user);
    if (shell == nullptr) {
        return;
    }
    reach_point_i32 point = { x, y };
    reach_shell_handle_global_mouse_down(shell, point);
}

void reach_shell_raise_launcher(reach_shell *shell)
{
    if (shell == nullptr || shell->launcher.window.ops.raise == nullptr) {
        return;
    }

    (void)shell->launcher.window.ops.raise(shell->launcher.window.window);
}

void reach_shell_notify_launcher_search_ready(reach_shell *shell)
{
    if (shell == nullptr || shell->launcher.window.ops.post_event == nullptr) {
        return;
    }

    (void)shell->launcher.window.ops.post_event(
        shell->launcher.window.window,
        REACH_UI_EVENT_LAUNCHER_SEARCH_READY);
}


static void reach_shell_capture_tray_input(reach_shell *shell)
{
    if (shell == nullptr ||
        shell->popup_capture.begin_capture == nullptr) {
        return;
    }

    if (shell->tray.window.window != nullptr) {
        (void)shell->popup_capture.begin_capture(
            shell->popup_capture.userdata,
            shell->tray.window.window);
    }
}

static void reach_shell_release_tray_input(reach_shell *shell)
{
    if (shell == nullptr ||
        shell->popup_capture.end_capture == nullptr) {
        return;
    }

    if (shell->tray.window.window != nullptr) {
        shell->popup_capture.end_capture(
            shell->popup_capture.userdata,
            shell->tray.window.window);
    }
}

static void reach_shell_capture_dock_input(reach_shell *shell)
{
    if (shell == nullptr ||
        shell->popup_capture.begin_capture == nullptr) {
        return;
    }

    if (shell->dock.window.window != nullptr) {
        (void)shell->popup_capture.begin_capture(
            shell->popup_capture.userdata,
            shell->dock.window.window);
    }
}

static void reach_shell_release_dock_input(reach_shell *shell)
{
    if (shell == nullptr ||
        shell->popup_capture.end_capture == nullptr) {
        return;
    }

    if (shell->dock.window.window != nullptr) {
        shell->popup_capture.end_capture(
            shell->popup_capture.userdata,
            shell->dock.window.window);
    }
}

static void reach_shell_capture_context_menu_input(reach_shell *shell)
{
    if (shell == nullptr ||
        shell->popup_capture.begin_capture == nullptr) {
        return;
    }

    if (shell->context_menu.window.window != nullptr) {
        (void)shell->popup_capture.begin_capture(
            shell->popup_capture.userdata,
            shell->context_menu.window.window);
    }
}

static void reach_shell_release_context_menu_input(reach_shell *shell)
{
    if (shell == nullptr ||
        shell->popup_capture.end_capture == nullptr) {
        return;
    }

    if (shell->context_menu.window.window != nullptr) {
        shell->popup_capture.end_capture(
            shell->popup_capture.userdata,
            shell->context_menu.window.window);
    }
}

static void reach_shell_handle_global_mouse_down(reach_shell *shell, reach_point_i32 point)
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
    int32_t on_quick_settings = shell->quick_settings_open &&
        (float)point.x >= shell->quick_settings_bounds.x &&
        (float)point.x <= shell->quick_settings_bounds.x + shell->quick_settings_bounds.width &&
        (float)point.y >= shell->quick_settings_bounds.y &&
        (float)point.y <= shell->quick_settings_bounds.y + shell->quick_settings_bounds.height;
    int32_t on_launcher = shell->ui.launcher.open &&
        (float)point.x >= shell->layout.launcher.bounds.x &&
        (float)point.x <= shell->layout.launcher.bounds.x + shell->layout.launcher.bounds.width &&
        (float)point.y >= shell->layout.launcher.bounds.y &&
        (float)point.y <= shell->layout.launcher.bounds.y + shell->layout.launcher.bounds.height;
    reach_dock_hit_result dock_hit = shell->has_layout ? reach_dock_hit_test(&shell->layout.dock, point.x, point.y) : reach_dock_hit_result {};
    int32_t on_tray_button = shell->tray_popup_open && dock_hit.type == REACH_DOCK_HIT_TRAY_BUTTON;
    int32_t on_quick_settings_button = shell->quick_settings_open && dock_hit.type == REACH_DOCK_HIT_QUICK_SETTINGS_BUTTON;
    int32_t on_power_button = shell->context_menu_open && shell->context_menu_power_open && dock_hit.type == REACH_DOCK_HIT_POWER_BUTTON;

    if (shell->tray_popup_open && !on_tray && !on_tray_button) {
        reach_shell_set_tray_popup_open(shell, 0);
    }
    if (shell->context_menu_open && !on_context && !on_power_button) {
        reach_shell_close_context_menu(shell);
        reach_shell_clear_sticky_dock_feedback(shell);
    }
    if (shell->quick_settings_open && !on_quick_settings && !on_quick_settings_button) {
        reach_shell_set_quick_settings_open(shell, 0);
        reach_shell_clear_sticky_dock_feedback(shell);
    }
    if (shell->ui.launcher.open && !on_launcher) {
        reach_shell_close_launcher(shell);
    }
}

void reach_shell_sync_popup_mouse_hook(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }

    if (shell->popup_capture.sync_mouse_hook != nullptr) {
        int32_t should_hook = shell->tray_popup_open || shell->context_menu_open || shell->quick_settings_open || shell->ui.launcher.open;
        (void)shell->popup_capture.sync_mouse_hook(
            shell->popup_capture.userdata,
            should_hook,
            reach_shell_on_popup_mouse_down,
            shell);
    }
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
        reach_shell_set_quick_settings_open(shell, 0);
        reach_shell_close_context_menu(shell);
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
    if (shell == nullptr || index > REACH_SHELL_DOCK_FEEDBACK_POWER_BUTTON) {
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
    if (shell == nullptr || index > REACH_SHELL_DOCK_FEEDBACK_POWER_BUTTON) {
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

static void reach_shell_press_quick_settings_button(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }
    shell->dock_click_feedback_pressed = 1;
    shell->dock_click_feedback_sticky = 0;
    reach_shell_start_dock_click_feedback(shell, REACH_SHELL_DOCK_FEEDBACK_QUICK_SETTINGS_BUTTON, 0.50f);
}

static void reach_shell_close_launcher(reach_shell *shell)
{
    if (shell == nullptr || !shell->ui.launcher.open) {
        return;
    }

    reach_shell_cancel_launcher_search(shell);
    reach_shell_release_launcher_result_icons(shell);
    (void)reach_ui_state_close_launcher(&shell->ui);
    shell->pressed_launcher_hit_type = REACH_LAUNCHER_HIT_NONE;
    shell->pressed_launcher_index = REACH_MAX_PINNED_APPS;
    shell->layout_dirty = 1;
    shell->launcher.dirty_flags = 1;
    reach_shell_sync_popup_mouse_hook(shell);
    if (shell->launcher.window.ops.hide != nullptr) {
        (void)shell->launcher.window.ops.hide(shell->launcher.window.window);
    }
}

static void reach_shell_press_power_button(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }
    shell->dock_click_feedback_pressed = 1;
    shell->dock_click_feedback_sticky = 0;
    reach_shell_start_dock_click_feedback(shell, REACH_SHELL_DOCK_FEEDBACK_POWER_BUTTON, 0.50f);
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
    case REACH_UI_EVENT_DELETE:
    case REACH_UI_EVENT_ENTER:
    case REACH_UI_EVENT_ARROW_UP:
    case REACH_UI_EVENT_ARROW_DOWN:
    case REACH_UI_EVENT_ARROW_LEFT:
    case REACH_UI_EVENT_ARROW_RIGHT:
    case REACH_UI_EVENT_HOME:
    case REACH_UI_EVENT_END:
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

    if (shell->ui.launcher.result_count > 0 &&
        shell->ui.launcher.selected_result_index < shell->ui.launcher.result_count) {
        const reach_search_candidate *result = &shell->ui.launcher.results[shell->ui.launcher.selected_result_index];
        if (result->path[0] == 0) {
            return REACH_OK;
        }
        if (result->kind == REACH_SEARCH_RESULT_APP && shell->app_launcher.ops.launch != nullptr) {
            reach_app_launch_request request = {};
            reach_copy_utf16(request.path, 260, result->path);
            return shell->app_launcher.ops.launch(shell->app_launcher.launcher, &request);
        }
        if (shell->explorer_service.ops.open_path != nullptr) {
            return shell->explorer_service.ops.open_path(shell->explorer_service.service, result->path);
        }
        return REACH_OK;
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

    if (shell->explorer_service.ops.path_exists != nullptr &&
        shell->explorer_service.ops.path_exists(shell->explorer_service.service, query) &&
        shell->explorer_service.ops.open_path != nullptr) {
        return shell->explorer_service.ops.open_path(shell->explorer_service.service, query);
    }

    if (shell->explorer_service.ops.open_default != nullptr) {
        return shell->explorer_service.ops.open_default(shell->explorer_service.service);
    }
    return REACH_OK;
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
    if (shell->dock_model.items[item_index].pinned &&
        shell->dock_model.items[item_index].pinned_index < shell->ui.pinned_app_count) {
        reach_copy_utf16(
            request.arguments,
            260,
            shell->ui.pinned_apps[shell->dock_model.items[item_index].pinned_index].arguments);
    }
    request.force_new_instance = force_new_instance;
    return shell->app_launcher.ops.launch(shell->app_launcher.launcher, &request);
}

void reach_shell_close_context_menu(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }
    shell->context_menu_open = 0;
    shell->context_menu_power_open = 0;
    shell->context_menu_target_index = REACH_MAX_PINNED_APPS;
    shell->context_menu_hovered_index = REACH_MAX_PINNED_APPS;
    shell->context_menu_item_count = 0;
    for (size_t index = 0; index < REACH_CONTEXT_MENU_MAX_ITEMS; ++index) {
        shell->context_menu_item_icon_ids[index] = 0;
    }
    shell->context_menu.dirty_flags = 1;
    reach_shell_release_context_menu_input(shell);
    if (shell->context_menu.window.ops.set_pointer_move_enabled != nullptr) {
        (void)shell->context_menu.window.ops.set_pointer_move_enabled(
            shell->context_menu.window.window,
            0);
    }
    if (shell->context_menu.window.ops.hide != nullptr) {
        (void)shell->context_menu.window.ops.hide(shell->context_menu.window.window);
    }
    reach_shell_sync_popup_mouse_hook(shell);
}

static reach_result reach_shell_execute_context_command(reach_shell *shell, uint32_t command)
{
    if (shell == nullptr) {
        return REACH_OK;
    }

    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_LOCK) {
        reach_shell_close_context_menu(shell);
        return shell->power_session.ops.lock != nullptr
            ? shell->power_session.ops.lock(shell->power_session.session)
            : REACH_ERROR;
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_SLEEP) {
        reach_shell_close_context_menu(shell);
        return shell->power_session.ops.sleep != nullptr
            ? shell->power_session.ops.sleep(shell->power_session.session)
            : REACH_ERROR;
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_RESTART) {
        reach_shell_close_context_menu(shell);
        return shell->power_session.ops.restart != nullptr
            ? shell->power_session.ops.restart(shell->power_session.session)
            : REACH_ERROR;
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_SHUTDOWN) {
        reach_shell_close_context_menu(shell);
        return shell->power_session.ops.shutdown != nullptr
            ? shell->power_session.ops.shutdown(shell->power_session.session)
            : REACH_ERROR;
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_SIGN_OUT) {
        reach_shell_close_context_menu(shell);
        return shell->power_session.ops.sign_out != nullptr
            ? shell->power_session.ops.sign_out(shell->power_session.session)
            : REACH_ERROR;
    }

    if (shell->context_menu_target_index >= shell->dock_model.item_count) {
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
        if (pinned_index < shell->ui.pinned_app_count) {
            reach_copy_utf16(request.arguments, 260, shell->ui.pinned_apps[pinned_index].arguments);
        }
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
        reach_pinned_app_model app = {};

        size_t open_index = shell->dock_model.items[item_index].open_index;
        if (open_index < shell->open_window_count) {
            const reach_window_snapshot *window = &shell->open_windows[open_index];

            if (shell->window_manager.ops.pin_app_for_window != nullptr &&
                shell->window_manager.ops.pin_app_for_window(
                    shell->window_manager.manager,
                    window->id,
                    window,
                    &app) == REACH_OK) {
            } else {
                if (window->title[0] != 0) {
                    (void)reach_copy_utf16(app.title, 128, window->title);
                }
                (void)reach_copy_utf16(app.path, 260, window->path);
                (void)reach_copy_utf16(app.icon_ref, 260, window->path);
                (void)reach_copy_utf16(app.app_user_model_id, 260, window->app_user_model_id);
            }
        } else if (item_path[0] != 0) {
            (void)reach_copy_utf16(app.path, 260, item_path);
            (void)reach_copy_utf16(app.icon_ref, 260, item_path);
        }

        if (app.path[0] != 0 &&
            reach_pin_config_pin_app(&shell->config_store, &app) == REACH_OK) {
            return reach_shell_reload_pins(shell);
        }

        return REACH_ERROR;
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_CLOSE && shell->window_manager.ops.close != nullptr) {
        return shell->window_manager.ops.close(shell->window_manager.manager, window_id);
    }

    return REACH_OK;
}

static reach_result reach_shell_show_power_context_menu(reach_shell *shell)
{
    if (shell == nullptr || !shell->has_layout) {
        return REACH_INVALID_ARGUMENT;
    }
    reach_shell_set_quick_settings_open(shell, 0);
    reach_shell_set_tray_popup_open(shell, 0);

    reach_context_menu_build_power_commands(
        shell->context_menu_item_commands,
        shell->context_menu_item_icon_ids,
        &shell->context_menu_item_count);

    float popup_width = 176.0f;
    float item_height = 34.0f;
    float padding = 8.0f;
    float notch_height = reach_popup_notch_height();
    float popup_body_height = padding * 2.0f + item_height * (float)shell->context_menu_item_count;
    float popup_height = popup_body_height + notch_height;
    float anchor_x = shell->layout.dock.power_button.x + shell->layout.dock.power_button.width * 0.5f;
    float popup_x = anchor_x - popup_width * 0.72f;
    float popup_y = shell->layout.dock.bounds.y - popup_height - 8.0f;

    reach_rect_f32 monitor = {};
    const reach_monitor_info *primary_monitor =
        shell->monitors.list != nullptr && shell->monitors.ops.primary != nullptr
            ? shell->monitors.ops.primary(shell->monitors.list)
            : nullptr;
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

    shell->context_menu_bounds = { popup_x, popup_y, popup_width, popup_height };
    for (size_t index = 0; index < shell->context_menu_item_count; ++index) {
        shell->context_menu_item_slots[index] = {
            popup_x + padding,
            popup_y + padding + item_height * (float)index,
            popup_width - padding * 2.0f,
            item_height
        };
    }
    shell->context_menu_target_index = REACH_MAX_PINNED_APPS;
    shell->context_menu_hovered_index = REACH_MAX_PINNED_APPS;
    shell->context_menu_power_open = 1;
    shell->context_menu_open = 1;
    shell->context_menu.dirty_flags = 1;
    if (shell->context_menu.window.ops.set_pointer_move_enabled != nullptr) {
        (void)shell->context_menu.window.ops.set_pointer_move_enabled(
            shell->context_menu.window.window,
            1);
    }
    reach_shell_capture_context_menu_input(shell);
    reach_shell_sync_popup_mouse_hook(shell);
    return REACH_OK;
}

static reach_result reach_shell_show_dock_app_context_menu(reach_shell *shell, size_t item_index, int32_t x, int32_t y)
{
    (void)x;
    (void)y;
    if (shell == nullptr || item_index >= shell->dock_model.item_count) {
        return REACH_INVALID_ARGUMENT;
    }
    reach_shell_set_quick_settings_open(shell, 0);
    reach_shell_set_tray_popup_open(shell, 0);

    reach_context_menu_build_dock_item_commands(
        shell->dock_model.items[item_index].pinned,
        reach_shell_dock_item_path(shell, item_index) != nullptr,
        shell->dock_model.items[item_index].window != 0,
        shell->context_menu_item_commands,
        &shell->context_menu_item_count);
    for (size_t index = 0; index < REACH_CONTEXT_MENU_MAX_ITEMS; ++index) {
        shell->context_menu_item_icon_ids[index] = 0;
    }

    float popup_width = 208.0f;
    float item_height = 34.0f;
    float padding = 8.0f;
    float notch_height = reach_popup_notch_height();
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
        const reach_monitor_info *primary_monitor =
            shell->monitors.list != nullptr && shell->monitors.ops.primary != nullptr
                ? shell->monitors.ops.primary(shell->monitors.list)
                : nullptr;
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
    shell->context_menu_power_open = 0;
    shell->context_menu_open = 1;
    shell->context_menu.dirty_flags = 1;
    if (shell->context_menu.window.ops.set_pointer_move_enabled != nullptr) {
        (void)shell->context_menu.window.ops.set_pointer_move_enabled(
            shell->context_menu.window.window,
            1);
    }
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
            (void)reach_shell_refresh_open_windows(shell, nullptr);
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
            (void)reach_shell_refresh_open_windows(shell, nullptr);
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

    if (shell->dock.window.ops.set_pointer_move_enabled != nullptr) {
        (void)shell->dock.window.ops.set_pointer_move_enabled(
            shell->dock.window.window,
            0);
    }

    if (shell->dock_drag_active) {
        uint32_t pin_id = shell->dock_drag_pin_id;
        int32_t dragged_pinned = shell->dock_drag_pinned;
        int32_t moved = shell->dock_drag_moved;
        size_t pressed_dock_index = shell->pressed_dock_index;
        size_t target_pinned_index = dragged_pinned ? reach_shell_pinned_order_index(shell, pin_id) : REACH_MAX_PINNED_APPS;
        size_t target_index = reach_shell_find_dock_item_key(shell, shell->dock_drag_pinned, shell->dock_drag_pin_id, shell->dock_drag_window);
        shell->dock_drag_active = 0;
        shell->dock_drag_moved = 0;
        shell->pressed_dock_index = moved ? REACH_MAX_PINNED_APPS : pressed_dock_index;
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

    if (shell->quick_settings_dragging_volume) {
        shell->quick_settings_dragging_volume = 0;
        shell->quick_settings_drag_type = REACH_QUICK_SETTINGS_HIT_NONE;
        shell->quick_settings_drag_level_valid = 0;
        shell->quick_settings_drag_session_index = 0;
        shell->quick_settings_drag_session_instance_id[0] = 0;

        if (shell->quick_settings.window.ops.set_pointer_move_enabled != nullptr) {
            (void)shell->quick_settings.window.ops.set_pointer_move_enabled(
                shell->quick_settings.window.window,
                0);
        }

        return REACH_OK;
    }

    if (shell->ui.launcher.open) {
        reach_launcher_hit_result launcher_hit = reach_launcher_hit_test(&shell->ui, &shell->layout.launcher, event->x, event->y);
        reach_launcher_action launcher_action = reach_launcher_action_for_hit(&shell->ui, launcher_hit);
        int32_t launcher_pressed_match =
            shell->pressed_launcher_hit_type == launcher_hit.type &&
            shell->pressed_launcher_index == launcher_hit.index;
        shell->pressed_launcher_hit_type = REACH_LAUNCHER_HIT_NONE;
        shell->pressed_launcher_index = REACH_MAX_PINNED_APPS;
        if (launcher_action.type == REACH_LAUNCHER_ACTION_LAUNCH_PINNED && launcher_pressed_match) {
            reach_ui_event routed = {};
            routed.type = REACH_UI_EVENT_DOCK_APP_CLICK;
            routed.id = launcher_action.pin_id;
            return reach_shell_handle_event(shell, &routed);
        }
        if (launcher_action.type == REACH_LAUNCHER_ACTION_OPEN_RESULT && launcher_pressed_match) {
            (void)reach_shell_open_launcher_result(shell);
            reach_shell_close_launcher(shell);
            return REACH_OK;
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

    if (dock_hit.type == REACH_DOCK_HIT_QUICK_SETTINGS_BUTTON) {
        reach_shell_toggle_quick_settings(shell);
        return REACH_OK;
    }

    if (dock_hit.type == REACH_DOCK_HIT_POWER_BUTTON) {
        if (shell->suppress_power_button_release) {
            shell->suppress_power_button_release = 0;
            return REACH_OK;
        }
        if (shell->context_menu_open && shell->context_menu_power_open) {
            reach_shell_close_context_menu(shell);
            return REACH_OK;
        }
        return reach_shell_show_power_context_menu(shell);
    }

    if (shell->tray_popup_open && !reach_rect_contains(shell->tray.last_bounds, event->x, event->y)) {
        reach_shell_set_tray_popup_open(shell, 0);
        if (shell->tray.window.ops.hide != nullptr) {
            (void)shell->tray.window.ops.hide(shell->tray.window.window);
        }
    }

    if (dock_hit.type == REACH_DOCK_HIT_ITEM && shell->pressed_dock_index == dock_hit.index) {
        shell->pressed_dock_index = REACH_MAX_PINNED_APPS;
        return reach_shell_execute_dock_item_action(shell, reach_dock_item_action_for_index(&shell->dock_model, dock_hit.index));
    }
    shell->pressed_dock_index = REACH_MAX_PINNED_APPS;

    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_down(reach_shell *shell, const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->has_layout) {
        return REACH_OK;
    }

    reach_shell_clear_sticky_dock_feedback(shell);

    if (shell->quick_settings_open) {
        reach_dock_hit_result dock_hit = reach_dock_hit_test(&shell->layout.dock, event->x, event->y);
        if (dock_hit.type == REACH_DOCK_HIT_QUICK_SETTINGS_BUTTON) {
            reach_shell_press_quick_settings_button(shell);
            return REACH_OK;
        }

        if (reach_rect_contains(shell->quick_settings_bounds, event->x, event->y)) {
            reach_quick_settings_hit_result hit = reach_quick_settings_hit_test(
                &shell->quick_settings_layout,
                &shell->quick_settings_model,
                (float)event->x - shell->quick_settings_bounds.x,
                (float)event->y - shell->quick_settings_bounds.y);
            reach_quick_settings_action action = reach_quick_settings_action_for_hit(hit);
            if (action.type != REACH_QUICK_SETTINGS_ACTION_NONE) {
                shell->quick_settings_dragging_volume =
                    action.type == REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME ||
                    action.type == REACH_QUICK_SETTINGS_ACTION_SET_SESSION_VOLUME ||
                    action.type == REACH_QUICK_SETTINGS_ACTION_SET_BRIGHTNESS;

                if (shell->quick_settings_dragging_volume &&
                    shell->quick_settings.window.ops.set_pointer_move_enabled != nullptr) {
                    (void)shell->quick_settings.window.ops.set_pointer_move_enabled(
                        shell->quick_settings.window.window,
                        1);
                }

                shell->quick_settings_drag_type = hit.type;
                shell->quick_settings_drag_last_level = action.volume_level;
                shell->quick_settings_drag_level_valid = shell->quick_settings_dragging_volume;
                shell->quick_settings_drag_session_index = hit.session_index;
                reach_copy_utf16(
                    shell->quick_settings_drag_session_instance_id,
                    REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY,
                    hit.session_instance_id);
                reach_shell_execute_quick_settings_action(shell, action);
            }
            return REACH_OK;
        }

        reach_shell_set_quick_settings_open(shell, 0);
        return REACH_OK;
    }

    if (shell->context_menu_open) {
        reach_dock_hit_result dock_hit = reach_dock_hit_test(&shell->layout.dock, event->x, event->y);
        if (shell->context_menu_power_open && dock_hit.type == REACH_DOCK_HIT_POWER_BUTTON) {
            reach_shell_close_context_menu(shell);
            reach_shell_clear_sticky_dock_feedback(shell);
            shell->suppress_power_button_release = 1;
            return REACH_OK;
        }
        if (!reach_rect_contains(shell->context_menu_bounds, event->x, event->y)) {
            reach_shell_close_context_menu(shell);
        } else {
            reach_shell_capture_context_menu_input(shell);
        }
        return REACH_OK;
    }

    if (shell->ui.launcher.open) {
        reach_launcher_hit_result launcher_hit = reach_launcher_hit_test(&shell->ui, &shell->layout.launcher, event->x, event->y);
        if (launcher_hit.type == REACH_LAUNCHER_HIT_NONE &&
            !reach_rect_contains(shell->layout.launcher.bounds, event->x, event->y)) {
            reach_shell_close_launcher(shell);
            return REACH_OK;
        }
        if (launcher_hit.type == REACH_LAUNCHER_HIT_SEARCH_RESULT &&
            launcher_hit.index < shell->ui.launcher.result_count) {
            shell->ui.launcher.selected_result_index = launcher_hit.index;
            shell->launcher.dirty_flags = 1;
        }
        shell->pressed_launcher_hit_type = launcher_hit.type;
        shell->pressed_launcher_index = launcher_hit.index;
        return REACH_OK;
    }

    reach_dock_hit_result dock_hit = reach_dock_hit_test(&shell->layout.dock, event->x, event->y);
    if (shell->suppress_power_button_release && dock_hit.type != REACH_DOCK_HIT_POWER_BUTTON) {
        shell->suppress_power_button_release = 0;
    }
    if (dock_hit.type == REACH_DOCK_HIT_TRAY_BUTTON) {
        reach_shell_press_tray_button(shell);
        return REACH_OK;
    }

    if (dock_hit.type == REACH_DOCK_HIT_QUICK_SETTINGS_BUTTON) {
        reach_shell_press_quick_settings_button(shell);
        return REACH_OK;
    }

    if (dock_hit.type == REACH_DOCK_HIT_POWER_BUTTON) {
        reach_shell_press_power_button(shell);
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
        shell->pressed_dock_index = index;
        reach_shell_press_dock_item(shell, index);
        if (index < shell->dock_model.item_count) {
            shell->dock_drag_active = 1;
            if (shell->dock.window.ops.set_pointer_move_enabled != nullptr) {
                (void)shell->dock.window.ops.set_pointer_move_enabled(
                    shell->dock.window.window,
                    1);
            }
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
    shell->pressed_dock_index = REACH_MAX_PINNED_APPS;
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

    if (shell->quick_settings_dragging_volume) {
        reach_shell_request_update(shell);
        reach_rect_f32 track = shell->quick_settings_layout.main_slider_track;
        if (shell->quick_settings_drag_type == REACH_QUICK_SETTINGS_HIT_SESSION_SLIDER &&
            shell->quick_settings_drag_session_index < shell->quick_settings_layout.app_volume_row_count) {
            track = shell->quick_settings_layout
                .app_volume_rows[shell->quick_settings_drag_session_index]
                .slider_full_range_line;
        } else if (shell->quick_settings_drag_type == REACH_QUICK_SETTINGS_HIT_BRIGHTNESS_SLIDER) {
            track = shell->quick_settings_layout.brightness_slider_track;
        }
        if (track.width > 0.0f) {
            float local_x = (float)event->x - shell->quick_settings_bounds.x;
            float next_level = reach_shell_clamp_float((local_x - track.x) / track.width, 0.0f, 1.0f);
            if (shell->quick_settings_drag_level_valid &&
                fabsf(next_level - shell->quick_settings_drag_last_level) < 0.005f) {
                return REACH_OK;
            }
            shell->quick_settings_drag_last_level = next_level;
            shell->quick_settings_drag_level_valid = 1;

            reach_quick_settings_action action = {};
            if (shell->quick_settings_drag_type == REACH_QUICK_SETTINGS_HIT_SESSION_SLIDER) {
                action.type = REACH_QUICK_SETTINGS_ACTION_SET_SESSION_VOLUME;
            } else if (shell->quick_settings_drag_type == REACH_QUICK_SETTINGS_HIT_BRIGHTNESS_SLIDER) {
                action.type = REACH_QUICK_SETTINGS_ACTION_SET_BRIGHTNESS;
            } else {
                action.type = REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME;
            }
            action.volume_level = next_level;
            action.session_index = shell->quick_settings_drag_session_index;
            reach_copy_utf16(
                action.session_instance_id,
                REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY,
                shell->quick_settings_drag_session_instance_id);
            reach_shell_execute_quick_settings_action(shell, action);
        }
        return REACH_OK;
    }

    if (shell->dock_drag_active) {
        reach_shell_request_update(shell);
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
    if (shell == nullptr) {
        return REACH_OK;
    }

    if (shell->ui.dock.auto_hide &&
        (!shell->dock_target_hidden ||
         shell->dock_reveal_active ||
         shell->dock_animating)) {
        shell->dock_reveal_check_dirty = 1;
        reach_shell_request_update(shell);
    }

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
    if (shell->quick_settings_open) {
        reach_shell_set_quick_settings_open(shell, 0);
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
            (void)reach_shell_refresh_open_windows(shell, nullptr);
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
    if (event->type == REACH_UI_EVENT_ESCAPE && shell->quick_settings_open) {
        reach_shell_set_quick_settings_open(shell, 0);
        return REACH_OK;
    }
    if (event->type == REACH_UI_EVENT_ESCAPE && shell->context_menu_open) {
        reach_shell_close_context_menu(shell);
        return REACH_OK;
    }
    int32_t launcher_was_open = shell->ui.launcher.open;
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
    if (event->type == REACH_UI_EVENT_CONFIG_CHANGED) {
        return reach_shell_reload_config(shell);
    }
    if (event->type == REACH_UI_EVENT_LAUNCHER_SEARCH_READY) {
        reach_shell_apply_launcher_search_results(shell);
        return REACH_OK;
    }
    if (event->type == REACH_UI_EVENT_DISPLAY_CHANGED) {
        shell->monitors_dirty = 1;
        shell->layout_dirty = 1;
        shell->dock.dirty_flags = 1;
        shell->launcher.dirty_flags = 1;
        shell->tray.dirty_flags = 1;
        shell->switcher.dirty_flags = 1;
        shell->context_menu.dirty_flags = 1;
        shell->quick_settings.dirty_flags = 1;
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
    if (launcher_was_open != shell->ui.launcher.open) {
        if (!shell->ui.launcher.open) {
            reach_shell_cancel_launcher_search(shell);
            reach_shell_release_launcher_result_icons(shell);
            (void)reach_ui_state_clear_launcher_results(&shell->ui);
        }
        reach_shell_sync_popup_mouse_hook(shell);
    }

    if (intent.type == REACH_UI_INTENT_OPEN_TRAY_MENU) {
        reach_shell_toggle_tray_popup(shell);
    } else if (intent.type == REACH_UI_INTENT_LAUNCH_APP) {
        for (size_t index = 0; index < shell->ui.pinned_app_count; ++index) {
            if (shell->ui.pinned_apps[index].id == intent.id && shell->app_launcher.ops.launch != nullptr) {
                reach_app_launch_request request = {};
                reach_copy_utf16(request.path, 260, shell->ui.pinned_apps[index].path);
                reach_copy_utf16(request.arguments, 260, shell->ui.pinned_apps[index].arguments);
                (void)shell->app_launcher.ops.launch(shell->app_launcher.launcher, &request);
            }
        }
    } else if (intent.type == REACH_UI_INTENT_RUN_SEARCH) {
        (void)reach_shell_schedule_launcher_search(shell);
    } else if (intent.type == REACH_UI_INTENT_OPEN_LAUNCHER_RESULT) {
        (void)reach_shell_open_launcher_result(shell);
        reach_shell_close_launcher(shell);
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
