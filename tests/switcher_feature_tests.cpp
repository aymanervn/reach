#include "reach/features/switcher.h"

#include <stdio.h>

static int failures;
static reach_window_snapshot fake_windows[2];

static size_t fake_window_count(const reach_window_manager *manager)
{
    (void)manager;
    return 2;
}

static reach_result fake_window_at(const reach_window_manager *manager, size_t index,
                                   reach_window_snapshot *out_window)
{
    (void)manager;
    if (index >= 2 || out_window == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_window = fake_windows[index];
    return REACH_OK;
}

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        ++failures;
        fprintf(stderr, "FAILED: %s\n", message);
    }
}

static void test_switcher_presents_immediately_and_closes_before_activation(void)
{
    fake_windows[0] = {};
    fake_windows[0].id = 11;
    fake_windows[0].title[0] = 'A';
    fake_windows[1] = {};
    fake_windows[1].id = 42;
    fake_windows[1].title[0] = 'B';

    reach_window_manager_port window_manager = {};
    window_manager.ops.window_count = fake_window_count;
    window_manager.ops.window_at = fake_window_at;
    reach_window_tracking *windows = nullptr;
    expect_true(reach_window_tracking_create(window_manager, &windows) == REACH_OK,
                "window tracking is created");
    expect_true(reach_window_tracking_refresh(windows, nullptr) == REACH_OK,
                "window tracking is refreshed");
    reach_window_tracking_note_foreground(windows, 11);

    reach_switcher *switcher = nullptr;
    expect_true(reach_switcher_create(&switcher) == REACH_OK, "switcher is created");
    if (switcher == nullptr)
    {
        reach_window_tracking_destroy(windows);
        return;
    }
    reach_switcher_attach_services(switcher, nullptr, windows);

    reach_ui_event event = {};
    event.type = REACH_UI_EVENT_APP_SWITCH_BEGIN;
    reach_capsule_event_result result = {};
    reach_switcher_capsule_ops()->handle_event(switcher, &event, &result);

    expect_true(result.handled, "switcher handles begin");
    expect_true(reach_switcher_is_open(switcher), "switcher interaction opens immediately");
    expect_true(reach_switcher_capsule_ops()->presentation_visible(switcher),
                "switcher presentation begins immediately");

    event.type = REACH_UI_EVENT_APP_SWITCH_COMMIT;
    result = {};
    reach_switcher_capsule_ops()->handle_event(switcher, &event, &result);

    expect_true(result.handled, "switcher handles commit");
    expect_true(result.action.kind == REACH_FEATURE_ACTION_ACTIVATE_WINDOW,
                "switcher commit requests ordinary activation");
    expect_true(result.action.window == 42, "switcher commit preserves the selected window");
    expect_true(!reach_switcher_is_open(switcher), "switcher interaction closes before activation");
    expect_true(reach_switcher_capsule_ops()->presentation_visible(switcher),
                "switcher keeps presenting through its normal close transition");

    reach_feature_tick_result tick = {};
    reach_switcher_capsule_ops()->tick(switcher, 0.13, &tick);
    expect_true(!reach_switcher_capsule_ops()->presentation_visible(switcher),
                "switcher presentation ends after the shared close duration");

    reach_switcher_destroy(switcher);
    reach_window_tracking_destroy(windows);
}

int main(void)
{
    test_switcher_presents_immediately_and_closes_before_activation();
    return failures == 0 ? 0 : 1;
}
