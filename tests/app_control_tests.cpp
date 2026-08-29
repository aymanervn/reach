#include "reach/services/app_control.h"
#include "test_utf16.h"

#include <atomic>
#include <chrono>
#include <stdio.h>
#include <functional>
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
static std::atomic<int> fake_terminal_called;
static reach_terminal_launch_request fake_terminal_request;

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

static reach_result fake_terminal_launch(reach_terminal_launcher *launcher,
                                         const reach_terminal_launch_request *request)
{
    (void)launcher;
    if (request == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    fake_terminal_request = *request;
    fake_terminal_called.store(1);
    return REACH_OK;
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

static std::atomic<unsigned long long> fake_explorer_thread;
static std::atomic<int> fake_path_exists_calls;
static std::atomic<int> fake_shell_calls;
static std::atomic<int> fake_open_path_calls;
static std::atomic<int> fake_open_default_calls;

static unsigned long long current_thread_id(void)
{
    return (unsigned long long)std::hash<std::thread::id>{}(std::this_thread::get_id());
}

static int32_t fake_path_exists(reach_explorer_service *service, const uint16_t *path)
{
    (void)service;
    fake_explorer_thread.store(current_thread_id());
    ++fake_path_exists_calls;
    return path != nullptr && path[0] == 'y';
}

static reach_result fake_open_path(reach_explorer_service *service, const uint16_t *path)
{
    (void)service;
    (void)path;
    fake_explorer_thread.store(current_thread_id());
    ++fake_open_path_calls;
    return REACH_OK;
}

static reach_result fake_open_shell(reach_explorer_service *service, const uint16_t *location)
{
    (void)service;
    (void)location;
    fake_explorer_thread.store(current_thread_id());
    ++fake_shell_calls;
    return REACH_OK;
}

static reach_result fake_open_default(reach_explorer_service *service)
{
    (void)service;
    fake_explorer_thread.store(current_thread_id());
    ++fake_open_default_calls;
    return REACH_OK;
}

static int wait_for_explorer(const std::atomic<int> &counter)
{
    for (int attempt = 0; attempt < 500; ++attempt)
    {
        if (counter.load() > 0)
        {
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return 0;
}

/* Opening a location is a blocking ShellExecute and the existence probe can stall on a
   disconnected share, so neither may touch the port on the thread that asked. */
static void test_open_location_never_touches_the_port_on_the_caller(void)
{
    reach_app_launcher_port launcher = {};
    reach_terminal_launcher_port terminal_launcher = {};
    reach_window_manager_port window_manager = {};
    reach_explorer_service_port explorer = {};
    explorer.service = reinterpret_cast<reach_explorer_service *>(1);
    explorer.ops.path_exists = fake_path_exists;
    explorer.ops.open_path = fake_open_path;
    explorer.ops.open_shell_location = fake_open_shell;
    explorer.ops.open_default = fake_open_default;

    reach_app_control *service = nullptr;
    if (reach_app_control_create(launcher, terminal_launcher, explorer, window_manager, nullptr,
                                 nullptr, &service) != REACH_OK)
    {
        ++failures;
        fprintf(stderr, "FAILED: open-location test could not create app control\n");
        return;
    }

    const unsigned long long caller = current_thread_id();
    fake_explorer_thread.store(caller);

    const uint16_t shell_location[] = {'s', 'h', 'e', 'l', 'l', 0};
    expect_true(reach_app_control_schedule_open_location(
                    service, REACH_APP_CONTROL_LOCATION_SHELL, shell_location) == REACH_OK,
                "a shell location is accepted for scheduling");
    expect_true(fake_shell_calls.load() == 0,
                "scheduling a shell location does not open it on the caller");
    expect_true(wait_for_explorer(fake_shell_calls), "the worker opens the shell location");
    expect_true(fake_explorer_thread.load() != caller,
                "the shell location is opened off the calling thread");

    const uint16_t missing[] = {'n', 'o', 'p', 'e', 0};
    expect_true(reach_app_control_schedule_open_location(
                    service, REACH_APP_CONTROL_LOCATION_PATH, missing) == REACH_OK,
                "a path is accepted for scheduling");
    expect_true(fake_path_exists_calls.load() == 0,
                "the existence probe does not run on the caller");
    expect_true(wait_for_explorer(fake_open_default_calls),
                "a path that does not exist falls back to the default location");
    expect_true(fake_open_path_calls.load() == 0, "and does not open the missing path");
    expect_true(fake_explorer_thread.load() != caller,
                "the existence probe runs off the calling thread");

    reach_app_control_stop(service);
    reach_app_control_destroy(service);
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
    reach_terminal_launcher_port terminal_launcher = {};
    if (reach_app_control_create(launcher, terminal_launcher, explorer, window_manager, fake_notify,
                                 nullptr, &service) != REACH_OK ||
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

static void test_terminal_command_is_queued_unchanged(void)
{
    fake_terminal_called.store(0);
    fake_terminal_request = {};

    reach_terminal_launcher_port terminal_launcher = {};
    terminal_launcher.ops.launch = fake_terminal_launch;
    reach_app_control *service = nullptr;
    reach_app_launcher_port launcher = {};
    reach_explorer_service_port explorer = {};
    reach_window_manager_port window_manager = {};
    if (reach_app_control_create(launcher, terminal_launcher, explorer, window_manager, nullptr,
                                 nullptr, &service) != REACH_OK ||
        service == nullptr)
    {
        ++failures;
        fprintf(stderr, "FAILED: terminal app control creation\n");
        return;
    }

    reach_terminal_launch_request request = {};
    const char *command = "ls | Where-Object { $_.Length -gt 0 }; Write-Output \"done\"";
    for (size_t index = 0; command[index] != 0; ++index)
    {
        request.command[index] = (uint16_t)(unsigned char)command[index];
    }

    expect_true(reach_app_control_schedule_terminal_launch(service, &request) == REACH_OK,
                "terminal command scheduling succeeds");
    for (int attempt = 0; attempt < 500 && !fake_terminal_called.load(); ++attempt)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    expect_true(fake_terminal_called.load(), "terminal command reaches the launch port");
    expect_true(reach_test_utf16_equals_ascii(fake_terminal_request.command, command),
                "terminal command reaches the launch port unchanged");

    reach_app_control_destroy(service);
}

int main(void)
{
    test_open_location_never_touches_the_port_on_the_caller();
    test_schedule_windows_closes_every_window();
    test_terminal_command_is_queued_unchanged();
    return failures == 0 ? 0 : 1;
}
