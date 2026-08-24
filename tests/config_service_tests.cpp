#include "reach/services/config.h"

#include <atomic>
#include <condition_variable>
#include <mutex>

struct test_config_store
{
    reach_config_snapshot snapshot;
    std::mutex mutex;
    std::condition_variable cv;
    int32_t block_save;
    int32_t save_entered;
    int32_t allow_save;
    int32_t fail_save;
    int32_t save_count;
    std::atomic<int32_t> transaction_depth;
    std::atomic<int32_t> transaction_count;
    std::atomic<int32_t> access_outside_transaction;
};

struct test_notifications
{
    std::atomic<int32_t> snapshot_changed;
    std::atomic<int32_t> persisted;
    std::atomic<int32_t> persist_failed;
};

static int expect(int condition)
{
    return condition ? 0 : 1;
}

static void copy_ascii(uint16_t *dst, size_t dst_count, const char *src)
{
    size_t index = 0;
    while (index + 1 < dst_count && src[index] != 0)
    {
        dst[index] = (uint16_t)src[index];
        ++index;
    }
    dst[index] = 0;
}

static reach_result test_load(reach_config_store *store, reach_config_snapshot *out_snapshot)
{
    if (store == nullptr || out_snapshot == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    test_config_store *actual = reinterpret_cast<test_config_store *>(store);
    if (actual->transaction_depth.load() == 0)
    {
        ++actual->access_outside_transaction;
    }
    std::lock_guard<std::mutex> lock(actual->mutex);
    *out_snapshot = actual->snapshot;
    return REACH_OK;
}

static reach_result test_save(reach_config_store *store, const reach_config_snapshot *snapshot)
{
    if (store == nullptr || snapshot == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    test_config_store *actual = reinterpret_cast<test_config_store *>(store);
    if (actual->transaction_depth.load() == 0)
    {
        ++actual->access_outside_transaction;
    }
    std::unique_lock<std::mutex> lock(actual->mutex);
    actual->save_entered = 1;
    actual->cv.notify_all();
    actual->cv.wait(lock, [actual]() { return !actual->block_save || actual->allow_save; });
    if (actual->fail_save)
    {
        return REACH_ERROR;
    }
    actual->snapshot = *snapshot;
    ++actual->save_count;
    return REACH_OK;
}

static reach_result test_begin_transaction(reach_config_store *store)
{
    if (store == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    test_config_store *actual = reinterpret_cast<test_config_store *>(store);
    ++actual->transaction_depth;
    ++actual->transaction_count;
    return REACH_OK;
}

static void test_end_transaction(reach_config_store *store)
{
    if (store != nullptr)
    {
        test_config_store *actual = reinterpret_cast<test_config_store *>(store);
        --actual->transaction_depth;
    }
}

static void test_notify(void *user, reach_config_service_event event)
{
    test_notifications *notifications = static_cast<test_notifications *>(user);
    if (event == REACH_CONFIG_SERVICE_SNAPSHOT_CHANGED)
    {
        ++notifications->snapshot_changed;
    }
    else if (event == REACH_CONFIG_SERVICE_PERSISTED)
    {
        ++notifications->persisted;
    }
    else if (event == REACH_CONFIG_SERVICE_PERSIST_FAILED)
    {
        ++notifications->persist_failed;
    }
}

static reach_config_store_port test_port(test_config_store *store)
{
    reach_config_store_port port = {};
    port.store = reinterpret_cast<reach_config_store *>(store);
    port.ops.begin_transaction = test_begin_transaction;
    port.ops.end_transaction = test_end_transaction;
    port.ops.load = test_load;
    port.ops.save = test_save;
    return port;
}

static void seed_pin(reach_config_snapshot *snapshot, size_t index, uint32_t id, const char *path)
{
    snapshot->pinned_apps[index].id = id;
    copy_ascii(snapshot->pinned_apps[index].path, 260, path);
    copy_ascii(snapshot->pinned_apps[index].icon_ref, 260, path);
}

int main()
{
    int failed = 0;

    static test_config_store store = {};
    store.snapshot.pinned_app_count = 3;
    seed_pin(&store.snapshot, 0, 1, "one.exe");
    seed_pin(&store.snapshot, 1, 2, "two.exe");
    seed_pin(&store.snapshot, 2, 3, "three.exe");
    store.block_save = 1;

    test_notifications notifications = {};
    reach_config_service *service = nullptr;
    failed += expect(reach_config_service_create(test_port(&store), test_notify, &notifications,
                                                 &service) == REACH_OK);
    failed += expect(reach_config_service_unpin_id(service, 1) == REACH_OK);
    failed += expect(notifications.snapshot_changed.load() == 1);

    static reach_config_snapshot live = {};
    failed += expect(reach_config_service_snapshot(service, &live) == REACH_OK);
    failed += expect(live.pinned_app_count == 2);
    failed += expect(live.pinned_apps[0].id == 2);

    {
        std::unique_lock<std::mutex> lock(store.mutex);
        store.cv.wait(lock, []() { return store.save_entered != 0; });
    }

    failed += expect(reach_config_service_unpin_id(service, 2) == REACH_OK);
    failed += expect(reach_config_service_move_pin(service, 3, 0) == REACH_OK);
    failed += expect(notifications.snapshot_changed.load() == 2);

    {
        std::lock_guard<std::mutex> lock(store.mutex);
        store.allow_save = 1;
    }
    store.cv.notify_all();
    failed += expect(reach_config_service_flush(service) == REACH_OK);
    failed += expect(store.save_count == 2);
    failed += expect(store.snapshot.pinned_app_count == 1);
    failed += expect(store.snapshot.pinned_apps[0].id == 3);
    failed += expect(reach_config_service_dirty(service) == 0);
    failed += expect(store.transaction_count.load() == 3);
    failed += expect(store.access_outside_transaction.load() == 0);
    reach_config_service_destroy(service);

    static test_config_store rebased_store = {};
    rebased_store.snapshot.pinned_app_count = 2;
    seed_pin(&rebased_store.snapshot, 0, 1, "one.exe");
    seed_pin(&rebased_store.snapshot, 1, 2, "two.exe");
    test_notifications rebased_notifications = {};
    service = nullptr;
    failed += expect(reach_config_service_create(test_port(&rebased_store), test_notify,
                                                 &rebased_notifications, &service) == REACH_OK);
    {
        std::lock_guard<std::mutex> lock(rebased_store.mutex);
        rebased_store.snapshot.light_theme = 1;
    }
    failed += expect(reach_config_service_unpin_id(service, 1) == REACH_OK);
    failed += expect(reach_config_service_flush(service) == REACH_OK);
    failed += expect(rebased_store.snapshot.light_theme == 1);
    failed += expect(rebased_store.snapshot.pinned_app_count == 1);
    live = {};
    failed += expect(reach_config_service_snapshot(service, &live) == REACH_OK);
    failed += expect(live.light_theme == 1);
    failed += expect(live.pinned_app_count == 1);
    reach_config_service_destroy(service);

    static test_config_store failed_store = {};
    failed_store.snapshot.pinned_app_count = 1;
    seed_pin(&failed_store.snapshot, 0, 1, "one.exe");
    failed_store.fail_save = 1;
    test_notifications failed_notifications = {};
    service = nullptr;
    failed += expect(reach_config_service_create(test_port(&failed_store), test_notify,
                                                 &failed_notifications, &service) == REACH_OK);
    failed += expect(reach_config_service_unpin_id(service, 1) == REACH_OK);
    failed += expect(reach_config_service_flush(service) == REACH_ERROR);
    failed += expect(reach_config_service_dirty(service) == 1);
    failed += expect(failed_notifications.persist_failed.load() == 1);
    failed_store.fail_save = 0;
    const uint16_t retry_path[] = {'r', 'e', 't', 'r', 'y', '.', 'j', 'p', 'g', 0};
    failed +=
        expect(reach_config_service_set_monitor_wallpaper(service, 0, retry_path) == REACH_OK);
    failed += expect(reach_config_service_flush(service) == REACH_OK);
    failed += expect(failed_store.snapshot.pinned_app_count == 0);
    failed += expect(failed_store.snapshot.monitor_wallpaper_paths[0][0] == 'r');
    failed += expect(reach_config_service_dirty(service) == 0);
    failed += expect(failed_store.access_outside_transaction.load() == 0);
    reach_config_service_destroy(service);

    return failed == 0 ? 0 : 1;
}
