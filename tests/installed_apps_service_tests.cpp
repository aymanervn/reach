#include "reach/services/installed_apps.h"

#include "reach/support/util.h"

#include <condition_variable>
#include <mutex>
#include <memory>
#include <stdio.h>
#include <string.h>

static int failures = 0;
static std::mutex notify_mutex;
static std::condition_variable notify_cv;
static int notify_count = 0;
static int open_count = 0;
static int uninstall_count = 0;
static uint16_t opened_aumid[260] = {};

static void expect_true(int value, const char *message)
{
    if (!value)
    {
        ++failures;
        printf("FAIL: %s\n", message);
    }
}

static int equals_ascii(const uint16_t *value, const char *expected)
{
    size_t index = 0;
    while (expected[index] != 0 && value[index] == (uint16_t)(unsigned char)expected[index])
    {
        ++index;
    }
    return expected[index] == 0 && value[index] == 0;
}

static reach_result enumerate_apps(reach_installed_apps *, reach_installed_app_list *out_list)
{
    memset(out_list, 0, sizeof(*out_list));
    out_list->count = 3;
    reach_installed_app *entry = &out_list->entries[0];
    reach_copy_ascii_to_utf16(entry->display_name, REACH_INSTALLED_APP_NAME_CAPACITY, "Notepad");
    entry->kind = REACH_INSTALLED_APP_DESKTOP;
    entry->can_open = 1;

    entry = &out_list->entries[1];
    reach_copy_ascii_to_utf16(entry->display_name, REACH_INSTALLED_APP_NAME_CAPACITY,
                              "Windows Security");
    reach_copy_ascii_to_utf16(entry->package_full_name, REACH_INSTALLED_APP_TEXT_CAPACITY,
                              "Microsoft.SecHealthUI_test");
    entry->kind = REACH_INSTALLED_APP_PACKAGED;
    entry->can_open = 1;
    entry->can_manage = 1;

    entry = &out_list->entries[2];
    reach_copy_ascii_to_utf16(entry->display_name, REACH_INSTALLED_APP_NAME_CAPACITY, "ChatGPT");
    reach_copy_ascii_to_utf16(entry->launch_path, REACH_INSTALLED_APP_TEXT_CAPACITY,
                              "shell:AppsFolder\\OpenAI.Codex_test!App");
    reach_copy_ascii_to_utf16(entry->app_user_model_id, REACH_INSTALLED_APP_TEXT_CAPACITY,
                              "OpenAI.Codex_test!App");
    reach_copy_ascii_to_utf16(entry->package_full_name, REACH_INSTALLED_APP_TEXT_CAPACITY,
                              "OpenAI.Codex_1.0.0.0_x64__test");
    entry->kind = REACH_INSTALLED_APP_PACKAGED;
    entry->can_open = 1;
    entry->can_uninstall = 1;
    entry->can_manage = 1;
    return REACH_OK;
}

static reach_result uninstall_app(reach_installed_apps *, const reach_installed_app *entry)
{
    if (entry != nullptr && entry->package_full_name[0] != 0)
    {
        ++uninstall_count;
        return REACH_OK;
    }
    return REACH_ERROR;
}

static reach_result launch_app(reach_app_launcher *, const reach_app_launch_request *request,
                               reach_app_launch_failure *out_failure)
{
    if (out_failure != nullptr)
    {
        *out_failure = REACH_APP_LAUNCH_FAILURE_NONE;
    }
    ++open_count;
    reach_copy_utf16(opened_aumid, 260, request->app_user_model_id);
    return REACH_OK;
}

static void notify(void *)
{
    {
        std::lock_guard<std::mutex> lock(notify_mutex);
        ++notify_count;
    }
    notify_cv.notify_one();
}

static void wait_for_notify(int expected)
{
    std::unique_lock<std::mutex> lock(notify_mutex);
    notify_cv.wait_for(lock, std::chrono::seconds(2),
                       [expected]() { return notify_count >= expected; });
}

int main(void)
{
    reach_installed_apps_port port = {};
    port.ops.enumerate = enumerate_apps;
    port.ops.uninstall = uninstall_app;
    reach_app_launcher_port launcher = {};
    launcher.ops.launch = launch_app;
    reach_installed_apps_service *service = nullptr;
    expect_true(reach_installed_apps_service_create(port, launcher, notify, nullptr, &service) ==
                    REACH_OK,
                "service creates");

    reach_installed_apps_service_refresh(service);
    wait_for_notify(1);
    std::unique_ptr<reach_installed_apps_snapshot> snapshot(new reach_installed_apps_snapshot());
    expect_true(reach_installed_apps_service_take(service, snapshot.get()), "refresh completes");
    expect_true(snapshot->apps.count == 1, "refresh keeps only manageable packaged apps");
    expect_true(equals_ascii(snapshot->apps.entries[0].display_name, "ChatGPT"),
                "refresh removes desktop and internal Windows apps");

    reach_installed_apps_service_open(service, 0);
    wait_for_notify(2);
    expect_true(reach_installed_apps_service_take(service, snapshot.get()), "open completes");
    expect_true(open_count == 1 && equals_ascii(opened_aumid, "OpenAI.Codex_test!App"),
                "open preserves the filtered app's AppUserModelID");

    reach_installed_apps_service_uninstall(service, 0);
    wait_for_notify(3);
    expect_true(reach_installed_apps_service_take(service, snapshot.get()), "uninstall completes");
    expect_true(uninstall_count == 1 && snapshot->command_succeeded,
                "uninstall delegates to the Windows boundary");

    reach_installed_apps_service_destroy(service);
    if (failures == 0)
    {
        printf("installed apps service tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
