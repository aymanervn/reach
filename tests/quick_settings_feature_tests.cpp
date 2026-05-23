#include "reach/features/quick_settings.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_failures = 0;

static void expect_true(int condition, const char *message)
{
    if (!condition) {
        ++g_failures;
        printf("FAIL: %s\n", message);
    }
}

static void expect_near(float actual, float expected, float epsilon, const char *message)
{
    if (fabsf(actual - expected) > epsilon) {
        ++g_failures;
        printf(
            "FAIL: %s: expected %.3f, got %.3f\n",
            message,
            expected,
            actual);
    }
}

static reach_theme test_theme(void)
{
    reach_theme theme = {};
    theme.quick_settings_slider_track_color = { 0.20f, 0.20f, 0.20f, 1.0f };
    theme.quick_settings_slider_fill_color = { 0.85f, 0.85f, 0.85f, 1.0f };
    theme.quick_settings_slider_muted_fill_color = { 0.45f, 0.45f, 0.45f, 1.0f };
    theme.quick_settings_expand_button_color = { 0.16f, 0.16f, 0.16f, 1.0f };
    theme.quick_settings_expand_text_color = { 0.90f, 0.90f, 0.90f, 1.0f };
    theme.quick_settings_expand_icon_color = { 0.75f, 0.75f, 0.75f, 1.0f };
    return theme;
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

static void test_layout_places_expand_below_slider(void)
{
    reach_theme theme = test_theme();
    reach_rect_f32 bounds = {};
    bounds.x = 100.0f;
    bounds.y = 200.0f;
    bounds.width = 240.0f;
    bounds.height = 90.0f;

    reach_quick_settings_layout layout =
        reach_quick_settings_layout_for_content_bounds(bounds, &theme);

    expect_true(
        layout.main_slider_track.y >= bounds.y,
        "slider starts inside content bounds");

    expect_true(
        layout.expand_button.y >
            layout.main_slider_track.y + layout.main_slider_track.height,
        "expand button is below slider");

    expect_true(
        layout.main_slider_track.x >= bounds.x &&
            layout.main_slider_track.x + layout.main_slider_track.width <= bounds.x + bounds.width,
        "slider stays inside horizontal content bounds");

    expect_true(
        layout.expand_button.x >= bounds.x &&
            layout.expand_button.x + layout.expand_button.width <= bounds.x + bounds.width,
        "expand button stays inside horizontal content bounds");
}

static void test_slider_hit_maps_to_volume(void)
{
    reach_theme theme = test_theme();
    reach_rect_f32 bounds = {};
    bounds.x = 0.0f;
    bounds.y = 0.0f;
    bounds.width = 224.0f;
    bounds.height = 90.0f;

    reach_quick_settings_layout layout =
        reach_quick_settings_layout_for_content_bounds(bounds, &theme);

    float left_x = layout.main_slider_track.x;
    float mid_x = layout.main_slider_track.x + layout.main_slider_track.width * 0.5f;
    float right_x = layout.main_slider_track.x + layout.main_slider_track.width;
    float y = layout.main_slider_track.y + layout.main_slider_track.height * 0.5f;

    reach_quick_settings_hit_result left =
        reach_quick_settings_hit_test(&layout, left_x, y);
    reach_quick_settings_hit_result mid =
        reach_quick_settings_hit_test(&layout, mid_x, y);
    reach_quick_settings_hit_result right =
        reach_quick_settings_hit_test(&layout, right_x, y);

    expect_true(left.type == REACH_QUICK_SETTINGS_HIT_MAIN_SLIDER, "left hit is slider");
    expect_true(mid.type == REACH_QUICK_SETTINGS_HIT_MAIN_SLIDER, "middle hit is slider");
    expect_true(right.type == REACH_QUICK_SETTINGS_HIT_MAIN_SLIDER, "right hit is slider");

    expect_near(left.volume_level, 0.0f, 0.001f, "left hit maps to zero volume");
    expect_near(mid.volume_level, 0.5f, 0.001f, "middle hit maps to half volume");
    expect_near(right.volume_level, 1.0f, 0.001f, "right hit maps to full volume");
}

static void test_slider_fill_width_follows_volume(void)
{
    reach_theme theme = test_theme();
    reach_rect_f32 bounds = {};
    bounds.width = 224.0f;
    bounds.height = 90.0f;

    float levels[] = { 0.0f, 0.5f, 1.0f };
    for (size_t index = 0; index < 3; ++index) {
        reach_quick_settings_model model = {};
        reach_quick_settings_model_init(&model);
        reach_quick_settings_model_set_main_volume(&model, levels[index], 0);

        reach_quick_settings_render_input input = {};
        input.model = model;
        input.layout = reach_quick_settings_layout_for_content_bounds(bounds, &theme);
        input.theme = theme;

        reach_render_command_buffer commands = {};
        reach_result result = reach_quick_settings_build_render_commands(&input, &commands);
        expect_true(result == REACH_OK, "fill width render build succeeds");
        if (levels[index] == 0.0f) {
            expect_true(commands.commands[1].type == REACH_RENDER_COMMAND_RECT, "button follows track when fill omitted at zero");
            expect_near(commands.commands[1].rect.width, input.layout.expand_button.width, 0.001f, "zero fill is omitted");
        } else {
            expect_near(
                commands.commands[1].rect.width,
                input.layout.main_slider_track.width * levels[index],
                0.001f,
                "fill width follows model volume");
        }
    }
}

static void test_hit_outside_returns_none(void)
{
    reach_theme theme = test_theme();
    reach_rect_f32 bounds = {};
    bounds.width = 224.0f;
    bounds.height = 90.0f;

    reach_quick_settings_layout layout =
        reach_quick_settings_layout_for_content_bounds(bounds, &theme);

    reach_quick_settings_hit_result hit =
        reach_quick_settings_hit_test(&layout, -20.0f, -20.0f);

    expect_true(hit.type == REACH_QUICK_SETTINGS_HIT_NONE, "outside hit returns none");
}

static void test_expand_button_hit_maps_to_expand_action(void)
{
    reach_theme theme = test_theme();
    reach_rect_f32 bounds = {};
    bounds.x = 0.0f;
    bounds.y = 0.0f;
    bounds.width = 224.0f;
    bounds.height = 90.0f;

    reach_quick_settings_layout layout =
        reach_quick_settings_layout_for_content_bounds(bounds, &theme);

    float x = layout.expand_button.x + layout.expand_button.width * 0.5f;
    float y = layout.expand_button.y + layout.expand_button.height * 0.5f;

    reach_quick_settings_hit_result hit =
        reach_quick_settings_hit_test(&layout, x, y);

    reach_quick_settings_action action =
        reach_quick_settings_action_for_hit(hit);

    expect_true(hit.type == REACH_QUICK_SETTINGS_HIT_EXPAND_BUTTON, "expand button hit");
    expect_true(action.type == REACH_QUICK_SETTINGS_ACTION_EXPAND, "expand hit maps to expand action");
}

static void test_slider_hit_maps_to_set_volume_action(void)
{
    reach_theme theme = test_theme();
    reach_rect_f32 bounds = {};
    bounds.x = 0.0f;
    bounds.y = 0.0f;
    bounds.width = 224.0f;
    bounds.height = 90.0f;

    reach_quick_settings_layout layout =
        reach_quick_settings_layout_for_content_bounds(bounds, &theme);

    float x = layout.main_slider_track.x + layout.main_slider_track.width * 0.25f;
    float y = layout.main_slider_track.y + layout.main_slider_track.height * 0.5f;

    reach_quick_settings_hit_result hit =
        reach_quick_settings_hit_test(&layout, x, y);

    reach_quick_settings_action action =
        reach_quick_settings_action_for_hit(hit);

    expect_true(action.type == REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME, "slider hit maps to set volume action");
    expect_near(action.volume_level, 0.25f, 0.001f, "set volume action carries hit volume");
}

static void test_render_emits_slider_and_expand_commands(void)
{
    reach_theme theme = test_theme();

    reach_rect_f32 bounds = {};
    bounds.x = 0.0f;
    bounds.y = 0.0f;
    bounds.width = 224.0f;
    bounds.height = 90.0f;

    reach_quick_settings_model model = {};
    reach_quick_settings_model_init(&model);
    reach_quick_settings_model_set_main_volume(&model, 0.5f, 0);

    reach_quick_settings_render_input input = {};
    input.model = model;
    input.layout = reach_quick_settings_layout_for_content_bounds(bounds, &theme);
    input.theme = theme;

    reach_render_command_buffer commands = {};

    reach_result result =
        reach_quick_settings_build_render_commands(&input, &commands);

    expect_true(result == REACH_OK, "render command build succeeds");
    expect_true(commands.count >= 5, "render emits slider, fill, button, label, icon commands");

    expect_true(
        commands.commands[0].type == REACH_RENDER_COMMAND_RECT,
        "first command is slider track");

    expect_true(
        commands.commands[1].type == REACH_RENDER_COMMAND_RECT,
        "second command is slider fill");

    expect_near(
        commands.commands[1].rect.width,
        input.layout.main_slider_track.width * 0.5f,
        0.001f,
        "slider fill width follows volume");

    expect_true(
        commands.commands[2].type == REACH_RENDER_COMMAND_RECT,
        "third command is expand button background");
}

int main(void)
{
    test_model_clamps_volume();
    test_layout_places_expand_below_slider();
    test_slider_hit_maps_to_volume();
    test_slider_fill_width_follows_volume();
    test_hit_outside_returns_none();
    test_expand_button_hit_maps_to_expand_action();
    test_slider_hit_maps_to_set_volume_action();
    test_render_emits_slider_and_expand_commands();

    if (g_failures != 0) {
        printf("%d quick settings feature test(s) failed.\n", g_failures);
        return 1;
    }

    printf("quick settings feature tests passed.\n");
    return 0;
}
