#include "motion_clock_test_support.h"

#include "reach/core/theme.h"
#include "reach/features/stage.h"

#include <math.h>
#include <memory>
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

static void expect_near(float actual, float expected, const char *message)
{
    if (fabsf(actual - expected) > 0.01f)
    {
        ++failures;
        fprintf(stderr, "FAILED: %s (expected %f, got %f)\n", message, (double)expected,
                (double)actual);
    }
}

static reach_rect_f32 make_rect(float x, float y, float width, float height)
{
    reach_rect_f32 rect = {x, y, width, height};
    return rect;
}

static reach_stage_open_window make_window(uintptr_t id, reach_rect_f32 frame)
{
    reach_stage_open_window window = {};
    window.window = id;
    window.frame = frame;
    return window;
}

static reach_stage_open_window make_desktop(uintptr_t id, reach_rect_f32 frame)
{
    reach_stage_open_window window = make_window(id, frame);
    window.desktop = 1;
    return window;
}

static size_t find_desktop_tile(const reach_stage_state *state)
{
    for (size_t index = 0; index < state->tile_count; ++index)
    {
        if (state->tiles[index].desktop)
        {
            return index;
        }
    }
    return state->tile_count;
}

static void tick_stage(reach_stage *stage, double delta, reach_feature_tick_result *tick)
{
    const reach_feature_capsule_ops *ops = reach_stage_capsule_ops();
    reach_test_motion_clock_advance(delta);
    ops->tick(stage, delta, tick);
    reach_feature_surface_geometry geometry = {};
    ops->surface_geometry(stage, &geometry);
    if (geometry.synchronize_presentation)
    {
        ops->presentation_committed(stage, REACH_OK, tick);
    }
}

static void advance_stage(reach_stage *stage, int steps, double delta_seconds)
{
    const reach_feature_capsule_ops *ops = reach_stage_capsule_ops();
    reach_feature_tick_result tick = {};
    for (int step = 0; step < steps; ++step)
    {
        tick_stage(stage, delta_seconds, &tick);
    }
}

static reach_capsule_pointer_result click_tile(reach_stage *stage, size_t index);

static void test_open_and_close_state_machine(void)
{
    reach_stage *stage = nullptr;
    expect_true(reach_stage_create(&stage) == REACH_OK && stage != nullptr, "stage is created");
    if (stage == nullptr)
    {
        return;
    }

    expect_near(reach_stage_state_ptr(stage)->animation_seconds,
                reach_theme_default()->stage_animation_seconds,
                "stage animation duration comes from the theme");
    expect_true(!reach_stage_is_open(stage), "a new stage is closed");

    expect_true(reach_stage_open(stage, make_rect(0.0f, 0.0f, 1000.0f, 1000.0f), 1.0f, nullptr,
                                 0) != REACH_OK,
                "opening with no windows fails");
    expect_true(!reach_stage_is_open(stage), "a failed open leaves the stage closed");

    reach_stage_open_window windows[2] = {make_window(1, make_rect(0.0f, 0.0f, 400.0f, 300.0f)),
                                          make_window(2, make_rect(400.0f, 0.0f, 400.0f, 300.0f))};

    expect_true(reach_stage_open(stage, make_rect(0.0f, 0.0f, 1000.0f, 1000.0f), 1.0f, windows,
                                 2) == REACH_OK,
                "opening with windows succeeds");
    expect_true(reach_stage_is_open(stage), "stage is open after a successful open");
    expect_true(reach_stage_thumbnail_count(stage) == 2, "stage tracks both windows");

    const reach_stage_state *state = reach_stage_state_ptr(stage);
    expect_near(state->progress, 0.0f, "stage starts at progress zero");
    expect_true(reach_stage_animation_active(stage), "opening animation is active");

    reach_stage_thumbnail_placement placement = {};
    expect_true(reach_stage_thumbnail_at(stage, 0, &placement) == REACH_OK,
                "placement is readable");
    expect_true(placement.window == 1, "placement reports the source window");
    expect_true(reach_stage_thumbnail_at(stage, 2, &placement) != REACH_OK,
                "out of range placement is rejected");

    const reach_feature_capsule_ops *ops = reach_stage_capsule_ops();
    reach_feature_tick_result tick = {};
    for (int step = 0; step < 40; ++step)
    {
        tick_stage(stage, 0.016, &tick);
    }
    expect_near(reach_stage_state_ptr(stage)->progress, 1.0f, "stage settles at progress one");
    expect_true(!reach_stage_animation_active(stage), "settled stage stops animating");

    expect_true(reach_stage_thumbnail_at(stage, 0, &placement) == REACH_OK,
                "settled placement is readable");

    reach_stage_begin_close(stage);
    expect_true(reach_stage_is_open(stage), "stage stays open while closing animates");
    expect_true(reach_stage_animation_active(stage), "closing animation is active");

    for (int step = 0; step < 40; ++step)
    {
        tick_stage(stage, 0.016, &tick);
    }
    expect_true(!reach_stage_is_open(stage), "stage closes once the animation finishes");
    expect_true(reach_stage_thumbnail_count(stage) == 0, "closed stage tracks no windows");

    reach_stage_destroy(stage);
}

static void test_force_close_keeps_configured_animation(void)
{
    reach_stage *stage = nullptr;
    if (reach_stage_create(&stage) != REACH_OK || stage == nullptr)
    {
        expect_true(0, "stage is created for animation config");
        return;
    }

    reach_stage_set_animation_seconds(stage, 0.5f);
    expect_near(reach_stage_state_ptr(stage)->animation_seconds, 0.5f,
                "animation duration is configurable");

    reach_stage_set_animation_seconds(stage, 0.0f);
    expect_near(reach_stage_state_ptr(stage)->animation_seconds, 0.5f,
                "a non positive duration is ignored");

    reach_stage_open_window window = make_window(1, make_rect(0.0f, 0.0f, 400.0f, 300.0f));
    (void)reach_stage_open(stage, make_rect(0.0f, 0.0f, 1000.0f, 1000.0f), 1.0f, &window, 1);
    reach_stage_force_close(stage);

    expect_true(!reach_stage_is_open(stage), "force close closes immediately");
    expect_near(reach_stage_state_ptr(stage)->animation_seconds, 0.5f,
                "force close keeps the configured duration");

    reach_stage_destroy(stage);
}

static void test_closing_stage_finishes_without_external_wake_ups(void)
{
    reach_stage *stage = nullptr;
    if (reach_stage_create(&stage) != REACH_OK || stage == nullptr)
    {
        expect_true(0, "stage is created for the closing lifecycle");
        return;
    }

    reach_stage_open_window window = make_window(1, make_rect(0.0f, 0.0f, 400.0f, 300.0f));
    (void)reach_stage_open(stage, make_rect(0.0f, 0.0f, 1000.0f, 1000.0f), 1.0f, &window, 1);

    const reach_feature_capsule_ops *ops = reach_stage_capsule_ops();
    reach_feature_tick_result tick = {};
    for (int step = 0; step < 40; ++step)
    {
        tick_stage(stage, 0.016, &tick);
    }
    expect_true(ops->is_open(stage), "a settled open stage reports open");

    reach_stage_begin_close(stage);
    expect_true(reach_stage_is_open(stage), "a closing stage stays visible");
    expect_true(!ops->is_open(stage), "a closing stage stops holding transient surfaces open");

    int32_t always_requested_frames = 1;
    int guard = 0;
    while (reach_stage_is_open(stage) && guard < 200)
    {
        always_requested_frames = always_requested_frames && ops->needs_frame(stage);
        tick_stage(stage, 0.016, &tick);
        ++guard;
    }

    expect_true(always_requested_frames,
                "a closing stage never stops requesting frames before it closes");
    expect_true(!reach_stage_is_open(stage), "the close completes without external wake ups");
    expect_true(!ops->needs_frame(stage), "a closed stage stops requesting frames");

    reach_stage_destroy(stage);
}

static void test_close_before_the_first_tick_completes(void)
{
    reach_stage *stage = nullptr;
    if (reach_stage_create(&stage) != REACH_OK || stage == nullptr)
    {
        expect_true(0, "stage is created for the immediate close");
        return;
    }

    reach_stage_open_window window = make_window(1, make_rect(0.0f, 0.0f, 400.0f, 300.0f));
    (void)reach_stage_open(stage, make_rect(0.0f, 0.0f, 1000.0f, 1000.0f), 1.0f, &window, 1);
    reach_stage_begin_close(stage);

    const reach_feature_capsule_ops *ops = reach_stage_capsule_ops();
    expect_true(!reach_stage_animation_active(stage),
                "closing at progress zero leaves no animation to run");
    expect_true(ops->needs_frame(stage),
                "a stage closed before its first tick still requests a frame");

    reach_feature_tick_result tick = {};
    int guard = 0;
    while (reach_stage_is_open(stage) && guard < 200)
    {
        tick_stage(stage, 0.016, &tick);
        ++guard;
    }
    expect_true(!reach_stage_is_open(stage), "an immediate close still completes");

    reach_stage_destroy(stage);
}

static void test_closing_lands_on_the_current_window_frame(void)
{
    reach_stage *stage = nullptr;
    if (reach_stage_create(&stage) != REACH_OK || stage == nullptr)
    {
        expect_true(0, "stage is created for the displaced close");
        return;
    }

    reach_rect_f32 opened = make_rect(0.0f, 0.0f, 1000.0f, 800.0f);
    reach_stage_open_window window = make_window(1, opened);
    (void)reach_stage_open(stage, make_rect(0.0f, 0.0f, 1000.0f, 1000.0f), 1.0f, &window, 1);

    const reach_feature_capsule_ops *ops = reach_stage_capsule_ops();
    reach_feature_tick_result tick = {};
    for (int step = 0; step < 40; ++step)
    {
        tick_stage(stage, 0.016, &tick);
    }

    reach_rect_f32 grid_rect = reach_stage_state_ptr(stage)->tiles[0].target_rect;

    reach_rect_f32 displaced = make_rect(0.0f, 40.0f, 1000.0f, 760.0f);
    reach_stage_open_window pushed = make_window(1, displaced);
    reach_stage_refresh_tile_frames(stage, &pushed, 1);

    const reach_stage_tile *tile = &reach_stage_state_ptr(stage)->tiles[0];
    expect_near(tile->source_rect.y, displaced.y, "a refresh re-seats the tile landing rect");
    expect_near(tile->target_rect.y, grid_rect.y,
                "a refresh leaves the tile where it sits in the grid");
    expect_near(tile->target_rect.height, grid_rect.height,
                "a refresh does not resize the tile in the grid");

    reach_stage_begin_close(stage);
    reach_rect_f32 last = tile->current_rect;
    int guard = 0;
    while (reach_stage_is_open(stage) && guard < 200)
    {
        tick_stage(stage, 0.016, &tick);
        if (reach_stage_is_open(stage))
        {
            last = tile->current_rect;
        }
        ++guard;
    }

    expect_true(fabsf(last.y - displaced.y) < fabsf(last.y - opened.y),
                "the close animation settles onto the moved window, not where it opened");
    expect_true(fabsf(last.height - displaced.height) < fabsf(last.height - opened.height),
                "the close animation settles at the moved window size");

    reach_stage_destroy(stage);
}

static void test_desktop_keeps_solo_geometry_behind_apps(void)
{
    reach_stage *stage = nullptr;
    if (reach_stage_create(&stage) != REACH_OK || stage == nullptr)
    {
        expect_true(0, "stage is created for desktop placement");
        return;
    }

    reach_rect_f32 bounds = make_rect(0.0f, 0.0f, 1920.0f, 1080.0f);
    reach_stage_open_window desktop = make_desktop(100, bounds);
    expect_true(reach_stage_open(stage, bounds, 1.0f, &desktop, 1) == REACH_OK,
                "desktop-only stage opens");
    reach_rect_f32 available = make_rect(0.0f, 40.0f, 1920.0f, 960.0f);
    expect_true(reach_stage_set_desktop_bounds(stage, available),
                "desktop accepts the space reserved between the bars");
    advance_stage(stage, 40, 0.016);

    const reach_stage_state *state = reach_stage_state_ptr(stage);
    size_t desktop_index = find_desktop_tile(state);
    expect_true(desktop_index < state->tile_count, "desktop tile is present");
    reach_rect_f32 solo = state->tiles[desktop_index].target_rect;
    float border = reach_theme_border_thickness(reach_theme_default(), 1.0f);
    expect_near(solo.y - border, 60.0f, "desktop outer border leaves 20 dp below the top bar");
    expect_near(solo.y + solo.height + border, 980.0f,
                "desktop outer border leaves 20 dp above the Dock");
    expect_true(solo.width > 1600.0f, "desktop thumbnail is noticeably larger");
    expect_near(state->tiles[desktop_index].bar_height, 0.0f,
                "desktop tile has no artificial top bar");

    reach_stage_thumbnail_placement placement = {};
    expect_true(reach_stage_thumbnail_at(stage, desktop_index, &placement) == REACH_OK,
                "desktop thumbnail placement is readable");
    expect_true(placement.behind_surface,
                "desktop thumbnail uses the plane behind the Stage surface");

    reach_stage_force_close(stage);
    reach_stage_open_window windows[3] = {make_window(1, make_rect(100.0f, 80.0f, 1200.0f, 800.0f)),
                                          make_window(2, make_rect(300.0f, 160.0f, 900.0f, 700.0f)),
                                          desktop};
    expect_true(reach_stage_open(stage, bounds, 1.0f, windows, 3) == REACH_OK,
                "stage opens with apps and desktop");
    expect_true(reach_stage_set_desktop_bounds(stage, available),
                "reopened desktop accepts the space reserved between the bars");
    advance_stage(stage, 40, 0.016);

    state = reach_stage_state_ptr(stage);
    desktop_index = find_desktop_tile(state);
    expect_true(desktop_index < state->tile_count, "desktop remains present with apps");
    const reach_stage_tile *desktop_tile = &state->tiles[desktop_index];
    expect_near(desktop_tile->target_rect.x, solo.x, "desktop keeps its solo horizontal position");
    expect_near(desktop_tile->target_rect.y, solo.y, "desktop keeps its solo vertical position");
    expect_near(desktop_tile->target_rect.width, solo.width, "desktop keeps its solo width");
    expect_near(desktop_tile->target_rect.height, solo.height, "desktop keeps its solo height");
    expect_true(state->tiles[0].target_rect.width < desktop_tile->target_rect.width,
                "app thumbnails remain smaller than the desktop thumbnail");

    expect_true(reach_stage_thumbnail_at(stage, 0, &placement) == REACH_OK,
                "app thumbnail placement is readable");
    expect_true(!placement.behind_surface, "app thumbnail stays on the Stage surface plane");

    reach_stage_destroy(stage);
}

static void test_apps_take_pointer_priority_over_desktop(void)
{
    reach_stage *stage = nullptr;
    if (reach_stage_create(&stage) != REACH_OK || stage == nullptr)
    {
        expect_true(0, "stage is created for overlapping desktop interaction");
        return;
    }

    reach_rect_f32 bounds = make_rect(0.0f, 0.0f, 1920.0f, 1080.0f);
    reach_stage_open_window desktop = make_desktop(100, bounds);
    (void)reach_stage_open(stage, bounds, 1.0f, &desktop, 1);
    advance_stage(stage, 40, 0.016);

    reach_stage_open_window windows[2] = {
        desktop, make_window(1, make_rect(100.0f, 80.0f, 1200.0f, 800.0f))};
    expect_true(reach_stage_update_windows(stage, windows, 2),
                "an app can join a desktop-only Stage");
    advance_stage(stage, 40, 0.016);

    const reach_stage_state *state = reach_stage_state_ptr(stage);
    size_t app_index = state->tiles[0].desktop ? 1 : 0;
    size_t desktop_index = find_desktop_tile(state);
    reach_point_f32 point = {state->tiles[app_index].current_rect.x +
                                 state->tiles[app_index].current_rect.width * 0.5f,
                             state->tiles[app_index].current_rect.y +
                                 state->tiles[app_index].current_rect.height * 0.5f};
    const reach_rect_f32 desktop_rect = state->tiles[desktop_index].current_rect;
    expect_true(point.x >= desktop_rect.x && point.x <= desktop_rect.x + desktop_rect.width &&
                    point.y >= desktop_rect.y && point.y <= desktop_rect.y + desktop_rect.height,
                "app center overlaps the fixed desktop thumbnail");

    size_t hit_index = state->tile_count;
    expect_true(reach_stage_tile_at_point(stage, point, &hit_index),
                "overlapping app thumbnail is interactive");
    expect_true(hit_index == app_index && !state->tiles[hit_index].desktop,
                "app thumbnail wins hit testing over desktop");

    reach_stage_destroy(stage);
}

static void test_close_requires_completion_before_teardown(void)
{
    reach_stage *stage = nullptr;
    expect_true(reach_stage_create(&stage) == REACH_OK, "handoff stage is created");
    reach_stage_open_window window = make_window(1, make_rect(100, 120, 640, 480));
    reach_stage_open(stage, make_rect(0, 0, 1920, 1080), 1, &window, 1);
    advance_stage(stage, 30, 0.016);
    const reach_feature_capsule_ops *ops = reach_stage_capsule_ops();
    reach_feature_tick_result tick = {};
    reach_stage_begin_close(stage);
    reach_test_motion_clock_advance(1.0);
    ops->tick(stage, 1.0, &tick);
    const reach_stage_state *state = reach_stage_state_ptr(stage);
    expect_true(state->open && state->close_phase == REACH_STAGE_CLOSE_ALIGNED,
                "a completed movement retains its final thumbnail for presentation");
    reach_test_motion_clock_advance(1.0);
    ops->tick(stage, 1.0, &tick);
    expect_true(state->close_phase == REACH_STAGE_CLOSE_ALIGNED,
                "elapsed time cannot bypass the first completion gate");
    ops->presentation_committed(stage, REACH_OK, &tick);
    reach_test_motion_clock_advance(1.0);
    ops->tick(stage, 1.0, &tick);
    expect_true(state->open && state->close_phase == REACH_STAGE_CLOSE_TRANSPARENT,
                "the transparent backdrop must be presented before disposal");
    ops->presentation_committed(stage, REACH_OK, &tick);
    expect_true(ops->needs_frame(stage), "disposal still schedules the final cleanup frame");
    reach_test_motion_clock_advance(0.016);
    ops->tick(stage, 0.016, &tick);
    expect_true(!reach_stage_is_open(stage), "acknowledged reveal can release Stage");
    reach_stage_destroy(stage);
}

static void test_close_handoff_holds_only_the_reveal(void)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_stage_open_window window = make_window(1, make_rect(100, 120, 640, 480));
    reach_stage_open(stage, make_rect(0, 0, 1920, 1080), 1, &window, 1);
    advance_stage(stage, 30, 0.016);
    const reach_feature_capsule_ops *ops = reach_stage_capsule_ops();
    reach_feature_tick_result tick = {};
    ops->set_close_handoff_pending(stage, 1, &tick);
    reach_stage_begin_close(stage);
    reach_test_motion_clock_advance(1.0);
    ops->tick(stage, 1.0, &tick);
    expect_true(reach_stage_state_ptr(stage)->close_phase == REACH_STAGE_CLOSE_ALIGNED,
                "a pending handoff does not delay Stage movement");
    ops->presentation_committed(stage, REACH_OK, &tick);
    expect_true(reach_stage_state_ptr(stage)->close_phase == REACH_STAGE_CLOSE_ALIGNED,
                "a pending handoff retains the aligned opaque frame");
    expect_true(!ops->needs_frame(stage),
                "an aligned Stage waits for the event without polling frames");
    ops->set_close_handoff_pending(stage, 0, &tick);
    expect_true(reach_stage_state_ptr(stage)->close_phase == REACH_STAGE_CLOSE_REVEALING,
                "handoff completion releases the shared reveal");
    reach_stage_destroy(stage);
}

static void test_app_click_uses_shared_close_animation(void)
{
    reach_stage *button_close = nullptr;
    reach_stage *app_close = nullptr;
    reach_stage_create(&button_close);
    reach_stage_create(&app_close);
    reach_rect_f32 bounds = make_rect(0, 0, 1920, 1080);
    reach_stage_open_window windows[3] = {make_window(1, make_rect(100, 120, 640, 480)),
                                          make_window(2, make_rect(800, 160, 900, 700)),
                                          make_desktop(100, bounds)};
    reach_stage_open(button_close, bounds, 1, windows, 3);
    reach_stage_open(app_close, bounds, 1, windows, 3);
    advance_stage(button_close, 30, 0.016);
    advance_stage(app_close, 30, 0.016);

    reach_capsule_pointer_result action = click_tile(app_close, 0);
    expect_true(action.action.kind == REACH_FEATURE_ACTION_ACTIVATE_WINDOW,
                "an app click publishes activation before the shared close");
    expect_true((action.action.flags & REACH_FEATURE_ACTION_FLAG_CLOSE_HANDOFF) != 0,
                "an app click associates activation with the shared close handoff");
    reach_stage_begin_close(button_close);
    reach_stage_begin_close(app_close);

    reach_feature_tick_result button_tick = {};
    reach_feature_tick_result app_tick = {};
    reach_test_motion_clock_advance(0.050);
    reach_stage_capsule_ops()->tick(button_close, 0.050, &button_tick);
    reach_stage_capsule_ops()->tick(app_close, 0.050, &app_tick);
    const reach_stage_state *button_state = reach_stage_state_ptr(button_close);
    const reach_stage_state *app_state = reach_stage_state_ptr(app_close);
    expect_near(app_state->progress, button_state->progress,
                "app and button closes share app progress");
    expect_near(app_state->desktop_progress, button_state->desktop_progress,
                "app and button closes share Desktop progress");
    for (size_t index = 0; index < app_state->tile_count; ++index)
    {
        expect_near(app_state->tiles[index].current_rect.x,
                    button_state->tiles[index].current_rect.x,
                    "app and button closes share tile x");
        expect_near(app_state->tiles[index].current_rect.y,
                    button_state->tiles[index].current_rect.y,
                    "app and button closes share tile y");
        expect_near(app_state->tiles[index].current_rect.width,
                    button_state->tiles[index].current_rect.width,
                    "app and button closes share tile width");
        expect_near(app_state->tiles[index].current_rect.height,
                    button_state->tiles[index].current_rect.height,
                    "app and button closes share tile height");
        reach_stage_thumbnail_placement button_placement = {};
        reach_stage_thumbnail_placement app_placement = {};
        reach_stage_thumbnail_at(button_close, index, &button_placement);
        reach_stage_thumbnail_at(app_close, index, &app_placement);
        expect_true(app_placement.visible == button_placement.visible,
                    "app and button closes share thumbnail visibility");
    }

    reach_stage_destroy(button_close);
    reach_stage_destroy(app_close);
}

static void test_desktop_uses_a_shorter_relative_animation(void)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_stage_set_animation_seconds(stage, 1.0f);
    reach_stage_open_window windows[2] = {make_window(1, make_rect(100, 120, 1200, 675)),
                                          make_desktop(100, make_rect(0, 0, 1920, 1080))};
    reach_stage_open(stage, make_rect(0, 0, 1920, 1080), 1, windows, 2);

    reach_feature_tick_result tick = {};
    reach_test_motion_clock_advance(0.35);
    reach_stage_capsule_ops()->tick(stage, 0.35, &tick);
    const reach_stage_state *state = reach_stage_state_ptr(stage);
    expect_near(state->desktop_progress, 1.0f,
                "Desktop settles after its relative animation segment");
    expect_true(state->progress > 0.0f && state->progress < 1.0f,
                "app thumbnails retain the configured Stage duration");
    reach_stage_destroy(stage);
}

static void test_desktop_only_close_has_no_hidden_app_wait(void)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_stage_set_animation_seconds(stage, 1.0f);
    reach_stage_open_window desktop = make_desktop(100, make_rect(0, 0, 1920, 1080));
    reach_stage_open(stage, make_rect(0, 0, 1920, 1080), 1, &desktop, 1);
    advance_stage(stage, 1, 0.35);

    reach_stage_begin_close(stage);
    reach_feature_tick_result tick = {};
    reach_test_motion_clock_advance(0.35);
    reach_stage_capsule_ops()->tick(stage, 0.35, &tick);
    expect_true(reach_stage_state_ptr(stage)->close_phase == REACH_STAGE_CLOSE_ALIGNED,
                "Desktop-only close does not wait for an invisible app track");
    reach_stage_destroy(stage);
}

static reach_capsule_pointer_result click_tile(reach_stage *stage, size_t index)
{
    const reach_stage_tile *tile = &reach_stage_state_ptr(stage)->tiles[index];
    reach_pointer_event event = {};
    event.button = REACH_POINTER_BUTTON_PRIMARY;
    event.x = (int32_t)(tile->current_rect.x + tile->current_rect.width * 0.5f);
    event.y = (int32_t)(tile->current_rect.y + tile->current_rect.height * 0.5f);
    event.kind = REACH_POINTER_EVENT_DOWN;
    reach_capsule_pointer_result result = {};
    reach_stage_capsule_ops()->handle_pointer(stage, &event, &result);
    event.kind = REACH_POINTER_EVENT_UP;
    reach_stage_capsule_ops()->handle_pointer(stage, &event, &result);
    return result;
}

static void test_tile_clicks_publish_immediate_window_actions(void)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_stage_open_window app = make_window(1, make_rect(100, 120, 1200, 675));
    reach_stage_open(stage, make_rect(0, 0, 1920, 1080), 1, &app, 1);
    advance_stage(stage, 30, 0.016);
    reach_capsule_pointer_result result = click_tile(stage, 0);
    expect_true(result.action.kind == REACH_FEATURE_ACTION_ACTIVATE_WINDOW,
                "an app tile publishes ordinary activation");
    expect_true((result.action.flags & REACH_FEATURE_ACTION_FLAG_CLOSE_SELF_FIRST) == 0,
                "an app tile queues activation before the shared Stage close");
    expect_true((result.action.flags & REACH_FEATURE_ACTION_FLAG_CLOSE_HANDOFF) != 0,
                "an app tile requests a synchronized close handoff");

    reach_stage_force_close(stage);
    app.minimized = 1;
    reach_stage_open(stage, make_rect(0, 0, 1920, 1080), 1, &app, 1);
    advance_stage(stage, 30, 0.016);
    result = click_tile(stage, 0);
    expect_true(result.action.kind == REACH_FEATURE_ACTION_ACTIVATE_WINDOW,
                "a minimized app tile publishes ordinary activation");
    expect_true((result.action.flags & REACH_FEATURE_ACTION_FLAG_CLOSE_SELF_FIRST) == 0,
                "a minimized app uses the same activation and close ordering");
    expect_true((result.action.flags & REACH_FEATURE_ACTION_FLAG_CLOSE_HANDOFF) != 0,
                "a minimized app uses the same synchronized handoff");

    reach_stage_force_close(stage);
    reach_stage_open_window desktop = make_desktop(100, make_rect(0, 0, 1920, 1080));
    reach_stage_open(stage, make_rect(0, 0, 1920, 1080), 1, &desktop, 1);
    advance_stage(stage, 30, 0.016);
    result = click_tile(stage, 0);
    expect_true(result.action.kind == REACH_FEATURE_ACTION_MINIMIZE_ALL_WINDOWS,
                "the Desktop tile publishes minimize all");
    expect_true((result.action.flags & REACH_FEATURE_ACTION_FLAG_CLOSE_SELF_FIRST) == 0,
                "the Desktop tile uses the shared Stage close");
    expect_true((result.action.flags & REACH_FEATURE_ACTION_FLAG_CLOSE_HANDOFF) != 0,
                "the Desktop tile waits for minimize-all before reveal");
    reach_stage_destroy(stage);
}

static void test_closing_stage_retargets_restored_windows(void)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_rect_f32 bounds = make_rect(0, 0, 1920, 1080);
    reach_stage_open_window app = make_window(1, make_rect(100, 120, 1200, 675));
    app.minimized = 1;
    reach_stage_open(stage, bounds, 1, &app, 1);
    advance_stage(stage, 30, 0.016);
    reach_stage_begin_close(stage);
    reach_feature_tick_result tick = {};
    reach_test_motion_clock_advance(0.016);
    reach_stage_capsule_ops()->tick(stage, 0.016, &tick);
    reach_rect_f32 before = reach_stage_state_ptr(stage)->tiles[0].current_rect;

    app.minimized = 0;
    app.frame = make_rect(300, 220, 900, 700);
    expect_true(reach_stage_update_windows(stage, &app, 1),
                "a restored window retargets the shared close path");
    reach_test_motion_clock_advance(0.0);
    reach_stage_capsule_ops()->tick(stage, 0.0, &tick);
    const reach_stage_state *state = reach_stage_state_ptr(stage);
    expect_near(state->tiles[0].current_rect.x, before.x,
                "retargeting preserves the current visual x");
    expect_near(state->tiles[0].current_rect.y, before.y,
                "retargeting preserves the current visual y");
    reach_stage_thumbnail_placement placement = {};
    reach_stage_thumbnail_at(stage, 0, &placement);
    expect_true(placement.visible, "the restored live thumbnail joins the shared close");
    reach_test_motion_clock_advance(1.0);
    reach_stage_capsule_ops()->tick(stage, 1.0, &tick);
    state = reach_stage_state_ptr(stage);
    expect_near(state->tiles[0].current_rect.x, app.frame.x,
                "the shared close lands on the restored x");
    expect_near(state->tiles[0].current_rect.y, app.frame.y,
                "the shared close lands on the restored y");
    reach_stage_destroy(stage);
}

static void test_restored_thumbnail_is_committed_before_reveal(void)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_stage_open_window app = make_window(1, make_rect(100, 120, 1200, 675));
    app.minimized = 1;
    reach_stage_open(stage, make_rect(0, 0, 1920, 1080), 1, &app, 1);
    advance_stage(stage, 30, 0.016);
    const reach_feature_capsule_ops *ops = reach_stage_capsule_ops();
    reach_feature_tick_result tick = {};
    ops->set_close_handoff_pending(stage, 1, &tick);
    reach_stage_begin_close(stage);
    reach_test_motion_clock_advance(1.0);
    ops->tick(stage, 1.0, &tick);
    ops->presentation_committed(stage, REACH_OK, &tick);

    app.minimized = 0;
    expect_true(reach_stage_update_windows(stage, &app, 1),
                "a restored thumbnail changes the aligned presentation");
    expect_true(!reach_stage_state_ptr(stage)->close_aligned_committed,
                "the restored thumbnail invalidates the old aligned frame");
    expect_true(reach_stage_state_ptr(stage)->close_phase == REACH_STAGE_CLOSE_MOVING,
                "a late restored thumbnail retargets from the minimized destination");
    ops->set_close_handoff_pending(stage, 0, &tick);
    expect_true(reach_stage_state_ptr(stage)->close_phase == REACH_STAGE_CLOSE_MOVING,
                "handoff completion waits for restored thumbnail movement");
    expect_true(ops->needs_frame(stage), "the restored thumbnail schedules a synchronized frame");
    reach_test_motion_clock_advance(1.0);
    ops->tick(stage, 1.0, &tick);
    expect_true(reach_stage_state_ptr(stage)->close_phase == REACH_STAGE_CLOSE_ALIGNED,
                "the restored thumbnail reaches its live frame before reveal");
    ops->presentation_committed(stage, REACH_OK, &tick);
    expect_true(reach_stage_state_ptr(stage)->close_phase == REACH_STAGE_CLOSE_REVEALING,
                "the shared reveal starts after the restored thumbnail is committed");
    reach_stage_destroy(stage);
}

static void test_minimized_close_exits_top_left_without_expanding(void)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_rect_f32 bounds = make_rect(0, 0, 1920, 1080);
    reach_stage_open_window app = make_window(1, make_rect(0, 0, 1920, 1040));
    app.minimized = 1;
    reach_stage_open(stage, bounds, 1, &app, 1);
    expect_near(reach_stage_state_ptr(stage)->tiles[0].current_rect.width, app.frame.width,
                "a minimized window keeps its opening source width");
    expect_near(reach_stage_state_ptr(stage)->tiles[0].current_rect.height, app.frame.height,
                "a minimized window keeps its opening source height");
    advance_stage(stage, 30, 0.016);

    reach_rect_f32 settled = reach_stage_state_ptr(stage)->tiles[0].current_rect;
    reach_stage_begin_close(stage);
    reach_feature_tick_result tick = {};
    reach_test_motion_clock_advance(0.1);
    reach_stage_capsule_ops()->tick(stage, 0.1, &tick);
    const reach_stage_state *state = reach_stage_state_ptr(stage);
    expect_near(state->tiles[0].current_rect.width, settled.width,
                "a minimized placeholder does not expand while closing");
    expect_near(state->tiles[0].current_rect.height, settled.height,
                "a minimized placeholder keeps its settled height while closing");

    reach_test_motion_clock_advance(1.0);
    reach_stage_capsule_ops()->tick(stage, 1.0, &tick);
    state = reach_stage_state_ptr(stage);
    expect_true(state->tiles[0].current_rect.x + state->tiles[0].current_rect.width <= bounds.x,
                "a minimized placeholder lands beyond the left edge");
    expect_true(state->tiles[0].current_rect.y + state->tiles[0].current_rect.height <= bounds.y,
                "a minimized placeholder lands beyond the top edge");
    reach_stage_destroy(stage);
}

static void test_closing_stage_retargets_newly_minimized_windows(void)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_rect_f32 bounds = make_rect(0, 0, 1920, 1080);
    reach_stage_open_window app = make_window(1, make_rect(100, 120, 1200, 675));
    reach_stage_open(stage, bounds, 1, &app, 1);
    advance_stage(stage, 30, 0.016);
    reach_stage_begin_close(stage);

    reach_feature_tick_result tick = {};
    reach_test_motion_clock_advance(0.016);
    reach_stage_capsule_ops()->tick(stage, 0.016, &tick);
    reach_rect_f32 before = reach_stage_state_ptr(stage)->tiles[0].current_rect;
    app.minimized = 1;
    expect_true(reach_stage_update_windows(stage, &app, 1),
                "a newly minimized window retargets the shared close path");
    reach_test_motion_clock_advance(0.0);
    reach_stage_capsule_ops()->tick(stage, 0.0, &tick);
    const reach_stage_state *state = reach_stage_state_ptr(stage);
    expect_near(state->tiles[0].current_rect.x, before.x,
                "minimize retargeting preserves the current visual x");
    expect_near(state->tiles[0].current_rect.y, before.y,
                "minimize retargeting preserves the current visual y");

    reach_test_motion_clock_advance(1.0);
    reach_stage_capsule_ops()->tick(stage, 1.0, &tick);
    state = reach_stage_state_ptr(stage);
    expect_true(state->tiles[0].current_rect.x + state->tiles[0].current_rect.width <= bounds.x,
                "a newly minimized placeholder lands beyond the left edge");
    expect_true(state->tiles[0].current_rect.y + state->tiles[0].current_rect.height <= bounds.y,
                "a newly minimized placeholder lands beyond the top edge");
    reach_stage_destroy(stage);
}

static void test_departing_tile_fades_three_times_faster_than_reflow(void)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_stage_open_window windows[2] = {make_window(1, make_rect(100, 120, 900, 600)),
                                          make_window(2, make_rect(1100, 160, 700, 500))};
    reach_stage_open(stage, make_rect(0, 0, 1920, 1080), 1, windows, 2);
    advance_stage(stage, 30, 0.016);

    reach_rect_f32 close_button = reach_stage_tile_close_button_rect(stage, 0);
    reach_pointer_event event = {};
    event.button = REACH_POINTER_BUTTON_PRIMARY;
    event.x = (int32_t)(close_button.x + close_button.width * 0.5f);
    event.y = (int32_t)(close_button.y + close_button.height * 0.5f);
    event.kind = REACH_POINTER_EVENT_DOWN;
    reach_capsule_pointer_result result = {};
    reach_stage_capsule_ops()->handle_pointer(stage, &event, &result);
    event.kind = REACH_POINTER_EVENT_UP;
    reach_stage_capsule_ops()->handle_pointer(stage, &event, &result);
    expect_true(result.action.kind == REACH_FEATURE_ACTION_CLOSE_WINDOW,
                "the Stage close button starts a window departure");
    reach_feature_tick_result tick = {};
    reach_test_motion_clock_advance((double)reach_theme_default()->stage_reflow_seconds / 3.0);
    reach_stage_capsule_ops()->tick(
        stage, (double)reach_theme_default()->stage_reflow_seconds / 3.0, &tick);
    const reach_stage_state *state = reach_stage_state_ptr(stage);
    expect_near(state->tiles[0].presence, 0.0f,
                "a closed Stage window finishes fading after one third of reflow");
    expect_true(state->reflow > 0.0f && state->reflow < 1.0f,
                "remaining Stage windows keep the original reflow duration");
    expect_true(state->tile_count == 2,
                "the faded window remains retained until layout reflow settles");

    reach_test_motion_clock_advance(1.0);
    reach_stage_capsule_ops()->tick(stage, 1.0, &tick);
    expect_true(reach_stage_state_ptr(stage)->tile_count == 1,
                "the closed Stage window is removed when reflow settles");
    reach_stage_destroy(stage);
}

static void test_app_grid_fits_inside_desktop_preview(void)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_rect_f32 bounds = make_rect(0, 0, 1920, 1080);
    reach_stage_open_window windows[4] = {
        make_window(1, make_rect(0, 0, 1600, 900)), make_window(2, make_rect(1600, 0, 900, 800)),
        make_window(3, make_rect(1600, 800, 900, 800)), make_desktop(100, bounds)};
    windows[1].monitor_index = 1;
    windows[1].monitor_portrait = 1;
    windows[2].monitor_index = 1;
    windows[2].monitor_portrait = 1;
    reach_stage_open(stage, bounds, 1, windows, 4);
    advance_stage(stage, 30, 0.016);

    const reach_stage_state *state = reach_stage_state_ptr(stage);
    size_t desktop_index = find_desktop_tile(state);
    const reach_rect_f32 desktop = state->tiles[desktop_index].target_rect;
    for (size_t index = 0; index < state->tile_count; ++index)
    {
        const reach_stage_tile *tile = &state->tiles[index];
        if (tile->desktop)
        {
            continue;
        }
        expect_true(tile->target_rect.x >= desktop.x + 23.0f,
                    "app grid keeps its left inset inside Desktop");
        expect_true(tile->target_rect.x + tile->target_rect.width <=
                        desktop.x + desktop.width - 23.0f,
                    "app grid keeps its right inset inside Desktop");
        expect_true(tile->target_rect.y - tile->bar_height >= desktop.y + 23.0f,
                    "app grid keeps its top inset inside Desktop");
        expect_true(tile->target_rect.y + tile->target_rect.height <=
                        desktop.y + desktop.height - 23.0f,
                    "app grid keeps its bottom inset inside Desktop");
    }
    reach_stage_destroy(stage);
}

static void test_portrait_monitor_apps_stack_by_screen_position(void)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_stage_open_window windows[2] = {make_window(1, make_rect(1920, 900, 900, 700)),
                                          make_window(2, make_rect(1920, 0, 900, 800))};
    windows[0].monitor_portrait = 1;
    windows[1].monitor_portrait = 1;
    reach_stage_open(stage, make_rect(0, 0, 1920, 1080), 1, windows, 2);
    advance_stage(stage, 30, 0.016);

    const reach_stage_state *state = reach_stage_state_ptr(stage);
    float lower_center = state->tiles[0].target_rect.x + state->tiles[0].target_rect.width * 0.5f;
    float upper_center = state->tiles[1].target_rect.x + state->tiles[1].target_rect.width * 0.5f;
    expect_true(state->tiles[1].target_rect.y < state->tiles[0].target_rect.y,
                "portrait-monitor apps preserve their vertical screen order");
    expect_near(lower_center, upper_center, "portrait-monitor apps share one vertical column");
    expect_true(state->tiles[0].target_rect.height > 350.0f &&
                    state->tiles[1].target_rect.height > 350.0f,
                "portrait-monitor apps use the available vertical space");
    reach_stage_destroy(stage);
}

static void test_portrait_monitor_receives_more_scale_when_width_is_constrained(void)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_rect_f32 bounds = make_rect(0, 0, 1920, 1080);
    reach_stage_open_window windows[3] = {make_window(1, make_rect(0, 0, 1600, 900)),
                                          make_window(2, make_rect(1920, 0, 900, 1600)),
                                          make_desktop(100, bounds)};
    windows[1].monitor_index = 1;
    windows[1].monitor_portrait = 1;
    reach_stage_open(stage, bounds, 1, windows, 3);
    advance_stage(stage, 30, 0.016);

    const reach_stage_state *state = reach_stage_state_ptr(stage);
    expect_true(state->tiles[1].target_rect.height > state->tiles[0].target_rect.height * 1.25f,
                "portrait-monitor apps receive more scale than landscape-monitor apps");
    reach_stage_destroy(stage);
}

static void test_app_targets_are_visually_centered(void)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_stage_open_window windows[2] = {make_window(1, make_rect(0, 0, 1200, 900)),
                                          make_window(2, make_rect(1920, 0, 2100, 900))};
    windows[1].monitor_index = 1;
    reach_stage_open(stage, make_rect(0, 0, 1920, 1080), 1, windows, 2);
    advance_stage(stage, 30, 0.016);

    const reach_stage_state *state = reach_stage_state_ptr(stage);
    float left = state->tiles[0].target_rect.x < state->tiles[1].target_rect.x
                     ? state->tiles[0].target_rect.x
                     : state->tiles[1].target_rect.x;
    float right0 = state->tiles[0].target_rect.x + state->tiles[0].target_rect.width;
    float right1 = state->tiles[1].target_rect.x + state->tiles[1].target_rect.width;
    float right = right0 > right1 ? right0 : right1;
    expect_near(left - 24.0f, 1896.0f - right,
                "actual fitted thumbnails have balanced outer margins");
    reach_stage_destroy(stage);
}

static void test_adjacent_apps_use_a_compact_gap(void)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_stage_open_window windows[2] = {make_window(1, make_rect(0, 0, 1600, 900)),
                                          make_window(2, make_rect(1600, 0, 1600, 900))};
    reach_stage_open(stage, make_rect(0, 0, 1920, 1080), 1, windows, 2);
    advance_stage(stage, 30, 0.016);

    const reach_stage_state *state = reach_stage_state_ptr(stage);
    float gap = state->tiles[1].target_rect.x -
                (state->tiles[0].target_rect.x + state->tiles[0].target_rect.width);
    expect_true(gap >= 16.0f && gap <= 32.0f, "adjacent apps use a compact visual gap");
    reach_stage_destroy(stage);
}

static void test_single_app_uses_the_available_stage_area(void)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_stage_open_window app = make_window(1, make_rect(0, 0, 1600, 900));
    reach_stage_open(stage, make_rect(0, 0, 1920, 1080), 1, &app, 1);
    advance_stage(stage, 30, 0.016);

    const reach_rect_f32 target = reach_stage_state_ptr(stage)->tiles[0].target_rect;
    expect_true(target.width > 1600.0f && target.height > 850.0f,
                "a single app uses most of the available Stage area");
    reach_stage_destroy(stage);
}

static size_t stage_group_count(reach_stage *stage, reach_render_command_buffer *commands)
{
    reach_stage_render_context ctx = {};
    ctx.theme = reach_theme_default();
    ctx.bounds = make_rect(0, 0, 1920, 1080);
    ctx.dpi_scale = 1.0f;
    expect_true(reach_stage_append_render_commands(stage, &ctx, commands) == REACH_OK,
                "stage render commands are appended");
    return commands->group_count;
}

static void move_pointer_to(reach_stage *stage, reach_point_f32 point)
{
    reach_pointer_event event = {};
    event.kind = REACH_POINTER_EVENT_MOVE;
    event.x = (int32_t)point.x;
    event.y = (int32_t)point.y;
    reach_capsule_pointer_result result = {};
    reach_stage_capsule_ops()->handle_pointer(stage, &event, &result);
}

static void test_close_hover_groups_only_while_hovered(void)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_stage_open_window windows[3] = {make_window(1, make_rect(100, 120, 640, 480)),
                                          make_window(2, make_rect(300, 220, 640, 480)),
                                          make_window(3, make_rect(500, 320, 640, 480))};
    reach_stage_open(stage, make_rect(0, 0, 1920, 1080), 1, windows, 3);
    advance_stage(stage, 30, 0.016);
    auto commands = std::make_unique<reach_render_command_buffer>();
    expect_true(stage_group_count(stage, commands.get()) == 0,
                "no close hover groups exist before any hover");

    reach_rect_f32 button = reach_stage_tile_close_button_rect(stage, 1);
    move_pointer_to(stage, {button.x + button.width * 0.5f, button.y + button.height * 0.5f});
    advance_stage(stage, 1, 0.016);
    expect_true(stage_group_count(stage, commands.get()) == 2,
                "the hovered close button emits its backing and glyph groups");

    move_pointer_to(stage, {1.0f, 1.0f});
    advance_stage(stage, 60, 0.016);
    expect_true(stage_group_count(stage, commands.get()) == 0,
                "a settled leave drops the stale close hover groups");
    reach_stage_destroy(stage);
}

static const uint16_t golden_alpha_label[] = {'A', 'l', 'p', 'h', 'a', 0};
static const uint16_t golden_beta_label[] = {'B', 'e', 't', 'a', 0};

static reach_stage *open_golden_stage(float dpi)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_stage_open_window windows[3] = {make_window(1, make_rect(100, 120, 640, 480)),
                                          make_window(2, make_rect(300, 220, 800, 500)),
                                          make_desktop(5, make_rect(0, 0, 1920, 1080))};
    windows[0].label = golden_alpha_label;
    windows[1].label = golden_beta_label;
    windows[1].minimized = 1;
    windows[1].icon_id = 9;
    reach_stage_open(stage, make_rect(0, 0, 1920, 1080), dpi, windows, 3);
    advance_stage(stage, 80, 0.016);
    return stage;
}

static void append_golden_commands(reach_stage *stage, float dpi,
                                   reach_render_command_buffer *commands)
{
    reach_stage_render_context ctx = {};
    ctx.theme = reach_theme_default();
    ctx.bounds = make_rect(0, 0, 1920, 1080);
    ctx.dpi_scale = dpi;
    expect_true(reach_stage_append_render_commands(stage, &ctx, commands) == REACH_OK,
                "golden stage render commands are appended");
}

struct golden_row
{
    int type;
    float x, y, width, height, radius;
    int corner_mask;
    float r, g, b, a;
    unsigned tag;
};

static const golden_row golden_rows_100[] = {
    {1, 60.0f, 220.0f, 810.0f, 640.0f, 13.0f, 3, 0.39607844f, 0.39607844f, 0.39607844f, 1.0f, 0},
    {1, 61.0f, 221.0f, 808.0f, 638.0f, 12.0f, 3, 0.06666667f, 0.07058824f, 0.06666667f, 1.0f, 0},
    {1, 61.333374f, 221.04547f, 807.8788f, 32.0f, 12.0f, 3, 0.14509805f, 0.14901961f, 0.14509805f,
     0.45f, 0},
    {2, 69.333374f, 221.04547f, 775.8788f, 32.0f, 0.0f, 0, 0.79607844f, 0.79607844f, 0.79607844f,
     1.0f, 65},
    {1, 845.21216f, 229.04547f, 16.0f, 16.0f, 8.0f, 0, 0.5803922f, 0.5882353f, 0.5803922f, 1.0f, 0},
    {10, 849.0522f, 232.88547f, 8.32f, 8.32f, 0.0f, 0, 1.0f, 1.0f, 1.0f, 0.8f, 35},
    {1, 888.0f, 220.0f, 972.0f, 640.0f, 13.0f, 3, 0.39607844f, 0.39607844f, 0.39607844f, 1.0f, 0},
    {1, 889.0f, 221.0f, 970.0f, 638.0f, 12.0f, 3, 0.06666667f, 0.07058824f, 0.06666667f, 1.0f, 0},
    {1, 889.21216f, 221.04547f, 969.4545f, 32.0f, 12.0f, 3, 0.14509805f, 0.14901961f, 0.14509805f,
     0.45f, 0},
    {2, 897.21216f, 221.04547f, 937.4545f, 32.0f, 0.0f, 0, 0.79607844f, 0.79607844f, 0.79607844f,
     1.0f, 66},
    {1, 1834.6667f, 229.04547f, 16.0f, 16.0f, 8.0f, 0, 0.5803922f, 0.5882353f, 0.5803922f, 1.0f, 0},
    {10, 1838.5067f, 232.88547f, 8.32f, 8.32f, 0.0f, 0, 1.0f, 1.0f, 1.0f, 0.8f, 35},
    {1, 889.21216f, 253.04547f, 969.4545f, 605.90906f, 0.0f, 0, 0.13333334f, 0.14117648f,
     0.13333334f, 1.0f, 0},
    {3, 1349.9395f, 532.0f, 48.0f, 48.0f, 0.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, 9},
    {1, 36.333313f, 20.0f, 1847.3334f, 1.0f, 0.0f, 0, 0.39607844f, 0.39607844f, 0.39607844f, 1.0f,
     0},
    {1, 36.333313f, 1059.0f, 1847.3334f, 1.0f, 0.0f, 0, 0.39607844f, 0.39607844f, 0.39607844f, 1.0f,
     0},
    {1, 36.333313f, 21.0f, 1.0f, 1038.0f, 0.0f, 0, 0.39607844f, 0.39607844f, 0.39607844f, 1.0f, 0},
    {1, 1882.6667f, 21.0f, 1.0f, 1038.0f, 0.0f, 0, 0.39607844f, 0.39607844f, 0.39607844f, 1.0f, 0},
};

static const golden_row golden_rows_125[] = {
    {1, 75.0f, 222.0f, 795.0f, 636.0f, 16.25f, 3, 0.39607844f, 0.39607844f, 0.39607844f, 1.0f, 0},
    {1, 76.25f, 223.25f, 792.5f, 633.5f, 15.0f, 3, 0.06666667f, 0.07058824f, 0.06666667f, 1.0f, 0},
    {1, 76.66675f, 223.12503f, 791.6666f, 40.0f, 15.0f, 3, 0.14509805f, 0.14901961f, 0.14509805f,
     0.45f, 0},
    {2, 86.66675f, 223.12503f, 751.6666f, 40.0f, 0.0f, 0, 0.79607844f, 0.79607844f, 0.79607844f,
     1.0f, 65},
    {1, 838.3334f, 233.12503f, 20.0f, 20.0f, 10.0f, 0, 0.5803922f, 0.5882353f, 0.5803922f, 1.0f, 0},
    {10, 843.13336f, 237.92503f, 10.400001f, 10.400001f, 0.0f, 0, 1.0f, 1.0f, 1.0f, 0.8f, 35},
    {1, 892.0f, 222.0f, 953.0f, 636.0f, 16.25f, 3, 0.39607844f, 0.39607844f, 0.39607844f, 1.0f, 0},
    {1, 893.25f, 223.25f, 950.5f, 633.5f, 15.0f, 3, 0.06666667f, 0.07058824f, 0.06666667f, 1.0f, 0},
    {1, 893.3334f, 223.12503f, 949.99994f, 40.0f, 15.0f, 3, 0.14509805f, 0.14901961f, 0.14509805f,
     0.45f, 0},
    {2, 903.3334f, 223.12503f, 909.99994f, 40.0f, 0.0f, 0, 0.79607844f, 0.79607844f, 0.79607844f,
     1.0f, 66},
    {1, 1813.3333f, 233.12503f, 20.0f, 20.0f, 10.0f, 0, 0.5803922f, 0.5882353f, 0.5803922f, 1.0f,
     0},
    {10, 1818.1333f, 237.92503f, 10.400001f, 10.400001f, 0.0f, 0, 1.0f, 1.0f, 1.0f, 0.8f, 35},
    {1, 893.3334f, 263.12503f, 949.99994f, 593.74994f, 0.0f, 0, 0.13333334f, 0.14117648f,
     0.13333334f, 1.0f, 0},
    {3, 1338.3334f, 530.0f, 60.0f, 60.0f, 0.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, 9},
    {1, 45.416687f, 25.0f, 1829.1666f, 1.25f, 0.0f, 0, 0.39607844f, 0.39607844f, 0.39607844f, 1.0f,
     0},
    {1, 45.416687f, 1053.75f, 1829.1666f, 1.25f, 0.0f, 0, 0.39607844f, 0.39607844f, 0.39607844f,
     1.0f, 0},
    {1, 45.416687f, 26.25f, 1.25f, 1027.5f, 0.0f, 0, 0.39607844f, 0.39607844f, 0.39607844f, 1.0f,
     0},
    {1, 1873.3333f, 26.25f, 1.25f, 1027.5f, 0.0f, 0, 0.39607844f, 0.39607844f, 0.39607844f, 1.0f,
     0},
};

static const golden_row golden_rows_150[] = {
    {1, 91.0f, 224.0f, 778.0f, 632.0f, 19.5f, 3, 0.39607844f, 0.39607844f, 0.39607844f, 1.0f, 0},
    {1, 92.5f, 225.5f, 775.0f, 629.0f, 18.0f, 3, 0.06666667f, 0.07058824f, 0.06666667f, 1.0f, 0},
    {1, 92.00012f, 225.20459f, 775.45447f, 48.0f, 18.0f, 3, 0.14509805f, 0.14901961f, 0.14509805f,
     0.45f, 0},
    {2, 104.00012f, 225.20459f, 727.45447f, 48.0f, 0.0f, 0, 0.79607844f, 0.79607844f, 0.79607844f,
     1.0f, 65},
    {1, 831.4546f, 237.20459f, 24.0f, 24.0f, 12.0f, 0, 0.5803922f, 0.5882353f, 0.5803922f, 1.0f, 0},
    {10, 837.2146f, 242.96458f, 12.4800005f, 12.4800005f, 0.0f, 0, 1.0f, 1.0f, 1.0f, 0.8f, 35},
    {1, 896.0f, 224.0f, 934.0f, 632.0f, 19.5f, 3, 0.39607844f, 0.39607844f, 0.39607844f, 1.0f, 0},
    {1, 897.5f, 225.5f, 931.0f, 629.0f, 18.0f, 3, 0.06666667f, 0.07058824f, 0.06666667f, 1.0f, 0},
    {1, 897.4546f, 225.20459f, 930.54535f, 48.0f, 18.0f, 3, 0.14509805f, 0.14901961f, 0.14509805f,
     0.45f, 0},
    {2, 909.4546f, 225.20459f, 882.54535f, 48.0f, 0.0f, 0, 0.79607844f, 0.79607844f, 0.79607844f,
     1.0f, 66},
    {1, 1792.0f, 237.20459f, 24.0f, 24.0f, 12.0f, 0, 0.5803922f, 0.5882353f, 0.5803922f, 1.0f, 0},
    {10, 1797.76f, 242.96458f, 12.4800005f, 12.4800005f, 0.0f, 0, 1.0f, 1.0f, 1.0f, 0.8f, 35},
    {1, 897.4546f, 273.2046f, 930.54535f, 581.5908f, 0.0f, 0, 0.13333334f, 0.14117648f, 0.13333334f,
     1.0f, 0},
    {3, 1326.7273f, 528.0f, 72.0f, 72.0f, 0.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, 9},
    {1, 54.5f, 30.0f, 1811.0f, 1.5f, 0.0f, 0, 0.39607844f, 0.39607844f, 0.39607844f, 1.0f, 0},
    {1, 54.5f, 1048.5f, 1811.0f, 1.5f, 0.0f, 0, 0.39607844f, 0.39607844f, 0.39607844f, 1.0f, 0},
    {1, 54.5f, 31.5f, 1.5f, 1017.0f, 0.0f, 0, 0.39607844f, 0.39607844f, 0.39607844f, 1.0f, 0},
    {1, 1864.0f, 31.5f, 1.5f, 1017.0f, 0.0f, 0, 0.39607844f, 0.39607844f, 0.39607844f, 1.0f, 0},
};

static void expect_golden_rows(float dpi, const golden_row *rows, size_t count)
{
    reach_stage *stage = open_golden_stage(dpi);
    auto commands = std::make_unique<reach_render_command_buffer>();
    append_golden_commands(stage, dpi, commands.get());
    expect_true(commands->count == count, "the settled stage emits the golden command count");
    for (size_t index = 0; index < count && index < commands->count; ++index)
    {
        const reach_render_command &c = commands->commands[index];
        const golden_row &g = rows[index];
        unsigned tag =
            c.type == REACH_RENDER_COMMAND_TEXT ? (unsigned)c.text[0] : (unsigned)c.icon_id;
        bool same = (int)c.type == g.type && c.rect.x == g.x && c.rect.y == g.y &&
                    c.rect.width == g.width && c.rect.height == g.height && c.radius == g.radius &&
                    c.corner_mask == g.corner_mask && c.color.r == g.r && c.color.g == g.g &&
                    c.color.b == g.b && c.color.a == g.a && tag == g.tag;
        if (!same)
        {
            fprintf(stderr, "golden dpi %.2f command %zu differs\n", (double)dpi, index);
        }
        expect_true(same, "a settled stage command matches the golden geometry and color");
    }
    reach_stage_destroy(stage);
}

static void test_stage_settled_commands_match_golden(void)
{
    expect_golden_rows(1.0f, golden_rows_100, sizeof(golden_rows_100) / sizeof(golden_rows_100[0]));
    expect_golden_rows(1.25f, golden_rows_125,
                       sizeof(golden_rows_125) / sizeof(golden_rows_125[0]));
    expect_golden_rows(1.5f, golden_rows_150, sizeof(golden_rows_150) / sizeof(golden_rows_150[0]));
}

static void expect_rect_near(reach_rect_f32 actual, reach_rect_f32 expected, const char *message)
{
    expect_near(actual.x, expected.x, message);
    expect_near(actual.y, expected.y, message);
    expect_near(actual.width, expected.width, message);
    expect_near(actual.height, expected.height, message);
}

static int32_t tile_motion_active(const reach_stage_tile *tile, double now)
{
    return reach_motion_active_at(&tile->rect_motion.x, now) ||
           reach_motion_active_at(&tile->rect_motion.y, now) ||
           reach_motion_active_at(&tile->rect_motion.width, now) ||
           reach_motion_active_at(&tile->rect_motion.height, now) ||
           reach_motion_active_at(&tile->bar_motion, now) ||
           reach_motion_active_at(&tile->fade_motion, now);
}

static void expect_tile_motions_sample_to_state(reach_stage *stage, const char *phase)
{
    const reach_stage_state *state = reach_stage_state_ptr(stage);
    double now = reach_motion_now_seconds();
    for (size_t index = 0; index < state->tile_count; ++index)
    {
        const reach_stage_tile *tile = &state->tiles[index];
        expect_rect_near(reach_rect_motion_value_at(&tile->rect_motion, now), tile->current_rect,
                         phase);
        expect_near(reach_motion_value_at(&tile->bar_motion, now), tile->current_bar.height, phase);
        expect_near(reach_motion_value_at(&tile->fade_motion, now),
                    state->progress * tile->presence, phase);
        reach_stage_thumbnail_placement placement = {};
        expect_true(reach_stage_thumbnail_at(stage, index, &placement) == REACH_OK,
                    "the tile publishes its thumbnail placement");
        expect_rect_near(reach_rect_motion_value_at(&placement.destination_motion, now),
                         placement.destination, phase);
    }
}

static void test_stage_tile_motions_sample_to_current_rect(void)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_stage_open_window windows[3] = {make_window(1, make_rect(100, 120, 640, 480)),
                                          make_window(2, make_rect(300, 220, 800, 500)),
                                          make_desktop(5, make_rect(0, 0, 1920, 1080))};
    windows[1].minimized = 1;
    reach_stage_open(stage, make_rect(0, 0, 1920, 1080), 1.0f, windows, 3);
    for (int step = 0; step < 6; ++step)
    {
        advance_stage(stage, 1, 0.016);
        expect_tile_motions_sample_to_state(stage, "opening motions sample to the tile state");
    }
    const reach_stage_state *state = reach_stage_state_ptr(stage);
    expect_true(tile_motion_active(&state->tiles[0], reach_motion_now_seconds()),
                "an opening app tile publishes animated motions");

    advance_stage(stage, 60, 0.016);
    expect_tile_motions_sample_to_state(stage, "settled motions sample to the tile state");
    for (size_t index = 0; index < state->tile_count; ++index)
    {
        const reach_stage_tile *tile = &state->tiles[index];
        expect_true(!tile_motion_active(tile, reach_motion_now_seconds()),
                    "a settled tile publishes constant motions");
        expect_true(reach_motion_value(&tile->rect_motion.x) == tile->current_rect.x &&
                        reach_motion_value(&tile->rect_motion.width) == tile->current_rect.width &&
                        reach_motion_value(&tile->bar_motion) == tile->current_bar.height,
                    "settled motions equal the tile state exactly");
    }

    reach_stage_begin_close(stage);
    for (int step = 0; step < 6; ++step)
    {
        advance_stage(stage, 1, 0.016);
        expect_tile_motions_sample_to_state(stage, "closing motions sample to the tile state");
    }
    reach_stage_destroy(stage);
}

static void test_stage_reflow_motions_sample_to_current_rect(void)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_stage_open_window windows[3] = {make_window(1, make_rect(100, 120, 640, 480)),
                                          make_window(2, make_rect(300, 220, 800, 500)),
                                          make_window(3, make_rect(500, 320, 640, 480))};
    reach_stage_open(stage, make_rect(0, 0, 1920, 1080), 1.0f, windows, 3);
    advance_stage(stage, 60, 0.016);
    reach_stage_update_windows(stage, windows, 2);
    for (int step = 0; step < 8; ++step)
    {
        advance_stage(stage, 1, 0.016);
        expect_tile_motions_sample_to_state(stage, "departing reflow motions sample to the tile");
    }
    windows[2] = make_window(4, make_rect(700, 100, 640, 480));
    reach_stage_update_windows(stage, windows, 3);
    for (int step = 0; step < 8; ++step)
    {
        advance_stage(stage, 1, 0.016);
        expect_tile_motions_sample_to_state(stage, "arriving reflow motions sample to the tile");
    }
    advance_stage(stage, 60, 0.016);
    expect_tile_motions_sample_to_state(stage, "settled reflow motions sample to the tile");
    reach_stage_destroy(stage);
}

int main(void)
{
    test_stage_tile_motions_sample_to_current_rect();
    test_stage_reflow_motions_sample_to_current_rect();
    test_stage_settled_commands_match_golden();
    test_close_hover_groups_only_while_hovered();
    test_close_requires_completion_before_teardown();
    test_close_handoff_holds_only_the_reveal();
    test_app_click_uses_shared_close_animation();
    test_desktop_uses_a_shorter_relative_animation();
    test_desktop_only_close_has_no_hidden_app_wait();
    test_tile_clicks_publish_immediate_window_actions();
    test_closing_stage_retargets_restored_windows();
    test_restored_thumbnail_is_committed_before_reveal();
    test_minimized_close_exits_top_left_without_expanding();
    test_closing_stage_retargets_newly_minimized_windows();
    test_departing_tile_fades_three_times_faster_than_reflow();
    test_app_grid_fits_inside_desktop_preview();
    test_portrait_monitor_apps_stack_by_screen_position();
    test_portrait_monitor_receives_more_scale_when_width_is_constrained();
    test_app_targets_are_visually_centered();
    test_adjacent_apps_use_a_compact_gap();
    test_single_app_uses_the_available_stage_area();
    test_open_and_close_state_machine();
    test_force_close_keeps_configured_animation();
    test_closing_stage_finishes_without_external_wake_ups();
    test_close_before_the_first_tick_completes();
    test_closing_lands_on_the_current_window_frame();
    test_desktop_keeps_solo_geometry_behind_apps();
    test_apps_take_pointer_priority_over_desktop();
    return failures == 0 ? 0 : 1;
}
