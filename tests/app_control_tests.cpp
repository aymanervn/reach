#include "reach/services/app_control.h"

#include <atomic>
#include <chrono>
#include <stdio.h>
#include <thread>

static int failures;

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        ++failures;
        fprintf(stderr, "FAILED: %s\n", message);
    }
}

enum
{
    FAKE_MAX_CALLS = 16
};

static uintptr_t fake_closed[FAKE_MAX_CALLS];
static size_t fake_closed_count;
static size_t fake_minimized_count;
static size_t fake_activated_count;
static std::atomic<int> fake_notified;

static reach_result fake_close(reach_window_manager *manager, reach_window_id window_id)
{
    (void)manager;
    if (fake_closed_count < FAKE_MAX_CALLS)
    {
        fake_closed[fake_closed_count++] = window_id;
    }
    return REACH_OK;
}

static reach_result fake_minimize(reach_window_manager *manager, reach_window_id window_id)
{
    (void)manager;
    (void)window_id;
    ++fake_minimized_count;
    return REACH_OK;
}

static reach_result fake_activate(reach_window_manager *manager, reach_window_id window_id)
{
    (void)manager;
    (void)window_id;
    ++fake_activated_count;
    return REACH_OK;
}

static int32_t fake_privileged_available(const reach_window_manager *manager)
{
    (void)manager;
    return 1;
}

static void fake_notify(void *user)
{
    (void)user;
    fake_notified.store(1);
}

static int wait_for_completion(reach_app_control *service, reach_result *out_result)
{
    for (int attempt = 0; attempt < 500; ++attempt)
    {
        if (fake_notified.load() && reach_app_control_take_window_completed(service, out_result))
        {
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return 0;
}

static void test_schedule_windows_closes_every_window(void)
{
    fake_closed_count = 0;
    fake_minimized_count = 0;
    fake_activated_count = 0;
    fake_notified.store(0);

    reach_window_manager_port window_manager = {};
    window_manager.ops.close = fake_close;
    window_manager.ops.minimize = fake_minimize;
    window_manager.ops.activate = fake_activate;
    window_manager.ops.privileged_control_available = fake_privileged_available;

    reach_app_control *service = nullptr;
    reach_app_launcher_port launcher = {};
    reach_explorer_service_port explorer = {};
    if (reach_app_control_create(launcher, explorer, window_manager, fake_notify, nullptr,
                                 &service) != REACH_OK ||
        service == nullptr)
    {
        ++failures;
        fprintf(stderr, "FAILED: app control creation\n");
        return;
    }

    const uintptr_t windows[3] = {11, 22, 33};
    expect_true(reach_app_control_schedule_windows(service, REACH_WINDOW_CONTROL_CLOSE, windows,
                                                   3) == REACH_OK,
                "scheduling a batch of closes succeeds");

    reach_result result = REACH_ERROR;
    expect_true(wait_for_completion(service, &result), "batch reports completion");
    expect_true(result == REACH_OK, "batch completes without error");
    expect_true(fake_closed_count == 3, "every window in the batch is closed");
    expect_true(fake_closed[0] == 11 && fake_closed[1] == 22 && fake_closed[2] == 33,
                "windows are closed in the order given");
    expect_true(fake_minimized_count == 0 && fake_activated_count == 0,
                "a close batch uses only the close action");

    reach_app_control_destroy(service);
}

int main(void)
{
    test_schedule_windows_closes_every_window();
    return failures == 0 ? 0 : 1;
}
