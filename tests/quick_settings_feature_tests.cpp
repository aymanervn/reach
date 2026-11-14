#include "reach/support/util.h"
#include "reach/features/quick_settings.h"
#include "reach/features/common/popup.h"

#include <math.h>
#include <stdio.h>

static int g_failures = 0;

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        ++g_failures;
        printf("FAIL: %s\n", message);
    }
}

static void expect_near(float actual, float expected, float epsilon, const char *message)
{
    if (fabsf(actual - expected) > epsilon)
    {
        ++g_failures;
        printf("FAIL: %s: expected %.3f, got %.3f\n", message, expected, actual);
    }
}

static reach_quick_settings_model test_model_with_sessions(size_t count)
{
    reach_quick_settings_model model = {};
    reach_quick_settings_model_init(&model);
    reach_quick_settings_model_set_main_volume(&model, 0.5f, 0);

    reach_audio_volume_session_list sessions = {};
    sessions.count = count;
    for (size_t index = 0; index < count && index < REACH_AUDIO_VOLUME_MAX_SESSIONS; ++index)
    {
        sessions.sessions[index].level = 0.25f + (float)index * 0.05f;
        sessions.sessions[index].muted = 0;
        sessions.sessions[index].process_id = (uint32_t)(1000 + index);
        sessions.sessions[index].icon_id = (uint64_t)(9000 + index);
        reach_copy_ascii_to_utf16(sessions.sessions[index].session_instance_id,
                                  REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY,
                                  index == 0 ? "session-a" : "session-b");
        reach_copy_ascii_to_utf16(sessions.sessions[index].label,
                                  REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY,
                                  index == 0 ? "App A.exe" : "App B.exe");
    }

    reach_quick_settings_model_set_sessions(&model, &sessions);
    return model;
}

static void test_model_clamps_volume(void)
{
    reach_quick_settings_model model = {};
    reach_quick_settings_model_init(&model);

    reach_quick_settings_model_set_main_volume(&model, -1.0f, 0);
    expect_near(model.main_volume_level, 0.0f, 0.001f, "volume clamps below zero");

    reach_quick_settings_model_set_main_volume(&model, 2.0f, 1);
    expect_near(model.main_volume_level, 1.0f, 0.001f, "volume clamps above one");
    expect_true(model.main_muted == 1, "muted flag normalizes to one");
}

static void test_session_list_cap_is_respected(void)
{
    reach_quick_settings_model model =
        test_model_with_sessions(REACH_AUDIO_VOLUME_MAX_SESSIONS + 4);

    expect_true(model.sessions.count == REACH_AUDIO_VOLUME_MAX_SESSIONS, "session model caps list");
}

static void test_volume_icon_selection(void)
{
    expect_true(reach_quick_settings_volume_icon_id(0.8f, 1) == REACH_VECTOR_ICON_VOLUME_ZERO,
                "muted volume uses zero icon");
    expect_true(reach_quick_settings_volume_icon_id(0.0f, 0) == REACH_VECTOR_ICON_VOLUME_ZERO,
                "zero volume uses zero icon");
    expect_true(reach_quick_settings_volume_icon_id(0.25f, 0) == REACH_VECTOR_ICON_VOLUME_LOW,
                "low volume uses low icon");
    expect_true(reach_quick_settings_volume_icon_id(0.75f, 0) == REACH_VECTOR_ICON_VOLUME_HIGH,
                "high volume uses high icon");
}

static void age_transition(reach_feature_transition *transition, double seconds)
{
    transition->opacity.started_seconds -= seconds;
    transition->completion_seconds -= seconds;
}

static void test_feature_transition_owns_presentation(void)
{
    const reach_theme *theme = reach_theme_default();
    reach_feature_transition transition = {};
    reach_feature_transition_init(&transition, REACH_FEATURE_TRANSITION_FROM_ABOVE);
    reach_feature_transition_configure(&transition, theme, 1.5f,
                                       REACH_FEATURE_TRANSITION_FROM_ABOVE);

    expect_true(reach_feature_transition_set_open(&transition, 1),
                "opening starts the shared card transition");
    reach_feature_surface_geometry geometry = {};
    geometry.visible_bounds = {100.0f, 40.0f, 320.0f, 200.0f};
    reach_feature_transition_presentation(&transition, &geometry);
    expect_true(geometry.presentation.retained, "the card reports managed presentation");
    expect_near(geometry.presentation.group.opacity.from, 0.0f, 0.001f,
                "the card begins transparent");
    expect_true(reach_motion_value(&geometry.presentation.group.scale) == 1.0f &&
                    reach_motion_value(&geometry.presentation.group.y) == 0.0f,
                "the card opens at full size in place");
    expect_near(
        (float)geometry.presentation.group.opacity.duration_seconds,
        (float)reach_theme_motion_preset(theme, REACH_MOTION_PRESET_CARD_FADE_IN).duration_seconds,
        1e-6f, "opacity uses the card fade-in preset");

    age_transition(&transition,
                   reach_motion_deadline(&transition.opacity) - transition.opacity.started_seconds);
    expect_true(reach_feature_transition_tick(&transition, 0.0),
                "the open completes at its fade deadline");
    expect_true(reach_feature_transition_visible(&transition), "the opened card stays visible");

    age_transition(&transition, 1.0);
    expect_true(reach_feature_transition_set_open(&transition, 0), "closing starts the card fade");
    reach_feature_transition_presentation(&transition, &geometry);
    expect_true(geometry.wait_for_presentation, "the close waits for presentation");
    expect_near(
        (float)reach_feature_transition_exit_seconds(&transition),
        (float)reach_theme_motion_preset(theme, REACH_MOTION_PRESET_CARD_FADE_OUT).duration_seconds,
        1e-6f, "the close fade uses the card fade-out preset");
    expect_near((float)transition.opacity.duration_seconds,
                (float)reach_feature_transition_exit_seconds(&transition), 1e-6f,
                "the close fade lasts the exit time");
    expect_true(transition.completion_seconds == reach_motion_deadline(&transition.opacity),
                "close completion is the fade deadline");
    age_transition(&transition, reach_feature_transition_exit_seconds(&transition) - 0.01);
    expect_true(!reach_feature_transition_tick(&transition, 0.0) &&
                    reach_feature_transition_visible(&transition),
                "the card stays visible until its fade deadline");
    age_transition(&transition, 0.02);
    expect_true(reach_feature_transition_tick(&transition, 0.0) &&
                    !reach_feature_transition_visible(&transition),
                "the card hides at its fade deadline");
}

static void test_feature_transition_reopen_is_continuous(void)
{
    reach_feature_transition transition = {};
    reach_feature_transition_init(&transition, REACH_FEATURE_TRANSITION_FROM_BELOW);
    reach_feature_transition_configure(&transition, reach_theme_default(), 1.25f,
                                       REACH_FEATURE_TRANSITION_FROM_BELOW);
    (void)reach_feature_transition_set_open(&transition, 1);
    age_transition(&transition, 0.02);
    reach_feature_transition before = transition;
    expect_true(reach_feature_transition_set_open(&transition, 0), "closing mid-open retargets");
    double at = transition.opacity.started_seconds;
    expect_near(transition.opacity.from, reach_motion_value_at(&before.opacity, at), 1e-4f,
                "closing mid-open keeps opacity");

    age_transition(&transition, 0.2);
    before = transition;
    expect_true(reach_feature_transition_set_open(&transition, 1), "reopening mid-close retargets");
    at = transition.opacity.started_seconds;
    expect_near(transition.opacity.from, reach_motion_value_at(&before.opacity, at), 1e-4f,
                "reopening mid-close keeps opacity");
    expect_true(transition.visible && transition.opacity.to == 1.0f,
                "reopening never restarts from the closed origin");
}

static void test_feature_transition_scales_about_its_anchor(void)
{
    reach_feature_transition transition = {};
    reach_feature_transition_init(&transition, REACH_FEATURE_TRANSITION_FROM_ABOVE);
    reach_feature_surface_geometry geometry = {};
    geometry.visible_bounds = {100.0f, 50.0f, 300.0f, 200.0f};
    reach_feature_transition_presentation(&transition, &geometry);
    expect_near(reach_motion_value(&geometry.presentation.center_x), 150.0f, 0.001f,
                "an unanchored dropdown scales from its top-center");
    expect_near(reach_motion_value(&geometry.presentation.center_y), 0.0f, 0.001f,
                "an unanchored dropdown scales from its top edge");

    reach_feature_transition_set_trigger(&transition, geometry.visible_bounds, 160.0f);
    geometry = {};
    geometry.visible_bounds = {100.0f, 50.0f, 300.0f, 200.0f};
    reach_feature_transition_presentation(&transition, &geometry);
    expect_near(reach_motion_value(&geometry.presentation.center_x), 60.0f, 0.001f,
                "a dropdown scales from the trigger x");
    expect_near(reach_motion_value(&geometry.presentation.center_y), 0.0f, 0.001f,
                "a dropdown scales from the edge facing its trigger");

    reach_feature_transition_configure(&transition, reach_theme_default(), 1.0f,
                                       REACH_FEATURE_TRANSITION_FROM_BELOW);
    reach_feature_transition_set_trigger(&transition, geometry.visible_bounds, 900.0f);
    geometry = {};
    geometry.visible_bounds = {100.0f, 50.0f, 300.0f, 200.0f};
    reach_feature_transition_presentation(&transition, &geometry);
    expect_near(reach_motion_value(&geometry.presentation.center_x), 300.0f, 0.001f,
                "a trigger beyond the popup clamps to its edge");
    expect_near(reach_motion_value(&geometry.presentation.center_y), 200.0f, 0.001f,
                "a Dock popup scales from its bottom edge");

    reach_feature_transition_set_anchor(&transition, 12.0f, 34.0f);
    (void)reach_feature_transition_set_open(&transition, 1);
    geometry = {};
    reach_feature_transition_presentation(&transition, &geometry);
    expect_true(reach_motion_value(&geometry.presentation.center_x) == 12.0f &&
                    reach_motion_value(&geometry.presentation.center_y) == 34.0f,
                "an explicit anchor becomes the presentation center");
}

static void test_expansion_keeps_popup_anchor_position(void)
{
    reach_quick_settings *quick_settings = nullptr;
    expect_true(reach_quick_settings_create(&quick_settings) == REACH_OK,
                "quick settings capsule is created");
    if (quick_settings == nullptr)
    {
        return;
    }

    reach_quick_settings_layout_context ctx = {};
    ctx.theme = reach_theme_default();
    ctx.dpi_scale = 1.25f;
    ctx.anchor_button = {120.25f, 8.25f, 32.0f, 24.0f};
    ctx.monitor = {0.0f, 0.0f, 1920.0f, 1080.0f};
    ctx.bar_edge_y = 40.25f;
    ctx.drop_direction = REACH_POPUP_DROP_DOWN;

    (void)reach_quick_settings_set_open(quick_settings, 1);
    reach_quick_settings_model model = test_model_with_sessions(3);
    reach_quick_settings_apply_sessions(quick_settings, &model.sessions);
    reach_quick_settings_refresh_layout(quick_settings, &ctx);
    reach_feature_surface_geometry opening_geometry = {};
    reach_quick_settings_capsule_ops()->surface_geometry(quick_settings, &opening_geometry);
    expect_true(opening_geometry.presentation.group.opacity.from == 0.0f &&
                    opening_geometry.presentation.group.opacity.to == 1.0f,
                "quick settings opens with the shared card fade");
    const reach_quick_settings_state *opening = reach_quick_settings_state_ptr(quick_settings);
    expect_near(reach_motion_value(&opening_geometry.presentation.center_x),
                opening->notch_anchor_x - opening->render_bounds.x, 0.001f,
                "quick settings grows from its trigger x");
    expect_near(reach_motion_value(&opening_geometry.presentation.center_y), 0.0f, 0.001f,
                "quick settings grows from the edge facing the top bar");
    reach_feature_tick_result tick = {};
    reach_quick_settings_capsule_ops()->tick(quick_settings, 1.0, &tick);
    reach_quick_settings_capsule_ops()->surface_geometry(quick_settings, &opening_geometry);
    expect_near(opening_geometry.presentation.group.y.to, 0.0f, 0.001f,
                "quick settings opening motion settles on its anchor");
    const reach_quick_settings_state *state = reach_quick_settings_state_ptr(quick_settings);
    const float narrow_width = state->bounds.width;
    const float content_width = state->content_bounds.width;

    reach_theme wide_border_theme = *reach_theme_default();
    wide_border_theme.border_thickness = 3.0f;
    ctx.theme = &wide_border_theme;
    reach_quick_settings_refresh_layout(quick_settings, &ctx);
    state = reach_quick_settings_state_ptr(quick_settings);
    expect_near(state->bounds.width - narrow_width, 5.0f, 0.001f,
                "quick settings outer width derives both DPI-scaled border sides");
    expect_near(state->content_bounds.width, content_width, 0.001f,
                "quick settings content width stays stable across border widths");
    const float initial_y = state->bounds.y;

    (void)reach_quick_settings_toggle_expanded(quick_settings);
    reach_quick_settings_relayout(quick_settings, &ctx, 1);
    expect_true(reach_quick_settings_state_ptr(quick_settings)->target_bounds.height >
                    reach_quick_settings_state_ptr(quick_settings)->bounds.height,
                "app rows increase the quick settings target height");
    for (size_t frame = 0; frame < 12; ++frame)
    {
        reach_quick_settings_capsule_ops()->tick(quick_settings, 0.03, &tick);
        reach_quick_settings_refresh_layout(quick_settings, &ctx);
        expect_near(reach_quick_settings_state_ptr(quick_settings)->bounds.y, initial_y, 0.001f,
                    "height animation preserves the popup anchor position on every frame");
    }

    (void)reach_quick_settings_set_open(quick_settings, 0);
    reach_quick_settings_capsule_ops()->tick(quick_settings, 0.06, &tick);
    reach_feature_surface_geometry closing_geometry = {};
    reach_quick_settings_capsule_ops()->surface_geometry(quick_settings, &closing_geometry);
    expect_true(closing_geometry.presentation.group.opacity.to == 0.0f,
                "quick settings closes with the shared card fade");
    expect_true(closing_geometry.presentation.group.opacity.to < 1.0f,
                "quick settings keeps the shared dropdown closing fade");
    reach_quick_settings_destroy(quick_settings);
}

static void test_capsule_accepts_surface_local_pointer(void)
{
    reach_quick_settings *quick_settings = nullptr;
    expect_true(reach_quick_settings_create(&quick_settings) == REACH_OK,
                "quick settings capsule is created for pointer input");
    if (quick_settings == nullptr)
    {
        return;
    }

    reach_quick_settings_layout_context ctx = {};
    ctx.theme = reach_theme_default();
    ctx.dpi_scale = 1.0f;
    ctx.anchor_button = {1200.0f, 8.0f, 32.0f, 24.0f};
    ctx.monitor = {0.0f, 0.0f, 1920.0f, 1080.0f};
    ctx.bar_edge_y = 40.0f;
    ctx.drop_direction = REACH_POPUP_DROP_DOWN;
    (void)reach_quick_settings_set_open(quick_settings, 1);
    reach_quick_settings_refresh_layout(quick_settings, &ctx);

    const reach_rect_f32 tile =
        reach_quick_settings_state_ptr(quick_settings)->layout.network_tile.bounds;
    reach_pointer_event pointer = {};
    pointer.kind = REACH_POINTER_EVENT_DOWN;
    pointer.coordinate_space = REACH_POINTER_COORDINATE_SURFACE_LOCAL;
    pointer.button = REACH_POINTER_BUTTON_PRIMARY;
    pointer.x = (int32_t)(tile.x + tile.width * 0.5f);
    pointer.y = (int32_t)(tile.y + tile.height * 0.5f);
    reach_capsule_pointer_result result = {};
    reach_quick_settings_capsule_ops()->handle_pointer(quick_settings, &pointer, &result);
    expect_true(result.handled, "surface-local pointer input reaches a quick settings tile");

    reach_quick_settings_destroy(quick_settings);
}

static void test_capsule_owns_popup_pointer_policy(void)
{
    reach_quick_settings *quick_settings = nullptr;
    expect_true(reach_quick_settings_create(&quick_settings) == REACH_OK,
                "quick settings capsule is created for popup policy");
    if (quick_settings == nullptr)
    {
        return;
    }
    (void)reach_quick_settings_set_open(quick_settings, 1);

    reach_pointer_event pointer = {};
    pointer.kind = REACH_POINTER_EVENT_DOWN;
    pointer.coordinate_space = REACH_POINTER_COORDINATE_SURFACE_LOCAL;
    pointer.surface_relation = REACH_POINTER_SURFACE_OUTSIDE;
    pointer.button = REACH_POINTER_BUTTON_PRIMARY;
    pointer.owner_trigger = 1;
    reach_capsule_pointer_result result = {};
    reach_quick_settings_capsule_ops()->handle_pointer(quick_settings, &pointer, &result);
    expect_true(result.handled && result.continue_source_sequence &&
                    result.action.kind == REACH_FEATURE_ACTION_NONE,
                "the owner trigger keeps quick settings open for its release toggle");

    pointer.owner_trigger = 0;
    reach_quick_settings_capsule_ops()->handle_pointer(quick_settings, &pointer, &result);
    expect_true(result.handled && result.cancel_source_sequence &&
                    result.action.kind == REACH_FEATURE_ACTION_CLOSE_SELF,
                "an outside primary press closes quick settings and cancels its source");

    pointer.button = REACH_POINTER_BUTTON_SECONDARY;
    reach_quick_settings_capsule_ops()->handle_pointer(quick_settings, &pointer, &result);
    expect_true(result.handled && result.continue_source_sequence &&
                    !result.cancel_source_sequence &&
                    result.action.kind == REACH_FEATURE_ACTION_CLOSE_SELF,
                "an outside secondary press closes quick settings and continues to its source");

    reach_quick_settings_destroy(quick_settings);
}

int main(void)
{
    test_model_clamps_volume();
    test_session_list_cap_is_respected();
    test_volume_icon_selection();
    test_feature_transition_owns_presentation();
    test_feature_transition_reopen_is_continuous();
    test_feature_transition_scales_about_its_anchor();
    test_expansion_keeps_popup_anchor_position();
    test_capsule_accepts_surface_local_pointer();
    test_capsule_owns_popup_pointer_policy();

    if (g_failures != 0)
    {
        printf("%d quick settings feature test(s) failed.\n", g_failures);
        return 1;
    }

    printf("quick settings feature tests passed.\n");
    return 0;
}
