#include "shell_internal.h"

#include <math.h>
#include <stdio.h>
#include <time.h>

static int32_t reach_shell_utf16_equal(const uint16_t *a, const uint16_t *b);

static int32_t reach_shell_float_animation_active(const reach_float_animation *animation)
{
    return animation != nullptr && animation->elapsed_seconds < animation->duration_seconds;
}

static void reach_shell_dispatch_surface_events(reach_surface_runtime *surface)
{
    if (surface != nullptr && surface->window.ops.dispatch_events != nullptr) {
        (void)surface->window.ops.dispatch_events(surface->window.window);
    }
}

reach_result reach_shell_dispatch_events(reach_shell *shell)
{
    if (shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_shell_dispatch_surface_events(&shell->launcher);
    reach_shell_dispatch_surface_events(&shell->dock);
    reach_shell_dispatch_surface_events(&shell->tray);
    reach_shell_dispatch_surface_events(&shell->switcher);
    reach_shell_dispatch_surface_events(&shell->context_menu);
    reach_shell_dispatch_surface_events(&shell->quick_settings);
    shell->events_dispatched_this_cycle = 1;
    return REACH_OK;
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

static int32_t reach_shell_dock_key_equal(int32_t a_pinned, uint32_t a_pin_id, uintptr_t a_window, int32_t b_pinned, uint32_t b_pin_id, uintptr_t b_window)
{
    reach_dock_order_key a = { a_pinned, a_pin_id, a_window };
    reach_dock_order_key b = { b_pinned, b_pin_id, b_window };
    return reach_dock_key_equal(&a, &b);
}

size_t reach_shell_find_dock_order_key(const reach_shell *shell, int32_t pinned, uint32_t pin_id, uintptr_t window)
{
    if (shell == nullptr) {
        return REACH_MAX_PINNED_APPS;
    }
    reach_dock_order_key key = { pinned, pin_id, window };
    return reach_dock_feature_model_find_order_key(&shell->dock_model, key);
}

void reach_shell_move_dock_order(reach_shell *shell, size_t source, size_t target)
{
    if (shell != nullptr) {
        reach_dock_feature_model_move_order(&shell->dock_model, source, target);
    }
}

uint32_t reach_shell_dock_item_pin_id(const reach_shell *shell, size_t index)
{
    return shell != nullptr ? reach_dock_feature_model_item_pin_id(&shell->dock_model, index) : 0;
}

int32_t reach_shell_dock_item_matches_key(const reach_shell *shell, size_t index, int32_t pinned, uint32_t pin_id, uintptr_t window)
{
    if (shell == nullptr) {
        return 0;
    }
    reach_dock_order_key key = { pinned, pin_id, window };
    return reach_dock_feature_model_item_matches_key(&shell->dock_model, index, key);
}


static int32_t reach_shell_dock_icon_size_px(const reach_shell *shell)
{
    const reach_theme *theme = shell != nullptr && shell->theme != nullptr ? shell->theme : reach_theme_default();
    float dock_height = shell != nullptr ? shell->ui.dock.height : 48.0f;
    return (int32_t)(reach_theme_icon_box_size(theme, dock_height) * 4.0f);
}

reach_result reach_shell_load_pinned_icons(reach_shell *shell)
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
    if (a == nullptr || b == nullptr) {
        return 0;
    }

    size_t index = 0;
    while (a[index] != 0 && b[index] != 0) {
        uint16_t ca = a[index];
        uint16_t cb = b[index];
        if (ca >= 'A' && ca <= 'Z') {
            ca = (uint16_t)(ca + ('a' - 'A'));
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (uint16_t)(cb + ('a' - 'A'));
        }
        if (ca != cb) {
            return 0;
        }
        ++index;
    }

    return a[index] == b[index];
}

static int32_t reach_shell_nonempty_text_equals(const uint16_t *a, const uint16_t *b)
{
    return a != nullptr && b != nullptr && a[0] != 0 && b[0] != 0 &&
        reach_shell_path_equals(a, b);
}

static int32_t reach_shell_dock_window_matches_pinned(
    void *user,
    const reach_pinned_app_model *pinned_app,
    const reach_window_snapshot *window)
{
    (void)user;

    if (pinned_app == nullptr || window == nullptr) {
        return 0;
    }

    if (reach_shell_nonempty_text_equals(
            pinned_app->app_user_model_id,
            window->app_user_model_id)) {
        return 1;
    }

    return reach_shell_path_equals(pinned_app->path, window->path);
}

reach_result reach_shell_refresh_open_windows(reach_shell *shell, int32_t *out_changed)
{
    if (shell == nullptr || shell->window_manager.ops.window_count == nullptr || shell->window_manager.ops.window_at == nullptr) {
        if (out_changed != nullptr) {
            *out_changed = 0;
        }
        return REACH_OK;
    }

    uintptr_t old_windows[REACH_MAX_PINNED_APPS] = {};
    int32_t old_minimized[REACH_MAX_PINNED_APPS] = {};
    int32_t old_maximized[REACH_MAX_PINNED_APPS] = {};
    int32_t old_visible[REACH_MAX_PINNED_APPS] = {};
    reach_rect_i32 old_bounds[REACH_MAX_PINNED_APPS] = {};
    uint16_t old_paths[REACH_MAX_PINNED_APPS][260] = {};
    uint16_t old_titles[REACH_MAX_PINNED_APPS][260] = {};
    size_t old_count = shell->open_window_count;

    for (size_t index = 0; index < old_count; ++index) {
        old_windows[index] = shell->open_windows[index].id;
        old_minimized[index] = shell->open_windows[index].minimized;
        old_maximized[index] = shell->open_windows[index].maximized;
        old_visible[index] = shell->open_windows[index].visible;
        old_bounds[index] = shell->open_windows[index].bounds;
        reach_copy_utf16(old_paths[index], 260, shell->open_windows[index].path);
        reach_copy_utf16(old_titles[index], 260, shell->open_windows[index].title);
    }

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

    int32_t changed = old_count != shell->open_window_count;
    int32_t icon_identity_changed = changed;

    if (changed) {
        shell->dock_items_changed = 1;
    } else {
        for (size_t index = 0; index < shell->open_window_count; ++index) {
            int32_t dock_item_changed =
                old_windows[index] != shell->open_windows[index].id ||
                !reach_shell_utf16_equal(old_paths[index], shell->open_windows[index].path);

            if (dock_item_changed) {
                icon_identity_changed = 1;
                shell->dock_items_changed = 1;
            }

            if (dock_item_changed ||
                old_minimized[index] != shell->open_windows[index].minimized ||
                old_maximized[index] != shell->open_windows[index].maximized ||
                old_visible[index] != shell->open_windows[index].visible ||
                old_bounds[index].left != shell->open_windows[index].bounds.left ||
                old_bounds[index].top != shell->open_windows[index].bounds.top ||
                old_bounds[index].right != shell->open_windows[index].bounds.right ||
                old_bounds[index].bottom != shell->open_windows[index].bounds.bottom ||
                !reach_shell_utf16_equal(old_titles[index], shell->open_windows[index].title)) {
                changed = 1;
            }
        }
    }

    if (icon_identity_changed) {
        if (shell->dock.renderer.ops.release_icon != nullptr) {
            size_t release_count = old_count > REACH_MAX_PINNED_APPS ? REACH_MAX_PINNED_APPS : old_count;
            for (size_t index = 0; index < release_count; ++index) {
                if (shell->dock_icons.open_window_icons[index].id != 0) {
                    shell->dock.renderer.ops.release_icon(
                        shell->dock.renderer.backend,
                        (uintptr_t)shell->dock_icons.open_window_icons[index].id);
                }
            }
        }
        reach_dock_release_open_window_icons(&shell->dock_icons, &shell->icon_provider, old_count);
        (void)reach_dock_load_open_window_icons(
            &shell->dock_icons,
            &shell->icon_provider,
            shell->open_windows,
            shell->open_window_count,
            reach_shell_dock_icon_size_px(shell));
    }
    if (out_changed != nullptr) {
        *out_changed = changed;
    }
    return REACH_OK;
}

int32_t reach_shell_window_is_minimized(const reach_shell *shell, uintptr_t window_id)
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

void reach_shell_build_dock_items(reach_shell *shell, reach_dock_layout *layout)
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
        reach_shell_dock_window_matches_pinned,
        shell);

    layout->app_slot_count = shell->dock_model.item_count;
    float icon_size = shell->ui.dock.icon_size;
    float gap = shell->ui.dock.gap;
    size_t count = shell->dock_model.item_count;
    const reach_theme *theme = shell->theme != nullptr ? shell->theme : reach_theme_default();
    float clock_width = theme->dock_clock_width;
    float separator_width = theme->dock_system_separator_width;
    float separator_height = layout->bounds.height * theme->dock_system_separator_height_ratio;
    float dock_width = ceilf(icon_size * (float)(count + 3) + clock_width + separator_width + gap * (float)(count + 6));
    if (count == 0) {
        dock_width = ceilf(icon_size * 3.0f + clock_width + separator_width + gap * 5.0f);
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

    layout->power_button.width = icon_size;
    layout->power_button.height = icon_size;
    layout->power_button.x = layout->bounds.x + dock_width - icon_size - gap;
    layout->power_button.y = top;
    layout->clock.width = clock_width;
    layout->clock.height = icon_size;
    layout->clock.x = layout->power_button.x - gap - clock_width;
    layout->clock.y = top;
    layout->system_separator.width = separator_width;
    layout->system_separator.height = separator_height;
    layout->system_separator.x = layout->clock.x - gap - separator_width;
    layout->system_separator.y = layout->bounds.y + (layout->bounds.height - separator_height) * 0.5f;
    layout->quick_settings_button.width = icon_size;
    layout->quick_settings_button.height = icon_size;
    layout->quick_settings_button.x = layout->system_separator.x - gap - icon_size;
    layout->quick_settings_button.y = top;
    layout->tray_button.width = icon_size;
    layout->tray_button.height = icon_size;
    layout->tray_button.x = layout->quick_settings_button.x - gap - icon_size;
    layout->tray_button.y = top;
}

static void reach_shell_copy_ascii_to_utf16(uint16_t *dst, size_t dst_count, const char *src)
{
    if (dst == nullptr || dst_count == 0) {
        return;
    }
    size_t index = 0;
    if (src != nullptr) {
        while (index + 1 < dst_count && src[index] != 0) {
            dst[index] = (uint16_t)(unsigned char)src[index];
            ++index;
        }
    }
    dst[index] = 0;
}

static int32_t reach_shell_utf16_equal(const uint16_t *a, const uint16_t *b)
{
    size_t index = 0;
    if (a == nullptr || b == nullptr) {
        return a == b;
    }
    while (a[index] != 0 || b[index] != 0) {
        if (a[index] != b[index]) {
            return 0;
        }
        ++index;
    }
    return 1;
}

void reach_shell_request_update(reach_shell *shell)
{
    if (shell != nullptr) {
        shell->update_requested = 1;
    }
}

static void reach_shell_update_clock_text(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }

    time_t now = time(nullptr);
    int64_t current_minute = (int64_t)(now / 60);
    if (shell->dock_clock_initialized &&
        shell->dock_clock_last_minute == current_minute) {
        return;
    }

    struct tm local = {};
    if (now == (time_t)-1 || localtime_s(&local, &now) != 0) {
        return;
    }

    static const char *months[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    static const char *days[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
    };

    int hour = local.tm_hour % 12;
    if (hour == 0) {
        hour = 12;
    }
    const char *suffix = local.tm_hour >= 12 ? "PM" : "AM";

    char time_text[32] = {};
    char date_text[64] = {};
    snprintf(time_text, sizeof(time_text), "%d:%02d %s", hour, local.tm_min, suffix);
    if (local.tm_mon < 0 || local.tm_mon > 11 || local.tm_wday < 0 || local.tm_wday > 6) {
        return;
    }
    snprintf(date_text, sizeof(date_text), "%.3s %d, %.3s", months[local.tm_mon], local.tm_mday, days[local.tm_wday]);

    uint16_t next_time[32] = {};
    uint16_t next_date[64] = {};
    reach_shell_copy_ascii_to_utf16(next_time, 32, time_text);
    reach_shell_copy_ascii_to_utf16(next_date, 64, date_text);
    if (!shell->dock_clock_initialized ||
        !reach_shell_utf16_equal(shell->dock_time_text, next_time) ||
        !reach_shell_utf16_equal(shell->dock_date_text, next_date)) {
        reach_copy_utf16(shell->dock_time_text, 32, next_time);
        reach_copy_utf16(shell->dock_date_text, 64, next_date);
        shell->dock_clock_initialized = 1;
        shell->dock.dirty_flags = 1;
    }
    shell->dock_clock_last_minute = current_minute;
}

reach_result reach_shell_reload_pins(reach_shell *shell)
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

void reach_shell_seed_or_apply_wallpaper(reach_shell *shell, reach_config_snapshot *snapshot)
{
    if (shell == nullptr || snapshot == nullptr) {
        return;
    }
    int32_t changed = reach_wallpaper_seed_or_apply(
        &shell->wallpaper_service,
        &shell->wallpaper_surface,
        snapshot->wallpaper_path,
        260,
        snapshot->monitor_wallpaper_paths,
        REACH_MAX_WALLPAPER_MONITORS,
        shell->wallpaper_path,
        260);
    if (changed && shell->config_store.ops.save != nullptr) {
        (void)shell->config_store.ops.save(shell->config_store.store, snapshot);
    }
}

reach_result reach_shell_reload_config(reach_shell *shell)
{
    reach_result result = reach_shell_reload_pins(shell);
    if (result != REACH_OK) {
        return result;
    }
    reach_shell_reload_wallpaper(shell, 1);
    return REACH_OK;
}

void reach_shell_reload_wallpaper(reach_shell *shell, int32_t force)
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

static int32_t reach_shell_popup_blocks_dock_autohide(const reach_shell *shell)
{
    return shell != nullptr &&
        (shell->tray_popup_open ||
         shell->quick_settings_open ||
         shell->context_menu_open);
}
int32_t reach_shell_should_auto_hide_dock(const reach_shell *shell)
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

static int32_t reach_shell_get_cursor_position(reach_shell *shell, reach_point_i32 *out_cursor)
{
    if (shell == nullptr ||
        shell->input_source.ops.get_pointer_position == nullptr ||
        out_cursor == nullptr) {
        return 0;
    }
    return shell->input_source.ops.get_pointer_position(shell->input_source.source, out_cursor) == REACH_OK;
}

static int32_t reach_shell_point_in_rect(reach_point_i32 point, reach_rect_f32 rect)
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

static int32_t reach_shell_cursor_in_dock_reveal_bounds(
    reach_shell *shell,
    reach_rect_f32 shown_bounds,
    reach_rect_f32 monitor_bounds)
{
    reach_point_i32 cursor = {};
    if (!reach_shell_get_cursor_position(shell, &cursor)) {
        return 0;
    }
    return reach_shell_point_in_rect(cursor, reach_shell_dock_reveal_bounds(shown_bounds, monitor_bounds));
}

static int32_t reach_shell_popup_blocks_dock_reveal_edge(const reach_shell *shell)
{
    if (shell == nullptr) {
        return 1;
    }

    return shell->ui.launcher.open ||
        shell->switcher_open ||
        shell->tray_popup_open ||
        shell->quick_settings_open ||
        shell->context_menu_open;
}

static int32_t reach_shell_reveal_edge_rect_equal(reach_rect_f32 a, reach_rect_f32 b)
{
    return fabsf(a.x - b.x) < 0.5f &&
        fabsf(a.y - b.y) < 0.5f &&
        fabsf(a.width - b.width) < 0.5f &&
        fabsf(a.height - b.height) < 0.5f;
}

void reach_shell_sync_dock_reveal_edge(
    reach_shell *shell,
    reach_rect_f32 shown_dock_bounds,
    reach_rect_f32 monitor_bounds
)
{
    if (shell == nullptr || shell->dock_reveal_edge.edge == nullptr) {
        return;
    }

    int32_t should_auto_hide =
        shell->ui.dock.auto_hide &&
        reach_shell_should_auto_hide_dock(shell);

    int32_t popup_blocks = reach_shell_popup_blocks_dock_reveal_edge(shell);

    int32_t should_show =
        should_auto_hide &&
        !popup_blocks &&
        (shell->dock_target_hidden || shell->dock_reveal_active) &&
        !shell->dock_animating;

    float monitor_bottom = monitor_bounds.y + monitor_bounds.height;

    reach_rect_f32 edge_bounds = {};
    edge_bounds.x = shown_dock_bounds.x;
    edge_bounds.y = monitor_bottom - 2.0f;
    edge_bounds.width = shown_dock_bounds.width;
    edge_bounds.height = 3.0f;

    if (should_show) {
        if (!shell->dock_reveal_edge_bounds_valid ||
            !reach_shell_reveal_edge_rect_equal(shell->dock_reveal_edge_bounds, edge_bounds)) {
            if (shell->dock_reveal_edge.ops.set_bounds != nullptr &&
                shell->dock_reveal_edge.ops.set_bounds(shell->dock_reveal_edge.edge, edge_bounds) == REACH_OK) {
                shell->dock_reveal_edge_bounds = edge_bounds;
                shell->dock_reveal_edge_bounds_valid = 1;
            }
        }

        if (!shell->dock_reveal_edge_visible &&
            shell->dock_reveal_edge.ops.show != nullptr &&
            shell->dock_reveal_edge.ops.show(shell->dock_reveal_edge.edge) == REACH_OK) {
            shell->dock_reveal_edge_visible = 1;
        }
    } else if (shell->dock_reveal_edge_visible) {
        if (shell->dock_reveal_edge.ops.hide != nullptr &&
            shell->dock_reveal_edge.ops.hide(shell->dock_reveal_edge.edge) == REACH_OK) {
            shell->dock_reveal_edge_visible = 0;
        }
    }
}

reach_rect_f32 reach_shell_apply_dock_animation(reach_shell *shell, reach_rect_f32 shown_bounds, reach_rect_f32 monitor_bounds, double delta_seconds)
{
    REACH_ASSERT(shell != nullptr);
    float hidden_y = monitor_bounds.y + monitor_bounds.height + 4.0f;
    int32_t popup_blocks_autohide = reach_shell_popup_blocks_dock_autohide(shell);
    int32_t should_auto_hide =
        shell->ui.dock.auto_hide &&
        reach_shell_should_auto_hide_dock(shell);

    int32_t base_hidden = should_auto_hide && !popup_blocks_autohide;
    if (!should_auto_hide) {
        shell->dock_reveal_active = 0;
        shell->dock_reveal_requested = 0;
    } else if (!popup_blocks_autohide && shell->dock_reveal_requested) {
        shell->dock_reveal_active =
            reach_shell_cursor_in_dock_reveal_bounds(shell, shown_bounds, monitor_bounds);
        shell->dock_reveal_requested = 0;
    } else if (shell->dock_reveal_active &&
        shell->dock_reveal_check_dirty &&
        !reach_shell_cursor_in_dock_reveal_bounds(shell, shown_bounds, monitor_bounds)) {
        shell->dock_reveal_active = 0;
    }

    int32_t target_hidden = base_hidden && !shell->dock_reveal_active;
    float target_y = target_hidden ? hidden_y : shown_bounds.y;
    if (target_hidden && shell->tray_popup_open) {
        reach_shell_set_tray_popup_open(shell, 0);
    }
    if (target_hidden && shell->quick_settings_open) {
        reach_shell_set_quick_settings_open(shell, 0);
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
    }
    reach_rect_f32 animated = shown_bounds;
    animated.y = shell->dock_y_animation.value;
    return animated;
}

void reach_shell_apply_dock_width_animation(reach_shell *shell, reach_dock_layout *layout, double delta_seconds)
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
    const reach_theme *theme = shell->theme != nullptr ? shell->theme : reach_theme_default();
    float gap = shell->ui.dock.gap;
    layout->power_button.x = layout->bounds.x + layout->bounds.width - layout->power_button.width - gap;
    layout->clock.x = layout->power_button.x - gap - theme->dock_clock_width;
    layout->system_separator.x = layout->clock.x - gap - theme->dock_system_separator_width;
    layout->quick_settings_button.x = layout->system_separator.x - gap - layout->quick_settings_button.width;
    layout->tray_button.x = layout->quick_settings_button.x - gap - layout->tray_button.width;
}

static reach_result reach_shell_refresh_monitor_layout(reach_shell *shell)
{
    if (shell == nullptr ||
        !shell->monitors_dirty ||
        shell->monitors.list == nullptr ||
        shell->wallpaper_surface.ops.set_bounds == nullptr) {
        return REACH_OK;
    }

    if (shell->monitors.list == nullptr || shell->monitors.ops.refresh == nullptr ||
        shell->monitors.ops.count == nullptr || shell->monitors.ops.get == nullptr) {
        return REACH_OK;
    }

    (void)shell->monitors.ops.refresh(shell->monitors.list);
    size_t monitor_count = shell->monitors.ops.count(shell->monitors.list);
    if (monitor_count == 0) {
        shell->monitors_dirty = 0;
        return REACH_OK;
    }

    const reach_monitor_info *monitor = shell->monitors.ops.get(shell->monitors.list, 0);
    if (monitor == nullptr) {
        shell->monitors_dirty = 0;
        return REACH_OK;
    }

    int32_t left = monitor->bounds.left;
    int32_t top = monitor->bounds.top;
    int32_t right = monitor->bounds.right;
    int32_t bottom = monitor->bounds.bottom;
    for (size_t index = 1; index < monitor_count; ++index) {
        monitor = shell->monitors.ops.get(shell->monitors.list, index);
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
    if (!shell->wallpaper_bounds_valid ||
        !reach_shell_rect_equal(shell->wallpaper_bounds, wallpaper_bounds)) {
        reach_result result = shell->wallpaper_surface.ops.set_bounds(
            shell->wallpaper_surface.surface,
            wallpaper_bounds);
        if (result != REACH_OK) {
            return result;
        }
        shell->wallpaper_bounds = wallpaper_bounds;
        shell->wallpaper_bounds_valid = 1;
    }

    shell->monitors_dirty = 0;
    return REACH_OK;
}

float reach_shell_dock_slot_box_x(const reach_shell *shell, const reach_dock_layout *layout, size_t index)
{
    const reach_theme *theme = shell != nullptr && shell->theme != nullptr ? shell->theme : reach_theme_default();
    return reach_dock_slot_box_x(theme, layout, index);
}

float reach_shell_dock_drag_clamped_x(const reach_shell *shell, const reach_dock_layout *layout, int32_t cursor_x)
{
    const reach_theme *theme = shell != nullptr && shell->theme != nullptr ? shell->theme : reach_theme_default();
    return reach_dock_drag_clamped_x(theme, layout, cursor_x, shell != nullptr ? shell->dock_drag_grab_offset_x : 0.0f);
}

size_t reach_shell_find_dock_item_key(const reach_shell *shell, int32_t pinned, uint32_t pin_id, uintptr_t window)
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

size_t reach_shell_dock_reorder_target(const reach_shell *shell, size_t current_index, float dragged_box_x)
{
    return shell != nullptr && shell->has_layout
        ? reach_dock_reorder_target(&shell->dock_model, &shell->layout.dock, current_index, dragged_box_x)
        : REACH_MAX_PINNED_APPS;
}

size_t reach_shell_pinned_order_index(const reach_shell *shell, uint32_t pin_id)
{
    return shell != nullptr ? reach_dock_feature_model_pinned_order_index(&shell->dock_model, pin_id) : REACH_MAX_PINNED_APPS;
}

float reach_shell_dock_item_current_x(const reach_shell *shell, const reach_dock_layout *layout, size_t index)
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

void reach_shell_rebuild_dock_items_with_animations(reach_shell *shell, reach_dock_layout *layout)
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

static int32_t reach_shell_can_fast_update_dock_animation(const reach_shell *shell)
{
    if (shell == nullptr) {
        return 0;
    }

    return shell->dock_animating &&
        shell->has_layout &&
        !shell->layout_dirty &&
        !shell->render_dirty &&
        !shell->dock.dirty_flags &&
        !shell->launcher.dirty_flags &&
        !shell->tray.dirty_flags &&
        !shell->switcher.dirty_flags &&
        !shell->context_menu.dirty_flags &&
        !shell->quick_settings.dirty_flags &&
        !shell->dock_width_animating &&
        !shell->dock_drag_active &&
        !shell->dock_drag_snapping &&
        !shell->dock_click_feedback_animating &&
        !shell->tray_click_feedback_animating &&
        !reach_shell_popup_bounds_animation_active(&shell->quick_settings_bounds_animation);
}

static reach_result reach_shell_fast_update_dock_animation(reach_shell *shell, double delta_seconds)
{
    if (shell == nullptr || !shell->has_layout) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_float_animation_update(&shell->dock_y_animation, delta_seconds);
    shell->dock_animating = reach_shell_float_animation_active(&shell->dock_y_animation);

    reach_rect_f32 dock_bounds = shell->layout.dock.bounds;
    dock_bounds.y = shell->dock_y_animation.value;

    int32_t dock_window_changed = 0;
    reach_result result = reach_shell_apply_window_state(
        &shell->dock.window,
        dock_bounds,
        1.0f,
        &shell->dock.last_bounds,
        &shell->dock.last_opacity,
        &shell->dock.bounds_valid,
        &shell->dock.opacity_valid,
        &dock_window_changed);
    if (result != REACH_OK) {
        return result;
    }

    shell->layout.dock.bounds = dock_bounds;

    if (!shell->dock_animating) {
        shell->dock_reveal_check_dirty = 1;
    }

    shell->update_requested = 0;
    return REACH_OK;
}

reach_result reach_shell_update(reach_shell *shell, double delta_seconds)
{
    if (shell == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }
    if (shell->events_dispatched_this_cycle) {
        shell->events_dispatched_this_cycle = 0;
    } else {
        (void)reach_shell_dispatch_events(shell);
        shell->events_dispatched_this_cycle = 0;
    }
    if (reach_shell_can_fast_update_dock_animation(shell)) {
           return reach_shell_fast_update_dock_animation(shell, delta_seconds);
    }
    reach_shell_process_quick_settings_system_changes(shell);

    reach_shell_update_clock_text(shell);
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
    if (window_manager_dirty && shell->window_manager.ops.refresh != nullptr) {
        (void)shell->window_manager.ops.refresh(shell->window_manager.manager);
        int32_t open_windows_changed = 0;
        (void)reach_shell_refresh_open_windows(shell, &open_windows_changed);
        uintptr_t foreground_window = shell->window_manager.ops.foreground != nullptr
            ? shell->window_manager.ops.foreground(shell->window_manager.manager)
            : 0;
        int32_t foreground_changed = shell->foreground_window != foreground_window;
        shell->foreground_window = foreground_window;
        if (open_windows_changed || foreground_changed) {
            shell->dock.dirty_flags = 1;
        }
    }
    if (shell->tray_popup_open &&
        shell->tray_provider.ops.needs_refresh != nullptr &&
        shell->tray_provider.ops.needs_refresh(shell->tray_provider.provider)) {
        (void)reach_shell_refresh_tray_items(shell);
        shell->tray.dirty_flags = 1;
    }
    reach_result monitor_result = reach_shell_refresh_monitor_layout(shell);
    if (monitor_result != REACH_OK) {
        return monitor_result;
    }
    if (shell->launcher.window.ops.set_bounds != nullptr && shell->monitors.list != nullptr &&
        shell->monitors.ops.count != nullptr && shell->monitors.ops.primary != nullptr &&
        shell->monitors.ops.count(shell->monitors.list) > 0) {
        const reach_monitor_info *monitor = shell->monitors.ops.primary(shell->monitors.list);
        REACH_ASSERT(monitor != nullptr);
        REACH_ASSERT(monitor->primary || shell->monitors.ops.count(shell->monitors.list) == 1);
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
            if (reach_ui_layout_compute(&shell->ui, &input, &layout) == REACH_OK) {
                if (shell->dock_items_changed) {
                    reach_shell_rebuild_dock_items_with_animations(shell, &layout.dock);
                    shell->dock_items_changed = 0;
                } else {
                    reach_shell_build_dock_items(shell, &layout.dock);
                }
                reach_shell_apply_dock_width_animation(shell, &layout.dock, delta_seconds);
                reach_rect_f32 shown_dock_bounds = layout.dock.bounds;
                reach_rect_f32 animated_dock_bounds = reach_shell_apply_dock_animation(shell, shown_dock_bounds, bounds, delta_seconds);
                reach_shell_sync_dock_reveal_edge(shell, shown_dock_bounds, bounds);
                if (shell->dock_reveal_check_dirty) {
                    shell->dock_reveal_check_dirty = 0;
                }
                float dock_y_offset = animated_dock_bounds.y - shown_dock_bounds.y;
                layout.dock.bounds = animated_dock_bounds;
                for (size_t index = 0; index < layout.dock.app_slot_count; ++index) {
                    layout.dock.app_slots[index].y += dock_y_offset;
                }
                layout.dock.tray_button.y += dock_y_offset;
                layout.dock.quick_settings_button.y += dock_y_offset;
                layout.dock.system_separator.y += dock_y_offset;
                layout.dock.clock.y += dock_y_offset;
                layout.dock.power_button.y += dock_y_offset;
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
                    layout.dock.quick_settings_button.x += dock_x_offset;
                    layout.dock.system_separator.x += dock_x_offset;
                    layout.dock.clock.x += dock_x_offset;
                    layout.dock.power_button.x += dock_x_offset;
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
                    int32_t dock_reveal_position_only =
                        shell->dock_animating &&
                        !shell->render_dirty &&
                        !shell->dock.dirty_flags &&
                        !shell->dock_width_animating &&
                        !shell->dock_drag_active &&
                        !shell->dock_drag_snapping &&
                        !shell->dock_click_feedback_animating;

                    if (shell->render_dirty ||
                        shell->dock.dirty_flags ||
                        (!dock_reveal_position_only && (dock_window_changed || dock_layout_changed))) {
                        (void)reach_shell_render_dock_surface(shell, &layout.dock);
                    }
                }
                if (shell->tray.window.ops.set_bounds != nullptr) {
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
                        (void)shell->tray.window.ops.apply_rounded_corners(shell->tray.window.window, reach_popup_radius());
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
                if (shell->quick_settings.window.ops.set_bounds != nullptr) {
                    int32_t quick_window_changed = 0;
                    reach_shell_refresh_quick_settings_layout(shell);
                    reach_shell_update_quick_settings_animation(shell, delta_seconds);
                    result = reach_shell_apply_window_state(
                        &shell->quick_settings.window,
                        shell->quick_settings_bounds,
                        shell->quick_settings_open ? 1.0f : 0.0f,
                        &shell->quick_settings.last_bounds,
                        &shell->quick_settings.last_opacity,
                        &shell->quick_settings.bounds_valid,
                        &shell->quick_settings.opacity_valid,
                        &quick_window_changed);
                    if (result != REACH_OK) {
                        return result;
                    }
                    if (quick_window_changed && shell->quick_settings.window.ops.apply_rounded_corners != nullptr) {
                        (void)shell->quick_settings.window.ops.apply_rounded_corners(shell->quick_settings.window.window, reach_popup_radius());
                    }
                    if (shell->quick_settings_open) {
                        if (shell->quick_settings.window.ops.show != nullptr) {
                            (void)shell->quick_settings.window.ops.show(shell->quick_settings.window.window);
                        }
                        if (shell->render_dirty || shell->quick_settings.dirty_flags || quick_window_changed) {
                            (void)reach_shell_render_quick_settings_surface(shell);
                        }
                    } else if (shell->quick_settings.window.ops.hide != nullptr) {
                        (void)shell->quick_settings.window.ops.hide(shell->quick_settings.window.window);
                    }
                }
                if (shell->switcher.window.ops.set_bounds != nullptr) {
                    reach_rect_f32 switcher_bounds = reach_switcher_bounds_for_count(bounds, reach_shell_switcher_visible_count(shell));
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
                        (void)shell->context_menu.window.ops.apply_rounded_corners(shell->context_menu.window.window, reach_popup_radius());
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
    shell->update_requested = 0;
    shell->dock.dirty_flags = 0;
    shell->launcher.dirty_flags = 0;
    shell->tray.dirty_flags = 0;
    shell->switcher.dirty_flags = 0;
    shell->context_menu.dirty_flags = 0;
    shell->quick_settings.dirty_flags = 0;

    return REACH_OK;
}

int32_t reach_shell_needs_frame(const reach_shell *shell)
{
    int32_t dock_item_animating = 0;
    int32_t window_manager_needs_refresh = 0;

    if (shell != nullptr) {
        for (size_t index = 0; index < shell->dock_model.item_count; ++index) {
            if (shell->dock_item_x_animating[index]) {
                dock_item_animating = 1;
                break;
            }
        }

        if (shell->window_manager.manager != nullptr &&
            shell->window_manager.ops.needs_refresh != nullptr) {
            window_manager_needs_refresh =
                shell->window_manager.ops.needs_refresh(shell->window_manager.manager);
        }
    }

    return shell != nullptr &&
        (shell->update_requested ||
         window_manager_needs_refresh ||
         shell->render_dirty ||
         shell->dock.dirty_flags ||
         shell->launcher.dirty_flags ||
         shell->tray.dirty_flags ||
         shell->switcher.dirty_flags ||
         shell->context_menu.dirty_flags ||
         shell->quick_settings.dirty_flags ||
         shell->dock_reveal_check_dirty ||
         reach_shell_popup_bounds_animation_active(&shell->quick_settings_bounds_animation) ||
         shell->dock_animating ||
         shell->dock_width_animating ||
         shell->dock_drag_active ||
         shell->dock_drag_snapping ||
         dock_item_animating ||
         shell->dock_click_feedback_animating ||
         shell->tray_click_feedback_animating);
}
