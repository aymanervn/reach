#include "reach/shell.h"

#include "reach/core/render_commands.h"
#include "reach/core/ui_events.h"
#include "reach/core/ui_layout.h"
#include "reach/animation.h"
#include "reach/hotkeys.h"
#include "reach/monitor.h"
#include "reach/platform/windows_adapters.h"
#include "reach/pin_config.h"
#include "reach/theme.h"

#include <windows.h>
#include <shlwapi.h>

#include <math.h>
#include <new>

struct reach_shell {
    reach_hotkeys *hotkeys;
    reach_monitor_list *monitors;
    reach_ui_state ui;
    reach_platform_window_port launcher_window;
    reach_render_backend_port launcher_renderer;
    reach_platform_window_port dock_window;
    reach_render_backend_port dock_renderer;
    reach_platform_window_port tray_window;
    reach_render_backend_port tray_renderer;
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
    reach_icon_handle pinned_icons[REACH_MAX_PINNED_APPS];
    size_t pinned_icon_count;
    uint16_t pinned_icon_initials[REACH_MAX_PINNED_APPS];
    reach_window_snapshot open_windows[REACH_MAX_PINNED_APPS];
    reach_icon_handle open_window_icons[REACH_MAX_PINNED_APPS];
    uint16_t open_window_initials[REACH_MAX_PINNED_APPS];
    size_t open_window_count;
    uintptr_t dock_item_windows[REACH_MAX_PINNED_APPS];
    int32_t dock_item_pinned[REACH_MAX_PINNED_APPS];
    size_t dock_item_open_indices[REACH_MAX_PINNED_APPS];
    size_t dock_item_count;
    reach_ui_layout layout;
    int32_t has_layout;
    int32_t layout_dirty;
    int32_t render_dirty;
    int32_t dock_render_dirty;
    size_t hovered_dock_index;
    int32_t launcher_render_dirty;
    int32_t tray_render_dirty;
    int32_t dock_bounds_valid;
    int32_t launcher_bounds_valid;
    int32_t tray_bounds_valid;
    int32_t dock_opacity_valid;
    int32_t launcher_opacity_valid;
    int32_t tray_opacity_valid;
    int32_t dock_animation_initialized;
    int32_t dock_animating;
    int32_t dock_target_hidden;
    reach_rect_f32 last_dock_bounds;
    reach_rect_f32 last_launcher_bounds;
    reach_rect_f32 last_tray_bounds;
    float last_dock_opacity;
    float last_launcher_opacity;
    float last_tray_opacity;
    reach_float_animation dock_y_animation;
    double window_manager_refresh_elapsed;
    int32_t tray_popup_open;
    int32_t running;
    uint16_t wallpaper_path[260];
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

static int32_t reach_shell_path_equals(const uint16_t *a, const uint16_t *b)
{
    return a != nullptr && b != nullptr &&
        CompareStringOrdinal(reinterpret_cast<const wchar_t *>(a), -1, reinterpret_cast<const wchar_t *>(b), -1, TRUE) == CSTR_EQUAL;
}

static size_t reach_shell_find_pinned_for_path(reach_shell *shell, const uint16_t *path)
{
    if (shell == nullptr || path == nullptr || path[0] == 0) {
        return REACH_MAX_PINNED_APPS;
    }
    for (size_t index = 0; index < shell->ui.pinned_app_count; ++index) {
        if (reach_shell_path_equals(shell->ui.pinned_apps[index].path, path) ||
            reach_shell_path_equals(shell->ui.pinned_apps[index].icon_ref, path)) {
            return index;
        }
        const wchar_t *pinned_name = PathFindFileNameW(reinterpret_cast<const wchar_t *>(shell->ui.pinned_apps[index].path));
        const wchar_t *window_name = PathFindFileNameW(reinterpret_cast<const wchar_t *>(path));
        if (pinned_name != nullptr && window_name != nullptr && lstrcmpiW(pinned_name, window_name) == 0) {
            return index;
        }
    }
    return REACH_MAX_PINNED_APPS;
}

static void reach_shell_release_open_window_icons(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }
    for (size_t index = 0; index < shell->open_window_count; ++index) {
        if (shell->icon_provider.ops.release != nullptr && shell->open_window_icons[index].id != 0) {
            (void)shell->icon_provider.ops.release(shell->icon_provider.provider, shell->open_window_icons[index]);
        }
        shell->open_window_icons[index] = {};
    }
}

static reach_result reach_shell_refresh_open_windows(reach_shell *shell)
{
    if (shell == nullptr || shell->window_manager.ops.window_count == nullptr || shell->window_manager.ops.window_at == nullptr) {
        return REACH_OK;
    }

    reach_shell_release_open_window_icons(shell);
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
        shell->open_window_initials[out_index] = snapshot.title[0] != 0 ? snapshot.title[0] : '?';
        if (shell->icon_provider.ops.load != nullptr) {
            reach_icon_request request = {};
            request.size_px = (int32_t)shell->ui.dock.icon_size;
            (void)reach_copy_utf16(request.path, 260, snapshot.path);
            (void)shell->icon_provider.ops.load(shell->icon_provider.provider, &request, &shell->open_window_icons[out_index]);
        }
    }
    return REACH_OK;
}

static int32_t reach_shell_pinned_running(reach_shell *shell, size_t pinned_index, uintptr_t *out_window)
{
    if (out_window != nullptr) {
        *out_window = 0;
    }
    if (shell == nullptr || pinned_index >= shell->ui.pinned_app_count) {
        return 0;
    }
    for (size_t index = 0; index < shell->open_window_count; ++index) {
        if (reach_shell_find_pinned_for_path(shell, shell->open_windows[index].path) == pinned_index) {
            if (out_window != nullptr) {
                *out_window = shell->open_windows[index].id;
            }
            return 1;
        }
    }
    return 0;
}

static void reach_shell_build_dock_items(reach_shell *shell, reach_dock_layout *layout)
{
    if (shell == nullptr || layout == nullptr) {
        return;
    }
    shell->dock_item_count = 0;
    for (size_t index = 0; index < shell->ui.pinned_app_count && shell->dock_item_count < REACH_MAX_PINNED_APPS; ++index) {
        uintptr_t window_id = 0;
        shell->dock_item_pinned[shell->dock_item_count] = 1;
        shell->dock_item_windows[shell->dock_item_count] = reach_shell_pinned_running(shell, index, &window_id) ? window_id : 0;
        shell->dock_item_open_indices[shell->dock_item_count] = REACH_MAX_PINNED_APPS;
        ++shell->dock_item_count;
    }
    for (size_t index = 0; index < shell->open_window_count && shell->dock_item_count < REACH_MAX_PINNED_APPS; ++index) {
        if (reach_shell_find_pinned_for_path(shell, shell->open_windows[index].path) != REACH_MAX_PINNED_APPS) {
            continue;
        }
        shell->dock_item_pinned[shell->dock_item_count] = 0;
        shell->dock_item_windows[shell->dock_item_count] = shell->open_windows[index].id;
        shell->dock_item_open_indices[shell->dock_item_count] = index;
        ++shell->dock_item_count;
    }

    layout->app_slot_count = shell->dock_item_count;
    float icon_size = shell->ui.dock.icon_size;
    float gap = shell->ui.dock.gap;
    float left = layout->bounds.x + gap;
    float top = layout->bounds.y + (layout->bounds.height - icon_size) * 0.5f;
    for (size_t index = 0; index < layout->app_slot_count; ++index) {
        layout->app_slots[index].x = left + (icon_size + gap) * (float)index;
        layout->app_slots[index].y = top;
        layout->app_slots[index].width = icon_size;
        layout->app_slots[index].height = icon_size;
    }
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

static int32_t reach_shell_cursor_at_monitor_bottom(reach_rect_f32 monitor_bounds)
{
    POINT cursor = {};
    if (!GetCursorPos(&cursor)) {
        return 0;
    }

    float edge_top = monitor_bounds.y + monitor_bounds.height - 2.0f;
    return (float)cursor.x >= monitor_bounds.x &&
        (float)cursor.x < monitor_bounds.x + monitor_bounds.width &&
        (float)cursor.y >= edge_top &&
        (float)cursor.y < monitor_bounds.y + monitor_bounds.height + 1.0f;
}

static reach_rect_f32 reach_shell_apply_dock_animation(reach_shell *shell, reach_rect_f32 shown_bounds, reach_rect_f32 monitor_bounds, double delta_seconds)
{
    REACH_ASSERT(shell != nullptr);
    float hidden_y = monitor_bounds.y + monitor_bounds.height + 4.0f;
    int32_t reveal_requested = reach_shell_cursor_at_monitor_bottom(monitor_bounds);
    int32_t target_hidden = shell != nullptr &&
        shell->ui.dock.auto_hide &&
        reach_shell_should_auto_hide_dock(shell) &&
        !reveal_requested;
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
    case REACH_UI_EVENT_POINTER_MOVE:
    case REACH_UI_EVENT_POINTER_LEAVE:
    case REACH_UI_EVENT_POINTER_MIDDLE:
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
    if (shell->tray_window.ops.destroy != nullptr) {
        shell->tray_window.ops.destroy(shell->tray_window.window);
    }
    if (shell->tray_renderer.ops.destroy != nullptr) {
        shell->tray_renderer.ops.destroy(shell->tray_renderer.backend);
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
    reach_shell_release_open_window_icons(shell);
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
    shell->launcher_window = {};
    shell->launcher_renderer = {};
    shell->dock_window = {};
    shell->dock_renderer = {};
    shell->tray_window = {};
    shell->tray_renderer = {};
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
    shell->pinned_icon_count = 0;
}

static reach_result reach_shell_render_dock_surface(reach_shell *shell, const reach_dock_layout *layout)
{
    REACH_ASSERT(shell != nullptr);
    REACH_ASSERT(layout != nullptr);
    if (shell == nullptr || layout == nullptr || shell->dock_renderer.ops.begin_frame == nullptr) {
        return REACH_OK;
    }

    const reach_theme *theme = shell->theme != nullptr ? shell->theme : reach_theme_default();
    reach_render_command_buffer commands = {};
    reach_render_command command = {};

    float dock_radius = reach_theme_dock_corner_radius(theme, layout->bounds.height);
    float icon_box_size = reach_theme_icon_box_size(theme, layout->bounds.height);
    float icon_size = reach_theme_icon_size(theme, icon_box_size);
    float icon_box_radius = reach_theme_icon_box_corner_radius(theme, icon_box_size);

    uintptr_t focused_window = shell->window_manager.ops.foreground != nullptr
        ? shell->window_manager.ops.foreground(shell->window_manager.manager)
        : 0;

    /*
        Dock body.

        Do not draw fake internal shadow rectangles here. They create the visible
        "extra border" effect. A proper outside shadow should be handled by the
        DComp visual or by rendering into a larger transparent surface.
    */
    command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = 0.0f;
    command.rect.y = 0.0f;
    command.rect.width = layout->bounds.width;
    command.rect.height = layout->bounds.height;
    command.color = theme->dock_background;
    command.radius = dock_radius;
    reach_render_command_buffer_push(&commands, &command);

    if (theme->dock_border_thickness > 0.0f && theme->dock_border.a > 0.0f) {
        command = {};
        command.type = REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE;
        command.rect.x = theme->dock_border_thickness * 0.5f;
        command.rect.y = theme->dock_border_thickness * 0.5f;
        command.rect.width = layout->bounds.width - theme->dock_border_thickness;
        command.rect.height = layout->bounds.height - theme->dock_border_thickness;
        command.color = theme->dock_border;
        command.radius = dock_radius;
        command.stroke_width = theme->dock_border_thickness;
        reach_render_command_buffer_push(&commands, &command);
    }


    for (size_t index = 0; index < layout->app_slot_count; ++index) {
        int32_t pinned_item = index < shell->dock_item_count ? shell->dock_item_pinned[index] : 1;
        reach_icon_handle icon = {};
        uint16_t fallback_initial = '?';

        if (pinned_item) {
            if (index < shell->pinned_icon_count) {
                icon = shell->pinned_icons[index];
            }
            fallback_initial = index < REACH_MAX_PINNED_APPS ? shell->pinned_icon_initials[index] : '?';
        } else {
            size_t open_index = index < shell->dock_item_count
                ? shell->dock_item_open_indices[index]
                : REACH_MAX_PINNED_APPS;

            if (open_index < shell->open_window_count) {
                icon = shell->open_window_icons[open_index];
                fallback_initial = shell->open_window_initials[open_index];
            }
        }

        float box_x = layout->app_slots[index].x - layout->bounds.x
            + (layout->app_slots[index].width - icon_box_size) * 0.5f;

        float box_y = layout->app_slots[index].y - layout->bounds.y
            + (layout->app_slots[index].height - icon_box_size) * 0.5f;

        if (shell->hovered_dock_index == index) {
            command = {};
            command.type = REACH_RENDER_COMMAND_RECT;
            command.rect.x = box_x;
            command.rect.y = box_y;
            command.rect.width = icon_box_size;
            command.rect.height = icon_box_size;
            command.color = theme->icon_box_background;
            command.color.a *= 0.32f;
            command.radius = icon_box_radius;
            reach_render_command_buffer_push(&commands, &command);
        }

        if (icon.id != 0) {
            int32_t wants_backplate = icon.wants_backplate;
            float actual_icon_size = wants_backplate ? icon_size : icon_box_size;

            if (wants_backplate) {
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

            float icon_x = box_x + (icon_box_size - actual_icon_size) * 0.5f;
            float icon_y = box_y + (icon_box_size - actual_icon_size) * 0.5f;

            command = {};
            command.type = REACH_RENDER_COMMAND_ICON;
            command.rect.x = icon_x;
            command.rect.y = icon_y;
            command.rect.width = actual_icon_size;
            command.rect.height = actual_icon_size;
            command.icon_id = icon.id;
            command.color.a = 1.0f;
            command.radius = wants_backplate ? 0.0f : icon_box_radius;
            reach_render_command_buffer_push(&commands, &command);

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
        } else {
            /*
                Keep a subtle fallback tile only when no real icon exists.
                Real app icons should sit directly on the dock material.
            */
            command = {};
            command.type = REACH_RENDER_COMMAND_RECT;
            command.rect.x = box_x;
            command.rect.y = box_y;
            command.rect.width = icon_box_size;
            command.rect.height = icon_box_size;
            command.color = theme->icon_box_background;
            command.color.a *= 0.35f;
            command.radius = icon_box_radius;
            reach_render_command_buffer_push(&commands, &command);

            command = {};
            command.type = REACH_RENDER_COMMAND_TEXT;
            command.rect.x = box_x;
            command.rect.y = box_y;
            command.rect.width = icon_box_size;
            command.rect.height = icon_box_size;
            command.color = theme->fallback_icon_text;
            command.text[0] = fallback_initial;
            command.text[1] = 0;
            reach_render_command_buffer_push(&commands, &command);
        }

        if (index < shell->dock_item_count && shell->dock_item_windows[index] != 0) {
            int32_t focused = shell->dock_item_windows[index] == focused_window;

            command = {};
            command.type = REACH_RENDER_COMMAND_RECT;
            command.rect.x = box_x + (icon_box_size - 4.0f) * 0.5f;
            command.rect.y = layout->bounds.height - 7.0f;
            command.rect.width = 4.0f;
            command.rect.height = 4.0f;
            command.color.r = 1.0f;
            command.color.g = 1.0f;
            command.color.b = 1.0f;
            command.color.a = focused ? 1.0f : 0.5f;
            command.radius = 2.0f;
            reach_render_command_buffer_push(&commands, &command);
        }
    }

    float tray_box_x = layout->tray_button.x - layout->bounds.x
        + (layout->tray_button.width - icon_box_size) * 0.5f;

    float tray_box_y = layout->tray_button.y - layout->bounds.y
        + (layout->tray_button.height - icon_box_size) * 0.5f;

    /*
        Keep the tray button background subtle. macOS uses a separator area,
        not a strong individual rounded tile.
    */
    command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = tray_box_x;
    command.rect.y = tray_box_y;
    command.rect.width = icon_box_size;
    command.rect.height = icon_box_size;
    command.color = theme->icon_box_background;
    command.color.a *= 0.25f;
    command.radius = icon_box_radius;
    reach_render_command_buffer_push(&commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect.x = tray_box_x;
    command.rect.y = tray_box_y;
    command.rect.width = icon_box_size;
    command.rect.height = icon_box_size;
    command.color = theme->tray_glyph;
    command.text[0] = '^';
    command.text[1] = 0;
    reach_render_command_buffer_push(&commands, &command);

    if (shell->dock_renderer.ops.begin_frame(shell->dock_renderer.backend) != REACH_OK) {
        return REACH_ERROR;
    }

    reach_result result = shell->dock_renderer.ops.execute(shell->dock_renderer.backend, &commands);
    if (result != REACH_OK) {
        return result;
    }

    return shell->dock_renderer.ops.end_frame(shell->dock_renderer.backend);
}

static reach_result reach_shell_render_tray_surface(reach_shell *shell, reach_rect_f32 bounds)
{
    if (shell == nullptr || shell->tray_renderer.ops.begin_frame == nullptr) {
        return REACH_OK;
    }

    const reach_theme *theme = shell->theme != nullptr ? shell->theme : reach_theme_default();
    reach_render_command_buffer commands = {};
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = 5.0f;
    command.rect.y = 8.0f;
    command.rect.width = bounds.width - 10.0f;
    command.rect.height = bounds.height - 10.0f;
    command.color = theme->dock_shadow;
    command.radius = theme->tray_popup_corner_radius;
    reach_render_command_buffer_push(&commands, &command);

    command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect.x = 0.0f;
    command.rect.y = 0.0f;
    command.rect.width = bounds.width - 10.0f;
    command.rect.height = bounds.height - 10.0f;
    command.color = theme->dock_background;
    command.radius = theme->tray_popup_corner_radius;
    reach_render_command_buffer_push(&commands, &command);

    if (shell->tray_renderer.ops.begin_frame(shell->tray_renderer.backend) != REACH_OK) {
        return REACH_ERROR;
    }
    (void)shell->tray_renderer.ops.execute(shell->tray_renderer.backend, &commands);
    return shell->tray_renderer.ops.end_frame(shell->tray_renderer.backend);
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
    REACH_SHELL_CONTEXT_UNPIN = 100,
    REACH_SHELL_CONTEXT_CLOSE = 101,
    REACH_SHELL_CONTEXT_OPEN_NEW = 102
};

static HHOOK g_reach_context_menu_hook;

static LRESULT CALLBACK reach_shell_context_menu_hook_proc(int code, WPARAM wparam, LPARAM lparam)
{
    if (code == HCBT_ACTIVATE) {
        HWND hwnd = reinterpret_cast<HWND>(wparam);
        wchar_t class_name[32] = {};
        if (hwnd != nullptr &&
            GetClassNameW(hwnd, class_name, 32) != 0 &&
            lstrcmpiW(class_name, L"#32768") == 0) {
            RECT rect = {};
            if (GetWindowRect(hwnd, &rect)) {
                int width = rect.right - rect.left;
                int height = rect.bottom - rect.top;
                HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, 18, 18);
                if (region != nullptr) {
                    SetWindowRgn(hwnd, region, TRUE);
                }
            }
        }
    }
    return CallNextHookEx(g_reach_context_menu_hook, code, wparam, lparam);
}

static void reach_shell_append_context_menu_item(HMENU menu, UINT id, const wchar_t *text)
{
    AppendMenuW(menu, MF_OWNERDRAW, id, reinterpret_cast<LPCWSTR>(text));
}

static reach_result reach_shell_launch_dock_item(reach_shell *shell, size_t item_index, int32_t force_new_instance)
{
    if (shell == nullptr || item_index >= shell->dock_item_count || shell->app_launcher.ops.launch == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    const uint16_t *path = nullptr;
    if (shell->dock_item_pinned[item_index]) {
        path = item_index < shell->ui.pinned_app_count ? shell->ui.pinned_apps[item_index].path : nullptr;
    } else {
        size_t open_index = shell->dock_item_open_indices[item_index];
        path = open_index < shell->open_window_count ? shell->open_windows[open_index].path : nullptr;
    }
    if (path == nullptr || path[0] == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_app_launch_request request = {};
    reach_copy_utf16(request.path, 260, path);
    request.force_new_instance = force_new_instance;
    return shell->app_launcher.ops.launch(shell->app_launcher.launcher, &request);
}

static reach_result reach_shell_show_dock_app_context_menu(reach_shell *shell, size_t item_index, int32_t x, int32_t y)
{
    if (shell == nullptr || item_index >= shell->dock_item_count) {
        return REACH_INVALID_ARGUMENT;
    }

    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return REACH_ERROR;
    }

    HBRUSH menu_brush = CreateSolidBrush(RGB(0, 0, 0));
    if (menu_brush != nullptr) {
        MENUINFO info = {};
        info.cbSize = sizeof(info);
        info.fMask = MIM_BACKGROUND;
        info.hbrBack = menu_brush;
        SetMenuInfo(menu, &info);
    }

    reach_shell_append_context_menu_item(menu, REACH_SHELL_CONTEXT_OPEN_NEW, L"Open Another Instance");
    if (shell->dock_item_pinned[item_index] || shell->dock_item_windows[item_index] != 0) {
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }
    if (shell->dock_item_pinned[item_index]) {
        reach_shell_append_context_menu_item(menu, REACH_SHELL_CONTEXT_UNPIN, L"Unpin app from dock");
    }
    if (shell->dock_item_windows[item_index] != 0) {
        reach_shell_append_context_menu_item(menu, REACH_SHELL_CONTEXT_CLOSE, L"Close app");
    }
    HWND owner = shell->dock_window.ops.native_handle != nullptr
        ? static_cast<HWND>(shell->dock_window.ops.native_handle(shell->dock_window.window))
        : GetForegroundWindow();
    if (owner == nullptr) {
        owner = GetDesktopWindow();
    }
    SetForegroundWindow(owner);
    g_reach_context_menu_hook = SetWindowsHookExW(
        WH_CBT,
        reach_shell_context_menu_hook_proc,
        nullptr,
        GetCurrentThreadId());
    int command = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD, x, y, 0, owner, nullptr);
    if (g_reach_context_menu_hook != nullptr) {
        UnhookWindowsHookEx(g_reach_context_menu_hook);
        g_reach_context_menu_hook = nullptr;
    }
    DestroyMenu(menu);
    if (menu_brush != nullptr) {
        DeleteObject(menu_brush);
    }

    if (command == REACH_SHELL_CONTEXT_OPEN_NEW) {
        return reach_shell_launch_dock_item(shell, item_index, 1);
    }
    if (command == REACH_SHELL_CONTEXT_UNPIN) {
        uint32_t id = shell->ui.pinned_apps[item_index].id;
        if (reach_pin_config_unpin_id(&shell->config_store, id) == REACH_OK) {
            return reach_shell_reload_pins(shell);
        }
        return REACH_ERROR;
    }
    if (command == REACH_SHELL_CONTEXT_CLOSE && shell->window_manager.ops.close != nullptr) {
        return shell->window_manager.ops.close(shell->window_manager.manager, shell->dock_item_windows[item_index]);
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
        shell->tray_popup_open = !shell->tray_popup_open;
        shell->dock_render_dirty = 1;
        shell->tray_render_dirty = 1;
        if (!shell->tray_popup_open && shell->tray_window.ops.hide != nullptr) {
            return shell->tray_window.ops.hide(shell->tray_window.window);
        }
        return REACH_OK;
    }

    for (size_t index = 0; index < shell->layout.dock.app_slot_count; ++index) {
        if (reach_rect_contains(shell->layout.dock.app_slots[index], event->x, event->y)) {
            if (index < shell->dock_item_count && shell->dock_item_windows[index] != 0) {
                uintptr_t window_id = shell->dock_item_windows[index];
                uintptr_t foreground = shell->window_manager.ops.foreground != nullptr
                    ? shell->window_manager.ops.foreground(shell->window_manager.manager)
                    : 0;
                reach_result result = REACH_OK;
                if (foreground == window_id && shell->window_manager.ops.minimize != nullptr) {
                    result = shell->window_manager.ops.minimize(shell->window_manager.manager, window_id);
                } else if (shell->window_manager.ops.activate != nullptr) {
                    result = shell->window_manager.ops.activate(shell->window_manager.manager, window_id);
                }
                if (shell->window_manager.ops.refresh != nullptr) {
                    (void)shell->window_manager.ops.refresh(shell->window_manager.manager);
                    (void)reach_shell_refresh_open_windows(shell);
                }
                shell->dock_render_dirty = 1;
                return result;
            }
            if (index < shell->ui.pinned_app_count) {
                routed.type = REACH_UI_EVENT_DOCK_APP_CLICK;
                routed.id = shell->ui.pinned_apps[index].id;
                return reach_shell_handle_event(shell, &routed);
            }
            return REACH_OK;
        }
    }

    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_move(reach_shell *shell, const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->has_layout) {
        return REACH_OK;
    }

    size_t hovered = REACH_MAX_PINNED_APPS;
    for (size_t index = 0; index < shell->layout.dock.app_slot_count; ++index) {
        if (reach_rect_contains(shell->layout.dock.app_slots[index], event->x, event->y)) {
            hovered = index;
            break;
        }
    }

    if (shell->hovered_dock_index != hovered) {
        shell->hovered_dock_index = hovered;
        shell->dock_render_dirty = 1;
    }
    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_middle(reach_shell *shell, const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->has_layout) {
        return REACH_OK;
    }

    for (size_t index = 0; index < shell->layout.dock.app_slot_count; ++index) {
        if (reach_rect_contains(shell->layout.dock.app_slots[index], event->x, event->y)) {
            return reach_shell_launch_dock_item(shell, index, 1);
        }
    }
    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_leave(reach_shell *shell)
{
    if (shell != nullptr && shell->hovered_dock_index != REACH_MAX_PINNED_APPS) {
        shell->hovered_dock_index = REACH_MAX_PINNED_APPS;
        shell->dock_render_dirty = 1;
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
        result = reach_windows_create_d2d_render_backend(native_window, &dependencies.tray_renderer);
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

    reach_result result = reach_monitor_list_create(&shell->monitors);
    if (result == REACH_OK) {
        result = reach_hotkeys_create(&shell->hotkeys);
    }

    shell->launcher_window = dependencies->launcher_window;
    shell->launcher_renderer = dependencies->launcher_renderer;
    shell->dock_window = dependencies->dock_window;
    shell->dock_renderer = dependencies->dock_renderer;
    shell->tray_window = dependencies->tray_window;
    shell->tray_renderer = dependencies->tray_renderer;
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
    shell->dock_render_dirty = 1;
    shell->launcher_render_dirty = 1;
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
    if (shell->tray_window.ops.set_event_callback != nullptr) {
        result = shell->tray_window.ops.set_event_callback(shell->tray_window.window, reach_shell_on_window_event, shell);
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
        if (shell->wallpaper_surface.ops.show != nullptr) {
            result = shell->wallpaper_surface.ops.show(shell->wallpaper_surface.surface);
            if (result != REACH_OK) {
                return result;
            }
        }
        result = shell->dock_window.ops.show(shell->dock_window.window);
        if (result != REACH_OK) {
            return result;
        }
    }

    shell->running = 1;
    shell->layout_dirty = 1;
    shell->render_dirty = 1;
    shell->dock_render_dirty = 1;
    shell->launcher_render_dirty = 1;
    shell->tray_render_dirty = 1;
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
    }
    (void)reach_hotkeys_unregister_all(shell->hotkeys);
    if (shell->dock_window.ops.hide != nullptr) {
        (void)shell->dock_window.ops.hide(shell->dock_window.window);
    }
    if (shell->launcher_window.ops.hide != nullptr) {
        (void)shell->launcher_window.ops.hide(shell->launcher_window.window);
    }
    if (shell->tray_window.ops.hide != nullptr) {
        (void)shell->tray_window.ops.hide(shell->tray_window.window);
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
    if (event->type == REACH_UI_EVENT_POINTER_UP) {
        return reach_shell_handle_pointer_up(shell, event);
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
        if (shell->ui.launcher.open) {
            if (shell->launcher_window.ops.native_handle != nullptr &&
                shell->dock_window.ops.native_handle != nullptr) {
                HWND launcher_hwnd = (HWND)shell->launcher_window.ops.native_handle(shell->launcher_window.window);
                HWND dock_hwnd = (HWND)shell->dock_window.ops.native_handle(shell->dock_window.window);
                if (launcher_hwnd != nullptr && dock_hwnd != nullptr) {
                    SetWindowPos(launcher_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
                }
            }
            return shell->launcher_window.ops.show(shell->launcher_window.window);
        }
        return shell->launcher_window.ops.hide(shell->launcher_window.window);
    }

    return REACH_OK;
}

reach_result reach_shell_update(reach_shell *shell, double delta_seconds)
{
    if (shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    shell->window_manager_refresh_elapsed += delta_seconds;
    int32_t window_manager_dirty = shell->window_manager.ops.needs_refresh != nullptr &&
        shell->window_manager.ops.needs_refresh(shell->window_manager.manager);
    if ((window_manager_dirty || shell->window_manager_refresh_elapsed >= 0.25) && shell->window_manager.ops.refresh != nullptr) {
        (void)shell->window_manager.ops.refresh(shell->window_manager.manager);
        (void)reach_shell_refresh_open_windows(shell);
        shell->dock_render_dirty = 1;
        shell->window_manager_refresh_elapsed = 0.0;
    }
    if (shell->monitors != nullptr && reach_monitor_count(shell->monitors) > 0 && shell->wallpaper_surface.ops.set_bounds != nullptr) {
        const reach_monitor_info *monitor = reach_monitor_primary(shell->monitors);
        REACH_ASSERT(monitor != nullptr);
        REACH_ASSERT(monitor->primary || reach_monitor_count(shell->monitors) == 1);
        if (monitor != nullptr) {
            reach_rect_f32 wallpaper_bounds = {};
            wallpaper_bounds.x = (float)monitor->bounds.left;
            wallpaper_bounds.y = (float)monitor->bounds.top;
            wallpaper_bounds.width = (float)(monitor->bounds.right - monitor->bounds.left);
            wallpaper_bounds.height = (float)(monitor->bounds.bottom - monitor->bounds.top);
            (void)shell->wallpaper_surface.ops.set_bounds(shell->wallpaper_surface.surface, wallpaper_bounds);
        }
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
                reach_shell_build_dock_items(shell, &layout.dock);
                int32_t dock_layout_changed = !shell->has_layout || !reach_shell_rect_equal(shell->layout.dock.bounds, layout.dock.bounds);
                int32_t launcher_layout_changed = !shell->has_layout || !reach_shell_rect_equal(shell->layout.launcher.bounds, layout.launcher.bounds);
                shell->layout = layout;
                shell->has_layout = 1;
                if (shell->ui.launcher.open && (shell->render_dirty || shell->launcher_render_dirty || launcher_window_changed || launcher_layout_changed)) {
                    (void)reach_shell_render_launcher_surface(shell, &layout.launcher);
                }
                if (shell->dock_window.ops.set_bounds != nullptr) {
                    int32_t dock_window_changed = 0;
                    float dock_radius = reach_theme_dock_corner_radius(shell->theme, layout.dock.bounds.height);
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
                    if (dock_window_changed && shell->dock_window.ops.apply_rounded_corners != nullptr) {
                        (void)shell->dock_window.ops.apply_rounded_corners(shell->dock_window.window, dock_radius);
                    }
                    if (shell->render_dirty || shell->dock_render_dirty || dock_window_changed || dock_layout_changed) {
                        (void)reach_shell_render_dock_surface(shell, &layout.dock);
                    }
                }
                if (shell->tray_window.ops.set_bounds != nullptr) {
                    const reach_theme *theme = shell->theme != nullptr ? shell->theme : reach_theme_default();
                    reach_rect_f32 tray_bounds = {};
                    tray_bounds.width = theme->tray_popup_width;
                    tray_bounds.height = theme->tray_popup_height;
                    tray_bounds.x = layout.dock.tray_button.x + layout.dock.tray_button.width - tray_bounds.width;
                    tray_bounds.y = layout.dock.bounds.y - tray_bounds.height - 8.0f;
                    int32_t tray_window_changed = 0;
                    result = reach_shell_apply_window_state(
                        &shell->tray_window,
                        tray_bounds,
                        shell->tray_popup_open ? 1.0f : 0.0f,
                        &shell->last_tray_bounds,
                        &shell->last_tray_opacity,
                        &shell->tray_bounds_valid,
                        &shell->tray_opacity_valid,
                        &tray_window_changed);
                    if (result != REACH_OK) {
                        return result;
                    }
                    if (tray_window_changed && shell->tray_window.ops.apply_rounded_corners != nullptr) {
                        (void)shell->tray_window.ops.apply_rounded_corners(shell->tray_window.window, theme->tray_popup_corner_radius);
                    }
                    if (shell->tray_popup_open) {
                        if (shell->tray_window.ops.show != nullptr) {
                            (void)shell->tray_window.ops.show(shell->tray_window.window);
                        }
                        if (shell->render_dirty || shell->tray_render_dirty || tray_window_changed) {
                            (void)reach_shell_render_tray_surface(shell, tray_bounds);
                        }
                    } else if (shell->tray_window.ops.hide != nullptr) {
                        (void)shell->tray_window.ops.hide(shell->tray_window.window);
                    }
                }
            }
        }
    }
    shell->layout_dirty = 0;
    shell->render_dirty = 0;
    shell->dock_render_dirty = 0;
    shell->launcher_render_dirty = 0;
    shell->tray_render_dirty = 0;

    return REACH_OK;
}

int32_t reach_shell_needs_frame(const reach_shell *shell)
{
    return shell != nullptr &&
        (shell->render_dirty ||
         shell->dock_render_dirty ||
         shell->launcher_render_dirty ||
         shell->tray_render_dirty ||
         shell->dock_animating ||
         (shell->ui.dock.auto_hide &&
             (shell->dock_target_hidden ||
              reach_shell_should_auto_hide_dock(shell))));
}
