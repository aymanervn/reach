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
    request.reveal = reach_motion_spec_curve(0.25, REACH_EASING_EASE_IN_OUT);
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
    expect_true(split_geometry.bar_reveal_policy == REACH_FEATURE_BAR_REVEAL_FULLSCREEN,
                "static top bar enables auto hide only for fullscreen apps");

    (void)reach_top_bar_apply_config(top_bar, REACH_CONFIG_TOP_BAR_STYLE_TRANSPARENT,
                                     REACH_CONFIG_TOP_BAR_MODE_STATIC);
    reach_top_bar_build_layout(top_bar, &context);
    const reach_top_bar_state *transparent = reach_top_bar_state_ptr(top_bar);
    reach_feature_surface_geometry transparent_geometry = {};
    reach_top_bar_capsule_ops()->surface_geometry(top_bar, &transparent_geometry);
    reach_rect_f32 background = reach_top_bar_background_bounds(top_bar);
    expect_true(transparent->layout.bounds.y == 50.0f && transparent->layout.app_clearance == 0.0f,
                "transparent top bar reaches the screen edge without app clearance");
    expect_true(background.x == 0.0f && background.width == context.monitor_bounds.width,
                "transparent top bar background spans the monitor width");
    expect_true(transparent_geometry.reserve_monitor_work_area &&
                    transparent_geometry.work_area_clearance == 0.0f,
                "transparent top bar publishes zero extra work-area clearance");

    (void)reach_top_bar_apply_config(top_bar, REACH_CONFIG_TOP_BAR_STYLE_TRANSPARENT,
                                     REACH_CONFIG_TOP_BAR_MODE_DYNAMIC);
    reach_feature_surface_geometry dynamic_geometry = {};
    reach_top_bar_capsule_ops()->surface_geometry(top_bar, &dynamic_geometry);
    expect_true(dynamic_geometry.bar_reveal_policy == REACH_FEATURE_BAR_REVEAL_ALWAYS,
                "dynamic top bar keeps auto hide active for ordinary app overlap");

    reach_top_bar_destroy(top_bar);
}

static void test_transparent_top_bar_scrim_renders_within_budget(void)
{
    reach_top_bar *top_bar = nullptr;
    expect_true(reach_top_bar_create(&top_bar) == REACH_OK, "top bar is created");
    if (top_bar == nullptr)
    {
        return;
    }

    static reach_render_command_buffer buffer;
    const reach_theme_mode modes[] = {REACH_THEME_MODE_DARK, REACH_THEME_MODE_LIGHT};
    const float dpis[] = {1.0f, 1.25f, 1.5f, 2.0f, 3.0f};
    const reach_rect_f32 monitors[] = {{0.0f, 0.0f, 7680.0f, 4320.0f},
                                       {0.0f, 0.0f, 360.0f, 280.0f}};
    const reach_config_top_bar_mode bar_modes[] = {REACH_CONFIG_TOP_BAR_MODE_STATIC,
                                                   REACH_CONFIG_TOP_BAR_MODE_DYNAMIC};
    int32_t all_ok = 1;
    for (reach_theme_mode mode : modes)
    {
        for (reach_config_top_bar_mode bar_mode : bar_modes)
        {
            (void)reach_top_bar_apply_config(top_bar, REACH_CONFIG_TOP_BAR_STYLE_TRANSPARENT,
                                             bar_mode);
            for (float dpi : dpis)
            {
                for (reach_rect_f32 monitor : monitors)
                {
                    for (int32_t retained = 0; retained <= 1; ++retained)
                    {
                        reach_top_bar_build_context build = {};
                        build.theme = reach_theme_for_mode(mode);
                        build.monitor_bounds = monitor;
                        build.dpi_scale = dpi;
                        reach_top_bar_build_layout(top_bar, &build);
                        reach_top_bar_render_context render = {};
                        render.theme = build.theme;
                        render.dpi_scale = dpi;
                        render.icon_size_px = (int32_t)(16.0f * dpi);
                        render.retained_supported = retained;
                        if (reach_top_bar_append_render_commands(top_bar, &render, &buffer) !=
                                REACH_OK ||
                            buffer.group_count > REACH_MAX_RENDER_GROUPS ||
                            buffer.count > REACH_MAX_RENDER_COMMANDS ||
                            reach_render_command_buffer_validate_groups(&buffer) != REACH_OK)
                        {
                            all_ok = 0;
                        }
                        int32_t scrim_found = 0;
                        for (size_t index = 0; index < buffer.count; ++index)
                        {
                            all_ok &= buffer.commands[index].glass_intensity == 0;
                            scrim_found |= buffer.commands[index].fill_gradient ==
                                           REACH_RENDER_FILL_EASED_FADE_DOWN;
                        }
                        all_ok &= scrim_found;
                    }
                }
            }
        }
    }
    expect_true(all_ok, "transparent top bar with its scrim renders within the render budget");

    reach_top_bar_destroy(top_bar);
}

static void test_forced_hide_animates_and_suppresses_reveal(void)
{
    reach_motion y = {};
    reach_bar_visibility_state state = {};
    reach_bar_visibility_request request = base_request();

    (void)reach_bar_update_visibility(&state, &y, &request);
    reach_bar_begin_reveal_session(&state);
    request.force_hidden = 1;
    request.hold_open = 1;
    reach_bar_visibility_result hidden = reach_bar_update_visibility(&state, &y, &request);

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
    reach_motion y = {};
    reach_bar_visibility_state state = {};
    reach_bar_visibility_request request = base_request();
    request.can_hide = 1;
    request.force_shown = 1;
    request.force_hidden = 1;

    reach_bar_visibility_result result = reach_bar_update_visibility(&state, &y, &request);
    expect_true(!state.target_hidden && result.visible,
                "stage force-show takes precedence over manipulation suppression");
}

static void test_pointer_observation_wakes_hover_exit(void)
{
    reach_motion y = {};
    reach_bar_visibility_state state = {};
    reach_bar_visibility_request request = base_request();
    request.can_hide = 1;

    reach_bar_visibility_result hovered = reach_bar_update_visibility(&state, &y, &request);
    expect_true(hovered.pointer_observation_active,
                "a hideable shown bar observes its logical hover region");
    expect_true(hovered.pointer_observation_bounds.height >= request.shown_bounds.height,
                "pointer observation includes the bar and edge bridge");

    request.pointer = {500, 200};
    reach_bar_visibility_result left = reach_bar_update_visibility(&state, &y, &request);
    expect_true(state.target_hidden && left.reveal_transition_active,
                "leaving the observed region starts the hide animation");
}

static void test_settled_bar_tracks_resized_shown_bounds(void)
{
    reach_motion y = {};
    reach_bar_visibility_state state = {};
    reach_bar_visibility_request request = base_request();
    request.edge = REACH_BAR_EDGE_BOTTOM;
    request.shown_bounds = {100.0f, 718.0f, 800.0f, 64.0f};

    (void)reach_bar_update_visibility(&state, &y, &request);
    request.shown_bounds = {100.0f, 750.0f, 800.0f, 32.0f};
    reach_bar_visibility_result resized = reach_bar_update_visibility(&state, &y, &request);

    expect_true(resized.animated_bounds.y == 750.0f,
                "a settled bar follows its resized shown position");
    expect_true(y.to == 750.0f, "the settled position track adopts the resized shown target");
}

static void test_resized_shown_bounds_override_an_old_position_animation(void)
{
    reach_motion y = {};
    reach_bar_visibility_state state = {};
    reach_bar_visibility_request request = base_request();
    request.edge = REACH_BAR_EDGE_BOTTOM;
    request.shown_bounds = {100.0f, 718.0f, 800.0f, 64.0f};

    (void)reach_bar_update_visibility(&state, &y, &request);
    reach_motion_start_at(&y, 700.0f, 718.0f, 0.25, REACH_EASING_EASE_IN_OUT,
                          reach_motion_now_seconds());
    request.shown_bounds = {100.0f, 750.0f, 800.0f, 32.0f};
    reach_bar_visibility_result resized = reach_bar_update_visibility(&state, &y, &request);

    expect_true(resized.animated_bounds.y == 750.0f,
                "resized shown geometry overrides an obsolete position animation");
    expect_true(!reach_motion_active_at(&y, reach_motion_now_seconds()),
                "adopting resized shown geometry retires the obsolete animation");
}

static void test_keep_above_windows_only_for_revealed_overlay(void)
{
    expect_true(reach_bar_keep_above_windows(1, 1, 0.5f) == 1,
                "an overlay bar part way revealed stays above windows");
    expect_true(reach_bar_keep_above_windows(1, 1, 1.0f) == 1,
                "a fully revealed overlay bar stays above windows");
    expect_true(reach_bar_keep_above_windows(1, 1, 0.0f) == 0,
                "a hidden overlay bar does not ask to stay above windows");
    expect_true(reach_bar_keep_above_windows(0, 1, 1.0f) == 0,
                "a pushing bar does not ask to stay above windows");
    expect_true(reach_bar_keep_above_windows(1, 0, 1.0f) == 0,
                "an overlay bar without auto hide does not ask to stay above windows");
}

static void test_top_bar_overlay_setting(void)
{
    reach_top_bar *top_bar = nullptr;
    expect_true(reach_top_bar_create(&top_bar) == REACH_OK, "top bar is created for overlay");
    if (top_bar == nullptr)
    {
        return;
    }

    expect_true(reach_top_bar_state_ptr(top_bar)->overlay == 0, "overlay starts off");
    expect_true(reach_top_bar_set_overlay(top_bar, 1) == 1, "turning overlay on reports a change");
    expect_true(reach_top_bar_state_ptr(top_bar)->overlay == 1, "overlay is stored on");
    expect_true(reach_top_bar_set_overlay(top_bar, 1) == 0,
                "setting overlay on again is no change");
    expect_true(reach_top_bar_set_overlay(top_bar, 5) == 0, "a non-zero overlay is still on");
    expect_true(reach_top_bar_state_ptr(top_bar)->overlay == 1,
                "a non-zero overlay is stored as 1");
    expect_true(reach_top_bar_set_overlay(top_bar, 0) == 1, "turning overlay off reports a change");
    expect_true(reach_top_bar_state_ptr(top_bar)->overlay == 0, "overlay is stored off");

    reach_top_bar_destroy(top_bar);
}

int main(void)
{
    test_protected_bands_are_symmetric();
    test_top_bar_styles_publish_their_clearance();
    test_transparent_top_bar_scrim_renders_within_budget();
    test_forced_hide_animates_and_suppresses_reveal();
    test_stage_force_show_precedes_manipulation();
    test_pointer_observation_wakes_hover_exit();
    test_settled_bar_tracks_resized_shown_bounds();
    test_resized_shown_bounds_override_an_old_position_animation();
    test_keep_above_windows_only_for_revealed_overlay();
    test_top_bar_overlay_setting();
    return failures == 0 ? 0 : 1;
}
