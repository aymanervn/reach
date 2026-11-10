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
    transition->scale.started_seconds -= seconds;
    transition->offset.started_seconds -= seconds;
    transition->opacity.started_seconds -= seconds;
    transition->completion_seconds -= seconds;
}

static void test_feature_transition_owns_presentation(void)
{
    const reach_theme *theme = reach_theme_default();
    reach_feature_transition transition = {};
    reach_feature_transition_init(&transition, REACH_FEATURE_PRESENTATION_POPUP,
                                  REACH_FEATURE_TRANSITION_FROM_ABOVE);
    reach_feature_transition_configure(&transition, theme, 1.5f,
                                       REACH_FEATURE_TRANSITION_FROM_ABOVE);

    expect_true(reach_feature_transition_set_open(&transition, 1),
                "opening starts the shared popup transition");
    reach_feature_surface_geometry geometry = {};
    geometry.visible_bounds = {100.0f, 40.0f, 320.0f, 200.0f};
    reach_feature_transition_presentation(&transition, &geometry);
    expect_true(geometry.presentation.retained, "the popup reports managed presentation");
    expect_near(geometry.presentation.group.opacity.from, 0.0f, 0.001f,
                "the popup begins transparent");
    expect_near(geometry.presentation.group.scale.from, 0.94f, 0.001f,
                "the popup begins slightly reduced");
    expect_near(geometry.presentation.group.y.from, -9.0f, 0.001f,
                "a dropdown begins toward its anchor at DPI-scaled distance");
    expect_true(
        geometry.presentation.group.scale.easing == REACH_EASING_SPRING &&
            geometry.presentation.group.scale.spring_response ==
                reach_theme_motion_preset(theme, REACH_MOTION_PRESET_SNAPPY).response_seconds &&
            geometry.presentation.group.y.easing == REACH_EASING_SPRING,
        "scale and offset use the snappy spring");
    expect_near(
        (float)geometry.presentation.group.opacity.duration_seconds,
        (float)reach_theme_motion_preset(theme, REACH_MOTION_PRESET_FADE_IN).duration_seconds,
        1e-6f, "opacity uses the fade-in preset");
    float minimum = 0.0f, maximum = 0.0f;
    expect_true(reach_motion_range(&geometry.presentation.group.scale, &minimum, &maximum) ==
                        REACH_OK &&
                    maximum <= 1.0001f,
                "an opening popup does not bounce");

    age_transition(&transition,
                   reach_motion_deadline(&transition.opacity) - transition.scale.started_seconds);
    expect_true(reach_feature_transition_tick(&transition, 0.0),
                "the open completes at its fade deadline");
    expect_true(reach_feature_transition_visible(&transition), "the opened popup stays visible");

    age_transition(&transition, 1.0);
    expect_true(reach_feature_transition_set_open(&transition, 0),
                "closing reverses the shared popup transition");
    reach_feature_transition_presentation(&transition, &geometry);
    expect_true(geometry.wait_for_presentation, "the close waits for presentation");
    expect_near(transition.scale.to, 0.97f, 0.0001f, "the close reduces scale slightly");
    expect_near(geometry.presentation.group.y.to, -6.0f, 0.001f,
                "the close offsets toward its anchor");
    expect_near((float)transition.opacity.duration_seconds,
                (float)reach_feature_transition_exit_seconds(&transition), 1e-6f,
                "the close fade uses the fade-out preset");
    expect_true(transition.opacity.easing == REACH_EASING_EASE_OUT,
                "the close fade does not ease in");
    expect_true(transition.completion_seconds == reach_motion_deadline(&transition.opacity) &&
                    reach_motion_deadline(&transition.scale) > transition.completion_seconds + 0.2,
                "close completion is the fade deadline, not the spring tail");
    age_transition(&transition, reach_feature_transition_exit_seconds(&transition) - 0.01);
    expect_true(!reach_feature_transition_tick(&transition, 0.0) &&
                    reach_feature_transition_visible(&transition),
                "the popup stays visible until its fade deadline");
    age_transition(&transition, 0.02);
    expect_true(reach_feature_transition_tick(&transition, 0.0) &&
                    !reach_feature_transition_visible(&transition),
                "the popup hides at its fade deadline");
    expect_true(reach_motion_active(&transition.scale),
                "hiding does not wait for the invisible spring tail");

    reach_feature_transition_configure(&transition, theme, 1.0f,
                                       REACH_FEATURE_TRANSITION_FROM_BELOW);
    reach_feature_transition_reset(&transition);
    reach_feature_transition_presentation(&transition, &geometry);
    expect_near(geometry.presentation.group.y.to, 6.0f, 0.001f,
                "an upward popup begins below toward its anchor");
}

static void test_feature_transition_reopen_is_continuous(void)
{
    reach_feature_transition transition = {};
    reach_feature_transition_init(&transition, REACH_FEATURE_PRESENTATION_POPUP,
                                  REACH_FEATURE_TRANSITION_FROM_BELOW);
    reach_feature_transition_configure(&transition, reach_theme_default(), 1.25f,
                                       REACH_FEATURE_TRANSITION_FROM_BELOW);
    (void)reach_feature_transition_set_open(&transition, 1);
    age_transition(&transition, 0.09);
    reach_feature_transition before = transition;
    expect_true(reach_feature_transition_set_open(&transition, 0), "closing mid-open retargets");
    double at = transition.scale.started_seconds;
    const reach_motion *channels[][2] = {{&before.scale, &transition.scale},
                                         {&before.offset, &transition.offset}};
    for (auto &channel : channels)
    {
        expect_near(channel[1]->from, reach_motion_value_at(channel[0], at), 1e-4f,
                    "closing mid-open keeps every value");
        expect_near(reach_motion_velocity_at(channel[1], at),
                    reach_motion_velocity_at(channel[0], at), 1e-3f,
                    "closing mid-open keeps every velocity");
    }
    expect_near(transition.opacity.from, reach_motion_value_at(&before.opacity, at), 1e-4f,
                "closing mid-open keeps opacity");

    age_transition(&transition, 0.05);
    before = transition;
    expect_true(reach_feature_transition_set_open(&transition, 1), "reopening mid-close retargets");
    at = transition.scale.started_seconds;
    float scale_velocity = reach_motion_velocity_at(&before.scale, at);
    float offset_velocity = reach_motion_velocity_at(&before.offset, at);
    expect_true(fabsf(scale_velocity) > 0.01f && fabsf(offset_velocity) > 1.0f,
                "the interrupted close is still moving");
    expect_near(transition.scale.from, reach_motion_value_at(&before.scale, at), 1e-4f,
                "reopening mid-close keeps scale");
    expect_near(transition.offset.from, reach_motion_value_at(&before.offset, at), 1e-4f,
                "reopening mid-close keeps offset");
    expect_near(reach_motion_velocity_at(&transition.scale, at), scale_velocity, 1e-3f,
                "reopening mid-close keeps scale velocity");
    expect_near(reach_motion_velocity_at(&transition.offset, at), offset_velocity, 1e-2f,
                "reopening mid-close keeps offset velocity");
    expect_near(transition.opacity.from, reach_motion_value_at(&before.opacity, at), 1e-4f,
                "reopening mid-close keeps opacity");
    expect_true(transition.visible && transition.opacity.to == 1.0f && transition.scale.to == 1.0f,
                "reopening never restarts from the closed origin");
}

static void test_feature_transition_scales_about_its_anchor(void)
{
    reach_feature_transition transition = {};
    reach_feature_transition_init(&transition, REACH_FEATURE_PRESENTATION_POPUP,
                                  REACH_FEATURE_TRANSITION_FROM_ABOVE);
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
    float cx = reach_motion_value(&geometry.presentation.center_x);
    float cy = reach_motion_value(&geometry.presentation.center_y);
    float scale = geometry.presentation.group.scale.from;
    expect_true(scale < 1.0f && cx == 12.0f && cy == 34.0f,
                "an explicit anchor becomes the scale center");
    expect_near(cx + (12.0f - cx) * scale, 12.0f, 0.001f, "the anchor stays fixed under scale");
    expect_near(cx + (212.0f - cx) * scale, 12.0f + 200.0f * scale, 0.001f,
                "points away from the anchor contract toward it");
}

static void test_feature_transition_menu_style_is_subtler(void)
{
    const reach_theme *theme = reach_theme_default();
    reach_feature_transition menu = {};
    reach_feature_transition_init(&menu, REACH_FEATURE_PRESENTATION_MENU,
                                  REACH_FEATURE_TRANSITION_FROM_BELOW);
    reach_feature_transition_configure(&menu, theme, 2.0f, REACH_FEATURE_TRANSITION_FROM_BELOW);
    (void)reach_feature_transition_set_open(&menu, 1);
    reach_feature_surface_geometry geometry = {};
    reach_feature_transition_presentation(&menu, &geometry);
    expect_near(geometry.presentation.group.scale.from, 0.98f, 0.0001f,
                "a menu begins at a subtler scale");
    expect_true(geometry.presentation.group.y.from == 0.0f &&
                    geometry.presentation.group.y.to == 0.0f &&
                    geometry.presentation.translation_bounds.height == 0.0f,
                "a menu does not travel");
    double menu_fade =
        reach_theme_motion_preset(theme, REACH_MOTION_PRESET_MENU_FADE).duration_seconds;
    expect_near((float)menu.opacity.duration_seconds, (float)menu_fade, 1e-6f,
                "a menu opens with the menu fade");
    age_transition(&menu, 1.0);
    (void)reach_feature_transition_set_open(&menu, 0);
    expect_near((float)reach_feature_transition_exit_seconds(&menu), (float)menu_fade, 1e-6f,
                "a menu closes with the menu fade");
    expect_true(menu.offset.to == 0.0f && menu.scale.to > 0.98f && menu.scale.to < 1.0f,
                "a menu close shrinks subtly without travel");

    reach_feature_transition popup = {};
    reach_feature_transition_init(&popup, REACH_FEATURE_PRESENTATION_POPUP,
                                  REACH_FEATURE_TRANSITION_FROM_BELOW);
    reach_feature_transition_configure(&popup, theme, 2.0f, REACH_FEATURE_TRANSITION_FROM_BELOW);
    (void)reach_feature_transition_set_open(&popup, 1);
    geometry = {};
    reach_feature_transition_presentation(&popup, &geometry);
    expect_true(geometry.presentation.group.scale.from < 0.95f &&
                    geometry.presentation.group.y.from == 12.0f &&
                    geometry.presentation.translation_bounds.height == 12.0f &&
                    popup.opacity.duration_seconds > menu.opacity.duration_seconds,
                "a popup travels farther and fades longer than a menu");
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
    expect_near(opening_geometry.presentation.group.y.from, -7.5f, 0.001f,
                "quick settings keeps the shared dropdown opening motion");
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
    expect_true(closing_geometry.presentation.group.y.to < 0.0f,
                "quick settings keeps the shared dropdown closing motion");
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
    test_feature_transition_menu_style_is_subtler();
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
