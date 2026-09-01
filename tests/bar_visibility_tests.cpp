#include "reach/features/common/bar_visibility.h"

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
    test_forced_hide_animates_and_suppresses_reveal();
    test_stage_force_show_precedes_manipulation();
    test_pointer_observation_wakes_hover_exit();
    test_settled_bar_tracks_resized_shown_bounds();
    test_resized_shown_bounds_override_an_old_position_animation();
    return failures == 0 ? 0 : 1;
}
