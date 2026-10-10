#include "reach/adapters/windows/frame_wait.h"
#include "reach/platform/windows_adapters.h"

#include <windows.h>
#include <atomic>
#include <stdio.h>

static int failures;

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        ++failures;
        printf("FAIL: %s\n", message);
    }
}

static double now_seconds(void)
{
    LARGE_INTEGER counter = {};
    LARGE_INTEGER frequency = {};
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (double)counter.QuadPart / frequency.QuadPart;
}

static void test_native_wait_preserves_events(void)
{
    void *timer = reach_windows_frame_timer_create();
    expect_true(timer != nullptr, "native frame timer is created");
    HANDLE events[] = {CreateEventW(nullptr, FALSE, FALSE, nullptr),
                       CreateEventW(nullptr, FALSE, FALSE, nullptr)};
    expect_true(reach_windows_frame_wait(timer, events, 2, 10, now_seconds() + 0.001) ==
                    WAIT_TIMEOUT,
                "the frame timer wakes as a timeout");
    SetEvent(events[1]);
    expect_true(reach_windows_frame_wait(timer, events, 2, 100, now_seconds() + 0.1) ==
                    WAIT_OBJECT_0 + 1,
                "an activation event keeps its original wait index");
    SetEvent(events[0]);
    expect_true(reach_windows_frame_wait(timer, events, 2, 100, -1.0) == WAIT_OBJECT_0,
                "idle cancels the outstanding timer and preserves process events");
    MSG message = {};
    PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);
    PostThreadMessageW(GetCurrentThreadId(), WM_NULL, 0, 0);
    expect_true(reach_windows_frame_wait(timer, events, 2, 100, now_seconds() + 0.1) ==
                    WAIT_OBJECT_0 + 2,
                "message wakeups keep their original wait index");
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    {
    }
    expect_true(reach_windows_frame_wait(nullptr, nullptr, 0, 0, -1.0) == WAIT_TIMEOUT,
                "a missing timer preserves timeout fallback");
    CloseHandle(events[0]);
    CloseHandle(events[1]);
    reach_windows_frame_timer_destroy(timer);
}

struct power_notifications
{
    HANDLE changed;
    std::atomic<int32_t> count;
};

static void power_changed(void *user, uint32_t flags)
{
    auto *notifications = static_cast<power_notifications *>(user);
    if ((flags & REACH_SYSTEM_CONTROLS_CHANGE_POWER) != 0)
    {
        notifications->count.fetch_add(1);
        SetEvent(notifications->changed);
    }
}

static void test_power_notification_lifecycle(void)
{
    reach_system_controls_port controls = {};
    expect_true(reach_windows_create_system_controls(&controls) == REACH_OK,
                "system controls adapter is created");
    power_notifications notifications = {};
    notifications.changed = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    for (int cycle = 0; cycle < 2; ++cycle)
    {
        ResetEvent(notifications.changed);
        expect_true(controls.start_watching(controls.userdata, power_changed, &notifications) ==
                        REACH_OK,
                    "power notifications can start and restart");
        expect_true(WaitForSingleObject(notifications.changed, 2000) == WAIT_OBJECT_0,
                    "power registration publishes the current state without a toggle");
        controls.stop_watching(controls.userdata);
    }
    expect_true(notifications.count.load() >= 2, "each watcher lifetime receives a power update");
    controls.destroy(controls.userdata);
    CloseHandle(notifications.changed);
}

int main(void)
{
    test_native_wait_preserves_events();
    test_power_notification_lifecycle();
    return failures != 0;
}
