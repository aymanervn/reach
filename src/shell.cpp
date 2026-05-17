#include "reach/shell.h"

#include "reach/core/render_commands.h"
#include "reach/core/ui_events.h"
#include "reach/core/ui_layout.h"
#include "reach/animation.h"
#include "reach/dock.h"
#include "reach/hotkeys.h"
#include "reach/monitor.h"
#include "reach/platform/windows_adapters.h"
#include "reach/pin_config.h"
#include "reach/wm.h"

#include <windows.h>

#include <math.h>
#include <new>

struct reach_shell {
    reach_dock *dock;
    reach_hotkeys *hotkeys;
    reach_monitor_list *monitors;
    reach_wm *wm;
    reach_ui_state ui;
    reach_platform_window_port launcher_window;
    reach_render_backend_port launcher_renderer;
    reach_platform_window_port dock_window;
    reach_render_backend_port dock_renderer;
    reach_input_source_port input_source;
    reach_window_manager_port window_manager;
    reach_config_store_port config_store;
    reach_tray_provider_port tray_provider;
    reach_search_provider_port search_provider;
    reach_app_launcher_port app_launcher;
    reach_icon_provider_port icon_provider;
    reach_explorer_service_port explorer_service;
    reach_icon_handle pinned_icons[REACH_MAX_PINNED_APPS];
    size_t pinned_icon_count;
    uint16_t pinned_icon_initials[REACH_MAX_PINNED_APPS];
    reach_ui_layout layout;
    int32_t has_layout;
    int32_t layout_dirty;
    int32_t render_dirty;
    int32_t dock_render_dirty;
    int32_t launcher_render_dirty;
    int32_t dock_bounds_valid;
    int32_t launcher_bounds_valid;
    int32_t dock_opacity_valid;
    int32_t launcher_opacity_valid;
    int32_t dock_animation_initialized;
    int32_t dock_animating;
    int32_t dock_target_hidden;
    reach_rect_f32 last_dock_bounds;
    reach_rect_f32 last_launcher_bounds;
    float last_dock_opacity;
    float last_launcher_opacity;
    reach_float_animation dock_y_animation;
    double window_manager_refresh_elapsed;
    reach_float_animation tray_flat_animation;
    int32_t tray_animating;
    int32_t running;
};

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

static float reach_min_float(float a, float b)
{
    return a < b ? a : b;
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

static reach_result reach_shell_load_pinned_icons(reach_shell *shell)
{
    REACH_ASSERT(shell != nullptr);
    if (shell == nullptr || shell->icon_provider.ops.load == nullptr) {
        return REACH_OK;
    }

    for (size_t index = 0; index < shell->pinned_icon_count; ++index) {
        if (shell->icon_provider.ops.release != nullptr && shell->pinned_icons[index].id != 0) {
            (void)shell->icon_provider.ops.release(shell->icon_provider.provider, shell->pinned_icons[index]);
        }
        shell->pinned_icons[index] = {};
    }

    shell->pinned_icon_count = shell->ui.pinned_app_count;
    for (size_t index = 0; index < shell->ui.pinned_app_count; ++index) {
        shell->pinned_icon_initials[index] = shell->ui.pinned_apps[index].title[0] != 0 ? shell->ui.pinned_apps[index].title[0] : '?';
        const uint16_t *icon_path = shell->ui.pinned_apps[index].icon_ref[0] != 0
            ? shell->ui.pinned_apps[index].icon_ref
            : shell->ui.pinned_apps[index].path;
        if (icon_path[0] == 0) {
            continue;
        }

        reach_icon_request request = {};
        request.size_px = (int32_t)shell->ui.dock.icon_size;
        reach_copy_utf16(request.path, 260, icon_path);
        (void)shell->icon_provider.ops.load(shell->icon_provider.provider, &request, &shell->pinned_icons[index]);
    }

    return REACH_OK;
}

static reach_result reach_shell_reload_pins(reach_shell *shell)
{
    if (shell == nullptr || shell->config_store.ops.load == nullptr) {
        return REACH_INVALID_ARGUMENT;
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
    shell->layout_dirty = 1;
    shell->render_dirty = 1;
    shell->dock_render_dirty = 1;
    shell->launcher_render_dirty = 1;
    return result;
}

static int32_t reach_shell_any_window_maximized(reach_shell *shell)
{
    REACH_ASSERT(shell != nullptr);
    if (shell == nullptr) {
        return 0;
    }
    if (shell->window_manager.ops.any_window_is_maximized != nullptr) {
        return shell->window_manager.ops.any_window_is_maximized(shell->window_manager.manager);
    }
    if (shell->window_manager.ops.foreground_is_maximized != nullptr) {
        return shell->window_manager.ops.foreground_is_maximized(shell->window_manager.manager);
    }
    return 0;
}

static reach_rect_f32 reach_shell_apply_dock_animation(reach_shell *shell, reach_rect_f32 shown_bounds, reach_rect_f32 monitor_bounds, double delta_seconds)
{
    REACH_ASSERT(shell != nullptr);
    float hidden_y = monitor_bounds.y + monitor_bounds.height + 4.0f;
    int32_t target_hidden = shell != nullptr && shell->ui.dock.auto_hide && reach_shell_any_window_maximized(shell);
    float target_y = target_hidden ? hidden_y : shown_bounds.y;

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
        shell->dock_render_dirty = 1;
    }

    if (shell->dock_animating) {
        reach_float_animation_update(&shell->dock_y_animation, delta_seconds);
        shell->dock_animating = reach_shell_float_animation_active(&shell->dock_y_animation);
        shell->dock_render_dirty = 1;
    }
    if (shell->tray_animating) {
        reach_float_animation_update(&shell->tray_flat_animation, delta_seconds);
        shell->tray_animating = reach_shell_float_animation_active(&shell->tray_flat_animation);
        shell->dock_render_dirty = 1;
    }

    reach_rect_f32 animated = shown_bounds;
    animated.y = shell->dock_y_animation.value;
    return animated;
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
        shell->launcher_render_dirty = 1;
        break;
    case REACH_UI_EVENT_DOCK_APP_CLICK:
    case REACH_UI_EVENT_TRAY_BUTTON_CLICK:
    case REACH_UI_EVENT_POINTER_UP:
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

    reach_dock_destroy(shell->dock);
    reach_wm_destroy(shell->wm);
    reach_hotkeys_destroy(shell->hotkeys);
    reach_monitor_list_destroy(shell->monitors);
    if (shell->launcher_window.ops.destroy != nullptr) {
        shell->launcher_window.ops.destroy(shell->launcher_window.window);
    }
    if (shell->launcher_renderer.ops.destroy != nullptr) {
        shell->launcher_renderer.ops.destroy(shell->launcher_renderer.backend);
    }
    if (shell->dock_window.ops.destroy != nullptr) {
        shell->dock_window.ops.destroy(shell->dock_window.window);
    }
    if (shell->dock_renderer.ops.destroy != nullptr) {
        shell->dock_renderer.ops.destroy(shell->dock_renderer.backend);
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
    for (size_t index = 0; index < shell->pinned_icon_count; ++index) {
        if (shell->icon_provider.ops.release != nullptr && shell->pinned_icons[index].id != 0) {
            (void)shell->icon_provider.ops.release(shell->icon_provider.provider, shell->pinned_icons[index]);
        }
    }
    if (shell->app_launcher.ops.destroy != nullptr) {
        shell->app_launcher.ops.destroy(shell->app_launcher.launcher);
    }
    if (shell->icon_provider.ops.destroy != nullptr) {
        shell->icon_provider.ops.destroy(shell->icon_provider.provider);
    }
    if (shell->explorer_service.ops.destroy != nullptr) {
        shell->explorer_service.ops.destroy(shell->explorer_service.service);
    }
    shell->dock = nullptr;
    shell->wm = nullptr;
    shell->hotkeys = nullptr;
    shell->monitors = nullptr;
    shell->launcher_window = {};
    shell->launcher_renderer = {};
    shell->dock_window = {};
    shell->dock_renderer = {};
    shell->input_source = {};
    shell->window_manager = {};
    shell->config_store = {};
    shell->tray_provider = {};
    shell->search_provider = {};
    shell->app_launcher = {};
    shell->icon_provider = {};
    shell->explorer_service = {};
    shell->pinned_icon_count = 0;
}

static reach_result reach_shell_render_dock_surface(reach_shell *shell, const reach_dock_layout *layout)
{
    REACH_ASSERT(shell != nullptr);
    REACH_ASSERT(layout != nullptr);
    if (shell == nullptr || layout == nullptr || shell->dock_renderer.ops.begin_frame == nullptr) {
        return REACH_OK;
    }

    reach_render_command_buffer commands = {};
    reach_render_command command = {};
    float dock_radius = reach_min_float(layout->bounds.height * 0.32f, 22.0f);
    for (int shadow_index = 0; shadow_index < 3; ++shadow_index) {
        float inset = (float)(shadow_index * 2);
        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect.x = 2.0f + inset;
        command.rect.y = 5.0f + inset;
        command.rect.width = layout->bounds.width - inset * 2.0f;
        command.rect.height = layout->bounds.height - inset * 2.0f;
        command.color.r = 0.55f;
        command.color.g = 0.57f;
        command.color.b = 0.60f;
        command.color.a = 0.16f - (float)shadow_index * 0.035f;
        command.radius = dock_radius;
        reach_render_command_buffer_push(&commands, &command);
    }

    command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = 0.0f;
    command.rect.y = 0.0f;
    command.rect.width = layout->bounds.width;
    command.rect.height = layout->bounds.height;
    command.color.r = 1.0f;
    command.color.g = 1.0f;
    command.color.b = 1.0f;
    command.color.a = 1.0f;
    command.radius = dock_radius;
    reach_render_command_buffer_push(&commands, &command);

    for (size_t index = 0; index < layout->app_slot_count; ++index) {
        command = {};
        command.rect.x = layout->app_slots[index].x - layout->bounds.x;
        command.rect.y = layout->app_slots[index].y - layout->bounds.y;
        command.rect.width = layout->app_slots[index].width;
        command.rect.height = layout->app_slots[index].height;
        if (index < shell->pinned_icon_count && shell->pinned_icons[index].id != 0) {
            command.type = REACH_RENDER_COMMAND_ICON;
            command.icon_id = shell->pinned_icons[index].id;
            command.color.a = 1.0f;
            reach_render_command_buffer_push(&commands, &command);
        } else {
            command.type = REACH_RENDER_COMMAND_RECT;
            command.color.r = 0.18f;
            command.color.g = 0.20f;
            command.color.b = 0.24f;
            command.color.a = 1.0f;
            command.radius = 10.0f;
            reach_render_command_buffer_push(&commands, &command);

            command = {};
            command.type = REACH_RENDER_COMMAND_TEXT;
            command.rect.x = layout->app_slots[index].x - layout->bounds.x;
            command.rect.y = layout->app_slots[index].y - layout->bounds.y + 8.0f;
            command.rect.width = layout->app_slots[index].width;
            command.rect.height = layout->app_slots[index].height;
            command.color.r = 1.0f;
            command.color.g = 1.0f;
            command.color.b = 1.0f;
            command.color.a = 0.92f;
            command.text[0] = index < REACH_MAX_PINNED_APPS ? shell->pinned_icon_initials[index] : '?';
            command.text[1] = 0;
            reach_render_command_buffer_push(&commands, &command);
        }
    }

    command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = layout->tray_button.x - layout->bounds.x;
    command.rect.y = layout->tray_button.y - layout->bounds.y;
    command.rect.width = layout->tray_button.width;
    command.rect.height = layout->tray_button.height;
    command.color.r = 0.22f;
    command.color.g = 0.24f;
    command.color.b = 0.28f;
    command.color.a = 1.0f;
    command.radius = 10.0f;
    reach_render_command_buffer_push(&commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect.x = layout->tray_button.x - layout->bounds.x;
    command.rect.y = layout->tray_button.y - layout->bounds.y + 7.0f;
    command.rect.width = layout->tray_button.width;
    command.rect.height = layout->tray_button.height;
    command.color.r = 1.0f;
    command.color.g = 1.0f;
    command.color.b = 1.0f;
    command.color.a = 0.90f;
    command.text[0] = shell->tray_flat_animation.value > 0.5f ? '-' : '^';
    command.text[1] = 0;
    reach_render_command_buffer_push(&commands, &command);

    if (shell->dock_renderer.ops.begin_frame(shell->dock_renderer.backend) != REACH_OK) {
        return REACH_ERROR;
    }
    (void)shell->dock_renderer.ops.execute(shell->dock_renderer.backend, &commands);
    return shell->dock_renderer.ops.end_frame(shell->dock_renderer.backend);
}

static reach_result reach_shell_render_launcher_surface(reach_shell *shell, const reach_launcher_layout *layout)
{
    REACH_ASSERT(shell != nullptr);
    REACH_ASSERT(layout != nullptr);
    if (shell == nullptr || layout == nullptr || shell->launcher_renderer.ops.begin_frame == nullptr) {
        return REACH_OK;
    }

    reach_render_command_buffer commands = {};
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = 0.0f;
    command.rect.y = 0.0f;
    command.rect.width = layout->bounds.width;
    command.rect.height = layout->bounds.height;
    command.color.r = 0.0f;
    command.color.g = 0.0f;
    command.color.b = 0.0f;
    command.color.a = 0.55f;
    reach_render_command_buffer_push(&commands, &command);

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

    if (reach_ui_state_should_show_pinned_apps(&shell->ui)) {
        for (size_t index = 0; index < layout->pinned_app_slot_count; ++index) {
            command = {};
            command.type = REACH_RENDER_COMMAND_ICON;
            command.rect.x = layout->pinned_app_slots[index].x - layout->bounds.x;
            command.rect.y = layout->pinned_app_slots[index].y - layout->bounds.y;
            command.rect.width = layout->pinned_app_slots[index].width;
            command.rect.height = layout->pinned_app_slots[index].height;
            command.color.r = 0.18f;
            command.color.g = 0.20f;
            command.color.b = 0.24f;
            command.color.a = 1.0f;
            command.radius = 12.0f;
            if (index < shell->pinned_icon_count) {
                command.icon_id = shell->pinned_icons[index].id;
            }
            reach_render_command_buffer_push(&commands, &command);
        }
    } else if (reach_ui_state_should_show_search_placeholder(&shell->ui)) {
        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect.x = layout->search_results.x - layout->bounds.x;
        command.rect.y = layout->search_results.y - layout->bounds.y;
        command.rect.width = layout->search_results.width;
        command.rect.height = layout->search_results.height;
        command.color.r = 0.10f;
        command.color.g = 0.10f;
        command.color.b = 0.11f;
        command.color.a = 0.96f;
        command.radius = 8.0f;
        reach_render_command_buffer_push(&commands, &command);

        command = {};
        command.type = REACH_RENDER_COMMAND_TEXT;
        command.rect.x = layout->search_results.x - layout->bounds.x + 16.0f;
        command.rect.y = layout->search_results.y - layout->bounds.y + 16.0f;
        command.rect.width = layout->search_results.width - 32.0f;
        command.rect.height = 32.0f;
        command.color.r = 1.0f;
        command.color.g = 1.0f;
        command.color.b = 1.0f;
        command.color.a = 0.72f;
        reach_copy_utf16(command.text, 260, (const uint16_t *)L"Open Explorer");
        reach_render_command_buffer_push(&commands, &command);
    }

    if (shell->launcher_renderer.ops.begin_frame(shell->launcher_renderer.backend) != REACH_OK) {
        return REACH_ERROR;
    }
    (void)shell->launcher_renderer.ops.execute(shell->launcher_renderer.backend, &commands);
    return shell->launcher_renderer.ops.end_frame(shell->launcher_renderer.backend);
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

enum reach_shell_context_command {
    REACH_SHELL_CONTEXT_UNPIN = 100
};

static reach_result reach_shell_show_dock_app_context_menu(reach_shell *shell, size_t app_index, int32_t x, int32_t y)
{
    if (shell == nullptr || app_index >= shell->ui.pinned_app_count) {
        return REACH_INVALID_ARGUMENT;
    }

    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return REACH_ERROR;
    }

    AppendMenuW(menu, MF_STRING, REACH_SHELL_CONTEXT_UNPIN, L"Unpin app from dock");
    HWND owner = shell->dock_window.ops.native_handle != nullptr
        ? static_cast<HWND>(shell->dock_window.ops.native_handle(shell->dock_window.window))
        : GetForegroundWindow();
    if (owner == nullptr) {
        owner = GetDesktopWindow();
    }
    SetForegroundWindow(owner);
    int command = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD, x, y, 0, owner, nullptr);
    DestroyMenu(menu);

    if (command == REACH_SHELL_CONTEXT_UNPIN) {
        uint32_t id = shell->ui.pinned_apps[app_index].id;
        if (reach_pin_config_unpin_id(&shell->config_store, id) == REACH_OK) {
            return reach_shell_reload_pins(shell);
        }
        return REACH_ERROR;
    }

    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_up(reach_shell *shell, const reach_ui_event *event)
{
    if (!shell->has_layout) {
        return REACH_OK;
    }

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
            shell->launcher_render_dirty = 1;
            return shell->launcher_window.ops.hide != nullptr
                ? shell->launcher_window.ops.hide(shell->launcher_window.window)
                : REACH_OK;
        }
    }

    if (reach_rect_contains(shell->layout.dock.tray_button, event->x, event->y)) {
        reach_float_animation_start(&shell->tray_flat_animation, 1.0f, 0.0f, 0.18);
        shell->tray_animating = 1;
        shell->dock_render_dirty = 1;
        routed.type = REACH_UI_EVENT_TRAY_BUTTON_CLICK;
        return reach_shell_handle_event(shell, &routed);
    }

    for (size_t index = 0; index < shell->layout.dock.app_slot_count; ++index) {
        if (reach_rect_contains(shell->layout.dock.app_slots[index], event->x, event->y)) {
            routed.type = REACH_UI_EVENT_DOCK_APP_CLICK;
            routed.id = shell->ui.pinned_apps[index].id;
            return reach_shell_handle_event(shell, &routed);
        }
    }

    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_context(reach_shell *shell, const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->has_layout) {
        return REACH_OK;
    }

    for (size_t index = 0; index < shell->layout.dock.app_slot_count; ++index) {
        if (reach_rect_contains(shell->layout.dock.app_slots[index], event->x, event->y)) {
            return reach_shell_show_dock_app_context_menu(shell, index, event->x, event->y);
        }
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

reach_result reach_shell_create(const reach_shell_desc *desc, reach_shell **out_shell)
{
    reach_shell_dependencies dependencies = {};
    reach_result result = reach_windows_create_platform_window(REACH_SURFACE_LAUNCHER, &dependencies.launcher_window);
    if (result == REACH_OK) {
        void *native_window = dependencies.launcher_window.ops.native_handle(dependencies.launcher_window.window);
        result = reach_windows_create_d2d_render_backend(native_window, &dependencies.launcher_renderer);
    }
    if (result == REACH_OK) {
        result = reach_windows_create_platform_window(REACH_SURFACE_DOCK, &dependencies.dock_window);
    }
    if (result == REACH_OK) {
        void *native_window = dependencies.dock_window.ops.native_handle(dependencies.dock_window.window);
        result = reach_windows_create_d2d_render_backend(native_window, &dependencies.dock_renderer);
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

    reach_result result = reach_monitor_list_create(&shell->monitors);
    if (result == REACH_OK) {
        result = reach_wm_create(&shell->wm);
    }
    if (result == REACH_OK) {
        result = reach_hotkeys_create(&shell->hotkeys);
    }

    if (result == REACH_OK && dependencies->dock_window.window == nullptr) {
        reach_dock_config dock_config = {};
        dock_config.height_px = 56;
        dock_config.icon_size_px = 32;
        dock_config.auto_hide = 1;
        dock_config.animation_seconds = 0.16;

        reach_dock_desc dock_desc = {};
        dock_desc.config = &dock_config;
        result = reach_dock_create(&dock_desc, &shell->dock);
    }
    shell->launcher_window = dependencies->launcher_window;
    shell->launcher_renderer = dependencies->launcher_renderer;
    shell->dock_window = dependencies->dock_window;
    shell->dock_renderer = dependencies->dock_renderer;
    shell->input_source = dependencies->input_source;
    shell->window_manager = dependencies->window_manager;
    shell->config_store = dependencies->config_store;
    shell->tray_provider = dependencies->tray_provider;
    shell->search_provider = dependencies->search_provider;
    shell->app_launcher = dependencies->app_launcher;
    shell->icon_provider = dependencies->icon_provider;
    shell->explorer_service = dependencies->explorer_service;

    if (result == REACH_OK && shell->config_store.ops.load != nullptr) {
        (void)reach_pin_config_ensure_defaults(&shell->config_store);
        reach_config_snapshot snapshot = {};
        if (shell->config_store.ops.load(shell->config_store.store, &snapshot) == REACH_OK) {
            if (snapshot.dock_height > 0.0f) shell->ui.dock.height = snapshot.dock_height;
            if (snapshot.dock_width > 0.0f) shell->ui.dock.width = snapshot.dock_width;
            if (snapshot.dock_icon_size > 0.0f) shell->ui.dock.icon_size = snapshot.dock_icon_size;
            (void)reach_ui_state_set_pinned_apps(&shell->ui, snapshot.pinned_apps, snapshot.pinned_app_count);
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
    shell->dock_render_dirty = 1;
    shell->launcher_render_dirty = 1;
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
    } else {
        result = reach_wm_install_hooks(shell->wm);
    }
    if (result != REACH_OK) {
        return result;
    }

    if (shell->dock_window.ops.set_event_callback != nullptr) {
        result = shell->dock_window.ops.set_event_callback(shell->dock_window.window, reach_shell_on_window_event, shell);
        if (result != REACH_OK) {
            return result;
        }
    }
    if (shell->launcher_window.ops.set_event_callback != nullptr) {
        result = shell->launcher_window.ops.set_event_callback(shell->launcher_window.window, reach_shell_on_window_event, shell);
        if (result != REACH_OK) {
            return result;
        }
    }
    if (shell->launcher_window.ops.set_blur_enabled != nullptr) {
        result = shell->launcher_window.ops.set_blur_enabled(shell->launcher_window.window, 1);
        if (result != REACH_OK) {
            return result;
        }
    }

    if (shell->dock_window.ops.show != nullptr) {
        result = shell->dock_window.ops.show(shell->dock_window.window);
        if (result != REACH_OK) {
            return result;
        }
    } else {
        result = reach_dock_show(shell->dock);
        if (result != REACH_OK) {
            return result;
        }
    }

    shell->running = 1;
    shell->layout_dirty = 1;
    shell->render_dirty = 1;
    shell->dock_render_dirty = 1;
    shell->launcher_render_dirty = 1;
    return REACH_OK;
}

reach_result reach_shell_stop(reach_shell *shell)
{
    if (shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    shell->running = 0;
    if (shell->window_manager.ops.stop != nullptr) {
        (void)shell->window_manager.ops.stop(shell->window_manager.manager);
    } else {
        (void)reach_wm_uninstall_hooks(shell->wm);
    }
    (void)reach_hotkeys_unregister_all(shell->hotkeys);
    if (shell->dock != nullptr) {
        (void)reach_dock_hide(shell->dock);
    }
    if (shell->dock_window.ops.hide != nullptr) {
        (void)shell->dock_window.ops.hide(shell->dock_window.window);
    }
    if (shell->launcher_window.ops.hide != nullptr) {
        (void)shell->launcher_window.ops.hide(shell->launcher_window.window);
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
    if (event->type == REACH_UI_EVENT_POINTER_UP) {
        return reach_shell_handle_pointer_up(shell, event);
    }
    if (event->type == REACH_UI_EVENT_POINTER_CONTEXT) {
        return reach_shell_handle_pointer_context(shell, event);
    }

    reach_result result = reach_ui_handle_event(&shell->ui, event, &intent);
    if (result != REACH_OK) {
        return result;
    }

    reach_shell_mark_dirty_for_event(shell, event);

    if (intent.type == REACH_UI_INTENT_OPEN_TRAY_MENU) {
        if (shell->tray_provider.ops.open_menu != nullptr) {
            (void)shell->tray_provider.ops.open_menu(shell->tray_provider.provider, 0);
        }
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

    if (shell->launcher_window.ops.show != nullptr && shell->launcher_window.ops.hide != nullptr) {
        return shell->ui.launcher.open
            ? shell->launcher_window.ops.show(shell->launcher_window.window)
            : shell->launcher_window.ops.hide(shell->launcher_window.window);
    }

    return REACH_OK;
}

reach_result reach_shell_update(reach_shell *shell, double delta_seconds)
{
    if (shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    shell->window_manager_refresh_elapsed += delta_seconds;
    if (shell->window_manager_refresh_elapsed >= 0.25 && shell->window_manager.ops.refresh != nullptr) {
        (void)shell->window_manager.ops.refresh(shell->window_manager.manager);
        shell->window_manager_refresh_elapsed = 0.0;
    } else if (shell->window_manager.ops.refresh == nullptr) {
        (void)reach_wm_update_z_order(shell->wm);
    }
    if (shell->launcher_window.ops.set_bounds != nullptr && shell->monitors != nullptr && reach_monitor_count(shell->monitors) > 0) {
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

        int32_t launcher_window_changed = 0;
        reach_result result = reach_shell_apply_window_state(
            &shell->launcher_window,
            bounds,
            shell->ui.launcher.open ? 0.92f : 0.0f,
            &shell->last_launcher_bounds,
            &shell->last_launcher_opacity,
            &shell->launcher_bounds_valid,
            &shell->launcher_opacity_valid,
            &launcher_window_changed);
        if (result != REACH_OK) {
            return result;
        }

        if (shell->launcher_renderer.ops.begin_frame != nullptr) {
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
                int32_t dock_layout_changed = !shell->has_layout || !reach_shell_rect_equal(shell->layout.dock.bounds, layout.dock.bounds);
                int32_t launcher_layout_changed = !shell->has_layout || !reach_shell_rect_equal(shell->layout.launcher.bounds, layout.launcher.bounds);
                shell->layout = layout;
                shell->has_layout = 1;
                if (shell->ui.launcher.open && (shell->render_dirty || shell->launcher_render_dirty || launcher_window_changed || launcher_layout_changed)) {
                    (void)reach_shell_render_launcher_surface(shell, &layout.launcher);
                }
                if (shell->dock_window.ops.set_bounds != nullptr) {
                    int32_t dock_window_changed = 0;
                    result = reach_shell_apply_window_state(
                        &shell->dock_window,
                        layout.dock.bounds,
                        1.0f,
                        &shell->last_dock_bounds,
                        &shell->last_dock_opacity,
                        &shell->dock_bounds_valid,
                        &shell->dock_opacity_valid,
                        &dock_window_changed);
                    if (result != REACH_OK) {
                        return result;
                    }
                    if (shell->render_dirty || shell->dock_render_dirty || dock_window_changed || dock_layout_changed) {
                        (void)reach_shell_render_dock_surface(shell, &layout.dock);
                    }
                }
            }
        }
    }
    shell->layout_dirty = 0;
    shell->render_dirty = 0;
    shell->dock_render_dirty = 0;
    shell->launcher_render_dirty = 0;

    if (shell->dock != nullptr) {
        return reach_dock_update(shell->dock, delta_seconds);
    }

    return REACH_OK;
}

int32_t reach_shell_needs_frame(const reach_shell *shell)
{
    return shell != nullptr &&
        (shell->render_dirty ||
         shell->dock_render_dirty ||
         shell->launcher_render_dirty ||
         shell->dock_animating ||
         shell->tray_animating);
}
