#include "reach/features/battery.h"

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
    float difference = actual - expected;
    if (difference < 0.0f)
    {
        difference = -difference;
    }
    if (difference > tolerance)
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

static void test_percent_formatting(void)
{
    uint16_t text[8] = {};

    reach_battery_format_percent(text, 8, 72);
    expect_true(text_equals_ascii(text, "72%"), "two digit percent formats with a suffix");

    reach_battery_format_percent(text, 8, 100);
    expect_true(text_equals_ascii(text, "100%"), "full charge formats as three digits");

    reach_battery_format_percent(text, 8, 0);
    expect_true(text_equals_ascii(text, "0%"), "empty battery formats as zero");

    reach_battery_format_percent(text, 8, 7);
    expect_true(text_equals_ascii(text, "7%"), "single digit percent has no padding");

    reach_battery_format_percent(text, 8, -5);
    expect_true(text_equals_ascii(text, "0%"), "negative percent clamps to zero");

    reach_battery_format_percent(text, 8, 250);
    expect_true(text_equals_ascii(text, "100%"), "percent above full clamps to one hundred");
}

static void test_saver_effective_state(void)
{
    reach_battery_model model = {};

    model.saver_on = 0;
    model.saver_pending = 0;
    expect_true(reach_battery_model_saver_effective(&model) == 0,
                "no pending request reports the system state");

    model.saver_on = 1;
    expect_true(reach_battery_model_saver_effective(&model) == 1,
                "system state shows through when nothing is pending");

    model.saver_pending = 1;
    model.saver_pending_enabled = 0;
    expect_true(reach_battery_model_saver_effective(&model) == 0,
                "a pending disable wins over the system state");

    model.saver_pending_enabled = 1;
    model.saver_on = 0;
    expect_true(reach_battery_model_saver_effective(&model) == 1,
                "a pending enable wins over the system state");

    expect_true(reach_battery_model_saver_effective(nullptr) == 0,
                "a missing model reports saver off");
}

static void test_pending_reconciles_with_system(void)
{
    reach_battery *battery = nullptr;
    expect_true(reach_battery_create(&battery) == REACH_OK, "battery capsule is created");

    reach_battery_set_saver_pending(battery, 1, 1);
    expect_true(reach_battery_saver_pending(battery), "requesting saver marks the model pending");

    (void)reach_battery_set_power(battery, 55, 0);
    expect_true(reach_battery_saver_pending(battery),
                "pending survives a snapshot that still disagrees");
    expect_true(
        reach_battery_model_saver_effective(&reach_battery_state_ptr(battery)->model) == 1,
        "the requested state keeps showing while pending");

    (void)reach_battery_set_power(battery, 55, 1);
    expect_true(!reach_battery_saver_pending(battery),
                "pending clears once the system agrees with the request");
    expect_true(
        reach_battery_model_saver_effective(&reach_battery_state_ptr(battery)->model) == 1,
        "the system state carries the value after reconciling");

    reach_battery_destroy(battery);
}

static void test_pending_expires(void)
{
    reach_battery *battery = nullptr;
    expect_true(reach_battery_create(&battery) == REACH_OK, "battery capsule is created");

    (void)reach_battery_set_power(battery, 40, 0);
    reach_battery_set_saver_pending(battery, 1, 1);

    const reach_feature_capsule_ops *ops = reach_battery_capsule_ops();
    reach_feature_tick_result tick = {};
    ops->tick(battery, 0.5, &tick);
    expect_true(reach_battery_saver_pending(battery), "pending holds inside the grace window");
    expect_true(tick.request_update, "a pending request keeps asking for updates");

    tick = {};
    ops->tick(battery, 2.0, &tick);
    expect_true(!reach_battery_saver_pending(battery),
                "pending expires when the system never agrees");
    expect_true(tick.redraw, "expiring the request asks for a redraw");
    expect_true(
        reach_battery_model_saver_effective(&reach_battery_state_ptr(battery)->model) == 0,
        "the model falls back to the system state after expiry");

    reach_battery_destroy(battery);
}

static void test_power_values_clamp(void)
{
    reach_battery *battery = nullptr;
    expect_true(reach_battery_create(&battery) == REACH_OK, "battery capsule is created");

    expect_true(reach_battery_set_power(battery, 140, 0), "a new reading reports a change");
    expect_true(reach_battery_state_ptr(battery)->model.percent == 100,
                "percent above full clamps");

    expect_true(!reach_battery_set_power(battery, 140, 0),
                "an identical reading reports no change");

    (void)reach_battery_set_power(battery, -20, 0);
    expect_true(reach_battery_state_ptr(battery)->model.percent == 0,
                "negative percent clamps to zero");

    reach_battery_destroy(battery);
}

static void test_content_stays_in_surface_space(void)
{
    reach_battery *battery = nullptr;
    expect_true(reach_battery_create(&battery) == REACH_OK, "battery capsule is created");

    reach_battery_open_context ctx = {};
    ctx.theme = reach_theme_default();
    ctx.monitor = {0.0f, 0.0f, 1920.0f, 1080.0f};
    ctx.anchor_button = {1600.0f, 4.0f, 40.0f, 26.0f};
    ctx.bar_edge_y = 38.0f;
    ctx.dpi_scale = 1.0f;
    ctx.drop_direction = REACH_POPUP_DROP_DOWN;
    reach_battery_open(battery, &ctx);

    const reach_battery_state *state = reach_battery_state_ptr(battery);
    expect_true(state->open, "opening the popup marks it open");
    expect_true(state->bounds.x > 1000.0f, "the popup is placed near its anchor on screen");

    const reach_rect_f32 rows[4] = {state->percent_label, state->separator, state->saver_row,
                                    state->saver_toggle};
    for (size_t index = 0; index < 4; ++index)
    {
        expect_true(rows[index].x >= 0.0f && rows[index].y >= 0.0f,
                    "content rects start inside the surface, not at a screen offset");
        expect_true(rows[index].x + rows[index].width <= state->bounds.width + 0.5f,
                    "content rects stay within the surface width");
        expect_true(rows[index].y + rows[index].height <= state->bounds.height + 0.5f,
                    "content rects stay within the surface height");
    }

    float row_center_x = state->bounds.x + state->saver_row.x + state->saver_row.width * 0.5f;
    float row_center_y = state->bounds.y + state->saver_row.y + state->saver_row.height * 0.5f;
    expect_true(reach_battery_hit_test(state, (int32_t)row_center_x, (int32_t)row_center_y) ==
                    REACH_BATTERY_POINTER_ACTION_TOGGLE_SAVER,
                "a screen press over the saver row reaches the toggle");

    float narrow_width = state->bounds.width;
    float content_width = state->percent_label.width;
    reach_theme wide_border_theme = *reach_theme_default();
    wide_border_theme.border_thickness = 3.0f;
    ctx.theme = &wide_border_theme;
    reach_battery_open(battery, &ctx);
    state = reach_battery_state_ptr(battery);
    expect_near(state->bounds.width - narrow_width, 4.0f, 0.001f,
                "battery popup outer width derives both border sides");
    expect_near(state->percent_label.width, content_width, 0.001f,
                "battery popup content width stays stable across border widths");
    expect_near(state->percent_label.x - wide_border_theme.border_thickness, 12.0f, 0.001f,
                "battery row inset starts inside the runtime border");

    reach_battery_destroy(battery);
}

static void test_hit_test_maps_regions(void)
{
    reach_battery_state state = {};
    state.open = 1;
    state.bounds = {100.0f, 40.0f, 200.0f, 90.0f};
    state.saver_row = {10.0f, 50.0f, 180.0f, 30.0f};

    expect_true(reach_battery_hit_test(&state, 150, 100) ==
                    REACH_BATTERY_POINTER_ACTION_TOGGLE_SAVER,
                "a press on the saver row toggles saver");
    expect_true(reach_battery_hit_test(&state, 150, 60) == REACH_BATTERY_POINTER_ACTION_NONE,
                "a press elsewhere inside the popup does nothing");
    expect_true(reach_battery_hit_test(&state, 10, 10) == REACH_BATTERY_POINTER_ACTION_DISMISS,
                "a press outside the popup dismisses it");

    state.open = 0;
    expect_true(reach_battery_hit_test(&state, 150, 100) == REACH_BATTERY_POINTER_ACTION_NONE,
                "a closed popup ignores presses");
    expect_true(reach_battery_hit_test(nullptr, 0, 0) == REACH_BATTERY_POINTER_ACTION_NONE,
                "a missing state ignores presses");
}

int main(void)
{
    test_percent_formatting();
    test_saver_effective_state();
    test_pending_reconciles_with_system();
    test_pending_expires();
    test_power_values_clamp();
    test_content_stays_in_surface_space();
    test_hit_test_maps_regions();
    return failures == 0 ? 0 : 1;
}
