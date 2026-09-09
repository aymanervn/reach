#include "reach/features/common/bar_visibility.h"
#include "reach/features/top_bar.h"

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

static reach_bar_visibility_request base_request(void)
{
    reach_bar_visibility_request request = {};
    request.edge = REACH_BAR_EDGE_TOP;
    request.shown_bounds = {100.0f, 8.0f, 800.0f, 40.0f};
    request.monitor_bounds = {0.0f, 0.0f, 1000.0f, 800.0f};
    request.pointer = {500, 20};
    request.pointer_valid = 1;
    request.reveal_seconds = 0.25f;
    return request;
}

static void test_protected_bands_are_symmetric(void)
{
    reach_rect_f32 monitor = {0.0f, 0.0f, 1000.0f, 800.0f};
    reach_rect_f32 top = {100.0f, 8.0f, 800.0f, 40.0f};
    reach_rect_f32 bottom = {100.0f, 752.0f, 800.0f, 40.0f};
    reach_rect_f32 top_band = reach_bar_protected_band(REACH_BAR_EDGE_TOP, top, monitor, 7.5f);
    reach_rect_f32 bottom_band =
        reach_bar_protected_band(REACH_BAR_EDGE_BOTTOM, bottom, monitor, 7.5f);

    expect_true(top_band.y == 0.0f && top_band.height == 55.5f,
                "top protected band includes the rendered shadow clearance");
    expect_true(bottom_band.y == 744.5f && bottom_band.height == 55.5f,
                "bottom protected band mirrors the shadow-aware top band");

    reach_rect_f32 scaled_monitor = {0.0f, 0.0f, 2000.0f, 1600.0f};
    reach_rect_f32 scaled_top = {200.0f, 16.0f, 1600.0f, 80.0f};
    reach_rect_f32 scaled_band =
        reach_bar_protected_band(REACH_BAR_EDGE_TOP, scaled_top, scaled_monitor, 15.0f);
    expect_true(scaled_band.height == top_band.height * 2.0f,
                "protected-band clearance follows DPI-scaled geometry");
    expect_true(reach_bar_reserved_edge(REACH_BAR_EDGE_TOP, top, 8.0f) == 56.0f,
                "top reservation uses the explicit far-edge clearance");
    expect_true(reach_bar_reserved_edge(REACH_BAR_EDGE_BOTTOM, bottom, 8.0f) == 744.0f,
                "bottom reservation uses the explicit far-edge clearance");
}

static void test_top_bar_styles_publish_their_clearance(void)
{
    reach_top_bar *top_bar = nullptr;
    expect_true(reach_top_bar_create(&top_bar) == REACH_OK, "top bar is created");
    if (top_bar == nullptr)
    {
        return;
    }

    reach_top_bar_build_context context = {};
    context.theme = reach_theme_default();
    context.monitor_bounds = {100.0f, 50.0f, 1200.0f, 800.0f};
    context.dpi_scale = 1.0f;

    (void)reach_top_bar_apply_config(top_bar, REACH_CONFIG_TOP_BAR_STYLE_SPLIT,
                                     REACH_CONFIG_TOP_BAR_MODE_STATIC);
    reach_top_bar_build_layout(top_bar, &context);
    const reach_top_bar_state *split = reach_top_bar_state_ptr(top_bar);
    reach_feature_surface_geometry split_geometry = {};
    reach_top_bar_capsule_ops()->surface_geometry(top_bar, &split_geometry);
    expect_true(split->layout.bounds.y == 56.0f && split->layout.app_clearance == 6.0f,
                "split top bar keeps symmetric visual clearance");
    expect_true(split_geometry.reserve_monitor_work_area &&
                    split_geometry.work_area_clearance == 6.0f,
                "split top bar publishes its static work-area clearance");
    expect_true(split_geometry.yield_topmost_to_foreground_fullscreen,
                "static top bar yields its topmost layer to fullscreen apps");

    (void)reach_top_bar_apply_config(top_bar, REACH_CONFIG_TOP_BAR_STYLE_SIMPLE,
                                     REACH_CONFIG_TOP_BAR_MODE_STATIC);
    reach_top_bar_build_layout(top_bar, &context);
    const reach_top_bar_state *simple = reach_top_bar_state_ptr(top_bar);
    reach_feature_surface_geometry simple_geometry = {};
    reach_top_bar_capsule_ops()->surface_geometry(top_bar, &simple_geometry);
    reach_rect_f32 background = reach_top_bar_background_bounds(top_bar);
    expect_true(simple->layout.bounds.y == 50.0f && simple->layout.app_clearance == 0.0f,
                "simple top bar reaches the screen edge without app clearance");
    expect_true(background.x == 0.0f && background.width == context.monitor_bounds.width,
                "simple top bar background spans the monitor width");
    expect_true(simple_geometry.reserve_monitor_work_area &&
                    simple_geometry.work_area_clearance == 0.0f,
                "simple top bar publishes zero extra work-area clearance");

    (void)reach_top_bar_apply_config(top_bar, REACH_CONFIG_TOP_BAR_STYLE_SIMPLE,
                                     REACH_CONFIG_TOP_BAR_MODE_DYNAMIC);
    reach_feature_surface_geometry dynamic_geometry = {};
    reach_top_bar_capsule_ops()->surface_geometry(top_bar, &dynamic_geometry);
    expect_true(!dynamic_geometry.yield_topmost_to_foreground_fullscreen,
                "dynamic top bar keeps its independent reveal layer policy");

    reach_top_bar_destroy(top_bar);
}

static void test_forced_hide_animates_and_suppresses_reveal(void)
{
    reach_animation_track track = {};
    reach_animation_manager manager = {};
    reach_animation_manager_init(&manager, &track, 1);
    reach_bar_visibility_state state = {};
    reach_bar_visibility_request request = base_request();

    (void)reach_bar_update_visibility(&state, &manager, 0, &request);
    reach_bar_begin_reveal_session(&state);
    request.force_hidden = 1;
    request.hold_open = 1;
    reach_bar_visibility_result hidden = reach_bar_update_visibility(&state, &manager, 0, &request);

    expect_true(state.target_hidden, "manipulation forces a bar hidden even without trespass");
    expect_true(hidden.reveal_transition_active,
                "forced hiding begins the normal position animation");
    expect_true(hidden.animated_bounds.y == request.shown_bounds.y,
                "forced hiding does not cut directly to the hidden position");
    expect_true(!hidden.reveal_edge_shown && !hidden.pointer_observation_active,
                "forced hiding suppresses both reveal inputs");
    expect_true(!state.reveal_session_active, "forced hiding clears an active edge-reveal session");
}

static void test_stage_force_show_precedes_manipulation(void)
{
    reach_animation_track track = {};
    reach_animation_manager manager = {};
    reach_animation_manager_init(&manager, &track, 1);
    reach_bar_visibility_state state = {};
    reach_bar_visibility_request request = base_request();
    request.can_hide = 1;
    request.force_shown = 1;
    request.force_hidden = 1;

    reach_bar_visibility_result result = reach_bar_update_visibility(&state, &manager, 0, &request);
    expect_true(!state.target_hidden && result.visible,
                "stage force-show takes precedence over manipulation suppression");
}

static void test_pointer_observation_wakes_hover_exit(void)
{
    reach_animation_track track = {};
    reach_animation_manager manager = {};
    reach_animation_manager_init(&manager, &track, 1);
    reach_bar_visibility_state state = {};
    reach_bar_visibility_request request = base_request();
    request.can_hide = 1;

    reach_bar_visibility_result hovered =
        reach_bar_update_visibility(&state, &manager, 0, &request);
    expect_true(hovered.pointer_observation_active,
                "a hideable shown bar observes its logical hover region");
    expect_true(hovered.pointer_observation_bounds.height >= request.shown_bounds.height,
                "pointer observation includes the bar and edge bridge");

    request.pointer = {500, 200};
    reach_bar_visibility_result left = reach_bar_update_visibility(&state, &manager, 0, &request);
    expect_true(state.target_hidden && left.reveal_transition_active,
                "leaving the observed region starts the hide animation");
}

static void test_settled_bar_tracks_resized_shown_bounds(void)
{
    reach_animation_track track = {};
    reach_animation_manager manager = {};
    reach_animation_manager_init(&manager, &track, 1);
    reach_bar_visibility_state state = {};
    reach_bar_visibility_request request = base_request();
    request.edge = REACH_BAR_EDGE_BOTTOM;
    request.shown_bounds = {100.0f, 718.0f, 800.0f, 64.0f};

    (void)reach_bar_update_visibility(&state, &manager, 0, &request);
    request.shown_bounds = {100.0f, 750.0f, 800.0f, 32.0f};
    reach_bar_visibility_result resized =
        reach_bar_update_visibility(&state, &manager, 0, &request);

    expect_true(resized.animated_bounds.y == 750.0f,
                "a settled bar follows its resized shown position");
    expect_true(reach_animation_manager_target(&manager, 0) == 750.0f,
                "the settled position track adopts the resized shown target");
}

static void test_resized_shown_bounds_override_an_old_position_animation(void)
{
    reach_animation_track track = {};
    reach_animation_manager manager = {};
    reach_animation_manager_init(&manager, &track, 1);
    reach_bar_visibility_state state = {};
    reach_bar_visibility_request request = base_request();
    request.edge = REACH_BAR_EDGE_BOTTOM;
    request.shown_bounds = {100.0f, 718.0f, 800.0f, 64.0f};

    (void)reach_bar_update_visibility(&state, &manager, 0, &request);
    reach_animation_manager_start(&manager, 0, 700.0f, 718.0f, 0.25, REACH_EASING_EASE_IN_OUT);
    request.shown_bounds = {100.0f, 750.0f, 800.0f, 32.0f};
    reach_bar_visibility_result resized =
        reach_bar_update_visibility(&state, &manager, 0, &request);

    expect_true(resized.animated_bounds.y == 750.0f,
                "resized shown geometry overrides an obsolete position animation");
    expect_true(!reach_animation_manager_active(&manager, 0),
                "adopting resized shown geometry retires the obsolete animation");
}

int main(void)
{
    test_protected_bands_are_symmetric();
    test_top_bar_styles_publish_their_clearance();
    test_forced_hide_animates_and_suppresses_reveal();
    test_stage_force_show_precedes_manipulation();
    test_pointer_observation_wakes_hover_exit();
    test_settled_bar_tracks_resized_shown_bounds();
    test_resized_shown_bounds_override_an_old_position_animation();
    return failures == 0 ? 0 : 1;
}
