#include "reach/services/config.h"

#include "reach/services/pin_config.h"

#include <condition_variable>
#include <cstring>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

enum reach_config_work_type
{
    REACH_CONFIG_WORK_NONE = 0,
    REACH_CONFIG_WORK_SAVE,
    REACH_CONFIG_WORK_RELOAD
};

enum reach_config_operation_type
{
    REACH_CONFIG_OPERATION_ENSURE_DEFAULTS = 1,
    REACH_CONFIG_OPERATION_PIN_PATH,
    REACH_CONFIG_OPERATION_PIN_APP,
    REACH_CONFIG_OPERATION_UNPIN_ID,
    REACH_CONFIG_OPERATION_UNPIN_PATH,
    REACH_CONFIG_OPERATION_MOVE_PIN,
    REACH_CONFIG_OPERATION_SET_POWER,
    REACH_CONFIG_OPERATION_SET_DISPLAY,
    REACH_CONFIG_OPERATION_SET_WALLPAPERS,
    REACH_CONFIG_OPERATION_SET_MONITOR_WALLPAPER
};

struct reach_config_operation
{
    reach_config_operation_type type;
    reach_pinned_app_model app;
    uint16_t path[260];
    uint16_t monitor_wallpaper_paths[REACH_MAX_WALLPAPER_MONITORS][260];
    reach_config_power_settings power;
    reach_config_display_settings display;
    uint32_t pin_id;
    size_t target_index;
    size_t monitor_count;
};

struct reach_config_service
{
    reach_config_store_port store;
    reach_config_service_notify notify;
    void *notify_user;
    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;
    int32_t thread_started;
    int32_t stop;
    int32_t save_pending;
    int32_t reload_pending;
    int32_t in_flight;
    int32_t dirty;
    reach_result last_persist_result;
    uint64_t generation;
    uint64_t published_generation;
    uint64_t consumed_generation;
    reach_config_snapshot snapshot;
    std::vector<reach_config_operation> pending_operations;
};

static int32_t reach_config_text_equal(const uint16_t *a, const uint16_t *b)
{
    if (a == nullptr || b == nullptr)
    {
        return a == b;
    }
    size_t index = 0;
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

static reach_result reach_config_apply_operation(reach_config_snapshot *snapshot,
                                                 const reach_config_operation *operation,
                                                 int32_t *out_changed)
{
    if (snapshot == nullptr || operation == nullptr || out_changed == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_changed = 0;
    switch (operation->type)
    {
    case REACH_CONFIG_OPERATION_ENSURE_DEFAULTS:
        return reach_pin_config_ensure_defaults(snapshot, out_changed);
    case REACH_CONFIG_OPERATION_PIN_PATH:
        return reach_pin_config_pin_path(snapshot, operation->path, out_changed);
    case REACH_CONFIG_OPERATION_PIN_APP:
        return reach_pin_config_pin_app(snapshot, &operation->app, out_changed);
    case REACH_CONFIG_OPERATION_UNPIN_ID:
        return reach_pin_config_unpin_id(snapshot, operation->pin_id, out_changed);
    case REACH_CONFIG_OPERATION_UNPIN_PATH:
        return reach_pin_config_unpin_path(snapshot, operation->path, out_changed);
    case REACH_CONFIG_OPERATION_MOVE_PIN:
        return reach_pin_config_move_id(snapshot, operation->pin_id, operation->target_index,
                                        out_changed);
    case REACH_CONFIG_OPERATION_SET_POWER:
        *out_changed =
            snapshot->power_screen_off_minutes != operation->power.screen_off_minutes ||
            snapshot->power_sleep_minutes != operation->power.sleep_minutes ||
            snapshot->power_lock_minutes != operation->power.lock_minutes ||
            snapshot->power_shutdown_minutes != operation->power.shutdown_minutes ||
            snapshot->power_restart_minutes != operation->power.restart_minutes ||
            snapshot->power_sleep_wait_apps != operation->power.sleep_wait_apps ||
            snapshot->power_shutdown_wait_apps != operation->power.shutdown_wait_apps ||
            snapshot->power_restart_wait_apps != operation->power.restart_wait_apps;
        snapshot->power_screen_off_minutes = operation->power.screen_off_minutes;
        snapshot->power_sleep_minutes = operation->power.sleep_minutes;
        snapshot->power_lock_minutes = operation->power.lock_minutes;
        snapshot->power_shutdown_minutes = operation->power.shutdown_minutes;
        snapshot->power_restart_minutes = operation->power.restart_minutes;
        snapshot->power_sleep_wait_apps = operation->power.sleep_wait_apps;
        snapshot->power_shutdown_wait_apps = operation->power.shutdown_wait_apps;
        snapshot->power_restart_wait_apps = operation->power.restart_wait_apps;
        return REACH_OK;
    case REACH_CONFIG_OPERATION_SET_DISPLAY:
        *out_changed = snapshot->high_refresh_rate != operation->display.high_refresh_rate ||
                       snapshot->bundled_font != operation->display.bundled_font ||
                       snapshot->light_theme != operation->display.light_theme;
        snapshot->high_refresh_rate = operation->display.high_refresh_rate;
        snapshot->bundled_font = operation->display.bundled_font;
        snapshot->light_theme = operation->display.light_theme;
        return REACH_OK;
    case REACH_CONFIG_OPERATION_SET_WALLPAPERS:
        *out_changed = !reach_config_text_equal(snapshot->wallpaper_path, operation->path);
        (void)reach_copy_utf16(snapshot->wallpaper_path, 260, operation->path);
        for (size_t index = 0; index < REACH_MAX_WALLPAPER_MONITORS; ++index)
        {
            const uint16_t empty[] = {0};
            const uint16_t *path = index < operation->monitor_count
                                       ? operation->monitor_wallpaper_paths[index]
                                       : empty;
            *out_changed = *out_changed ||
                           !reach_config_text_equal(snapshot->monitor_wallpaper_paths[index], path);
            (void)reach_copy_utf16(snapshot->monitor_wallpaper_paths[index], 260, path);
        }
        return REACH_OK;
    case REACH_CONFIG_OPERATION_SET_MONITOR_WALLPAPER:
        if (operation->target_index >= REACH_MAX_WALLPAPER_MONITORS)
        {
            return REACH_INVALID_ARGUMENT;
        }
        *out_changed = !reach_config_text_equal(
            snapshot->monitor_wallpaper_paths[operation->target_index], operation->path);
        (void)reach_copy_utf16(snapshot->monitor_wallpaper_paths[operation->target_index], 260,
                               operation->path);
        return REACH_OK;
    default:
        return REACH_INVALID_ARGUMENT;
    }
}

static reach_result reach_config_apply_operations(
    reach_config_snapshot *snapshot, const std::vector<reach_config_operation> &operations)
{
    for (const reach_config_operation &operation : operations)
    {
        int32_t changed = 0;
        reach_result result = reach_config_apply_operation(snapshot, &operation, &changed);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    return REACH_OK;
}

static reach_result reach_config_service_load_store(reach_config_service *service,
                                                    reach_config_snapshot *out_snapshot)
{
    int32_t transaction_locked = 0;
    reach_result result = service->store.ops.begin_transaction != nullptr
                              ? service->store.ops.begin_transaction(service->store.store)
                              : REACH_OK;
    transaction_locked = result == REACH_OK;
    if (result == REACH_OK)
    {
        result = service->store.ops.load(service->store.store, out_snapshot);
    }
    if (transaction_locked && service->store.ops.end_transaction != nullptr)
    {
        service->store.ops.end_transaction(service->store.store);
    }
    return result;
}

static void reach_config_service_emit(reach_config_service *service,
                                      reach_config_service_event event)
{
    reach_config_service_notify notify = nullptr;
    void *notify_user = nullptr;
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        if (!service->stop)
        {
            notify = service->notify;
            notify_user = service->notify_user;
        }
    }
    if (notify != nullptr)
    {
        notify(notify_user, event);
    }
}

static void reach_config_service_thread_main(reach_config_service *service)
{
    for (;;)
    {
        reach_config_work_type work = REACH_CONFIG_WORK_NONE;
        reach_config_snapshot snapshot = {};
        std::vector<reach_config_operation> operations;
        {
            std::unique_lock<std::mutex> lock(service->mutex);
            service->cv.wait(lock, [service]() {
                return service->stop || service->save_pending || service->reload_pending;
            });
            if (service->stop && !service->save_pending && !service->reload_pending)
            {
                return;
            }
            if (service->save_pending)
            {
                work = REACH_CONFIG_WORK_SAVE;
                operations.swap(service->pending_operations);
                service->save_pending = 0;
            }
            else if (service->reload_pending)
            {
                work = REACH_CONFIG_WORK_RELOAD;
                service->reload_pending = 0;
            }
            service->in_flight = 1;
        }

        reach_result result = REACH_INVALID_ARGUMENT;
        int32_t publish_snapshot = 0;
        if (work == REACH_CONFIG_WORK_SAVE)
        {
            int32_t transaction_locked = 0;
            result = service->store.ops.begin_transaction != nullptr
                         ? service->store.ops.begin_transaction(service->store.store)
                         : REACH_OK;
            transaction_locked = result == REACH_OK;
            if (result == REACH_OK)
            {
                result = service->store.ops.load != nullptr
                             ? service->store.ops.load(service->store.store, &snapshot)
                             : REACH_INVALID_ARGUMENT;
            }
            if (result == REACH_OK)
            {
                result = reach_config_apply_operations(&snapshot, operations);
            }
            if (result == REACH_OK)
            {
                result = service->store.ops.save != nullptr
                             ? service->store.ops.save(service->store.store, &snapshot)
                             : REACH_INVALID_ARGUMENT;
            }
            if (transaction_locked && service->store.ops.end_transaction != nullptr)
            {
                service->store.ops.end_transaction(service->store.store);
            }
            {
                std::lock_guard<std::mutex> lock(service->mutex);
                service->last_persist_result = result;
                if (result == REACH_OK)
                {
                    reach_config_snapshot current = snapshot;
                    if (reach_config_apply_operations(&current, service->pending_operations) ==
                            REACH_OK &&
                        std::memcmp(&current, &service->snapshot, sizeof(current)) != 0)
                    {
                        service->snapshot = current;
                        ++service->generation;
                        service->published_generation = service->generation;
                        publish_snapshot = 1;
                    }
                    service->dirty = !service->pending_operations.empty();
                    service->save_pending = service->dirty;
                }
                else
                {
                    std::vector<reach_config_operation> pending;
                    pending.reserve(operations.size() + service->pending_operations.size());
                    pending.insert(pending.end(), operations.begin(), operations.end());
                    pending.insert(pending.end(), service->pending_operations.begin(),
                                   service->pending_operations.end());
                    service->pending_operations.swap(pending);
                    service->dirty = 1;
                }
            }
        }
        else if (work == REACH_CONFIG_WORK_RELOAD)
        {
            result = reach_config_service_load_store(service, &snapshot);
            if (result == REACH_OK)
            {
                std::lock_guard<std::mutex> lock(service->mutex);
                result = reach_config_apply_operations(&snapshot, service->pending_operations);
                if (result == REACH_OK)
                {
                    service->snapshot = snapshot;
                    ++service->generation;
                    service->published_generation = service->generation;
                    service->dirty = !service->pending_operations.empty();
                    service->save_pending = service->dirty;
                    publish_snapshot = 1;
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(service->mutex);
            service->in_flight = 0;
        }
        if (publish_snapshot)
        {
            reach_config_service_emit(service, REACH_CONFIG_SERVICE_SNAPSHOT_CHANGED);
        }
        if (work == REACH_CONFIG_WORK_SAVE)
        {
            reach_config_service_emit(service, result == REACH_OK ? REACH_CONFIG_SERVICE_PERSISTED
                                                                  : REACH_CONFIG_SERVICE_PERSIST_FAILED);
        }
        service->cv.notify_all();
    }
}

static reach_result reach_config_service_ensure_thread(reach_config_service *service)
{
    if (service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    std::lock_guard<std::mutex> lock(service->mutex);
    if (service->thread_started)
    {
        return REACH_OK;
    }
    service->stop = 0;
    try
    {
        service->thread = std::thread(reach_config_service_thread_main, service);
    }
    catch (...)
    {
        return REACH_ERROR;
    }
    service->thread_started = 1;
    return REACH_OK;
}

static reach_result reach_config_service_commit(reach_config_service *service,
                                                const reach_config_operation *operation)
{
    reach_result result = reach_config_service_ensure_thread(service);
    if (result != REACH_OK || operation == nullptr)
    {
        return result != REACH_OK ? result : REACH_INVALID_ARGUMENT;
    }
    int32_t changed = 0;
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        reach_config_snapshot candidate = service->snapshot;
        result = reach_config_apply_operation(&candidate, operation, &changed);
        if (result != REACH_OK || !changed)
        {
            return result;
        }
        try
        {
            service->pending_operations.push_back(*operation);
        }
        catch (...)
        {
            return REACH_ERROR;
        }
        service->snapshot = candidate;
        ++service->generation;
        service->published_generation = service->generation;
        service->save_pending = 1;
        service->dirty = 1;
    }
    reach_config_service_emit(service, REACH_CONFIG_SERVICE_SNAPSHOT_CHANGED);
    service->cv.notify_one();
    return REACH_OK;
}

reach_result reach_config_service_create(reach_config_store_port store,
                                         reach_config_service_notify notify, void *notify_user,
                                         reach_config_service **out_service)
{
    if (out_service == nullptr || store.store == nullptr || store.ops.load == nullptr ||
        store.ops.save == nullptr ||
        (store.ops.begin_transaction == nullptr) != (store.ops.end_transaction == nullptr))
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_service = nullptr;
    reach_config_service *service = new (std::nothrow) reach_config_service();
    if (service == nullptr)
    {
        return REACH_ERROR;
    }
    service->store = store;
    service->notify = notify;
    service->notify_user = notify_user;
    service->last_persist_result = REACH_OK;

    reach_result result = reach_config_service_load_store(service, &service->snapshot);
    if (result != REACH_OK)
    {
        delete service;
        return result;
    }

    service->generation = 1;
    service->published_generation = 1;
    service->consumed_generation = 1;
    *out_service = service;
    return REACH_OK;
}

void reach_config_service_destroy(reach_config_service *service)
{
    if (service == nullptr)
    {
        return;
    }
    reach_config_service_stop(service);
    delete service;
}

void reach_config_service_stop(reach_config_service *service)
{
    if (service == nullptr || !service->thread_started)
    {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        service->stop = 1;
        service->reload_pending = 0;
    }
    service->cv.notify_all();
    if (service->thread.joinable())
    {
        service->thread.join();
    }
    service->thread_started = 0;
}

reach_result reach_config_service_flush(reach_config_service *service)
{
    if (service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    std::unique_lock<std::mutex> lock(service->mutex);
    service->cv.wait(lock, [service]() {
        return !service->save_pending && !service->in_flight;
    });
    return service->dirty ? service->last_persist_result : REACH_OK;
}

reach_result reach_config_service_snapshot(const reach_config_service *service,
                                           reach_config_snapshot *out_snapshot)
{
    if (service == nullptr || out_snapshot == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_config_service *mutable_service = const_cast<reach_config_service *>(service);
    std::lock_guard<std::mutex> lock(mutable_service->mutex);
    *out_snapshot = mutable_service->snapshot;
    return REACH_OK;
}

int32_t reach_config_service_take_snapshot_update(reach_config_service *service,
                                                  reach_config_snapshot *out_snapshot)
{
    if (service == nullptr || out_snapshot == nullptr)
    {
        return 0;
    }
    std::lock_guard<std::mutex> lock(service->mutex);
    if (service->published_generation == service->consumed_generation)
    {
        return 0;
    }
    *out_snapshot = service->snapshot;
    service->consumed_generation = service->published_generation;
    return 1;
}

reach_result reach_config_service_reload(reach_config_service *service)
{
    reach_result result = reach_config_service_ensure_thread(service);
    if (result != REACH_OK)
    {
        return result;
    }
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        service->reload_pending = 1;
    }
    service->cv.notify_one();
    return REACH_OK;
}

reach_result reach_config_service_ensure_defaults(reach_config_service *service)
{
    reach_config_operation operation = {};
    operation.type = REACH_CONFIG_OPERATION_ENSURE_DEFAULTS;
    return reach_config_service_commit(service, &operation);
}

reach_result reach_config_service_pin_path(reach_config_service *service, const uint16_t *path)
{
    if (path == nullptr || path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_config_operation operation = {};
    operation.type = REACH_CONFIG_OPERATION_PIN_PATH;
    (void)reach_copy_utf16(operation.path, 260, path);
    return reach_config_service_commit(service, &operation);
}

reach_result reach_config_service_pin_app(reach_config_service *service,
                                          const reach_pinned_app_model *app)
{
    if (app == nullptr || app->path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_config_operation operation = {};
    operation.type = REACH_CONFIG_OPERATION_PIN_APP;
    operation.app = *app;
    return reach_config_service_commit(service, &operation);
}

reach_result reach_config_service_unpin_id(reach_config_service *service, uint32_t pin_id)
{
    if (pin_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_config_operation operation = {};
    operation.type = REACH_CONFIG_OPERATION_UNPIN_ID;
    operation.pin_id = pin_id;
    return reach_config_service_commit(service, &operation);
}

reach_result reach_config_service_unpin_path(reach_config_service *service, const uint16_t *path)
{
    if (path == nullptr || path[0] == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_config_operation operation = {};
    operation.type = REACH_CONFIG_OPERATION_UNPIN_PATH;
    (void)reach_copy_utf16(operation.path, 260, path);
    return reach_config_service_commit(service, &operation);
}

reach_result reach_config_service_move_pin(reach_config_service *service, uint32_t pin_id,
                                           size_t target_index)
{
    if (pin_id == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_config_operation operation = {};
    operation.type = REACH_CONFIG_OPERATION_MOVE_PIN;
    operation.pin_id = pin_id;
    operation.target_index = target_index;
    return reach_config_service_commit(service, &operation);
}

reach_result reach_config_service_set_power(reach_config_service *service,
                                            const reach_config_power_settings *settings)
{
    if (settings == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_config_operation operation = {};
    operation.type = REACH_CONFIG_OPERATION_SET_POWER;
    operation.power = *settings;
    return reach_config_service_commit(service, &operation);
}

reach_result reach_config_service_set_display(reach_config_service *service,
                                              const reach_config_display_settings *settings)
{
    if (settings == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_config_operation operation = {};
    operation.type = REACH_CONFIG_OPERATION_SET_DISPLAY;
    operation.display = *settings;
    return reach_config_service_commit(service, &operation);
}

reach_result reach_config_service_set_wallpapers(
    reach_config_service *service, const uint16_t *wallpaper_path,
    const uint16_t monitor_wallpaper_paths[][260], size_t monitor_count)
{
    if (wallpaper_path == nullptr || monitor_wallpaper_paths == nullptr ||
        monitor_count > REACH_MAX_WALLPAPER_MONITORS)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_config_operation operation = {};
    operation.type = REACH_CONFIG_OPERATION_SET_WALLPAPERS;
    operation.monitor_count = monitor_count;
    (void)reach_copy_utf16(operation.path, 260, wallpaper_path);
    for (size_t index = 0; index < monitor_count; ++index)
    {
        (void)reach_copy_utf16(operation.monitor_wallpaper_paths[index], 260,
                               monitor_wallpaper_paths[index]);
    }
    return reach_config_service_commit(service, &operation);
}

reach_result reach_config_service_set_monitor_wallpaper(reach_config_service *service,
                                                        size_t monitor_index,
                                                        const uint16_t *path)
{
    if (path == nullptr || monitor_index >= REACH_MAX_WALLPAPER_MONITORS)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_config_operation operation = {};
    operation.type = REACH_CONFIG_OPERATION_SET_MONITOR_WALLPAPER;
    operation.target_index = monitor_index;
    (void)reach_copy_utf16(operation.path, 260, path);
    return reach_config_service_commit(service, &operation);
}

int32_t reach_config_service_dirty(const reach_config_service *service)
{
    if (service == nullptr)
    {
        return 0;
    }
    reach_config_service *mutable_service = const_cast<reach_config_service *>(service);
    std::lock_guard<std::mutex> lock(mutable_service->mutex);
    return mutable_service->dirty;
}
