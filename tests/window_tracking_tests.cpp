#include "reach/support/util.h"
#include "reach/services/window_tracking.h"

#include <stdio.h>

static int failures;

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        ++failures;
        fprintf(stderr, "FAILED: %s\n", message);
    }
}

static reach_window_snapshot make_window(uintptr_t id, const char *path, const char *aumid)
{
    reach_window_snapshot window = {};
    window.id = id;
    window.visible = 1;
    reach_copy_ascii_to_utf16(window.title, 260, "window");
    uint16_t runtime_path[260] = {};
    reach_copy_ascii_to_utf16(runtime_path, 260, path);
    reach_application_identity_add_runtime_path(&window.identity, runtime_path);
    reach_copy_ascii_to_utf16(window.identity.app_user_model_id, 260, aumid);
    return window;
}

static reach_window_snapshot fake_windows[REACH_MAX_OPEN_WINDOWS];
static reach_rect_f32 fake_window_bounds[REACH_MAX_OPEN_WINDOWS];
static size_t fake_window_count;

static size_t fake_window_count_op(const reach_window_manager *manager)
{
    (void)manager;
    return fake_window_count;
}

static reach_result fake_window_at_op(const reach_window_manager *manager, size_t index,
                                      reach_window_snapshot *out_window)
{
    (void)manager;
    if (index >= fake_window_count || out_window == nullptr)
    {
        return REACH_ERROR;
    }
    *out_window = fake_windows[index];
    return REACH_OK;
}

static reach_result fake_outer_bounds_op(const reach_window_manager *manager,
                                         reach_window_id window_id, reach_rect_f32 *out_bounds)
{
    (void)manager;
    if (out_bounds == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    for (size_t index = 0; index < fake_window_count; ++index)
    {
        if (fake_windows[index].id == window_id)
        {
            *out_bounds = fake_window_bounds[index];
            return REACH_OK;
        }
    }
    return REACH_ERROR;
}

static reach_window_tracking *make_service(void)
{
    reach_window_manager_port port = {};
    port.ops.window_count = fake_window_count_op;
    port.ops.window_at = fake_window_at_op;
    port.ops.outer_bounds = fake_outer_bounds_op;
    reach_window_tracking *service = nullptr;
    if (reach_window_tracking_create(port, &service) != REACH_OK)
    {
        return nullptr;
    }
    return service;
}

static void set_windows(const reach_window_snapshot *windows, size_t count)
{
    fake_window_count = count;
    for (size_t index = 0; index < count; ++index)
    {
        fake_windows[index] = windows[index];
        fake_window_bounds[index] = {};
    }
}

static void test_trespass_uses_protected_band_and_monitor(void)
{
    reach_window_tracking *service = make_service();
    if (service == nullptr)
    {
        ++failures;
        return;
    }

    reach_window_snapshot windows[3] = {
        make_window(41, "C:\\apps\\bottom.exe", ""),
        make_window(42, "C:\\apps\\middle.exe", ""),
        make_window(43, "C:\\apps\\secondary.exe", ""),
    };
    set_windows(windows, 3);
    fake_window_bounds[0] = {100.0f, 200.0f, 600.0f, 580.0f};
    fake_window_bounds[1] = {100.0f, 200.0f, 600.0f, 400.0f};
    fake_window_bounds[2] = {1100.0f, 100.0f, 600.0f, 700.0f};
    (void)reach_window_tracking_refresh(service, nullptr);

    reach_rect_f32 monitor = {0.0f, 0.0f, 1000.0f, 800.0f};
    reach_rect_f32 bottom_band = {0.0f, 744.0f, 1000.0f, 56.0f};
    expect_true(reach_window_tracking_any_trespassing(service, monitor, bottom_band, 0),
                "floating window trespassing the Dock band is detected");
    expect_true(!reach_window_tracking_any_trespassing(service, monitor, bottom_band, 41),
                "the actively manipulated window can be excluded");

    windows[0].minimized = 1;
    set_windows(windows, 3);
    fake_window_bounds[0] = {100.0f, 200.0f, 600.0f, 580.0f};
    fake_window_bounds[1] = {100.0f, 200.0f, 600.0f, 400.0f};
    fake_window_bounds[2] = {1100.0f, 100.0f, 600.0f, 700.0f};
    (void)reach_window_tracking_refresh(service, nullptr);
    expect_true(!reach_window_tracking_any_trespassing(service, monitor, bottom_band, 0),
                "minimized and other-monitor windows do not trespass the bar band");

    reach_window_tracking_destroy(service);
}

static void test_non_game_fullscreen_survives_focus_and_respects_monitor(void)
{
    reach_window_tracking *service = make_service();
    if (service == nullptr)
    {
        ++failures;
        return;
    }

    reach_window_snapshot windows[3] = {
        make_window(51, "C:\\apps\\video.exe", ""),
        make_window(52, "C:\\apps\\game.exe", ""),
        make_window(53, "C:\\apps\\secondary.exe", ""),
    };
    windows[0].fullscreen = 1;
    windows[1].fullscreen = 1;
    windows[1].fullscreen_game = 1;
    windows[2].fullscreen = 1;
    set_windows(windows, 3);
    fake_window_bounds[0] = {0.0f, 0.0f, 1000.0f, 800.0f};
    fake_window_bounds[1] = {0.0f, 0.0f, 1000.0f, 800.0f};
    fake_window_bounds[2] = {1000.0f, 0.0f, 1000.0f, 800.0f};
    (void)reach_window_tracking_refresh(service, nullptr);

    reach_rect_f32 primary = {0.0f, 0.0f, 1000.0f, 800.0f};
    reach_rect_f32 secondary = {1000.0f, 0.0f, 1000.0f, 800.0f};
    expect_true(reach_window_tracking_any_non_game_fullscreen(service, primary),
                "a fullscreen app remains relevant without foreground state");
    expect_true(reach_window_tracking_any_non_game_fullscreen(service, secondary),
                "fullscreen relevance follows the hosting monitor");

    windows[0].minimized = 1;
    windows[2].visible = 0;
    set_windows(windows, 3);
    fake_window_bounds[0] = {0.0f, 0.0f, 1000.0f, 800.0f};
    fake_window_bounds[1] = {0.0f, 0.0f, 1000.0f, 800.0f};
    fake_window_bounds[2] = {1000.0f, 0.0f, 1000.0f, 800.0f};
    (void)reach_window_tracking_refresh(service, nullptr);
    expect_true(!reach_window_tracking_any_non_game_fullscreen(service, primary),
                "minimized apps and fullscreen games do not activate the override");
    expect_true(!reach_window_tracking_any_non_game_fullscreen(service, secondary),
                "hidden fullscreen apps do not activate the override");

    reach_window_tracking_destroy(service);
}

static uint32_t group_of(const reach_window_tracking *service, uintptr_t window_id)
{
    return reach_window_tracking_window_group_id(service, window_id);
}

static void test_identity_rule(void)
{
    reach_window_snapshot browser_a = make_window(1, "C:\\apps\\brave.exe", "");
    reach_window_snapshot browser_b = make_window(2, "c:\\APPS\\BRAVE.EXE", "");
    reach_window_snapshot pwa = make_window(3, "C:\\apps\\brave.exe", "Brave._crx_abc");
    reach_window_snapshot pwa_upper = make_window(4, "C:\\other\\brave.exe", "BRAVE._CRX_ABC");
    reach_window_snapshot uwp_a = make_window(5, "C:\\win\\ApplicationFrameHost.exe", "App.One");
    reach_window_snapshot uwp_b = make_window(6, "C:\\win\\ApplicationFrameHost.exe", "App.Two");
    reach_window_snapshot bare_a = make_window(7, "", "");
    reach_window_snapshot bare_b = make_window(8, "", "");

    expect_true(reach_window_tracking_windows_same_app(&browser_a, &browser_b),
                "same path matches case-insensitively");
    expect_true(reach_window_tracking_windows_same_app(&browser_a, &pwa),
                "one-sided aumid falls back to path comparison");
    expect_true(!reach_window_tracking_windows_same_app(&pwa, &uwp_a),
                "distinct aumids never match");
    expect_true(reach_window_tracking_windows_same_app(&pwa, &pwa_upper),
                "matching aumids compare case-insensitively regardless of path");
    expect_true(reach_window_tracking_windows_same_app(&uwp_a, &uwp_b),
                "shared executable matches even when aumids differ");
    expect_true(!reach_window_tracking_windows_same_app(&bare_a, &bare_b),
                "empty identities never match each other");

    reach_pinned_app_model app = {};
    reach_application_identity_add_runtime_path(&app.application.identity,
                                                (const uint16_t *)L"C:\\apps\\brave.exe");
    expect_true(reach_window_tracking_window_matches_app(&app, &browser_a),
                "pinned path matches browser window");
    expect_true(reach_window_tracking_window_matches_app(&app, &pwa),
                "pinned app without aumid falls back to path");
    reach_pinned_app_model pwa_pin = {};
    reach_application_identity_add_runtime_path(&pwa_pin.application.identity,
                                                (const uint16_t *)L"C:\\apps\\brave.exe");
    reach_copy_ascii_to_utf16(pwa_pin.application.identity.app_user_model_id, 260,
                              "Brave._crx_abc");
    expect_true(reach_window_tracking_window_matches_app(&pwa_pin, &browser_a),
                "pinned pwa matches by path when window has no aumid");
    expect_true(!reach_window_tracking_window_matches_app(&pwa_pin, &uwp_a),
                "pinned pwa does not match different aumid");
    reach_pinned_app_model shortcut_pin = {};
    reach_application_identity_add_runtime_path(&shortcut_pin.application.identity,
                                                (const uint16_t *)L"C:\\apps\\zed.exe");
    shortcut_pin.application.launch.kind = REACH_APPLICATION_LAUNCH_SHORTCUT;
    reach_copy_ascii_to_utf16(shortcut_pin.application.launch.path, 260, "C:\\Pins\\Zed.lnk");
    reach_copy_ascii_to_utf16(shortcut_pin.application.identity.app_user_model_id, 260,
                              "ZedIndustries.Zed");
    reach_window_snapshot zed = make_window(9, "c:\\APPS\\ZED.EXE", "Zed.Editor");
    expect_true(reach_window_tracking_window_matches_app(&shortcut_pin, &zed),
                "a shortcut pin falls back from a mismatched aumid to its resolved executable");
    expect_true(!reach_window_tracking_windows_same_app(&zed, &uwp_a),
                "shortcut fallback does not weaken running-window grouping");
}

static void test_group_id_assignment_and_stability(void)
{
    reach_window_tracking *service = make_service();
    expect_true(service != nullptr, "service creation succeeds");
    if (service == nullptr)
    {
        return;
    }

    reach_window_snapshot windows[4] = {
        make_window(11, "C:\\apps\\brave.exe", ""),
        make_window(12, "C:\\apps\\brave.exe", ""),
        make_window(13, "C:\\apps\\code.exe", ""),
        make_window(14, "C:\\apps\\brave.exe", ""),
    };
    set_windows(windows, 4);
    (void)reach_window_tracking_refresh(service, nullptr);

    uint32_t brave_group = group_of(service, 11);
    expect_true(brave_group != 0, "group id assigned");
    expect_true(group_of(service, 12) == brave_group, "same app shares group id");
    expect_true(group_of(service, 14) == brave_group, "third window joins group");
    expect_true(group_of(service, 13) != brave_group, "different app gets different group id");

    reach_window_snapshot reordered[3] = {windows[2], windows[3], windows[1]};
    set_windows(reordered, 3);
    (void)reach_window_tracking_refresh(service, nullptr);
    expect_true(group_of(service, 12) == brave_group,
                "group id survives reorder and representative close");
    expect_true(group_of(service, 14) == brave_group, "surviving windows keep the group id");

    reach_window_snapshot only_code[1] = {windows[2]};
    set_windows(only_code, 1);
    (void)reach_window_tracking_refresh(service, nullptr);
    reach_window_snapshot brave_back[2] = {windows[2], make_window(21, "C:\\apps\\brave.exe", "")};
    set_windows(brave_back, 2);
    (void)reach_window_tracking_refresh(service, nullptr);
    expect_true(group_of(service, 21) != 0 && group_of(service, 21) != brave_group,
                "app reopened after last window closed gets a new group id");

    reach_window_tracking_destroy(service);
}

static void test_empty_identity_and_shared_executable(void)
{
    reach_window_tracking *service = make_service();
    if (service == nullptr)
    {
        ++failures;
        return;
    }

    reach_window_snapshot windows[4] = {
        make_window(31, "", ""),
        make_window(32, "", ""),
        make_window(33, "C:\\apps\\brave.exe", "Brave._crx_abc"),
        make_window(34, "C:\\apps\\brave.exe", "Brave._crx_xyz"),
    };
    reach_copy_ascii_to_utf16(windows[0].title, 260, "bare one");
    reach_copy_ascii_to_utf16(windows[1].title, 260, "bare two");
    set_windows(windows, 4);
    (void)reach_window_tracking_refresh(service, nullptr);

    uint32_t bare_one = group_of(service, 31);
    uint32_t bare_two = group_of(service, 32);
    expect_true(bare_one != 0 && bare_two != 0 && bare_one != bare_two,
                "empty-identity windows get distinct group ids");
    expect_true(group_of(service, 33) == group_of(service, 34),
                "same path with differing aumids shares one group");

    (void)reach_window_tracking_refresh(service, nullptr);
    expect_true(group_of(service, 31) == bare_one && group_of(service, 32) == bare_two,
                "empty-identity group ids stay stable across refresh");

    reach_window_tracking_destroy(service);
}

static void test_identity_groups_are_transitive(void)
{
    reach_window_tracking *service = make_service();
    if (service == nullptr)
    {
        ++failures;
        return;
    }

    reach_window_snapshot windows[3] = {
        make_window(51, "C:\\apps\\launcher.exe", "App.One"),
        make_window(52, "C:\\apps\\launcher.exe", "App.Two"),
        make_window(53, "C:\\apps\\runtime.exe", "App.Two"),
    };
    set_windows(windows, 3);
    (void)reach_window_tracking_refresh(service, nullptr);

    uint32_t group = group_of(service, 51);
    expect_true(group != 0 && group_of(service, 52) == group && group_of(service, 53) == group,
                "path and aumid bridges produce one deterministic group");

    reach_window_tracking_destroy(service);
}

static void test_identity_groups_split_without_a_bridge(void)
{
    reach_window_tracking *service = make_service();
    if (service == nullptr)
    {
        ++failures;
        return;
    }

    reach_window_snapshot bridged[3] = {
        make_window(61, "C:\\apps\\launcher.exe", "App.One"),
        make_window(62, "C:\\apps\\launcher.exe", "App.Two"),
        make_window(63, "C:\\apps\\runtime.exe", "App.Two"),
    };
    set_windows(bridged, 3);
    (void)reach_window_tracking_refresh(service, nullptr);

    reach_window_snapshot split[2] = {bridged[0], bridged[2]};
    set_windows(split, 2);
    (void)reach_window_tracking_refresh(service, nullptr);

    expect_true(group_of(service, 61) != group_of(service, 63),
                "components split deterministically when their bridge closes");

    reach_window_tracking_destroy(service);
}

int main(void)
{
    test_identity_rule();
    test_group_id_assignment_and_stability();
    test_empty_identity_and_shared_executable();
    test_identity_groups_are_transitive();
    test_identity_groups_split_without_a_bridge();
    test_trespass_uses_protected_band_and_monitor();
    test_non_game_fullscreen_survives_focus_and_respects_monitor();
    return failures == 0 ? 0 : 1;
}
