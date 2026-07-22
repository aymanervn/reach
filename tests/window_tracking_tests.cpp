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

static void copy_ascii(uint16_t *dst, size_t cap, const char *src)
{
    size_t index = 0;
    while (src != nullptr && src[index] != 0 && index + 1 < cap)
    {
        dst[index] = (uint16_t)(unsigned char)src[index];
        ++index;
    }
    dst[index] = 0;
}

static reach_window_snapshot make_window(uintptr_t id, const char *path, const char *aumid)
{
    reach_window_snapshot window = {};
    window.id = id;
    window.visible = 1;
    copy_ascii(window.title, 260, "window");
    copy_ascii(window.path, 260, path);
    copy_ascii(window.app_user_model_id, 260, aumid);
    return window;
}

static reach_window_snapshot fake_windows[REACH_MAX_PINNED_APPS];
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

static reach_window_tracking *make_service(void)
{
    reach_window_manager_port port = {};
    port.ops.window_count = fake_window_count_op;
    port.ops.window_at = fake_window_at_op;
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
    }
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
    expect_true(!reach_window_tracking_windows_same_app(&uwp_a, &uwp_b),
                "shared host path with differing aumids stays separate");
    expect_true(!reach_window_tracking_windows_same_app(&bare_a, &bare_b),
                "empty identities never match each other");

    reach_pinned_app_model app = {};
    copy_ascii(app.path, 260, "C:\\apps\\brave.exe");
    expect_true(reach_window_tracking_window_matches_app(&app, &browser_a),
                "pinned path matches browser window");
    expect_true(reach_window_tracking_window_matches_app(&app, &pwa),
                "pinned app without aumid falls back to path");
    reach_pinned_app_model pwa_pin = {};
    copy_ascii(pwa_pin.path, 260, "C:\\apps\\brave.exe");
    copy_ascii(pwa_pin.app_user_model_id, 260, "Brave._crx_abc");
    expect_true(reach_window_tracking_window_matches_app(&pwa_pin, &browser_a),
                "pinned pwa matches by path when window has no aumid");
    expect_true(!reach_window_tracking_window_matches_app(&pwa_pin, &uwp_a),
                "pinned pwa does not match different aumid");
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

static void test_empty_identity_and_aumid_split(void)
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
    copy_ascii(windows[0].title, 260, "bare one");
    copy_ascii(windows[1].title, 260, "bare two");
    set_windows(windows, 4);
    (void)reach_window_tracking_refresh(service, nullptr);

    uint32_t bare_one = group_of(service, 31);
    uint32_t bare_two = group_of(service, 32);
    expect_true(bare_one != 0 && bare_two != 0 && bare_one != bare_two,
                "empty-identity windows get distinct group ids");
    expect_true(group_of(service, 33) != group_of(service, 34),
                "same path with differing aumids splits into two groups");

    (void)reach_window_tracking_refresh(service, nullptr);
    expect_true(group_of(service, 31) == bare_one && group_of(service, 32) == bare_two,
                "empty-identity group ids stay stable across refresh");

    reach_window_tracking_destroy(service);
}

int main(void)
{
    test_identity_rule();
    test_group_id_assignment_and_stability();
    test_empty_identity_and_aumid_split();
    return failures == 0 ? 0 : 1;
}
