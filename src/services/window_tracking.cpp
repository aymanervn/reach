#include "reach/services/window_tracking.h"

#include "reach/support/util.h"

#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>

struct reach_window_tracking
{
    reach_window_manager_port window_manager;

    reach_window_snapshot open_windows[REACH_MAX_PINNED_APPS];
    size_t open_window_count;
    uintptr_t foreground_window;
    uintptr_t focus_history[REACH_MAX_PINNED_APPS];
    size_t focus_history_count;

};

static int32_t reach_window_tracking_utf16_equal(const uint16_t *a, const uint16_t *b)
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

reach_result reach_window_tracking_create(reach_window_manager_port window_manager,
                                          reach_window_tracking **out_service)
{
    if (out_service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_window_tracking *service = new (std::nothrow) reach_window_tracking();
    if (service == nullptr)
    {
        return REACH_ERROR;
    }
    service->window_manager = window_manager;
    *out_service = service;
    return REACH_OK;
}

void reach_window_tracking_destroy(reach_window_tracking *service)
{
    delete service;
}

const reach_window_snapshot *reach_window_tracking_windows(const reach_window_tracking *service)
{
    return service != nullptr ? service->open_windows : nullptr;
}

size_t reach_window_tracking_window_count(const reach_window_tracking *service)
{
    return service != nullptr ? service->open_window_count : 0;
}

int32_t reach_window_tracking_window_is_minimized(const reach_window_tracking *service,
                                                  uintptr_t window_id)
{
    if (service == nullptr || window_id == 0)
    {
        return 0;
    }
    for (size_t index = 0; index < service->open_window_count; ++index)
    {
        if (service->open_windows[index].id == window_id)
        {
            return service->open_windows[index].minimized;
        }
    }
    return 0;
}

size_t reach_window_tracking_collect_unminimized(const reach_window_tracking *service,
                                                 uintptr_t *out_windows, size_t out_window_count)
{
    if (service == nullptr || out_windows == nullptr || out_window_count == 0)
    {
        return 0;
    }

    size_t window_count = 0;
    for (size_t index = 0;
         index < service->open_window_count && window_count < out_window_count; ++index)
    {
        if (service->open_windows[index].id != 0 && !service->open_windows[index].minimized)
        {
            out_windows[window_count++] = service->open_windows[index].id;
        }
    }
    return window_count;
}

const reach_window_snapshot *reach_window_tracking_window_by_id(
    const reach_window_tracking *service, uintptr_t window_id)
{
    if (service == nullptr || window_id == 0)
    {
        return nullptr;
    }
    for (size_t index = 0; index < service->open_window_count; ++index)
    {
        if (service->open_windows[index].id == window_id)
        {
            return &service->open_windows[index];
        }
    }
    return nullptr;
}

static int32_t reach_window_tracking_text_equals_ci(const uint16_t *a, const uint16_t *b)
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

static int32_t reach_window_tracking_nonempty_equals_ci(const uint16_t *a, const uint16_t *b)
{
    return a != nullptr && b != nullptr && a[0] != 0 && b[0] != 0 &&
           reach_window_tracking_text_equals_ci(a, b);
}

int32_t reach_window_tracking_window_matches_app(const reach_pinned_app_model *app,
                                                 const reach_window_snapshot *window)
{
    if (app == nullptr || window == nullptr)
    {
        return 0;
    }
    if (reach_window_tracking_nonempty_equals_ci(app->app_user_model_id,
                                                 window->app_user_model_id))
    {
        return 1;
    }
    return reach_window_tracking_text_equals_ci(app->path, window->path);
}

static int32_t reach_window_tracking_open_window_index(const reach_window_tracking *service,
                                                       uintptr_t window_id, size_t *out_index)
{
    if (service == nullptr || window_id == 0)
    {
        return 0;
    }

    for (size_t index = 0; index < service->open_window_count; ++index)
    {
        if (service->open_windows[index].id == window_id)
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

static void reach_window_tracking_remove_focus_history_window(reach_window_tracking *service,
                                                              uintptr_t window_id)
{
    if (service == nullptr || window_id == 0)
    {
        return;
    }

    size_t write = 0;
    for (size_t read = 0; read < service->focus_history_count; ++read)
    {
        if (service->focus_history[read] != window_id)
        {
            service->focus_history[write++] = service->focus_history[read];
        }
    }
    for (size_t index = write; index < service->focus_history_count; ++index)
    {
        service->focus_history[index] = 0;
    }
    service->focus_history_count = write;
}

static void reach_window_tracking_push_focus_history_front(reach_window_tracking *service,
                                                           uintptr_t window_id)
{
    if (service == nullptr || window_id == 0 ||
        !reach_window_tracking_open_window_index(service, window_id, nullptr))
    {
        return;
    }

    reach_window_tracking_remove_focus_history_window(service, window_id);
    size_t count = service->focus_history_count;
    if (count >= REACH_MAX_PINNED_APPS)
    {
        count = REACH_MAX_PINNED_APPS - 1;
    }
    for (size_t index = count; index > 0; --index)
    {
        service->focus_history[index] = service->focus_history[index - 1];
    }
    service->focus_history[0] = window_id;
    service->focus_history_count = count + 1;
}

static void reach_window_tracking_prune_focus_history(reach_window_tracking *service)
{
    if (service == nullptr)
    {
        return;
    }

    size_t write = 0;
    for (size_t read = 0; read < service->focus_history_count; ++read)
    {
        uintptr_t window = service->focus_history[read];
        if (window != 0 && window != service->foreground_window &&
            reach_window_tracking_open_window_index(service, window, nullptr))
        {
            int32_t duplicate = 0;
            for (size_t prior = 0; prior < write; ++prior)
            {
                if (service->focus_history[prior] == window)
                {
                    duplicate = 1;
                    break;
                }
            }
            if (!duplicate)
            {
                service->focus_history[write++] = window;
            }
        }
    }
    for (size_t index = write; index < service->focus_history_count; ++index)
    {
        service->focus_history[index] = 0;
    }
    service->focus_history_count = write;
}

void reach_window_tracking_note_foreground(reach_window_tracking *service,
                                           uintptr_t foreground_window)
{
    if (service == nullptr)
    {
        return;
    }

    uintptr_t previous = service->foreground_window;
    if (previous != foreground_window)
    {
        reach_window_tracking_push_focus_history_front(service, previous);
    }

    service->foreground_window = foreground_window;
    reach_window_tracking_remove_focus_history_window(service, foreground_window);
    reach_window_tracking_prune_focus_history(service);
}

uintptr_t reach_window_tracking_foreground(const reach_window_tracking *service)
{
    return service != nullptr ? service->foreground_window : 0;
}

const uintptr_t *reach_window_tracking_focus_history(const reach_window_tracking *service)
{
    return service != nullptr ? service->focus_history : nullptr;
}

size_t reach_window_tracking_focus_history_count(const reach_window_tracking *service)
{
    return service != nullptr ? service->focus_history_count : 0;
}

reach_result reach_window_tracking_refresh(reach_window_tracking *service,
                                           reach_window_tracking_refresh_report *out_report)
{
    if (out_report != nullptr)
    {
        *out_report = {};
    }
    if (service == nullptr || service->window_manager.ops.window_count == nullptr ||
        service->window_manager.ops.window_at == nullptr)
    {
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
    size_t old_count = service->open_window_count;
    if (old_count > REACH_MAX_PINNED_APPS)
    {
        old_count = REACH_MAX_PINNED_APPS;
    }

    for (size_t index = 0; index < old_count; ++index)
    {
        old_windows[index] = service->open_windows[index].id;
        old_minimized[index] = service->open_windows[index].minimized;
        old_maximized[index] = service->open_windows[index].maximized;
        old_visible[index] = service->open_windows[index].visible;
        reach_copy_utf16(old_paths[index], 260, service->open_windows[index].path);
        reach_copy_utf16(old_icon_refs[index], 260,
                         service->open_windows[index].icon_ref[0] != 0
                             ? service->open_windows[index].icon_ref
                             : service->open_windows[index].path);
        reach_copy_utf16(old_titles[index], 260, service->open_windows[index].title);
        reach_copy_utf16(old_app_user_model_ids[index], 260,
                         service->open_windows[index].app_user_model_id);
    }

    service->open_window_count = 0;
    size_t count = service->window_manager.ops.window_count(service->window_manager.manager);
    for (size_t index = 0;
         index < count && service->open_window_count < REACH_MAX_PINNED_APPS; ++index)
    {
        reach_window_snapshot snapshot = {};
        if (service->window_manager.ops.window_at(service->window_manager.manager, index,
                                                  &snapshot) != REACH_OK ||
            snapshot.id == 0 ||
            (snapshot.path[0] == 0 && snapshot.app_user_model_id[0] == 0 && snapshot.title[0] == 0))
        {
            continue;
        }

        size_t out_index = service->open_window_count++;
        service->open_windows[out_index] = snapshot;
    }

    int32_t changed = old_count != service->open_window_count;
    int32_t items_changed = changed;
    int32_t icon_identity_changed = changed;

    if (!changed)
    {
        for (size_t index = 0; index < service->open_window_count; ++index)
        {
            int32_t item_changed =
                old_windows[index] != service->open_windows[index].id ||
                !reach_window_tracking_utf16_equal(old_paths[index],
                                                   service->open_windows[index].path) ||
                !reach_window_tracking_utf16_equal(old_app_user_model_ids[index],
                                                   service->open_windows[index].app_user_model_id);
            const uint16_t *icon_ref = service->open_windows[index].icon_ref[0] != 0
                                           ? service->open_windows[index].icon_ref
                                           : service->open_windows[index].path;
            int32_t icon_ref_changed =
                !reach_window_tracking_utf16_equal(old_icon_refs[index], icon_ref);

            if (item_changed)
            {
                items_changed = 1;
                icon_identity_changed = 1;
            }
            if (icon_ref_changed)
            {
                icon_identity_changed = 1;
            }

            if (item_changed || old_minimized[index] != service->open_windows[index].minimized ||
                old_maximized[index] != service->open_windows[index].maximized ||
                old_visible[index] != service->open_windows[index].visible ||
                !reach_window_tracking_utf16_equal(old_titles[index],
                                                   service->open_windows[index].title))
            {
                changed = 1;
            }
        }
    }

    reach_window_tracking_prune_focus_history(service);

    if (out_report != nullptr)
    {
        out_report->changed = changed;
        out_report->items_changed = items_changed;
        out_report->icon_identity_changed = icon_identity_changed;
        out_report->old_count = old_count;
        for (size_t index = 0; index < old_count; ++index)
        {
            out_report->old_windows[index] = old_windows[index];
            reach_copy_utf16(out_report->old_icon_refs[index], 260, old_icon_refs[index]);
        }
    }
    return REACH_OK;
}

