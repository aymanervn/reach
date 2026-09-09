#include "reach/services/window_tracking.h"

#include "reach/support/util.h"

#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>

struct reach_window_tracking
{
    reach_window_manager_port window_manager;

    reach_window_snapshot window_sets[2][REACH_MAX_OPEN_WINDOWS];
    uint32_t group_id_sets[2][REACH_MAX_OPEN_WINDOWS];
    size_t active_set;

    reach_window_snapshot *open_windows;
    uint32_t *group_ids;
    size_t open_window_count;

    const reach_window_snapshot *previous_windows;
    const uint32_t *previous_group_ids;
    size_t previous_window_count;

    uint32_t next_group_id;
    uintptr_t foreground_window;
    uintptr_t current_foreground_window;
    uintptr_t focus_history[REACH_MAX_OPEN_WINDOWS];
    size_t focus_history_count;
};

static const uint16_t *reach_window_tracking_icon_ref(const reach_window_snapshot *window)
{
    const uint16_t *runtime_path =
        reach_application_identity_primary_runtime_path(&window->identity);
    return window->icon_ref[0] != 0 || runtime_path == nullptr ? window->icon_ref : runtime_path;
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
    service->next_group_id = 1;
    service->active_set = 0;
    service->open_windows = service->window_sets[0];
    service->group_ids = service->group_id_sets[0];
    service->previous_windows = service->window_sets[1];
    service->previous_group_ids = service->group_id_sets[1];
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
    for (size_t index = 0; index < service->open_window_count && window_count < out_window_count;
         ++index)
    {
        if (service->open_windows[index].id != 0 && !service->open_windows[index].minimized)
        {
            out_windows[window_count++] = service->open_windows[index].id;
        }
    }
    return window_count;
}

static int32_t reach_window_tracking_rects_overlap(reach_rect_f32 a, reach_rect_f32 b)
{
    return a.x < b.x + b.width && a.x + a.width > b.x && a.y < b.y + b.height &&
           a.y + a.height > b.y;
}

static int32_t reach_window_tracking_rect_centered_on_monitor(reach_rect_f32 bounds,
                                                              reach_rect_f32 monitor)
{
    float center_x = bounds.x + bounds.width * 0.5f;
    float center_y = bounds.y + bounds.height * 0.5f;
    return center_x >= monitor.x && center_x < monitor.x + monitor.width && center_y >= monitor.y &&
           center_y < monitor.y + monitor.height;
}

int32_t reach_window_tracking_any_trespassing(const reach_window_tracking *service,
                                              reach_rect_f32 monitor_bounds,
                                              reach_rect_f32 protected_band,
                                              uintptr_t excluded_window)
{
    if (service == nullptr || service->window_manager.ops.outer_bounds == nullptr)
    {
        return 0;
    }

    for (size_t index = 0; index < service->open_window_count; ++index)
    {
        const reach_window_snapshot *window = &service->open_windows[index];
        if (window->id == 0 || window->id == excluded_window || !window->visible ||
            window->minimized)
        {
            continue;
        }

        reach_rect_f32 bounds = {};
        if (service->window_manager.ops.outer_bounds(service->window_manager.manager, window->id,
                                                     &bounds) != REACH_OK ||
            !reach_window_tracking_rect_centered_on_monitor(bounds, monitor_bounds) ||
            !reach_window_tracking_rects_overlap(bounds, protected_band))
        {
            continue;
        }
        return 1;
    }
    return 0;
}

const reach_window_snapshot *
reach_window_tracking_window_by_id(const reach_window_tracking *service, uintptr_t window_id)
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

int32_t reach_window_tracking_window_matches_app(const reach_pinned_app_model *app,
                                                 const reach_window_snapshot *window)
{
    if (app == nullptr || window == nullptr)
    {
        return 0;
    }
    return reach_application_identity_matches(&app->application.identity, &window->identity);
}

void reach_window_tracking_app_display_name(const reach_window_snapshot *window, uint16_t *out_name,
                                            size_t out_name_count)
{
    if (out_name == nullptr || out_name_count == 0)
    {
        return;
    }

    out_name[0] = 0;
    if (window == nullptr)
    {
        return;
    }

    const uint16_t *runtime_path =
        reach_application_identity_primary_runtime_path(&window->identity);
    reach_copy_path_stem_utf16(out_name, out_name_count, runtime_path);
    if (out_name[0] == 0)
    {
        (void)reach_copy_utf16(out_name, out_name_count, window->title);
    }
}

int32_t reach_window_tracking_windows_same_app(const reach_window_snapshot *a,
                                               const reach_window_snapshot *b)
{
    if (a == nullptr || b == nullptr)
    {
        return 0;
    }
    return reach_application_identity_matches(&a->identity, &b->identity);
}

static size_t reach_window_tracking_group_root(size_t *parents, size_t index)
{
    while (parents[index] != index)
    {
        parents[index] = parents[parents[index]];
        index = parents[index];
    }
    return index;
}

static void reach_window_tracking_group_union(size_t *parents, size_t a, size_t b)
{
    size_t root_a = reach_window_tracking_group_root(parents, a);
    size_t root_b = reach_window_tracking_group_root(parents, b);
    if (root_a == root_b)
    {
        return;
    }
    if (root_a < root_b)
    {
        parents[root_b] = root_a;
    }
    else
    {
        parents[root_a] = root_b;
    }
}

const uint32_t *reach_window_tracking_window_group_ids(const reach_window_tracking *service)
{
    return service != nullptr ? service->group_ids : nullptr;
}

uint32_t reach_window_tracking_window_group_id(const reach_window_tracking *service,
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
            return service->group_ids[index];
        }
    }
    return 0;
}

uint32_t reach_window_tracking_group_id_for_app(const reach_window_tracking *service,
                                                const reach_pinned_app_model *app)
{
    if (service == nullptr || app == nullptr)
    {
        return 0;
    }
    for (size_t index = 0; index < service->open_window_count; ++index)
    {
        if (reach_window_tracking_window_matches_app(app, &service->open_windows[index]))
        {
            return service->group_ids[index];
        }
    }
    return 0;
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
    if (count >= REACH_MAX_OPEN_WINDOWS)
    {
        count = REACH_MAX_OPEN_WINDOWS - 1;
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

void reach_window_tracking_note_current_foreground(reach_window_tracking *service, uintptr_t window)
{
    if (service != nullptr)
    {
        service->current_foreground_window = window;
    }
}

uintptr_t reach_window_tracking_current_foreground(const reach_window_tracking *service)
{
    return service != nullptr ? service->current_foreground_window : 0;
}

int32_t reach_window_tracking_window_is_foreground(const reach_window_tracking *service,
                                                   uintptr_t window)
{
    if (service == nullptr || window == 0)
    {
        return 0;
    }
    return service->window_manager.ops.is_foreground != nullptr
               ? service->window_manager.ops.is_foreground(service->window_manager.manager, window)
               : service->current_foreground_window == window;
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

    service->previous_windows = service->window_sets[service->active_set];
    service->previous_group_ids = service->group_id_sets[service->active_set];
    service->previous_window_count = service->open_window_count;

    service->active_set ^= 1;
    service->open_windows = service->window_sets[service->active_set];
    service->group_ids = service->group_id_sets[service->active_set];

    const reach_window_snapshot *old_windows = service->previous_windows;
    const uint32_t *old_group_ids = service->previous_group_ids;
    size_t old_count = service->previous_window_count;

    service->open_window_count = 0;
    size_t count = service->window_manager.ops.window_count(service->window_manager.manager);
    for (size_t index = 0; index < count && service->open_window_count < REACH_MAX_OPEN_WINDOWS;
         ++index)
    {
        reach_window_snapshot snapshot = {};
        if (service->window_manager.ops.window_at(service->window_manager.manager, index,
                                                  &snapshot) != REACH_OK ||
            snapshot.id == 0 ||
            (snapshot.identity.runtime_path_count == 0 &&
             snapshot.identity.app_user_model_id[0] == 0 && snapshot.title[0] == 0))
        {
            continue;
        }

        size_t out_index = service->open_window_count++;
        service->open_windows[out_index] = snapshot;
    }

    size_t parents[REACH_MAX_OPEN_WINDOWS] = {};
    uint32_t component_group_ids[REACH_MAX_OPEN_WINDOWS] = {};
    for (size_t index = 0; index < service->open_window_count; ++index)
    {
        parents[index] = index;
    }
    for (size_t index = 0; index < service->open_window_count; ++index)
    {
        for (size_t prior = 0; prior < index; ++prior)
        {
            if (reach_window_tracking_windows_same_app(&service->open_windows[index],
                                                       &service->open_windows[prior]))
            {
                reach_window_tracking_group_union(parents, index, prior);
            }
        }
    }
    for (size_t index = 0; index < service->open_window_count; ++index)
    {
        size_t root = reach_window_tracking_group_root(parents, index);
        for (size_t old_index = 0; old_index < old_count; ++old_index)
        {
            if (old_windows[old_index].id != service->open_windows[index].id)
            {
                continue;
            }
            uint32_t old_group_id = old_group_ids[old_index];
            if (component_group_ids[root] == 0 || old_group_id < component_group_ids[root])
            {
                component_group_ids[root] = old_group_id;
            }
            break;
        }
    }
    for (size_t index = 0; index < service->open_window_count; ++index)
    {
        size_t root = reach_window_tracking_group_root(parents, index);
        for (size_t old_index = 0; old_index < old_count; ++old_index)
        {
            if (!reach_application_identity_matches(&service->open_windows[index].identity,
                                                    &old_windows[old_index].identity))
            {
                continue;
            }
            uint32_t old_group_id = old_group_ids[old_index];
            if (component_group_ids[root] == 0 || old_group_id < component_group_ids[root])
            {
                component_group_ids[root] = old_group_id;
            }
        }
    }
    for (size_t root = 0; root < service->open_window_count; ++root)
    {
        if (reach_window_tracking_group_root(parents, root) != root ||
            component_group_ids[root] == 0)
        {
            continue;
        }
        for (size_t prior = 0; prior < root; ++prior)
        {
            if (reach_window_tracking_group_root(parents, prior) == prior &&
                component_group_ids[prior] == component_group_ids[root])
            {
                component_group_ids[root] = 0;
                break;
            }
        }
    }
    for (size_t index = 0; index < service->open_window_count; ++index)
    {
        size_t root = reach_window_tracking_group_root(parents, index);
        if (component_group_ids[root] == 0)
        {
            component_group_ids[root] = service->next_group_id++;
        }
        service->group_ids[index] = component_group_ids[root];
    }

    int32_t changed = old_count != service->open_window_count;
    int32_t items_changed = changed;
    int32_t icon_identity_changed = changed;

    if (!changed)
    {
        for (size_t index = 0; index < service->open_window_count; ++index)
        {
            int32_t item_changed =
                old_windows[index].id != service->open_windows[index].id ||
                !reach_application_identity_same(&old_windows[index].identity,
                                                 &service->open_windows[index].identity);
            int32_t icon_ref_changed =
                !reach_utf16_equal(reach_window_tracking_icon_ref(&old_windows[index]),
                                   reach_window_tracking_icon_ref(&service->open_windows[index]));

            if (item_changed)
            {
                items_changed = 1;
                icon_identity_changed = 1;
            }
            if (icon_ref_changed)
            {
                icon_identity_changed = 1;
            }

            if (item_changed ||
                old_windows[index].minimized != service->open_windows[index].minimized ||
                old_windows[index].maximized != service->open_windows[index].maximized ||
                old_windows[index].visible != service->open_windows[index].visible ||
                !reach_utf16_equal(old_windows[index].title, service->open_windows[index].title))
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
    }
    return REACH_OK;
}
