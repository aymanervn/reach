#include "reach/features/common/level_presentation.h"
#include "reach/features/system_hud.h"

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

static void expect_near(float actual, float expected, float tolerance, const char *message)
{
    if (fabsf(actual - expected) > tolerance)
    {
        ++failures;
        fprintf(stderr, "FAILED: %s (expected %.3f, got %.3f)\n", message, expected, actual);
    }
}

static int text_equals_ascii(const uint16_t *text, const char *expected)
{
    size_t index = 0;
    while (expected[index] != 0)
    {
        if (text[index] != (uint16_t)(unsigned char)expected[index])
        {
            return 0;
        }
        ++index;
    }
    return text[index] == 0;
}

static void test_level_presentation(void)
{
    expect_near(reach_level_clamp01(-0.5f), 0.0f, 0.001f, "levels clamp below zero");
    expect_near(reach_level_clamp01(1.5f), 1.0f, 0.001f, "levels clamp above one");
    expect_true(reach_volume_level_icon(0.8f, 0) == REACH_VECTOR_ICON_VOLUME_HIGH,
                "high volume uses the high-volume glyph");
    expect_true(reach_volume_level_icon(0.8f, 1) == REACH_VECTOR_ICON_VOLUME_ZERO,
                "muted volume uses the muted glyph");

    uint16_t text[8] = {};
    reach_level_format_percent(text, 8, 0.56f);
    expect_true(text_equals_ascii(text, "56%"), "level percentages use a concise suffix");
}

static void test_payloads_and_repeat_policy(void)
{
    reach_system_hud *hud = nullptr;
    expect_true(reach_system_hud_create(&hud) == REACH_OK, "system HUD is created");

    reach_audio_volume_state volume = {};
    volume.level = 0.42f;
    volume.muted = 1;
    reach_system_hud_show_volume(hud, &volume);

    const reach_system_hud_state *state = reach_system_hud_state_ptr(hud);
    expect_true(state->open, "showing volume opens the HUD");
    expect_true(state->kind == REACH_SYSTEM_HUD_VOLUME, "volume selects the volume design");
    expect_near(state->volume.level, 0.42f, 0.001f, "the exact applied volume is retained");
    expect_true(state->volume.muted, "the exact applied mute state is retained");

    reach_feature_tick_result tick = {};
    reach_system_hud_capsule_ops()->tick(hud, 0.5, &tick);
    expect_true(state->visible_seconds >= 0.5, "the dismissal dwell advances while unhovered");

    volume.level = 0.67f;
    volume.muted = 0;
    reach_system_hud_show_volume(hud, &volume);
    expect_near((float)state->visible_seconds, 0.0f, 0.001f,
                "a repeated key press restarts the dismissal dwell");
    expect_near(state->volume.level, 0.67f, 0.001f,
                "a repeated key press replaces the displayed value");

    reach_brightness_state unavailable = {};
    reach_system_hud_show_brightness(hud, &unavailable);
    expect_true(state->kind == REACH_SYSTEM_HUD_VOLUME,
                "unavailable brightness does not replace an active HUD");

    reach_system_hud_show_media(hud, REACH_NOW_PLAYING_ACTION_NEXT);
    expect_true(state->kind == REACH_SYSTEM_HUD_MEDIA, "media selects the media design");
    expect_true(state->media_action == REACH_NOW_PLAYING_ACTION_NEXT,
                "the keyboard media action is retained for presentation");

    reach_system_hud_destroy(hud);
}

static void test_layout_and_blocking_input(void)
{
    reach_system_hud *hud = nullptr;
    expect_true(reach_system_hud_create(&hud) == REACH_OK, "system HUD is created");

    reach_brightness_state brightness = {};
    brightness.available = 1;
    brightness.level = 0.75f;
    reach_system_hud_show_brightness(hud, &brightness);

    reach_system_hud_arrange_context arrange = {};
    arrange.theme = reach_theme_default();
    arrange.monitor_bounds = {0.0f, 0.0f, 1920.0f, 1080.0f};
    arrange.dock_shown_bounds = {710.0f, 1000.0f, 500.0f, 64.0f};
    arrange.dpi_scale = 1.0f;
    expect_true(reach_system_hud_arrange(hud, &arrange), "initial arrangement changes layout");

    const reach_system_hud_state *state = reach_system_hud_state_ptr(hud);
    expect_near(state->layout.bounds.x + state->layout.bounds.width * 0.5f, 960.0f, 0.001f,
                "the HUD is centered on its monitor");
    expect_near(state->layout.bounds.y + state->layout.bounds.height, 988.0f, 0.001f,
                "the HUD sits twelve pixels above the shown Dock position");
    expect_near(state->layout.fill.width, state->layout.track.width * 0.75f, 0.001f,
                "the progress fill reflects the exact brightness");

    const reach_feature_capsule_ops *ops = reach_system_hud_capsule_ops();
    reach_rect_f32 region = {};
    expect_true(ops->input_regions(hud, &region, 1) == 1,
                "the visible card contributes one blocking input region");
    expect_near(region.width, state->layout.bounds.width, 0.001f,
                "the blocking region covers the visual card width");

    reach_pointer_event event = {};
    event.kind = REACH_POINTER_EVENT_DOWN;
    reach_capsule_pointer_result pointer = {};
    ops->handle_pointer(hud, &event, &pointer);
    expect_true(pointer.handled, "presses on the HUD are consumed");
    expect_true(pointer.action.kind == 0 && !pointer.capture,
                "consumed presses create no action or pointer capture");

    event.kind = REACH_POINTER_EVENT_MOVE;
    ops->handle_pointer(hud, &event, &pointer);
    expect_true(state->hovered, "hovering holds the HUD on screen");
    double held_seconds = state->visible_seconds;
    reach_feature_tick_result tick = {};
    ops->tick(hud, 2.0, &tick);
    expect_true(state->visible_seconds == held_seconds,
                "the dismissal dwell pauses for the entire hover");

    event.kind = REACH_POINTER_EVENT_LEAVE;
    ops->handle_pointer(hud, &event, &pointer);
    expect_true(!state->hovered, "leaving releases the hover hold");

    reach_system_hud_destroy(hud);
}

int main(void)
{
    test_level_presentation();
    test_payloads_and_repeat_policy();
    test_layout_and_blocking_input();
    return failures == 0 ? 0 : 1;
}
