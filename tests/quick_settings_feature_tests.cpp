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
        sessions.sessions[index].icon_id = (uint64_t)(9000 + index);
        copy_ascii(
            sessions.sessions[index].session_instance_id,
            REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY,
            index == 0 ? "session-a" : "session-b");
        copy_ascii(
            sessions.sessions[index].label,
            REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY,
            index == 0 ? "App A.exe" : "App B.exe");
    }

    reach_quick_settings_model_set_sessions(&model, &sessions);
    return model;
}

static reach_quick_settings_model test_model_with_output_devices(size_t count)
{
    reach_quick_settings_model model = {};
    reach_quick_settings_model_init(&model);
    reach_quick_settings_model_set_main_volume(&model, 0.5f, 0);

    reach_audio_output_device_list devices = {};
    devices.count = count;
    for (size_t index = 0;
        index < count && index < REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES;
        ++index) {
        devices.devices[index].icon_id = (uint64_t)(7000 + index);
        devices.devices[index].is_default = index == 1 ? 1 : 0;
        copy_ascii(
            devices.devices[index].device_id,
            REACH_AUDIO_VOLUME_DEVICE_ID_CAPACITY,
            index == 0 ? "device-a" : "device-b");
        copy_ascii(
            devices.devices[index].label,
            REACH_AUDIO_VOLUME_DEVICE_LABEL_CAPACITY,
            index == 0 ? "Speakers" : "Headphones");
    }

    reach_quick_settings_model_set_output_devices(&model, &devices);
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

static void test_layout_places_volume_pill_above_output_device_and_expand(void)
{
    reach_theme theme = test_theme();
    reach_rect_f32 bounds = {};
    bounds.x = 100.0f;
    bounds.y = 200.0f;
    bounds.width = 240.0f;
    bounds.height = 168.0f;

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
        layout.output_device_button.y >
            layout.main_volume_pill.bounds.y + layout.main_volume_pill.bounds.height,
        "output device button is below volume pill");
    expect_true(
        layout.expand_button.y >
            layout.output_device_button.y + layout.output_device_button.height,
        "expand button is below output device button");

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
        layout.output_device_button.width,
        layout.main_volume_pill.bounds.width,
        0.001f,
        "output device button and master pill have same width");
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

static void test_output_device_button_hit_maps_to_toggle_action(void)
{
    reach_quick_settings_model model = test_model_with_output_devices(2);
    reach_quick_settings_layout layout = test_layout_for_model(&model);
    float x = layout.output_device_button.x + layout.output_device_button.width * 0.5f;
    float y = layout.output_device_button.y + layout.output_device_button.height * 0.5f;

    reach_quick_settings_hit_result hit =
        reach_quick_settings_hit_test(&layout, &model, x, y);
    reach_quick_settings_action action =
        reach_quick_settings_action_for_hit(hit);

    expect_true(
        hit.type == REACH_QUICK_SETTINGS_HIT_OUTPUT_DEVICE_BUTTON,
        "output device button hit");
    expect_true(
        action.type == REACH_QUICK_SETTINGS_ACTION_TOGGLE_OUTPUT_DEVICES,
        "output device button maps to toggle action");
}

static void test_output_device_title_chevron_hit_maps_to_toggle_action(void)
{
    reach_quick_settings_model model = test_model_with_output_devices(2);
    model.output_devices_expanded = 1;
    reach_quick_settings_layout layout = test_layout_for_model(&model);
    float x = layout.output_devices_title_chevron.x +
        layout.output_devices_title_chevron.width * 0.5f;
    float y = layout.output_devices_title_chevron.y +
        layout.output_devices_title_chevron.height * 0.5f;

    reach_quick_settings_hit_result hit =
        reach_quick_settings_hit_test(&layout, &model, x, y);
    reach_quick_settings_action action =
        reach_quick_settings_action_for_hit(hit);

    expect_true(
        hit.type == REACH_QUICK_SETTINGS_HIT_OUTPUT_DEVICE_BUTTON,
        "output device title chevron hit");
    expect_true(
        action.type == REACH_QUICK_SETTINGS_ACTION_TOGGLE_OUTPUT_DEVICES,
        "output device title chevron maps to toggle action");
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

static void test_expanded_layout_shows_app_volume_panel(void)
{
    reach_quick_settings_model model = test_model_with_sessions(2);
    model.expanded = 1;

    reach_quick_settings_layout layout = test_layout_for_model(&model);

    expect_true(layout.app_volume_row_count == 2, "expanded layout exposes app rows");
    expect_true(
        layout.app_volumes_title.y >
            layout.output_device_button.y + layout.output_device_button.height,
        "app volumes title is below output device button");
    expect_true(
        layout.app_volumes_panel.y >
            layout.app_volumes_title.y + layout.app_volumes_title.height,
        "app volumes panel is below title");
    expect_near(
        layout.app_volumes_panel.width,
        layout.main_volume_pill.bounds.width,
        0.001f,
        "app volumes panel matches master pill width");
    expect_near(
        layout.app_volumes_panel.width,
        layout.expand_button.width,
        0.001f,
        "app volumes panel matches expand width");
    expect_near(
        layout.app_volumes_panel.height,
        120.0f,
        0.001f,
        "app volumes panel height includes visible rows and hide row");
    expect_true(
        layout.expand_button.y >= layout.app_volumes_panel.y &&
            layout.expand_button.y + layout.expand_button.height <=
                layout.app_volumes_panel.y + layout.app_volumes_panel.height,
        "hide button is inside app volumes panel");
    expect_near(
        layout.expand_button.y,
        layout.app_volume_rows[1].bounds.y + layout.app_volume_rows[1].bounds.height,
        0.001f,
        "hide button follows app volume rows");
}

static void test_expanded_output_device_layout_shows_panel(void)
{
    reach_quick_settings_model model = test_model_with_output_devices(2);
    model.output_devices_expanded = 1;

    reach_quick_settings_layout layout = test_layout_for_model(&model);

    expect_true(layout.output_device_row_count == 2, "expanded output layout exposes device rows");
    expect_true(
        layout.output_devices_title.y >
            layout.output_device_button.y + layout.output_device_button.height,
        "output devices title is below output button");
    expect_true(
        layout.output_devices_panel.y >
            layout.output_devices_title.y + layout.output_devices_title.height,
        "output devices panel is below title");
    expect_near(
        layout.output_devices_panel.width,
        layout.main_volume_pill.bounds.width,
        0.001f,
        "output devices panel matches master pill width");
    expect_near(
        layout.output_devices_panel.height,
        88.0f,
        0.001f,
        "output devices panel height follows device rows");
    expect_true(
        layout.output_device_rows[0].icon.x < layout.output_device_rows[0].label.x,
        "output device icon is before label");
    expect_true(
        layout.output_device_rows[0].label.x + layout.output_device_rows[0].label.width <
            layout.output_device_rows[0].checkmark.x,
        "output device label is before checkmark");
}

static void test_output_device_row_hit_maps_to_set_device_action(void)
{
    reach_quick_settings_model model = test_model_with_output_devices(2);
    model.output_devices_expanded = 1;
    reach_quick_settings_layout layout = test_layout_for_model(&model);

    float x = layout.output_device_rows[1].bounds.x +
        layout.output_device_rows[1].bounds.width * 0.5f;
    float y = layout.output_device_rows[1].bounds.y +
        layout.output_device_rows[1].bounds.height * 0.5f;

    reach_quick_settings_hit_result hit =
        reach_quick_settings_hit_test(&layout, &model, x, y);
    reach_quick_settings_action action =
        reach_quick_settings_action_for_hit(hit);

    expect_true(
        hit.type == REACH_QUICK_SETTINGS_HIT_OUTPUT_DEVICE_ROW,
        "output device row hit");
    expect_true(
        action.type == REACH_QUICK_SETTINGS_ACTION_SET_OUTPUT_DEVICE,
        "output device row maps to set output action");
    expect_true(action.output_device_index == 1, "output action carries device index");
    expect_true(
        text_equals_ascii(action.output_device_id, "device-b"),
        "output action carries device id");
}

static void test_app_volume_row_layout_is_compact(void)
{
    reach_quick_settings_model model = test_model_with_sessions(2);
    model.expanded = 1;

    reach_quick_settings_layout layout = test_layout_for_model(&model);
    const reach_quick_settings_app_volume_row_layout *row = &layout.app_volume_rows[1];

    expect_near(
        row->bounds.height,
        40.0f,
        0.001f,
        "app volume row uses compact fixed height");
    expect_true(
        row->app_icon.x < row->app_label.x,
        "app row icon is before label");
    expect_true(
        row->app_label.x + row->app_label.width < row->slider_full_range_line.x,
        "app row label is before slider");
    expect_true(
        row->slider_full_range_line.x + row->slider_full_range_line.width <
            row->app_volume_percent.x,
        "app row slider is before percent");
    expect_true(
        row->slider_full_range_line.x < row->slider_thumb.x + row->slider_thumb.width,
        "app row slider is before thumb edge");
    expect_near(
        row->slider_level_line.width,
        row->slider_full_range_line.width * model.sessions.sessions[1].level,
        0.001f,
        "app row level line follows session volume");
}

static void test_collapsed_layout_hides_app_volume_panel(void)
{
    reach_quick_settings_model model = test_model_with_sessions(2);
    model.expanded = 0;

    reach_quick_settings_layout layout = test_layout_for_model(&model);

    expect_true(layout.app_volume_row_count == 0, "collapsed layout hides app rows");
    expect_near(layout.app_volumes_panel.height, 0.0f, 0.001f, "collapsed layout hides app panel");
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

    float x = layout.app_volume_rows[1].slider_full_range_line.x +
        layout.app_volume_rows[1].slider_full_range_line.width * 0.75f;
    float y = layout.app_volume_rows[1].bounds.y +
        layout.app_volume_rows[1].bounds.height * 0.5f;

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
    expect_true(layout.app_volume_row_count == REACH_AUDIO_VOLUME_MAX_SESSIONS, "layout caps app volume rows");
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

static void test_expanded_render_emits_app_volume_panel_rows(void)
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
    expect_true(commands.count >= 27, "expanded render emits master, output button, panel rows, and collapse commands");
    expect_true(
        commands.commands[9].type == REACH_RENDER_COMMAND_TEXT &&
            text_equals_ascii(commands.commands[9].text, "App volumes"),
        "expanded render emits app volumes title");
    expect_true(
        commands.commands[10].type == REACH_RENDER_COMMAND_RECT,
        "expanded render emits app volumes panel background");
    expect_color_equal(
        commands.commands[10].color,
        theme.quick_settings_expand_button_color,
        "app volumes panel uses expand button color");
    expect_true(
        commands.commands[11].type == REACH_RENDER_COMMAND_ICON &&
            commands.commands[11].icon_id == 9000,
        "first app row starts with app icon");
    expect_true(
        commands.commands[12].type == REACH_RENDER_COMMAND_TEXT &&
            text_equals_ascii(commands.commands[12].text, "App A"),
        "first app row label strips exe suffix");
    expect_true(
        commands.commands[13].type == REACH_RENDER_COMMAND_RECT &&
            commands.commands[13].rect.height < 4.0f,
        "app row uses compact slider track");
    expect_true(
        commands.commands[16].type == REACH_RENDER_COMMAND_TEXT &&
            text_equals_ascii(commands.commands[16].text, "25%"),
        "first app row renders percentage");
    expect_true(
        commands.commands[18].type == REACH_RENDER_COMMAND_ICON &&
            commands.commands[18].icon_id == 9001,
        "second app row follows first compact row");
    expect_true(
        commands.commands[19].type == REACH_RENDER_COMMAND_TEXT &&
            text_equals_ascii(commands.commands[19].text, "App B"),
        "second app row label strips exe suffix");
    expect_true(
        commands.commands[23].type == REACH_RENDER_COMMAND_TEXT &&
            text_equals_ascii(commands.commands[23].text, "30%"),
        "second app row renders percentage");
    expect_true(
        commands.commands[25].type == REACH_RENDER_COMMAND_TEXT &&
            text_equals_ascii(commands.commands[25].text, "Hide app volumes"),
        "expanded render keeps collapse row label inside panel");
    expect_true(
        commands.commands[26].type == REACH_RENDER_COMMAND_VECTOR_ICON &&
            commands.commands[26].icon_id == REACH_VECTOR_ICON_ARROW_UP,
        "expanded collapse row uses arrow up icon");
}

static void test_expanded_output_device_render_emits_checkmark(void)
{
    reach_theme theme = test_theme();
    reach_quick_settings_model model = test_model_with_output_devices(2);
    model.output_devices_expanded = 1;

    reach_quick_settings_render_input input = {};
    input.model = model;
    input.layout = test_layout_for_model(&model);
    input.theme = theme;

    reach_render_command_buffer commands = {};
    reach_result result =
        reach_quick_settings_build_render_commands(&input, &commands);

    expect_true(result == REACH_OK, "expanded output render succeeds");
    expect_true(commands.count >= 16, "expanded output render emits device panel");
    expect_true(
        commands.commands[4].type == REACH_RENDER_COMMAND_TEXT &&
            text_equals_ascii(commands.commands[4].text, "Output devices"),
        "expanded output title replaces output button");
    expect_true(
        commands.commands[5].type == REACH_RENDER_COMMAND_VECTOR_ICON &&
            commands.commands[5].icon_id == REACH_VECTOR_ICON_ARROW_UP,
        "expanded output title shows close affordance");
    expect_true(
        commands.commands[6].type == REACH_RENDER_COMMAND_RECT,
        "expanded output render emits device panel background");
    expect_true(
        commands.commands[7].type == REACH_RENDER_COMMAND_ICON &&
            commands.commands[7].icon_id == 7000,
        "first output row renders device icon");
    expect_true(
        commands.commands[9].type != REACH_RENDER_COMMAND_VECTOR_ICON ||
            commands.commands[9].icon_id != REACH_VECTOR_ICON_CHECK,
        "non-default output row does not render checkmark");
    expect_true(
        commands.commands[10].type == REACH_RENDER_COMMAND_ICON &&
            commands.commands[10].icon_id == 7001,
        "default output row renders device icon");
    expect_true(
        commands.commands[12].type == REACH_RENDER_COMMAND_VECTOR_ICON &&
            commands.commands[12].icon_id == REACH_VECTOR_ICON_CHECK,
        "default output row renders checkmark");
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
        "fifth command is output device button background");
    expect_near(
        commands.commands[4].radius,
        reach_popup_radius(),
        0.001f,
        "output device button uses popup corner radius");
    expect_true(
        commands.commands[8].type == REACH_RENDER_COMMAND_VECTOR_ICON &&
            commands.commands[8].icon_id == REACH_VECTOR_ICON_ARROW_DOWN,
        "output device button uses arrow down icon");

    expect_true(
        commands.commands[9].type == REACH_RENDER_COMMAND_RECT,
        "ninth command is expand button background");
    expect_near(
        commands.commands[9].radius,
        reach_popup_radius(),
        0.001f,
        "expand button uses popup corner radius");

    expect_true(
        commands.commands[11].type == REACH_RENDER_COMMAND_VECTOR_ICON &&
            commands.commands[11].icon_id == REACH_VECTOR_ICON_ARROW_DOWN,
        "expand button uses arrow down icon");
    expect_color_equal(
        commands.commands[11].color,
        theme.icon_backplate_background,
        "expand icon uses app backplate color");
}

int main(void)
{
    test_model_clamps_volume();
    test_layout_places_volume_pill_above_output_device_and_expand();
    test_slider_hit_maps_to_volume();
    test_slider_fill_width_follows_volume();
    test_hit_outside_returns_none();
    test_expand_button_hit_maps_to_expand_action();
    test_output_device_button_hit_maps_to_toggle_action();
    test_output_device_title_chevron_hit_maps_to_toggle_action();
    test_slider_hit_maps_to_set_volume_action();
    test_expanded_output_device_layout_shows_panel();
    test_output_device_row_hit_maps_to_set_device_action();
    test_expanded_layout_shows_app_volume_panel();
    test_app_volume_row_layout_is_compact();
    test_collapsed_layout_hides_app_volume_panel();
    test_session_slider_hit_maps_to_session_action();
    test_session_list_cap_is_respected();
    test_volume_icon_selection();
    test_render_emits_volume_pill_and_expand_commands();
    test_expanded_output_device_render_emits_checkmark();
    test_expanded_render_emits_app_volume_panel_rows();

    if (g_failures != 0) {
        printf("%d quick settings feature test(s) failed.\n", g_failures);
        return 1;
    }

    printf("quick settings feature tests passed.\n");
    return 0;
}
