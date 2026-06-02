#include "shell_internal.h"

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

enum
{
    REACH_SHELL_CONFIG_JOB_RELOAD = 1,
    REACH_SHELL_CONFIG_JOB_PIN_APP = 2,
    REACH_SHELL_CONFIG_JOB_UNPIN_ID = 3,
    REACH_SHELL_CONFIG_JOB_MOVE_PIN = 4
};

static void reach_shell_config_reload_thread_main(reach_shell *shell)
{
    for (;;)
    {
        uint32_t generation = 0;
        int32_t operation = 0;
        reach_pinned_app_model app = {};
        uint32_t pin_id = 0;
        size_t target_index = 0;
        {
            std::unique_lock<std::mutex> lock(shell->config_reload.mutex);
            shell->config_reload.cv.wait(
                lock,
                [shell]() { return shell->config_reload.stop || shell->config_reload.pending; });

            if (shell->config_reload.stop)
            {
                return;
            }

            generation = shell->config_reload.pending_generation;
            operation = shell->config_reload.pending_operation;
            app = shell->config_reload.pending_app;
            pin_id = shell->config_reload.pending_pin_id;
            target_index = shell->config_reload.pending_target_index;
            shell->config_reload.pending = 0;
            shell->config_reload.in_flight = 1;
        }

        reach_config_snapshot snapshot = {};
        reach_result result = REACH_INVALID_ARGUMENT;
        switch (operation)
        {
        case REACH_SHELL_CONFIG_JOB_RELOAD:
            result = shell->config_store.ops.load != nullptr
                         ? shell->config_store.ops.load(shell->config_store.store, &snapshot)
                         : REACH_INVALID_ARGUMENT;
            break;
        case REACH_SHELL_CONFIG_JOB_PIN_APP:
            result = reach_pin_config_pin_app(&shell->config_store, &app);
            break;
        case REACH_SHELL_CONFIG_JOB_UNPIN_ID:
            result = reach_pin_config_unpin_id(&shell->config_store, pin_id);
            break;
        case REACH_SHELL_CONFIG_JOB_MOVE_PIN:
            result = reach_pin_config_move_id(&shell->config_store, pin_id, target_index);
            break;
        default:
            result = REACH_INVALID_ARGUMENT;
            break;
        }

        if (operation != REACH_SHELL_CONFIG_JOB_RELOAD && result == REACH_OK)
        {
            result = shell->config_store.ops.load != nullptr
                         ? shell->config_store.ops.load(shell->config_store.store, &snapshot)
                         : REACH_INVALID_ARGUMENT;
        }

        {
            std::lock_guard<std::mutex> lock(shell->config_reload.mutex);
            shell->config_reload.in_flight = 0;
            if (!shell->config_reload.stop)
            {
                shell->config_reload.completed_generation = generation;
                shell->config_reload.completed_result = result;
                shell->config_reload.completed_snapshot = snapshot;
                shell->config_reload.completed = 1;
            }
        }

        if (shell->launcher.window.ops.post_event != nullptr)
        {
            (void)shell->launcher.window.ops.post_event(shell->launcher.window.window,
                                                        REACH_UI_EVENT_CONFIG_CHANGED);
        }
        else
        {
            reach_shell_request_update(shell);
        }
    }
}

static reach_result reach_shell_start_config_reload_worker(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (shell->config_reload.thread_started)
    {
        return REACH_OK;
    }

    shell->config_reload.stop = 0;
    try
    {
        shell->config_reload.thread = std::thread(reach_shell_config_reload_thread_main, shell);
    }
    catch (...)
    {
        return REACH_ERROR;
    }

    shell->config_reload.thread_started = 1;
    return REACH_OK;
}

void reach_shell_stop_config_reload_worker(reach_shell *shell)
{
    if (shell == nullptr || !shell->config_reload.thread_started)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(shell->config_reload.mutex);
        shell->config_reload.stop = 1;
        shell->config_reload.pending = 0;
    }
    shell->config_reload.cv.notify_one();

    if (shell->config_reload.thread.joinable())
    {
        shell->config_reload.thread.join();
    }

    shell->config_reload.thread_started = 0;
    shell->config_reload.stop = 0;
    shell->config_reload.pending = 0;
    shell->config_reload.in_flight = 0;
    shell->config_reload.completed = 0;
}

reach_result reach_shell_schedule_config_reload(reach_shell *shell)
{
    if (shell == nullptr || shell->config_store.ops.load == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = reach_shell_start_config_reload_worker(shell);
    if (result != REACH_OK)
    {
        return result;
    }

    uint32_t generation = ++shell->config_reload.generation;
    {
        std::lock_guard<std::mutex> lock(shell->config_reload.mutex);
        shell->config_reload.pending_generation = generation;
        shell->config_reload.pending_operation = REACH_SHELL_CONFIG_JOB_RELOAD;
        shell->config_reload.pending_app = {};
        shell->config_reload.pending_pin_id = 0;
        shell->config_reload.pending_target_index = 0;
        shell->config_reload.pending = 1;
    }
    shell->config_reload.cv.notify_one();
    return REACH_OK;
}

static reach_result reach_shell_schedule_pin_config_job(reach_shell *shell, int32_t operation,
                                                        const reach_pinned_app_model *app,
                                                        uint32_t pin_id, size_t target_index)
{
    if (shell == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = reach_shell_start_config_reload_worker(shell);
    if (result != REACH_OK)
    {
        return result;
    }

    uint32_t generation = ++shell->config_reload.generation;
    {
        std::lock_guard<std::mutex> lock(shell->config_reload.mutex);
        shell->config_reload.pending_generation = generation;
        shell->config_reload.pending_operation = operation;
        shell->config_reload.pending_app = app != nullptr ? *app : reach_pinned_app_model{};
        shell->config_reload.pending_pin_id = pin_id;
        shell->config_reload.pending_target_index = target_index;
        shell->config_reload.pending = 1;
    }
    shell->config_reload.cv.notify_one();
    return REACH_OK;
}

reach_result reach_shell_schedule_pin_app(reach_shell *shell, const reach_pinned_app_model *app)
{
    if (app == nullptr || app->path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    return reach_shell_schedule_pin_config_job(shell, REACH_SHELL_CONFIG_JOB_PIN_APP, app, 0, 0);
}

reach_result reach_shell_schedule_unpin_id(reach_shell *shell, uint32_t pin_id)
{
    if (pin_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    return reach_shell_schedule_pin_config_job(shell, REACH_SHELL_CONFIG_JOB_UNPIN_ID, nullptr,
                                               pin_id, 0);
}

reach_result reach_shell_schedule_move_pin(reach_shell *shell, uint32_t pin_id, size_t target_index)
{
    if (pin_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    return reach_shell_schedule_pin_config_job(shell, REACH_SHELL_CONFIG_JOB_MOVE_PIN, nullptr,
                                               pin_id, target_index);
}

int32_t reach_shell_apply_config_reload_result(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return 0;
    }

    uint32_t generation = 0;
    reach_result result = REACH_OK;
    reach_config_snapshot snapshot = {};
    {
        std::lock_guard<std::mutex> lock(shell->config_reload.mutex);
        if (!shell->config_reload.completed)
        {
            return 0;
        }
        generation = shell->config_reload.completed_generation;
        result = shell->config_reload.completed_result;
        snapshot = shell->config_reload.completed_snapshot;
        shell->config_reload.completed = 0;
    }

    if (generation != shell->config_reload.generation)
    {
        return 1;
    }

    if (result == REACH_OK)
    {
        (void)reach_shell_apply_config_snapshot(shell, &snapshot, 1, 1);
    }
    return 1;
}

static reach_result reach_shell_apply_pins_from_snapshot(reach_shell *shell,
                                                         const reach_config_snapshot *snapshot)
{
    if (shell == nullptr || snapshot == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    int32_t old_order_pinned[REACH_MAX_PINNED_APPS] = {};
    uint32_t old_order_pin_ids[REACH_MAX_PINNED_APPS] = {};
    uintptr_t old_order_windows[REACH_MAX_PINNED_APPS] = {};
    uint16_t old_order_paths[REACH_MAX_PINNED_APPS][260] = {};
    size_t old_order_count = shell->dock_model.order_count;
    for (size_t order_index = 0; order_index < old_order_count; ++order_index)
    {
        old_order_pinned[order_index] = shell->dock_model.order[order_index].pinned;
        old_order_pin_ids[order_index] = shell->dock_model.order[order_index].pin_id;
        old_order_windows[order_index] = shell->dock_model.order[order_index].window;
        if (old_order_pinned[order_index])
        {
            for (size_t pin_index = 0; pin_index < shell->ui.pinned_app_count; ++pin_index)
            {
                if (shell->ui.pinned_apps[pin_index].id == old_order_pin_ids[order_index])
                {
                    reach_copy_utf16(old_order_paths[order_index], 260,
                                     shell->ui.pinned_apps[pin_index].path);
                    break;
                }
            }
        }
    }

    reach_result result = reach_ui_state_set_pinned_apps(&shell->ui, snapshot->pinned_apps,
                                                         snapshot->pinned_app_count);
    if (result != REACH_OK)
    {
        return result;
    }
    reach_shell_refresh_pinned_icon_slots(shell);
    shell->dock_model.order_count = old_order_count;
    for (size_t order_index = 0; order_index < shell->dock_model.order_count; ++order_index)
    {
        shell->dock_model.order[order_index].pinned = old_order_pinned[order_index];
        shell->dock_model.order[order_index].pin_id = old_order_pin_ids[order_index];
        shell->dock_model.order[order_index].window = old_order_windows[order_index];
        if (old_order_pinned[order_index] && old_order_paths[order_index][0] != 0)
        {
            for (size_t pin_index = 0; pin_index < shell->ui.pinned_app_count; ++pin_index)
            {
                if (reach_shell_path_equals(shell->ui.pinned_apps[pin_index].path,
                                            old_order_paths[order_index]))
                {
                    shell->dock_model.order[order_index].pin_id =
                        shell->ui.pinned_apps[pin_index].id;
                    break;
                }
            }
        }
    }
    shell->dirty.layout = 1;
    shell->dirty.render = 1;
    shell->dock.dirty_flags = 1;
    shell->launcher.dirty_flags = 1;
    shell->dock_items_changed = 1;
    reach_shell_request_update(shell);
    return result;
}

void reach_shell_seed_or_apply_wallpaper(reach_shell *shell, reach_config_snapshot *snapshot)
{
    if (shell == nullptr || snapshot == nullptr)
    {
        return;
    }
    int32_t changed = reach_wallpaper_seed_or_apply(
        &shell->wallpaper_service, &shell->wallpaper_surface, snapshot->wallpaper_path, 260,
        snapshot->monitor_wallpaper_paths, REACH_MAX_WALLPAPER_MONITORS,
        shell->wallpaper_state.path, 260);
    if (changed && shell->config_store.ops.save != nullptr)
    {
        (void)shell->config_store.ops.save(shell->config_store.store, snapshot);
    }
}

static int32_t reach_shell_pinned_apps_equal(const reach_pinned_app_model *a, size_t a_count,
                                             const reach_pinned_app_model *b, size_t b_count)
{
    if (a_count != b_count)
    {
        return 0;
    }

    for (size_t index = 0; index < a_count; ++index)
    {
        if (a[index].id != b[index].id ||
            !reach_shell_utf16_equal(a[index].title, b[index].title) ||
            !reach_shell_utf16_equal(a[index].path, b[index].path) ||
            !reach_shell_utf16_equal(a[index].arguments, b[index].arguments) ||
            !reach_shell_utf16_equal(a[index].icon_ref, b[index].icon_ref) ||
            !reach_shell_utf16_equal(a[index].app_user_model_id, b[index].app_user_model_id))
        {
            return 0;
        }
    }

    return 1;
}

reach_result reach_shell_apply_config_snapshot(reach_shell *shell,
                                               const reach_config_snapshot *snapshot,
                                               int32_t apply_pins, int32_t apply_wallpaper)
{
    if (shell == nullptr || snapshot == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (apply_pins &&
        !reach_shell_pinned_apps_equal(shell->ui.pinned_apps, shell->ui.pinned_app_count,
                                       snapshot->pinned_apps, snapshot->pinned_app_count))
    {
        reach_result pin_result = reach_shell_apply_pins_from_snapshot(shell, snapshot);
        if (pin_result != REACH_OK)
        {
            return pin_result;
        }
    }

    if (apply_wallpaper)
    {
        reach_config_snapshot writable_snapshot = *snapshot;
        reach_shell_seed_or_apply_wallpaper(shell, &writable_snapshot);
    }
    return REACH_OK;
}

void reach_shell_reload_wallpaper(reach_shell *shell, int32_t force)
{
    if (shell == nullptr || shell->config_store.ops.load == nullptr)
    {
        return;
    }

    reach_config_snapshot snapshot = {};
    if (shell->config_store.ops.load(shell->config_store.store, &snapshot) != REACH_OK)
    {
        return;
    }

    uint16_t new_path[260] = {};
    if (snapshot.wallpaper_path[0] != 0)
    {
        reach_copy_utf16(new_path, 260, snapshot.wallpaper_path);
    }

    if (!force && reach_shell_path_equals(shell->wallpaper_state.path, new_path))
    {
        return;
    }

    reach_copy_utf16(shell->wallpaper_state.path, 260, new_path);
    if (new_path[0] != 0 && shell->wallpaper_surface.ops.set_wallpaper != nullptr)
    {
        (void)shell->wallpaper_surface.ops.set_wallpaper(shell->wallpaper_surface.surface,
                                                         new_path);
    }
    else if (new_path[0] == 0 && shell->wallpaper_surface.ops.clear != nullptr)
    {
        (void)shell->wallpaper_surface.ops.clear(shell->wallpaper_surface.surface);
    }
    if (shell->wallpaper_surface.ops.set_monitor_wallpaper != nullptr &&
        shell->wallpaper_surface.ops.clear_monitor_wallpaper != nullptr)
    {
        for (size_t index = 0; index < REACH_MAX_WALLPAPER_MONITORS; ++index)
        {
            if (snapshot.monitor_wallpaper_paths[index][0] != 0)
            {
                (void)shell->wallpaper_surface.ops.set_monitor_wallpaper(
                    shell->wallpaper_surface.surface, index,
                    snapshot.monitor_wallpaper_paths[index]);
            }
            else
            {
                (void)shell->wallpaper_surface.ops.clear_monitor_wallpaper(
                    shell->wallpaper_surface.surface, index);
            }
        }
    }
}

int32_t reach_shell_config_reload_work_pending(const reach_shell *shell)
{
    if (shell == nullptr)
    {
        return 0;
    }

    reach_shell *mutable_shell = const_cast<reach_shell *>(shell);
    std::lock_guard<std::mutex> lock(mutable_shell->config_reload.mutex);
    return mutable_shell->config_reload.pending || mutable_shell->config_reload.in_flight ||
           mutable_shell->config_reload.completed;
}
