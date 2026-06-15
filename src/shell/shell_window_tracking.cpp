#include "shell_internal.h"

#include <math.h>

static int32_t reach_shell_utf16_equal(const uint16_t *a, const uint16_t *b)
{
    size_t index = 0;
    if (a == nullptr || b == nullptr)
    {
        return a == b;
    }
    while (a[index] != 0 || b[index] != 0)
    {
        if (a[index] != b[index])
        {
            return 0;
        }
        ++index;
    }
    return 1;
}

static int32_t reach_shell_path_equals(const uint16_t *a, const uint16_t *b)
{
    if (a == nullptr || b == nullptr)
    {
        return 0;
    }

    size_t index = 0;
    while (a[index] != 0 && b[index] != 0)
    {
        uint16_t ca = a[index];
        uint16_t cb = b[index];
        if (ca >= 'A' && ca <= 'Z')
        {
            ca = (uint16_t)(ca + ('a' - 'A'));
        }
        if (cb >= 'A' && cb <= 'Z')
        {
            cb = (uint16_t)(cb + ('a' - 'A'));
        }
        if (ca != cb)
        {
            return 0;
        }
        ++index;
    }

    return a[index] == b[index];
}

static int32_t reach_shell_nonempty_text_equals(const uint16_t *a, const uint16_t *b)
{
    return a != nullptr && b != nullptr && a[0] != 0 && b[0] != 0 && reach_shell_path_equals(a, b);
}

static int32_t reach_shell_dock_window_matches_pinned(void *user,
                                                      const reach_pinned_app_model *pinned_app,
                                                      const reach_window_snapshot *window)
{
    (void)user;

    if (pinned_app == nullptr || window == nullptr)
    {
        return 0;
    }

    if (reach_shell_nonempty_text_equals(pinned_app->app_user_model_id, window->app_user_model_id))
    {
        return 1;
    }

    return reach_shell_path_equals(pinned_app->path, window->path);
}

static int32_t reach_shell_open_window_index(const reach_shell *shell, uintptr_t window_id,
                                             size_t *out_index)
{
    if (shell == nullptr || window_id == 0)
    {
        return 0;
    }

    for (size_t index = 0; index < shell->open_window_count; ++index)
    {
        if (shell->open_windows[index].id == window_id)
        {
            if (out_index != nullptr)
            {
                *out_index = index;
            }
            return 1;
        }
    }
    return 0;
}

static void reach_shell_remove_focus_history_window(reach_shell *shell, uintptr_t window_id)
{
    if (shell == nullptr || window_id == 0)
    {
        return;
    }

    size_t write = 0;
    for (size_t read = 0; read < shell->focus_history_count; ++read)
    {
        if (shell->focus_history[read] != window_id)
        {
            shell->focus_history[write++] = shell->focus_history[read];
        }
    }
    for (size_t index = write; index < shell->focus_history_count; ++index)
    {
        shell->focus_history[index] = 0;
    }
    shell->focus_history_count = write;
}

static void reach_shell_push_focus_history_front(reach_shell *shell, uintptr_t window_id)
{
    if (shell == nullptr || window_id == 0 ||
        !reach_shell_open_window_index(shell, window_id, nullptr))
    {
        return;
    }

    reach_shell_remove_focus_history_window(shell, window_id);
    size_t count = shell->focus_history_count;
    if (count >= REACH_MAX_PINNED_APPS)
    {
        count = REACH_MAX_PINNED_APPS - 1;
    }
    for (size_t index = count; index > 0; --index)
    {
        shell->focus_history[index] = shell->focus_history[index - 1];
    }
    shell->focus_history[0] = window_id;
    shell->focus_history_count = count + 1;
}

static void reach_shell_prune_focus_history(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    size_t write = 0;
    for (size_t read = 0; read < shell->focus_history_count; ++read)
    {
        uintptr_t window = shell->focus_history[read];
        if (window != 0 && window != shell->foreground_window &&
            reach_shell_open_window_index(shell, window, nullptr))
        {
            int32_t duplicate = 0;
            for (size_t prior = 0; prior < write; ++prior)
            {
                if (shell->focus_history[prior] == window)
                {
                    duplicate = 1;
                    break;
                }
            }
            if (!duplicate)
            {
                shell->focus_history[write++] = window;
            }
        }
    }
    for (size_t index = write; index < shell->focus_history_count; ++index)
    {
        shell->focus_history[index] = 0;
    }
    shell->focus_history_count = write;
}

void reach_shell_note_foreground_window(reach_shell *shell, uintptr_t foreground_window)
{
    if (shell == nullptr)
    {
        return;
    }

    uintptr_t previous = shell->foreground_window;
    if (previous != foreground_window)
    {
        reach_shell_push_focus_history_front(shell, previous);
    }

    shell->foreground_window = foreground_window;
    reach_shell_remove_focus_history_window(shell, foreground_window);
    reach_shell_prune_focus_history(shell);
}

reach_result reach_shell_refresh_open_windows(reach_shell *shell, int32_t *out_changed)
{
    if (shell == nullptr || shell->window_manager.ops.window_count == nullptr ||
        shell->window_manager.ops.window_at == nullptr)
    {
        if (out_changed != nullptr)
        {
            *out_changed = 0;
        }
        return REACH_OK;
    }

    uintptr_t old_windows[REACH_MAX_PINNED_APPS] = {};
    int32_t old_minimized[REACH_MAX_PINNED_APPS] = {};
    int32_t old_maximized[REACH_MAX_PINNED_APPS] = {};
    int32_t old_visible[REACH_MAX_PINNED_APPS] = {};
    uint16_t old_paths[REACH_MAX_PINNED_APPS][260] = {};
    uint16_t old_icon_refs[REACH_MAX_PINNED_APPS][260] = {};
    uint16_t old_titles[REACH_MAX_PINNED_APPS][260] = {};
    uint16_t old_app_user_model_ids[REACH_MAX_PINNED_APPS][260] = {};
    reach_icon_handle old_icons[REACH_MAX_PINNED_APPS] = {};
    uint16_t old_initials[REACH_MAX_PINNED_APPS] = {};
    size_t old_count = shell->open_window_count;
    if (old_count > REACH_MAX_PINNED_APPS)
    {
        old_count = REACH_MAX_PINNED_APPS;
    }

    for (size_t index = 0; index < old_count; ++index)
    {
        old_windows[index] = shell->open_windows[index].id;
        old_minimized[index] = shell->open_windows[index].minimized;
        old_maximized[index] = shell->open_windows[index].maximized;
        old_visible[index] = shell->open_windows[index].visible;
        reach_copy_utf16(old_paths[index], 260, shell->open_windows[index].path);
        reach_copy_utf16(old_icon_refs[index], 260,
                         shell->open_windows[index].icon_ref[0] != 0
                             ? shell->open_windows[index].icon_ref
                             : shell->open_windows[index].path);
        reach_copy_utf16(old_titles[index], 260, shell->open_windows[index].title);
        reach_copy_utf16(old_app_user_model_ids[index], 260,
                         shell->open_windows[index].app_user_model_id);
        old_icons[index] = shell->dock_icons.open_window_icons[index];
        old_initials[index] = shell->dock_icons.open_window_initials[index];
    }

    shell->open_window_count = 0;
    size_t count = shell->window_manager.ops.window_count(shell->window_manager.manager);
    for (size_t index = 0; index < count && shell->open_window_count < REACH_MAX_PINNED_APPS;
         ++index)
    {
        reach_window_snapshot snapshot = {};
        if (shell->window_manager.ops.window_at(shell->window_manager.manager, index, &snapshot) !=
                REACH_OK ||
            snapshot.id == 0 ||
            (snapshot.path[0] == 0 && snapshot.app_user_model_id[0] == 0 &&
             snapshot.title[0] == 0))
        {
            continue;
        }

        size_t out_index = shell->open_window_count++;
        shell->open_windows[out_index] = snapshot;
    }

    int32_t changed = old_count != shell->open_window_count;
    int32_t icon_identity_changed = changed;

    if (changed)
    {
        shell->dock_items_changed = 1;
    }
    else
    {
        for (size_t index = 0; index < shell->open_window_count; ++index)
        {
            int32_t dock_item_changed =
                old_windows[index] != shell->open_windows[index].id ||
                !reach_shell_utf16_equal(old_paths[index], shell->open_windows[index].path) ||
                !reach_shell_utf16_equal(old_app_user_model_ids[index],
                                         shell->open_windows[index].app_user_model_id);
            const uint16_t *icon_ref = shell->open_windows[index].icon_ref[0] != 0
                                           ? shell->open_windows[index].icon_ref
                                           : shell->open_windows[index].path;
            int32_t icon_ref_changed = !reach_shell_utf16_equal(old_icon_refs[index], icon_ref);

            if (dock_item_changed)
            {
                icon_identity_changed = 1;
                shell->dock_items_changed = 1;
            }
            if (icon_ref_changed)
            {
                icon_identity_changed = 1;
            }

            if (dock_item_changed || old_minimized[index] != shell->open_windows[index].minimized ||
                old_maximized[index] != shell->open_windows[index].maximized ||
                old_visible[index] != shell->open_windows[index].visible ||
                !reach_shell_utf16_equal(old_titles[index], shell->open_windows[index].title))
            {
                changed = 1;
            }
        }
    }

    if (icon_identity_changed)
    {
        reach_shell_sync_open_window_icons(shell, old_windows, old_icon_refs, old_icons,
                                           old_initials, old_count);
    }
    reach_shell_prune_focus_history(shell);
    if (out_changed != nullptr)
    {
        *out_changed = changed;
    }
    return REACH_OK;
}

int32_t reach_shell_window_is_minimized(const reach_shell *shell, uintptr_t window_id)
{
    if (shell == nullptr || window_id == 0)
    {
        return 0;
    }
    for (size_t index = 0; index < shell->open_window_count; ++index)
    {
        if (shell->open_windows[index].id == window_id)
        {
            return shell->open_windows[index].minimized;
        }
    }
    return 0;
}

static reach_result reach_shell_dispatch_window_control(reach_shell *shell,
                                                        reach_shell_window_control_action action,
                                                        uintptr_t window_id)
{
    if (shell == nullptr || window_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    switch (action)
    {
    case REACH_SHELL_WINDOW_CONTROL_ACTIVATE:
        return shell->window_manager.ops.activate != nullptr
                   ? shell->window_manager.ops.activate(shell->window_manager.manager, window_id)
                   : REACH_ERROR;
    case REACH_SHELL_WINDOW_CONTROL_MINIMIZE:
        return shell->window_manager.ops.minimize != nullptr
                   ? shell->window_manager.ops.minimize(shell->window_manager.manager, window_id)
                   : REACH_ERROR;
    case REACH_SHELL_WINDOW_CONTROL_CLOSE:
        return shell->window_manager.ops.close != nullptr
                   ? shell->window_manager.ops.close(shell->window_manager.manager, window_id)
                   : REACH_ERROR;
    default:
        return REACH_INVALID_ARGUMENT;
    }
}

reach_result reach_shell_execute_window_control(reach_shell *shell,
                                                reach_shell_window_control_action action,
                                                uintptr_t window_id)
{
    if (shell == nullptr || window_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (reach_shell_window_is_settings_window(shell, window_id))
    {
        return reach_shell_execute_settings_window_control(shell, action);
    }

    if (shell->window_manager.ops.privileged_control_available != nullptr &&
        shell->window_manager.ops.privileged_control_available(shell->window_manager.manager))
    {
        return reach_shell_dispatch_window_control(shell, action, window_id);
    }

    if (shell->window_manager.ops.confirm_privileged_control_restart == nullptr ||
        !shell->window_manager.ops.confirm_privileged_control_restart(
            shell->window_manager.manager))
    {
        return REACH_ERROR;
    }

    if (shell->window_manager.ops.start_privileged_control == nullptr ||
        shell->window_manager.ops.start_privileged_control(shell->window_manager.manager) !=
            REACH_OK)
    {
        return REACH_ERROR;
    }

    if (shell->window_manager.ops.privileged_control_available != nullptr &&
        !shell->window_manager.ops.privileged_control_available(shell->window_manager.manager))
    {
        return REACH_ERROR;
    }

    return reach_shell_dispatch_window_control(shell, action, window_id);
}

static void reach_shell_window_control_thread_main(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    for (;;)
    {
        reach_shell_window_control_action action = REACH_SHELL_WINDOW_CONTROL_ACTIVATE;
        uintptr_t windows[REACH_MAX_PINNED_APPS] = {};
        size_t window_count = 0;

        {
            std::unique_lock<std::mutex> lock(shell->window_control.mutex);
            shell->window_control.cv.wait(lock, [shell]() {
                return shell->window_control.stop || shell->window_control.pending;
            });

            if (shell->window_control.stop)
            {
                return;
            }

            action = shell->window_control.pending_action;
            window_count = shell->window_control.pending_window_count;
            if (window_count > REACH_MAX_PINNED_APPS)
            {
                window_count = REACH_MAX_PINNED_APPS;
            }
            for (size_t index = 0; index < window_count; ++index)
            {
                windows[index] = shell->window_control.pending_windows[index];
            }
            shell->window_control.pending = 0;
        }

        reach_result result = window_count > 0 ? REACH_OK : REACH_INVALID_ARGUMENT;
        for (size_t index = 0; index < window_count; ++index)
        {
            reach_result window_result = reach_shell_execute_window_control(shell, action,
                                                                            windows[index]);
            if (window_result != REACH_OK && result == REACH_OK)
            {
                result = window_result;
            }
        }

        {
            std::lock_guard<std::mutex> lock(shell->window_control.mutex);
            if (!shell->window_control.stop)
            {
                shell->window_control.completed_result = result;
                shell->window_control.completed = 1;
            }
        }

        reach_shell_request_update(shell);
    }
}

static reach_result reach_shell_start_window_control_worker(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (shell->window_control.thread_started)
    {
        return REACH_OK;
    }

    shell->window_control.stop = 0;
    try
    {
        shell->window_control.thread = std::thread(reach_shell_window_control_thread_main, shell);
    }
    catch (...)
    {
        return REACH_ERROR;
    }

    shell->window_control.thread_started = 1;
    return REACH_OK;
}

reach_result reach_shell_schedule_window_control(reach_shell *shell,
                                                 reach_shell_window_control_action action,
                                                 uintptr_t window_id)
{
    if (shell == nullptr || window_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (reach_shell_window_is_settings_window(shell, window_id))
    {
        return reach_shell_execute_settings_window_control(shell, action);
    }

    reach_result result = reach_shell_start_window_control_worker(shell);
    if (result != REACH_OK)
    {
        return result;
    }

    {
        std::lock_guard<std::mutex> lock(shell->window_control.mutex);
        shell->window_control.pending_action = action;
        shell->window_control.pending_window = window_id;
        shell->window_control.pending_windows[0] = window_id;
        shell->window_control.pending_window_count = 1;
        shell->window_control.pending = 1;
    }

    shell->window_control.cv.notify_one();
    reach_shell_request_update(shell);
    return REACH_OK;
}

reach_result reach_shell_schedule_minimize_windows(reach_shell *shell, const uintptr_t *window_ids,
                                                   size_t window_count)
{
    if (shell == nullptr || window_ids == nullptr || window_count == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (window_count > REACH_MAX_PINNED_APPS)
    {
        window_count = REACH_MAX_PINNED_APPS;
    }

    reach_result result = reach_shell_start_window_control_worker(shell);
    if (result != REACH_OK)
    {
        return result;
    }

    {
        std::lock_guard<std::mutex> lock(shell->window_control.mutex);
        shell->window_control.pending_action = REACH_SHELL_WINDOW_CONTROL_MINIMIZE;
        shell->window_control.pending_window = window_ids[0];
        shell->window_control.pending_window_count = window_count;
        for (size_t index = 0; index < window_count; ++index)
        {
            shell->window_control.pending_windows[index] = window_ids[index];
        }
        for (size_t index = window_count; index < REACH_MAX_PINNED_APPS; ++index)
        {
            shell->window_control.pending_windows[index] = 0;
        }
        shell->window_control.pending = 1;
    }

    shell->window_control.cv.notify_one();
    reach_shell_request_update(shell);
    return REACH_OK;
}

void reach_shell_apply_window_control_result(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    int32_t completed = 0;
    reach_result result = REACH_OK;

    {
        std::lock_guard<std::mutex> lock(shell->window_control.mutex);
        completed = shell->window_control.completed;
        result = shell->window_control.completed_result;
        shell->window_control.completed = 0;
    }

    if (!completed)
    {
        return;
    }

    if (shell->window_manager.ops.refresh != nullptr)
    {
        (void)shell->window_manager.ops.refresh(shell->window_manager.manager);
        (void)reach_shell_refresh_open_windows(shell, nullptr);
    }

    if (result == REACH_OK)
    {
        shell->dock.dirty_flags = 1;
    }
}

void reach_shell_stop_window_control_worker(reach_shell *shell)
{
    if (shell == nullptr || !shell->window_control.thread_started)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(shell->window_control.mutex);
        shell->window_control.stop = 1;
        shell->window_control.pending = 0;
        shell->window_control.pending_window_count = 0;
    }
    shell->window_control.cv.notify_one();

    if (shell->window_control.thread.joinable())
    {
        shell->window_control.thread.join();
    }

    shell->window_control.thread_started = 0;
    shell->window_control.stop = 0;
    shell->window_control.pending = 0;
    shell->window_control.completed = 0;
    shell->window_control.pending_window = 0;
    shell->window_control.pending_window_count = 0;
}

void reach_shell_build_dock_items(reach_shell *shell, reach_dock_layout *layout)
{
    if (shell == nullptr || layout == nullptr)
    {
        return;
    }

    reach_dock_feature_model_build_items(
        &shell->dock_model, shell->ui.pinned_apps, shell->ui.pinned_app_count, shell->open_windows,
        shell->open_window_count, reach_shell_dock_window_matches_pinned, shell);

    layout->app_slot_count = shell->dock_model.item_count;
    float scale = reach_shell_layout_dpi_scale(shell);
    float icon_size = shell->ui.dock.icon_size * scale;
    float gap = shell->ui.dock.gap * scale;
    size_t count = shell->dock_model.item_count;
    const reach_theme *theme = shell->theme != nullptr ? shell->theme : reach_theme_default();
    float clock_width = theme->dock_clock_width * scale;
    float separator_width = theme->dock_system_separator_width * scale;
    float separator_height = layout->bounds.height * theme->dock_system_separator_height_ratio;
    float music_widget_width =
        reach_music_widget_desired_width(&shell->music_widget_model, theme, scale);
    float music_widget_height = reach_theme_music_widget_height(theme, layout->bounds.height);

    float dock_width = ceilf(icon_size * (float)(count + 3) + clock_width + separator_width +
                             gap * (float)(count + 5));

    if (count == 0)
    {
        dock_width = ceilf(icon_size * 3.0f + clock_width + separator_width + gap * 4.0f);
    }
    if (music_widget_width > 0.0f)
    {
        dock_width += ceilf(music_widget_width + gap);
    }

    float old_width = layout->bounds.width;
    if (dock_width != old_width)
    {
        layout->bounds.x += (old_width - dock_width) * 0.5f;
        layout->bounds.width = dock_width;
    }

    float left = gap;
    float top = (layout->bounds.height - icon_size) * 0.5f;
    layout->music_widget = {};
    if (music_widget_width > 0.0f)
    {
        layout->music_widget.x = left;
        layout->music_widget.y = (layout->bounds.height - music_widget_height) * 0.5f;
        layout->music_widget.width = music_widget_width;
        layout->music_widget.height = music_widget_height;
        left += music_widget_width + gap;
    }
    shell->music_widget_layout = reach_music_widget_compute_layout(
        &shell->music_widget_model, theme, layout->music_widget, scale);

    for (size_t index = 0; index < layout->app_slot_count; ++index)
    {
        layout->app_slots[index].x = left + (icon_size + gap) * (float)index;
        layout->app_slots[index].y = top;
        layout->app_slots[index].width = icon_size;
        layout->app_slots[index].height = icon_size;
    }

    layout->power_button.width = icon_size;
    layout->power_button.height = icon_size;
    layout->power_button.x = dock_width - icon_size - gap;
    layout->power_button.y = top;
    layout->clock.width = clock_width;
    layout->clock.height = icon_size;
    layout->clock.x = layout->power_button.x - gap - clock_width;
    layout->clock.y = top;
    layout->system_separator.width = separator_width;
    layout->system_separator.height = separator_height;
    layout->system_separator.x = layout->clock.x - gap - separator_width;
    layout->system_separator.y = (layout->bounds.height - separator_height) * 0.5f;
    layout->quick_settings_button.width = icon_size;
    layout->quick_settings_button.height = icon_size;
    layout->quick_settings_button.x = layout->system_separator.x - gap - icon_size;
    layout->quick_settings_button.y = top;
    layout->tray_button.width = icon_size;
    layout->tray_button.height = icon_size;
    layout->tray_button.x = layout->quick_settings_button.x - icon_size;
    layout->tray_button.y = top;
}
