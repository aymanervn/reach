#include "reach/features/switcher.h"

#include <stdio.h>
#include <chrono>
#include <thread>

static int failures;
static reach_window_snapshot fake_windows[16];
static reach_rect_f32 fake_frames[16];
static size_t fake_count = 2;

static size_t fake_window_count(const reach_window_manager *manager)
{
    (void)manager;
    return fake_count;
}

static reach_result fake_window_at(const reach_window_manager *manager, size_t index,
                                   reach_window_snapshot *out_window)
{
    (void)manager;
    if (index >= fake_count || out_window == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_window = fake_windows[index];
    return REACH_OK;
}

static reach_result fake_frame_bounds(const reach_window_manager *manager, reach_window_id window,
                                      reach_rect_f32 *out)
{
    (void)manager;
    for (size_t index = 0; index < fake_count; ++index)
    {
        if (fake_windows[index].id == window)
        {
            *out = fake_frames[index];
            return REACH_OK;
        }
    }
    return REACH_ERROR;
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

    reach_feature_surface_geometry geometry = {};
    reach_switcher_capsule_ops()->surface_geometry(switcher, &geometry);
    std::this_thread::sleep_for(
        std::chrono::milliseconds(reach_motion_wait_ms(geometry.deadline_seconds) + 1));
    reach_feature_tick_result tick = {};
    reach_switcher_capsule_ops()->tick(switcher, 0.13, &tick);
    expect_true(!reach_switcher_capsule_ops()->presentation_visible(switcher),
                "switcher presentation ends after the shared close duration");

    reach_switcher_destroy(switcher);
    reach_window_tracking_destroy(windows);
}

static void test_switcher_fits_variable_width_previews_and_keeps_selection_visible(void)
{
    fake_count = 16;
    for (size_t index = 0; index < fake_count; ++index)
    {
        fake_windows[index] = {};
        fake_windows[index].id = index + 1;
        fake_windows[index].title[0] = 'A';
        fake_frames[index] = {0.0f, 0.0f, 800.0f, 600.0f};
    }
    fake_frames[1] = {0.0f, 0.0f, 600.0f, 1200.0f};
    fake_frames[2] = {0.0f, 0.0f, 2400.0f, 900.0f};
    reach_window_manager_port port = {};
    port.ops.window_count = fake_window_count;
    port.ops.window_at = fake_window_at;
    port.ops.frame_bounds = fake_frame_bounds;
    reach_window_tracking *windows = nullptr;
    expect_true(reach_window_tracking_create(port, &windows) == REACH_OK,
                "preview tracking is created");
    (void)reach_window_tracking_refresh(windows, nullptr);
    reach_window_tracking_note_foreground(windows, 1);
    reach_switcher *switcher = nullptr;
    expect_true(reach_switcher_create(&switcher) == REACH_OK, "preview Switcher is created");
    reach_switcher_attach_services(switcher, nullptr, windows);
    reach_ui_event event = {};
    event.type = REACH_UI_EVENT_APP_SWITCH_BEGIN;
    reach_capsule_event_result result = {};
    reach_switcher_capsule_ops()->handle_event(switcher, &event, &result);
    reach_switcher_arrange_context arrange = {};
    arrange.theme = reach_theme_default();
    arrange.monitor_bounds = {100.0f, 50.0f, 1920.0f, 1080.0f};
    arrange.dpi_scale = 1.0f;
    arrange.transition_visible = 1;
    (void)reach_switcher_arrange(switcher, &arrange);
    reach_switcher_thumbnail_placement first = {}, portrait = {}, wide = {};
    (void)reach_switcher_thumbnail_at(switcher, 0, &first);
    (void)reach_switcher_thumbnail_at(switcher, 1, &portrait);
    (void)reach_switcher_thumbnail_at(switcher, 2, &wide);
    expect_true(fabsf(first.destination.height - portrait.destination.height) < 0.01f &&
                    fabsf(portrait.destination.height - wide.destination.height) < 0.01f,
                "portrait and wide previews share one height");
    expect_true(fabsf(portrait.destination.width / portrait.destination.height - 0.5f) < 0.01f &&
                    fabsf(wide.destination.width / wide.destination.height - 2400.0f / 900.0f) <
                        0.01f,
                "preview widths preserve each source aspect ratio");
    size_t generation = reach_switcher_preview_generation(switcher);
    event.type = REACH_UI_EVENT_APP_SWITCH_NEXT;
    reach_switcher_capsule_ops()->handle_event(switcher, &event, &result);
    (void)reach_switcher_arrange(switcher, &arrange);
    expect_true(reach_switcher_preview_generation(switcher) == generation,
                "selection within a visible set preserves preview membership");

    arrange.last_bounds = reach_switcher_state_ptr(switcher)->bounds;
    arrange.bounds_valid = 1;
    arrange.monitor_bounds = {100.0f, 50.0f, 600.0f, 360.0f};
    arrange.dpi_scale = 2.0f;
    for (size_t step = 0; step < fake_count * 3; ++step)
    {
        event.type = step < fake_count * 2 ? REACH_UI_EVENT_APP_SWITCH_NEXT
                                           : REACH_UI_EVENT_APP_SWITCH_PREVIOUS;
        reach_switcher_capsule_ops()->handle_event(switcher, &event, &result);
        (void)reach_switcher_arrange(switcher, &arrange);
        const reach_switcher_state *state = reach_switcher_state_ptr(switcher);
        expect_true(state->selected_index >= state->visible_start &&
                        state->selected_index < state->visible_start + state->visible_count,
                    "cycling and wrapping keep selection in the fitted range");
        float common_height = 0.0f;
        for (size_t index = 0; index < reach_switcher_thumbnail_count(switcher); ++index)
        {
            reach_switcher_thumbnail_placement item = {};
            (void)reach_switcher_thumbnail_at(switcher, index, &item);
            if (index == 0)
            {
                common_height = item.destination.height;
            }
            expect_true(fabsf(common_height - item.destination.height) < 0.01f,
                        "constrained previews retain a common height");
            expect_true(item.destination.x >= state->bounds.x &&
                            item.destination.x + item.destination.width <=
                                state->bounds.x + state->bounds.width + 0.01f,
                        "every preview stays inside the arranged surface");
        }
        expect_true(state->bounds.x >= arrange.monitor_bounds.x &&
                        state->bounds.x + state->bounds.width <=
                            arrange.monitor_bounds.x + arrange.monitor_bounds.width + 0.01f,
                    "constrained Switcher stays inside the monitor");
        arrange.last_bounds = state->bounds;
    }

    event.type = REACH_UI_EVENT_APP_SWITCH_BEGIN;
    reach_switcher_capsule_ops()->handle_event(switcher, &event, &result);
    arrange.monitor_bounds = {100.0f, 50.0f, 1920.0f, 1080.0f};
    arrange.dpi_scale = 1.0f;
    (void)reach_switcher_arrange(switcher, &arrange);
    generation = reach_switcher_preview_generation(switcher);
    fake_windows[1].minimized = 1;
    (void)reach_window_tracking_refresh(windows, nullptr);
    reach_feature_tick_result tick = {};
    reach_switcher_notify_windows_changed(switcher, &tick);
    (void)reach_switcher_arrange(switcher, &arrange);
    (void)reach_switcher_thumbnail_at(switcher, 1, &portrait);
    expect_true(!portrait.visible && portrait.window == 0 &&
                    reach_switcher_preview_generation(switcher) != generation,
                "minimized placeholders retire their native relationship");
    fake_windows[1].minimized = 0;
    fake_frames[1] = {0.0f, 0.0f, 1200.0f, 600.0f};
    (void)reach_window_tracking_refresh(windows, nullptr);
    reach_switcher_notify_windows_changed(switcher, &tick);
    (void)reach_switcher_arrange(switcher, &arrange);
    (void)reach_switcher_thumbnail_at(switcher, 1, &portrait);
    expect_true(portrait.visible && portrait.window == 2 &&
                    fabsf(portrait.destination.width / portrait.destination.height - 2.0f) < 0.01f,
                "restored and resized sources publish their current aspect ratio");
    reach_switcher_destroy(switcher);
    reach_window_tracking_destroy(windows);
    fake_count = 2;
}

int main(void)
{
    test_switcher_presents_immediately_and_closes_before_activation();
    test_switcher_fits_variable_width_previews_and_keeps_selection_visible();
    return failures == 0 ? 0 : 1;
}
