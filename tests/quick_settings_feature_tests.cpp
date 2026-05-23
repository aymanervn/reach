#include "reach/features/quick_settings.h"
#include "reach/features/popup.h"

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

static void expect_color_equal(reach_color actual, reach_color expected, const char *message)
{
    expect_near(actual.r, expected.r, 0.001f, message);
    expect_near(actual.g, expected.g, 0.001f, message);
    expect_near(actual.b, expected.b, 0.001f, message);
    expect_near(actual.a, expected.a, 0.001f, message);
}

static int text_equals_ascii(const uint16_t *text, const char *expected)
{
    size_t index = 0;
    while (expected[index] != 0) {
        if (text[index] != (uint16_t)(unsigned char)expected[index]) {
            return 0;
        }
        ++index;
    }
    return text[index] == 0;
}

static reach_theme test_theme(void)
{
    reach_theme theme = {};
    theme.quick_settings_slider_track_color = { 0.20f, 0.20f, 0.20f, 1.0f };
    theme.quick_settings_slider_fill_color = { 0.85f, 0.85f, 0.85f, 1.0f };
    theme.quick_settings_slider_muted_fill_color = { 0.45f, 0.45f, 0.45f, 1.0f };
    theme.quick_settings_expand_button_color = { 0.16f, 0.16f, 0.16f, 1.0f };
    theme.quick_settings_expand_text_color = { 0.90f, 0.90f, 0.90f, 1.0f };
    theme.icon_backplate_background = { 0.98f, 0.92f, 0.84f, 0.96f };
    return theme;
}

static void copy_ascii(uint16_t *dst, size_t dst_count, const char *src)
{
    if (dst == nullptr || dst_count == 0) {
        return;
    }
    size_t index = 0;
    if (src != nullptr) {
        while (index + 1 < dst_count && src[index] != 0) {
            dst[index] = (uint16_t)(unsigned char)src[index];
            ++index;
        }
    }
    dst[index] = 0;
}

static reach_quick_settings_model test_model_with_sessions(size_t count)
{
    reach_quick_settings_model model = {};
    reach_quick_settings_model_init(&model);
    reach_quick_settings_model_set_main_volume(&model, 0.5f, 0);

    reach_audio_volume_session_list sessions = {};
    sessions.count = count;
    for (size_t index = 0; index < count && index < REACH_AUDIO_VOLUME_MAX_SESSIONS; ++index) {
        sessions.sessions[index].level = 0.25f + (float)index * 0.05f;
        sessions.sessions[index].muted = 0;
        sessions.sessions[index].process_id = (uint32_t)(1000 + index);
        copy_ascii(
            sessions.sessions[index].session_instance_id,
            REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY,
            index == 0 ? "session-a" : "session-b");
        copy_ascii(
            sessions.sessions[index].label,
            REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY,
            index == 0 ? "App A" : "App B");
    }

    reach_quick_settings_model_set_sessions(&model, &sessions);
    return model;
}

static reach_quick_settings_layout test_layout_for_model(
    const reach_quick_settings_model *model)
{
    reach_theme theme = test_theme();
    reach_rect_f32 bounds = {};
    bounds.x = 0.0f;
    bounds.y = 0.0f;
    bounds.width = 224.0f;
    bounds.height = reach_quick_settings_content_height_for_model(model);
    return reach_quick_settings_layout_for_content_bounds(bounds, &theme, model);
}

static reach_quick_settings_layout test_layout(void)
{
    reach_quick_settings_model model = {};
    reach_quick_settings_model_init(&model);
    return test_layout_for_model(&model);
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

static void test_layout_places_volume_pill_above_expand(void)
{
    reach_theme theme = test_theme();
    reach_rect_f32 bounds = {};
    bounds.x = 100.0f;
    bounds.y = 200.0f;
    bounds.width = 240.0f;
    bounds.height = 112.0f;

    reach_quick_settings_layout layout =
        reach_quick_settings_layout_for_content_bounds(bounds, &theme, nullptr);

    const float expected_padding = 8.0f;

    expect_true(
        layout.main_volume_pill.bounds.y >= bounds.y,
        "volume pill starts inside content bounds");
    expect_near(
        layout.main_volume_pill.header_label.y - bounds.y,
        expected_padding,
        0.001f,
        "top content padding is compact");
    expect_true(
        layout.main_volume_pill.header_label.y + layout.main_volume_pill.header_label.height <=
            layout.main_volume_pill.bounds.y,
        "volume header is above slider pill");

    expect_true(
        layout.expand_button.y >
            layout.main_volume_pill.bounds.y + layout.main_volume_pill.bounds.height,
        "expand button is below volume pill");

    expect_true(
        layout.main_volume_pill.bounds.x >= bounds.x &&
            layout.main_volume_pill.bounds.x + layout.main_volume_pill.bounds.width <= bounds.x + bounds.width,
        "volume pill stays inside horizontal content bounds");

    expect_true(
        layout.expand_button.x >= bounds.x &&
            layout.expand_button.x + layout.expand_button.width <= bounds.x + bounds.width,
        "expand button stays inside horizontal content bounds");
    expect_near(
        layout.main_volume_pill.bounds.x - bounds.x,
        expected_padding,
        0.001f,
        "left content padding is compact");
    expect_near(
        bounds.x + bounds.width -
            (layout.main_volume_pill.bounds.x + layout.main_volume_pill.bounds.width),
        expected_padding,
        0.001f,
        "right content padding is compact");
    expect_near(
        bounds.y + bounds.height -
            (layout.expand_button.y + layout.expand_button.height),
        expected_padding,
        0.001f,
        "bottom content padding is compact");

    expect_near(
        layout.main_volume_pill.bounds.width,
        layout.expand_button.width,
        0.001f,
        "volume pill and expand button have same width");
    expect_near(
        layout.main_slider_track.height,
        layout.main_volume_pill.bounds.height,
        0.001f,
        "slider track fills volume pill height");

    expect_near(
        layout.main_volume_pill.header_icon.width,
        16.0f,
        0.001f,
        "master volume icon is larger");
}

static void test_slider_hit_maps_to_volume(void)
{
    reach_quick_settings_layout layout = test_layout();

    float left_x = layout.main_slider_track.x;
    float mid_x = layout.main_slider_track.x + layout.main_slider_track.width * 0.5f;
    float right_x = layout.main_slider_track.x + layout.main_slider_track.width;
    float y = layout.main_slider_track.y + layout.main_slider_track.height * 0.5f;

    reach_quick_settings_hit_result left =
        reach_quick_settings_hit_test(&layout, nullptr, left_x, y);
    reach_quick_settings_hit_result mid =
        reach_quick_settings_hit_test(&layout, nullptr, mid_x, y);
    reach_quick_settings_hit_result right =
        reach_quick_settings_hit_test(&layout, nullptr, right_x, y);

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
    float levels[] = { 0.0f, 0.02f, 0.5f, 1.0f };

    for (size_t index = 0; index < sizeof(levels) / sizeof(levels[0]); ++index) {
        reach_quick_settings_model model = {};
        reach_quick_settings_model_init(&model);
        reach_quick_settings_model_set_main_volume(&model, levels[index], 0);

        reach_quick_settings_render_input input = {};
        input.model = model;
        input.layout = test_layout();
        input.theme = theme;

        reach_render_command_buffer commands = {};
        reach_result result = reach_quick_settings_build_render_commands(&input, &commands);
        expect_true(result == REACH_OK, "fill width render build succeeds");
        if (levels[index] == 0.0f) {
            expect_true(
                commands.commands[2].type == REACH_RENDER_COMMAND_RECT,
                "track follows volume row when fill omitted at zero");
            expect_near(
                commands.commands[2].rect.width,
                input.layout.main_slider_track.width,
                0.001f,
                "zero fill is omitted");
        } else {
            expect_true(
                commands.commands[3].type == REACH_RENDER_COMMAND_CLIPPED_ROUNDED_RECT,
                "fill is clipped to slider pill");
            expect_near(
                commands.commands[3].rect.width,
                input.layout.main_slider_track.width * levels[index],
                0.001f,
                "fill width follows model volume");
        }
    }
}

static void test_hit_outside_returns_none(void)
{
    reach_quick_settings_layout layout = test_layout();
    reach_quick_settings_hit_result hit =
        reach_quick_settings_hit_test(&layout, nullptr, -20.0f, -20.0f);

    expect_true(hit.type == REACH_QUICK_SETTINGS_HIT_NONE, "outside hit returns none");
}

static void test_expand_button_hit_maps_to_expand_action(void)
{
    reach_quick_settings_layout layout = test_layout();
    float x = layout.expand_button.x + layout.expand_button.width * 0.5f;
    float y = layout.expand_button.y + layout.expand_button.height * 0.5f;

    reach_quick_settings_hit_result hit =
        reach_quick_settings_hit_test(&layout, nullptr, x, y);

    reach_quick_settings_action action =
        reach_quick_settings_action_for_hit(hit);

    expect_true(hit.type == REACH_QUICK_SETTINGS_HIT_EXPAND_BUTTON, "expand button hit");
    expect_true(action.type == REACH_QUICK_SETTINGS_ACTION_EXPAND, "expand hit maps to expand action");
}

static void test_slider_hit_maps_to_set_volume_action(void)
{
    reach_quick_settings_layout layout = test_layout();
    float x = layout.main_slider_track.x + layout.main_slider_track.width * 0.25f;
    float y = layout.main_slider_track.y + layout.main_slider_track.height * 0.5f;

    reach_quick_settings_hit_result hit =
        reach_quick_settings_hit_test(&layout, nullptr, x, y);

    reach_quick_settings_action action =
        reach_quick_settings_action_for_hit(hit);

    expect_true(action.type == REACH_QUICK_SETTINGS_ACTION_SET_MAIN_VOLUME, "slider hit maps to set volume action");
    expect_near(action.volume_level, 0.25f, 0.001f, "set volume action carries hit volume");
}

static void test_expanded_layout_stacks_session_pills(void)
{
    reach_quick_settings_model model = test_model_with_sessions(2);
    model.expanded = 1;

    reach_quick_settings_layout layout = test_layout_for_model(&model);

    expect_true(layout.session_pill_count == 2, "expanded layout exposes sessions");
    expect_true(
        layout.session_volume_pills[0].bounds.y >
            layout.main_volume_pill.bounds.y + layout.main_volume_pill.bounds.height,
        "first session is below master");
    expect_true(
        layout.session_volume_pills[1].bounds.y >
            layout.session_volume_pills[0].bounds.y + layout.session_volume_pills[0].bounds.height,
        "second session is below first session");
    expect_true(
        layout.expand_button.y >
            layout.session_volume_pills[1].bounds.y + layout.session_volume_pills[1].bounds.height,
        "expand button is below sessions");
    expect_near(
        layout.session_volume_pills[0].bounds.width,
        layout.expand_button.width,
        0.001f,
        "session pill aligns to expand width");
}

static void test_collapsed_layout_hides_session_pills(void)
{
    reach_quick_settings_model model = test_model_with_sessions(2);
    model.expanded = 0;

    reach_quick_settings_layout layout = test_layout_for_model(&model);

    expect_true(layout.session_pill_count == 0, "collapsed layout hides sessions");
    expect_true(
        layout.expand_button.y >
            layout.main_volume_pill.bounds.y + layout.main_volume_pill.bounds.height,
        "collapsed expand button follows master");
}

static void test_session_slider_hit_maps_to_session_action(void)
{
    reach_quick_settings_model model = test_model_with_sessions(2);
    model.expanded = 1;
    reach_quick_settings_layout layout = test_layout_for_model(&model);

    float x = layout.session_volume_pills[1].slider_track.x +
        layout.session_volume_pills[1].slider_track.width * 0.75f;
    float y = layout.session_volume_pills[1].slider_track.y +
        layout.session_volume_pills[1].slider_track.height * 0.5f;

    reach_quick_settings_hit_result hit =
        reach_quick_settings_hit_test(&layout, &model, x, y);
    reach_quick_settings_action action =
        reach_quick_settings_action_for_hit(hit);

    expect_true(hit.type == REACH_QUICK_SETTINGS_HIT_SESSION_SLIDER, "session slider hit");
    expect_true(action.type == REACH_QUICK_SETTINGS_ACTION_SET_SESSION_VOLUME, "session hit maps to session action");
    expect_true(action.session_index == 1, "session action carries index");
    expect_true(text_equals_ascii(action.session_instance_id, "session-b"), "session action carries key");
    expect_near(action.volume_level, 0.75f, 0.001f, "session action carries volume");
}

static void test_session_list_cap_is_respected(void)
{
    reach_quick_settings_model model = test_model_with_sessions(REACH_AUDIO_VOLUME_MAX_SESSIONS + 4);
    model.expanded = 1;
    reach_quick_settings_layout layout = test_layout_for_model(&model);

    expect_true(model.sessions.count == REACH_AUDIO_VOLUME_MAX_SESSIONS, "session model caps list");
    expect_true(layout.session_pill_count == REACH_AUDIO_VOLUME_MAX_SESSIONS, "layout caps session pills");
}

static void test_volume_icon_selection(void)
{
    expect_true(
        reach_quick_settings_volume_icon_id(0.8f, 1) == REACH_VECTOR_ICON_VOLUME_ZERO,
        "muted volume uses zero icon");
    expect_true(
        reach_quick_settings_volume_icon_id(0.0f, 0) == REACH_VECTOR_ICON_VOLUME_ZERO,
        "zero volume uses zero icon");
    expect_true(
        reach_quick_settings_volume_icon_id(0.25f, 0) == REACH_VECTOR_ICON_VOLUME_LOW,
        "low volume uses low icon");
    expect_true(
        reach_quick_settings_volume_icon_id(0.75f, 0) == REACH_VECTOR_ICON_VOLUME_HIGH,
        "high volume uses high icon");
}

static void test_expanded_render_emits_session_volume_pills(void)
{
    reach_theme theme = test_theme();
    reach_quick_settings_model model = test_model_with_sessions(2);
    model.expanded = 1;

    reach_quick_settings_render_input input = {};
    input.model = model;
    input.layout = test_layout_for_model(&model);
    input.theme = theme;

    reach_render_command_buffer commands = {};
    reach_result result =
        reach_quick_settings_build_render_commands(&input, &commands);

    expect_true(result == REACH_OK, "expanded render succeeds");
    expect_true(commands.count >= 15, "expanded render emits master, sessions, and expand commands");
    expect_true(commands.commands[4].type == REACH_RENDER_COMMAND_VECTOR_ICON, "first session icon follows master pill");
    expect_true(commands.commands[8].type == REACH_RENDER_COMMAND_VECTOR_ICON, "second session icon follows first session");
}

static void test_render_emits_volume_pill_and_expand_commands(void)
{
    reach_theme theme = test_theme();
    reach_quick_settings_model model = {};
    reach_quick_settings_model_init(&model);
    reach_quick_settings_model_set_main_volume(&model, 0.5f, 0);

    reach_quick_settings_render_input input = {};
    input.model = model;
    input.layout = test_layout();
    input.theme = theme;

    reach_render_command_buffer commands = {};
    reach_result result =
        reach_quick_settings_build_render_commands(&input, &commands);

    expect_true(result == REACH_OK, "render command build succeeds");
    expect_true(commands.count >= 7, "render emits volume row, slider, button, label, icon commands");

    expect_true(
        commands.commands[0].type == REACH_RENDER_COMMAND_VECTOR_ICON &&
            commands.commands[0].icon_id == REACH_VECTOR_ICON_VOLUME_HIGH,
        "first command is volume icon");
    expect_color_equal(
        commands.commands[0].color,
        theme.icon_backplate_background,
        "volume icon uses app backplate color");

    expect_true(
        commands.commands[1].type == REACH_RENDER_COMMAND_TEXT &&
            text_equals_ascii(commands.commands[1].text, "Master volume"),
        "second command is Master volume text");

    expect_true(
        commands.commands[2].type == REACH_RENDER_COMMAND_RECT,
        "third command is slider track");
    expect_near(
        commands.commands[2].radius,
        reach_popup_radius(),
        0.001f,
        "slider track uses popup corner radius");
    expect_near(
        commands.commands[2].rect.height,
        input.layout.main_volume_pill.bounds.height,
        0.001f,
        "slider track fills pill height");

    expect_true(
        commands.commands[3].type == REACH_RENDER_COMMAND_CLIPPED_ROUNDED_RECT,
        "fourth command is slider fill");

    expect_near(
        commands.commands[3].rect.width,
        input.layout.main_slider_track.width * 0.5f,
        0.001f,
        "slider fill width follows volume");
    expect_near(
        commands.commands[3].rect.height,
        commands.commands[2].rect.height,
        0.001f,
        "slider fill height matches track height");
    expect_near(
        commands.commands[3].rect.y,
        commands.commands[2].rect.y,
        0.001f,
        "slider fill y matches track y");
    expect_near(
        commands.commands[3].clip_rect.x,
        commands.commands[2].rect.x,
        0.001f,
        "slider fill clips to track x");
    expect_near(
        commands.commands[3].clip_rect.y,
        commands.commands[2].rect.y,
        0.001f,
        "slider fill clips to track y");
    expect_near(
        commands.commands[3].clip_rect.width,
        commands.commands[2].rect.width,
        0.001f,
        "slider fill clips to track width");
    expect_near(
        commands.commands[3].clip_rect.height,
        commands.commands[2].rect.height,
        0.001f,
        "slider fill clips to track height");
    expect_near(
        commands.commands[3].clip_radius,
        commands.commands[2].radius,
        0.001f,
        "slider fill clips to track radius");

    expect_true(
        commands.commands[4].type == REACH_RENDER_COMMAND_RECT,
        "fifth command is expand button background");
    expect_near(
        commands.commands[4].radius,
        reach_popup_radius(),
        0.001f,
        "expand button uses popup corner radius");

    expect_true(
        commands.commands[6].type == REACH_RENDER_COMMAND_VECTOR_ICON &&
            commands.commands[6].icon_id == REACH_VECTOR_ICON_ARROW_DOWN,
        "expand button uses arrow down icon");
    expect_color_equal(
        commands.commands[6].color,
        theme.icon_backplate_background,
        "expand icon uses app backplate color");
}

int main(void)
{
    test_model_clamps_volume();
    test_layout_places_volume_pill_above_expand();
    test_slider_hit_maps_to_volume();
    test_slider_fill_width_follows_volume();
    test_hit_outside_returns_none();
    test_expand_button_hit_maps_to_expand_action();
    test_slider_hit_maps_to_set_volume_action();
    test_expanded_layout_stacks_session_pills();
    test_collapsed_layout_hides_session_pills();
    test_session_slider_hit_maps_to_session_action();
    test_session_list_cap_is_respected();
    test_volume_icon_selection();
    test_render_emits_volume_pill_and_expand_commands();
    test_expanded_render_emits_session_volume_pills();

    if (g_failures != 0) {
        printf("%d quick settings feature test(s) failed.\n", g_failures);
        return 1;
    }

    printf("quick settings feature tests passed.\n");
    return 0;
}
