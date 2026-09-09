#include "reach/core/theme.h"
#include "reach/features/stage.h"

#include <math.h>
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
    ops->tick(stage, 1.0, &tick);
    const reach_stage_state *state = reach_stage_state_ptr(stage);
    expect_true(state->open && state->close_phase == REACH_STAGE_CLOSE_ALIGNED,
                "a completed movement retains its final thumbnail for presentation");
    ops->tick(stage, 1.0, &tick);
    expect_true(state->close_phase == REACH_STAGE_CLOSE_ALIGNED,
                "elapsed time cannot bypass the first completion gate");
    ops->presentation_committed(stage, REACH_OK, &tick);
    ops->tick(stage, 1.0, &tick);
    expect_true(state->open && state->close_phase == REACH_STAGE_CLOSE_TRANSPARENT,
                "the transparent backdrop must be presented before disposal");
    ops->presentation_committed(stage, REACH_OK, &tick);
    expect_true(ops->needs_frame(stage), "disposal still schedules the final cleanup frame");
    ops->tick(stage, 0.016, &tick);
    expect_true(!reach_stage_is_open(stage), "acknowledged reveal can release Stage");
    reach_stage_destroy(stage);
}

static void test_selected_close_starts_moving_immediately(void)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_stage_open_window window = make_window(1, make_rect(100, 120, 640, 480));
    reach_stage_open(stage, make_rect(0, 0, 1920, 1080), 1, &window, 1);
    advance_stage(stage, 30, 0.016);
    reach_stage_state *state = const_cast<reach_stage_state *>(reach_stage_state_ptr(stage));
    state->has_selection = 1;
    state->selected_index = 0;
    reach_rect_f32 settled = state->tiles[0].current_rect;
    reach_stage_begin_close(stage);
    const reach_feature_capsule_ops *ops = reach_stage_capsule_ops();
    reach_feature_tick_result tick = {};
    ops->tick(stage, 0.016, &tick);
    expect_true(state->close_phase == REACH_STAGE_CLOSE_MOVING,
                "a selected close starts in the moving phase");
    expect_true(!reach_rect_equal(state->tiles[0].current_rect, settled),
                "the selected thumbnail moves on the first animation tick");
    expect_true(ops->needs_frame(stage), "the selected close keeps requesting animation frames");
    reach_stage_destroy(stage);
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
    expect_true((result.action.flags & REACH_FEATURE_ACTION_FLAG_CLOSE_SELF_FIRST) != 0,
                "an app tile starts Stage close before activation");

    reach_stage_force_close(stage);
    app.minimized = 1;
    reach_stage_open(stage, make_rect(0, 0, 1920, 1080), 1, &app, 1);
    advance_stage(stage, 30, 0.016);
    result = click_tile(stage, 0);
    expect_true(result.action.kind == REACH_FEATURE_ACTION_ACTIVATE_WINDOW,
                "a minimized app tile publishes ordinary activation");
    expect_true((result.action.flags & REACH_FEATURE_ACTION_FLAG_CLOSE_SELF_FIRST) == 0,
                "a minimized app queues restore before Stage begins moving");

    reach_stage_force_close(stage);
    reach_stage_open_window desktop = make_desktop(100, make_rect(0, 0, 1920, 1080));
    reach_stage_open(stage, make_rect(0, 0, 1920, 1080), 1, &desktop, 1);
    advance_stage(stage, 30, 0.016);
    result = click_tile(stage, 0);
    expect_true(result.action.kind == REACH_FEATURE_ACTION_MINIMIZE_ALL_WINDOWS,
                "the Desktop tile publishes minimize all");
    expect_true((result.action.flags & REACH_FEATURE_ACTION_FLAG_CLOSE_SELF_FIRST) != 0,
                "the Desktop tile starts Stage close before minimizing apps");
    reach_stage_destroy(stage);
}

static void test_minimized_selection_accepts_restore_during_close(void)
{
    reach_stage *stage = nullptr;
    reach_stage_create(&stage);
    reach_rect_f32 bounds = make_rect(0, 0, 1920, 1080);
    reach_stage_open_window app = make_window(1, make_rect(100, 120, 1200, 675));
    app.minimized = 1;
    reach_stage_open(stage, bounds, 1, &app, 1);
    advance_stage(stage, 30, 0.016);
    reach_stage_state *state = const_cast<reach_stage_state *>(reach_stage_state_ptr(stage));
    state->has_selection = 1;
    state->selected_index = 0;
    reach_stage_begin_close(stage);
    reach_feature_tick_result tick = {};
    reach_stage_capsule_ops()->tick(stage, 0.016, &tick);

    app.minimized = 0;
    expect_true(reach_stage_update_windows(stage, &app, 1),
                "a selected minimized app can publish restoration during close");
    reach_stage_thumbnail_placement placement = {};
    reach_stage_thumbnail_at(stage, 0, &placement);
    expect_true(placement.visible, "the restored live thumbnail joins the running animation");
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

int main(void)
{
    test_close_requires_completion_before_teardown();
    test_selected_close_starts_moving_immediately();
    test_desktop_uses_a_shorter_relative_animation();
    test_desktop_only_close_has_no_hidden_app_wait();
    test_tile_clicks_publish_immediate_window_actions();
    test_minimized_selection_accepts_restore_during_close();
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
