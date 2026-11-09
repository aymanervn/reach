#include "reach/features/common/layout.h"
#include "reach/core/ui_layout.h"
#include "reach/core/render_commands.h"
#include "reach/features/common/popup.h"

#include <math.h>
#include <stdio.h>
#include <initializer_list>
#include <memory>

static int failures;

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        ++failures;
        fprintf(stderr, "FAILED: %s\n", message);
    }
}

static void expect_int(int32_t actual, int32_t expected, const char *message)
{
    if (actual != expected)
    {
        ++failures;
        fprintf(stderr, "FAILED: %s (expected %d, got %d)\n", message, (int)expected, (int)actual);
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

static void test_dock_reserves_side_safe_margins(void)
{
    reach_dock_model dock = {};
    reach_dock_model_defaults(&dock);
    reach_ui_layout_input input = {};
    input.work_area = {100.0f, 50.0f, 1000.0f, 700.0f};
    input.dpi_scale = 1.5f;
    reach_dock_layout layout = {};

    expect_true(reach_dock_layout_compute(&dock, &input, &layout) == REACH_OK,
                "Dock placement accepts a valid work area");
    expect_true(REACH_DOCK_SIDE_MARGIN_DP > REACH_DOCK_BOTTOM_MARGIN_DP,
                "Dock side safe margins exceed its bottom margin");
    expect_near(layout.available_width, 904.0f, 0.001f,
                "Dock fitting width excludes both DPI-scaled side margins");
    expect_near(layout.bounds.y + layout.bounds.height, 732.0f, 0.001f,
                "native Dock bounds preserve the DPI-scaled bottom margin");

    input.work_area.width = 40.0f;
    expect_true(reach_dock_layout_compute(&dock, &input, &layout) == REACH_OK,
                "Dock placement accepts a work area narrower than both side margins");
    expect_near(layout.available_width, 1.0f, 0.001f,
                "an extremely narrow work area retains a positive fitting width");
}

static void test_launcher_border_wraps_stable_content_geometry(void)
{
    reach_launcher_model launcher = {};
    launcher.result_count = 1;
    reach_ui_layout_input input = {};
    input.monitor_bounds = {0.0f, 0.0f, 1920.0f, 1080.0f};
    input.dpi_scale = 1.0f;
    input.border_thickness = 1.0f;
    reach_launcher_layout narrow_border = {};
    expect_true(reach_launcher_layout_compute(&launcher, &input, &narrow_border) == REACH_OK,
                "launcher layout accepts a runtime border thickness");
    expect_near(narrow_border.search_box.width, 642.0f, 0.001f,
                "launcher outer width includes both 1dp border sides");
    expect_near(narrow_border.bounds.height, 126.0f, 0.001f,
                "launcher outer height includes both 1dp border sides");
    expect_near(narrow_border.envelope_bounds.height,
                70.0f + 56.0f * (float)REACH_SEARCH_VISIBLE_RESULTS, 0.001f,
                "launcher reserves one stable maximum-results envelope");

    input.border_thickness = 3.0f;
    reach_launcher_layout wide_border = {};
    expect_true(reach_launcher_layout_compute(&launcher, &input, &wide_border) == REACH_OK,
                "launcher layout accepts a wider runtime border");
    expect_near(wide_border.search_box.width, 646.0f, 0.001f,
                "launcher outer width derives arbitrary border thickness");
    expect_near(wide_border.bounds.height, 130.0f, 0.001f,
                "launcher outer height derives arbitrary border thickness");
    expect_near(wide_border.envelope_bounds.height,
                74.0f + 56.0f * (float)REACH_SEARCH_VISIBLE_RESULTS, 0.001f,
                "launcher envelope includes arbitrary border thickness");
    expect_near(wide_border.search_text_input.x, narrow_border.search_text_input.x, 0.001f,
                "launcher text content stays fixed while the outer border grows");
    expect_near(wide_border.search_result_items.x, narrow_border.search_result_items.x, 0.001f,
                "launcher result content stays fixed while the outer border grows");
}

static const reach_layout_entry *entry_for(const reach_layout_plan *plan,
                                           reach_layout_participant participant)
{
    for (size_t index = 0; index < plan->count; ++index)
    {
        if (plan->entries[index].participant == participant)
        {
            return &plan->entries[index];
        }
    }
    return nullptr;
}

static size_t position_of(const reach_layout_plan *plan, reach_layout_participant participant)
{
    for (size_t index = 0; index < plan->count; ++index)
    {
        if (plan->entries[index].participant == participant)
        {
            return index;
        }
    }
    return plan->count;
}

static void test_resolve_orders_descending_by_layer(void)
{
    reach_layout layout = {};
    reach_layout_participant low = 0;
    reach_layout_participant high = 0;
    reach_layout_participant middle = 0;
    reach_layout_register(&layout, 100, &low);
    reach_layout_register(&layout, 200, &high);
    reach_layout_register(&layout, 150, &middle);

    reach_layout_plan plan = {};
    reach_layout_resolve(&layout, &plan);

    expect_int((int32_t)plan.count, 3, "every participant is emitted");
    expect_true(plan.entries[0].participant == high, "highest layer is emitted first");
    expect_true(plan.entries[1].participant == middle, "middle layer is emitted second");
    expect_true(plan.entries[2].participant == low, "lowest layer is emitted last");
    expect_int(plan.entries[0].visible, 1, "participants are visible by default");
}

static void test_override_precedence(void)
{
    reach_layout layout = {};
    reach_layout_participant bar = 0;
    reach_layout_register(&layout, 0, &bar);
    reach_layout_register_override(&layout, bar, REACH_LAYOUT_CONDITION_BARS_HELD, 130);
    reach_layout_register_override(&layout, bar, REACH_LAYOUT_CONDITION_BARS_FORCED, 90);

    reach_layout_plan plan = {};
    reach_layout_resolve(&layout, &plan);
    expect_int(plan.entries[0].layer, 0, "no active condition leaves the base layer");

    reach_layout_set_condition(&layout, REACH_LAYOUT_CONDITION_BARS_FORCED, 1);
    reach_layout_resolve(&layout, &plan);
    expect_int(plan.entries[0].layer, 90, "a single override replaces the base layer");

    reach_layout_set_layer_intent(&layout, bar, 1, 120);
    reach_layout_set_condition(&layout, REACH_LAYOUT_CONDITION_BARS_HELD, 1);
    reach_layout_resolve(&layout, &plan);
    expect_int(plan.entries[0].layer, 130, "the highest matching override wins");

    reach_layout_set_condition(&layout, REACH_LAYOUT_CONDITION_BARS_HELD, 0);
    reach_layout_set_condition(&layout, REACH_LAYOUT_CONDITION_BARS_FORCED, 0);
    reach_layout_set_layer_intent(&layout, bar, 0, 120);
    reach_layout_resolve(&layout, &plan);
    expect_int(plan.entries[0].layer, 0, "clearing every condition falls back to the base layer");
}

static void test_set_condition_order_independence(void)
{
    reach_layout forward = {};
    reach_layout backward = {};
    reach_layout_participant participant = 0;

    reach_layout_register(&forward, 0, &participant);
    reach_layout_register_override(&forward, participant, REACH_LAYOUT_CONDITION_BARS_FORCED, 90);
    reach_layout_register_override(&forward, participant, REACH_LAYOUT_CONDITION_BARS_HELD, 130);
    backward = forward;

    reach_layout_set_condition(&forward, REACH_LAYOUT_CONDITION_BARS_FORCED, 1);
    reach_layout_set_condition(&forward, REACH_LAYOUT_CONDITION_BARS_HELD, 1);
    reach_layout_set_condition(&backward, REACH_LAYOUT_CONDITION_BARS_HELD, 1);
    reach_layout_set_condition(&backward, REACH_LAYOUT_CONDITION_BARS_FORCED, 1);

    reach_layout_plan forward_plan = {};
    reach_layout_plan backward_plan = {};
    reach_layout_resolve(&forward, &forward_plan);
    reach_layout_resolve(&backward, &backward_plan);
    expect_true(reach_layout_plan_equal(&forward_plan, &backward_plan),
                "condition order does not change the plan");

    reach_layout_set_condition(&forward, REACH_LAYOUT_CONDITION_BARS_HELD, 1);
    reach_layout_plan repeated = {};
    reach_layout_resolve(&forward, &repeated);
    expect_true(reach_layout_plan_equal(&forward_plan, &repeated),
                "setting an already active condition is a no-op");
}

static void test_tie_break_is_registration_order(void)
{
    reach_layout layout = {};
    reach_layout_participant first = 0;
    reach_layout_participant second = 0;
    reach_layout_participant third = 0;
    reach_layout_register(&layout, 100, &first);
    reach_layout_register(&layout, 100, &second);
    reach_layout_register(&layout, 100, &third);

    reach_layout_plan plan = {};
    reach_layout_resolve(&layout, &plan);

    expect_true(plan.entries[0].participant == first, "equal layers keep registration order");
    expect_true(plan.entries[1].participant == second, "equal layers keep registration order");
    expect_true(plan.entries[2].participant == third, "equal layers keep registration order");

    reach_layout_plan again = {};
    reach_layout_resolve(&layout, &again);
    expect_true(reach_layout_plan_equal(&plan, &again), "resolve is stable across calls");
}

static void test_hidden_participants(void)
{
    reach_layout layout = {};
    reach_layout_participant above = 0;
    reach_layout_participant hidden = 0;
    reach_layout_participant below = 0;
    reach_layout_register(&layout, 200, &above);
    reach_layout_register(&layout, 150, &hidden);
    reach_layout_register(&layout, 100, &below);
    reach_layout_register_visibility(&layout, hidden, REACH_LAYOUT_CONDITION_GAME_MODE, 0);

    reach_layout_set_visible(&layout, hidden, 0);
    reach_layout_plan plan = {};
    reach_layout_resolve(&layout, &plan);

    const reach_layout_entry *entry = entry_for(&plan, hidden);
    expect_true(entry != nullptr, "a hidden participant is still emitted");
    expect_int(entry->visible, 0, "intent alone can hide a participant");
    expect_int(entry->layer, 150, "a hidden participant keeps its layer");
    expect_true(position_of(&plan, above) < position_of(&plan, below),
                "a hidden participant does not reorder the visible ones");

    reach_layout_set_visible(&layout, hidden, 1);
    reach_layout_set_condition(&layout, REACH_LAYOUT_CONDITION_GAME_MODE, 1);
    reach_layout_resolve(&layout, &plan);
    expect_int(entry_for(&plan, hidden)->visible, 0, "an override hides against the intent");

    reach_layout_register_visibility(&layout, hidden, REACH_LAYOUT_CONDITION_BARS_HELD, 1);
    reach_layout_set_condition(&layout, REACH_LAYOUT_CONDITION_BARS_HELD, 1);
    reach_layout_resolve(&layout, &plan);
    expect_int(entry_for(&plan, hidden)->visible, 0, "hidden wins over a conflicting override");

    reach_layout_set_condition(&layout, REACH_LAYOUT_CONDITION_GAME_MODE, 0);
    reach_layout_set_visible(&layout, hidden, 0);
    reach_layout_resolve(&layout, &plan);
    expect_int(entry_for(&plan, hidden)->visible, 1, "an override replaces the intent");
}

static void test_layer_zero_resolves_as_exit(void)
{
    reach_layout layout = {};
    reach_layout_participant resting = 0;
    reach_layout_participant banded = 0;
    reach_layout_register(&layout, 0, &resting);
    reach_layout_register(&layout, 50, &banded);
    reach_layout_plan plan = {};
    reach_layout_resolve(&layout, &plan);
    expect_true(plan.entries[0].participant == banded, "a banded participant outranks layer 0");
    expect_int(entry_for(&plan, resting)->layer, 0, "layer 0 is reported, not skipped");

    reach_layout_set_layer_intent(&layout, resting, 1, 130);
    reach_layout_resolve(&layout, &plan);
    expect_true(plan.entries[0].participant == resting,
                "the promoted participant rises above the band");

    reach_layout_set_layer_intent(&layout, resting, 0, 130);
    reach_layout_resolve(&layout, &plan);
    expect_int(entry_for(&plan, resting)->layer, 0, "the participant falls back on its own");
}

static void test_plan_equality_detects_changes(void)
{
    reach_layout layout = {};
    reach_layout_participant participant = 0;
    reach_layout_register(&layout, 100, &participant);
    reach_layout_register_override(&layout, participant, REACH_LAYOUT_CONDITION_BARS_HELD, 130);

    reach_layout_plan before = {};
    reach_layout_plan after = {};
    reach_layout_resolve(&layout, &before);
    reach_layout_set_condition(&layout, REACH_LAYOUT_CONDITION_BARS_HELD, 1);
    reach_layout_resolve(&layout, &after);

    expect_true(!reach_layout_plan_equal(&before, &after), "a layer change makes plans differ");

    reach_layout_set_visible(&layout, participant, 0);
    reach_layout_plan hidden = {};
    reach_layout_resolve(&layout, &hidden);
    expect_true(!reach_layout_plan_equal(&after, &hidden),
                "a visibility change makes plans differ");
}

static void test_registration_limits(void)
{
    reach_layout layout = {};
    reach_layout_participant participant = 0;
    for (size_t index = 0; index < REACH_LAYOUT_MAX_PARTICIPANTS; ++index)
    {
        expect_true(reach_layout_register(&layout, (int32_t)index, &participant) == REACH_OK,
                    "registration succeeds up to the capacity");
    }
    expect_true(reach_layout_register(&layout, 0, &participant) != REACH_OK,
                "registration past the capacity fails");
    expect_true(reach_layout_register_override(&layout, REACH_LAYOUT_MAX_PARTICIPANTS,
                                               REACH_LAYOUT_CONDITION_GAME_MODE, 10) != REACH_OK,
                "an unknown participant is rejected");
}

static void test_animated_clip_samples_absolute_geometry(void)
{
    reach_render_group group = {};
    group.has_clip = 1;
    group.clip = {100.0f, 200.0f, 80.0f, 40.0f};
    reach_motion_set(&group.clip_insets[REACH_RENDER_CLIP_LEFT], 10.0f);
    reach_motion_set(&group.clip_insets[REACH_RENDER_CLIP_TOP], 2.0f);
    reach_motion_set(&group.clip_insets[REACH_RENDER_CLIP_BOTTOM], 5.0f);
    group.clip_insets[REACH_RENDER_CLIP_RIGHT] = {0.0f, 60.0f, 10.0, 2.0, REACH_EASING_EASE_IN_OUT,
                                                  1};
    reach_rect_f32 middle = reach_render_group_clip_at(&group, 11.0);
    expect_near(middle.x, 110.0f, 0.001f, "clip samples preserve the stationary viewport origin");
    expect_near(middle.y, 202.0f, 0.001f, "clip top inset uses the same coordinate space");
    expect_near(middle.width, 40.0f, 0.001f,
                "clip width follows the canonical easing at event time");
    expect_near(middle.height, 33.0f, 0.001f,
                "clip height combines independent top and bottom insets");
    reach_rect_f32 final = reach_render_group_clip_at(&group, 14.0);
    expect_near(final.width, 10.0f, 0.001f, "late clip samples clamp to the endpoint");
    reach_motion_set(&group.clip_insets[REACH_RENDER_CLIP_RIGHT], 90.0f);
    expect_near(reach_render_group_clip_at(&group, 14.0).width, 0.0f, 0.001f,
                "crossed clip edges produce an empty viewport rather than negative width");
    group.has_clip = 0;
    expect_near(reach_render_group_clip_at(&group, 14.0).height, 0.0f, 0.001f,
                "disabled clipping ignores retained inset descriptors");
}

static void test_retained_tint_samples_one_timeline(void)
{
    reach_render_group group = {};
    group.has_tint = 1;
    reach_motion progress = {};
    reach_motion_start_at(&progress, 0.0f, 1.0f, 2.0, REACH_EASING_EASE_OUT, 10.0);
    group.tint[REACH_RENDER_COLOR_RED] = reach_motion_affine(&progress, 0.6f, 0.2f);
    group.tint[REACH_RENDER_COLOR_GREEN] = reach_motion_affine(&progress, -0.4f, 0.9f);
    group.tint[REACH_RENDER_COLOR_BLUE] = reach_motion_affine(&progress, 0.0f, 0.3f);
    group.tint[REACH_RENDER_COLOR_ALPHA] = reach_motion_affine(&progress, -0.5f, 1.0f);
    reach_color sample = reach_render_group_tint_at(&group, 11.0);
    float value = reach_motion_value_at(&progress, 11.0);
    expect_near(sample.r, 0.2f + 0.6f * value, 0.001f, "tint red derives the same progress");
    expect_near(sample.g, 0.9f - 0.4f * value, 0.001f, "tint green supports descending values");
    expect_near(sample.b, 0.3f, 0.001f, "constant tint channel remains stable");
    expect_near(sample.a, 1.0f - 0.5f * value, 0.001f, "tint preserves its alpha timeline");
    group.tint_rgb_target = {0.95f, 0.15f, 0.6f, 0};
    reach_motion weight = reach_motion_affine(&progress, 0.4f, 0);
    group.tint_rgb_mix = reach_motion_clamp(&weight, 0, 1);
    reach_color mixed = reach_render_group_tint_at(&group, 11.0);
    expect_near(mixed.r, sample.r + (0.95f - sample.r) * value * 0.4f, 0.001f,
                "nested tint preserves the product of canonical motions");
    expect_near(mixed.g, sample.g + (0.15f - sample.g) * value * 0.4f, 0.001f,
                "nested tint blends descending channels without altering coverage");
    expect_near(mixed.a, sample.a, 0.001f, "RGB mixing preserves the independent alpha timeline");
    expect_near(reach_render_group_tint_at(&group, 20.0).r, 0.86f, 0.001f,
                "late RGB mix sampling uses both final endpoints");
    group.id = 1;
    group.bounds = {0, 0, 100, 50};
    auto buffer = std::make_unique<reach_render_command_buffer>();
    reach_render_command_buffer_clear(buffer.get());
    expect_true(reach_render_command_buffer_begin_group(buffer.get(), &group) == REACH_OK,
                "normalized RGB mix intent is accepted before native mutation");
    reach_render_command_buffer_end_group(buffer.get());
    reach_render_command_buffer_clear(buffer.get());
    group.tint_rgb_mix.maximum = 2;
    expect_true(reach_render_command_buffer_begin_group(buffer.get(), &group) ==
                    REACH_INVALID_ARGUMENT,
                "out-of-range RGB mixing is rejected before native mutation");
    group.tint_rgb_mix.maximum = 1;
    group.has_tint = 0;
    expect_near(reach_render_group_tint_at(&group, 11.0).a, 1.0f, 0.001f,
                "disabled tint does not change content");
}

static void test_focal_stretch_geometry_preserves_fixed_features(void)
{
    expect_true(reach_render_stretch_patch_count(nullptr) == 0,
                "absent stretch descriptors expose no patch allocation");
    auto buffer = std::make_unique<reach_render_command_buffer>();
    reach_render_group absent_source = {};
    reach_render_command absent_command = {};
    expect_true(reach_render_push_stretched_chrome(buffer.get(), &absent_source, nullptr,
                                                   &absent_command, 1) == REACH_INVALID_ARGUMENT,
                "absent descriptor batches reject before dereferencing geometry");
    for (float dpi : {1.0f, 1.25f, 1.5f, 2.0f})
        for (int reverse = 0; reverse < 2; ++reverse)
        {
            reach_render_group source = {};
            source.id = 400;
            source.bounds = {-12 * dpi, -8 * dpi, 600 * dpi, 120 * dpi};
            reach_render_stretch stretch = {};
            stretch.left = stretch.right = 32 * dpi;
            stretch.focal_left = 180 * dpi;
            stretch.focal_right = 260 * dpi;
            stretch.anchor_x = 0.5f;
            reach_motion_start_at(&stretch.width, (reverse ? 900 : 300) * dpi,
                                  (reverse ? 300 : 900) * dpi, 2, REACH_EASING_EASE_IN_OUT, 10);
            reach_motion_set(&stretch.height, source.bounds.height);
            expect_true(reach_motion_hinge(&stretch.width, 440 * dpi, 220 * dpi, 0.5f, 0,
                                           &stretch.focal_x) == REACH_OK &&
                            reach_render_stretch_patch_count(&stretch) == 15,
                        "fixed feature resize exposes five columns on the existing patch contract");
            for (double now : {10.0, 10.3, 10.7, 11.0, 11.5, 12.0})
            {
                float width = reach_motion_value_at(&stretch.width, now);
                float focus = fminf(width * 0.5f, 220 * dpi);
                float left = source.bounds.x - (width - source.bounds.width) * 0.5f;
                float expected[] = {left,
                                    left + stretch.left,
                                    left + focus - 40 * dpi,
                                    left + focus + 40 * dpi,
                                    left + width - stretch.right,
                                    left + width};
                for (size_t column = 0; column < 5; ++column)
                {
                    reach_render_group patch = {};
                    expect_true(reach_render_stretch_patch(&source, &stretch, 5 + column, &patch) ==
                                    REACH_OK,
                                "focus patches accept both directions across the breakpoint");
                    reach_point_f32 scale = reach_render_group_scale_at(&patch, now);
                    expect_near(patch.bounds.x + reach_motion_value_at(&patch.x, now),
                                expected[column], 0.001f,
                                "patch translation shares the anchored width timeline");
                    expect_near(patch.bounds.width * scale.x,
                                expected[column + 1] - expected[column], 0.001f,
                                "stretch shoulders meet the fixed corners and focal feature");
                    expect_near(scale.y, 1, 0.00001f, "fixed height never stretches the feature");
                    if (column == 0 || column == 2 || column == 4)
                        expect_near(scale.x, 1, 0.00001f,
                                    "corners and central feature retain their source pixels");
                }
            }
            reach_render_group patch = {};
            stretch.focal_left = stretch.left;
            expect_true(reach_render_stretch_patch(&source, &stretch, 7, &patch) ==
                            REACH_INVALID_ARGUMENT,
                        "overlapping corner and feature caps require a separate geometry decision");
        }
    reach_render_group source = {};
    source.id = 100;
    source.bounds = {0, 0, 200, 100};
    reach_render_stretch stretch = {};
    stretch.left = stretch.right = 30;
    reach_motion width = {};
    reach_motion_start_at(&width, 0, 2, 2, REACH_EASING_EASE_IN_OUT, 10);
    expect_true(reach_motion_hinge(&width, 1, 50, -150, 150, &stretch.width) == REACH_OK,
                "nonmonotonic dimension fixture");
    reach_motion_set(&stretch.height, 100);
    reach_render_group patch = {};
    expect_true(reach_render_stretch_patch(&source, &stretch, 4, &patch) == REACH_INVALID_ARGUMENT,
                "dimension validation rejects an interior compact size despite safe endpoints");
}

static void test_stretch_geometry_preserves_caps_and_anchor(void)
{
    for (float dpi : {1.0f, 1.25f, 1.5f, 2.0f})
    {
        reach_render_group source = {};
        source.id = 20;
        source.bounds = {0, 0, 120 * dpi, 100 * dpi};
        reach_render_stretch stretch = {};
        stretch.left = stretch.right = 20 * dpi;
        stretch.top = stretch.bottom = 24 * dpi;
        stretch.anchor_x = 0.5f;
        stretch.anchor_y = 1;
        reach_motion_start_at(&stretch.width, 120 * dpi, 260 * dpi, 2, REACH_EASING_EASE_IN_OUT,
                              10);
        reach_motion_start_at(&stretch.height, 100 * dpi, 200 * dpi, 2, REACH_EASING_EASE_OUT, 10);
        for (double now : {10.0, 10.5, 11.0, 12.0})
        {
            float width = reach_motion_value_at(&stretch.width, now);
            float height = reach_motion_value_at(&stretch.height, now);
            float left = -(width - source.bounds.width) * 0.5f;
            float top = -(height - source.bounds.height);
            for (size_t index = 0; index < 9; ++index)
            {
                reach_render_group patch = {};
                expect_true(reach_render_stretch_patch(&source, &stretch, index, &patch) ==
                                REACH_OK,
                            "valid stretch geometry derives every patch");
                reach_point_f32 scale = reach_render_group_scale_at(&patch, now);
                size_t column = index % 3, row = index / 3;
                float expected_x = column == 0   ? left
                                   : column == 1 ? left + stretch.left
                                                 : left + width - stretch.right;
                float expected_y = row == 0   ? top
                                   : row == 1 ? top + stretch.top
                                              : top + height - stretch.bottom;
                expect_near(patch.bounds.x + reach_motion_value_at(&patch.x, now), expected_x,
                            0.001f, "every column shares the same moving anchor");
                expect_near(patch.bounds.y + reach_motion_value_at(&patch.y, now), expected_y,
                            0.001f, "every row keeps the bottom anchor fixed");
                expect_near(patch.bounds.width * scale.x,
                            column == 0   ? stretch.left
                            : column == 2 ? stretch.right
                                          : width - stretch.left - stretch.right,
                            0.001f, "stretching preserves fixed horizontal caps");
                expect_near(patch.bounds.height * scale.y,
                            row == 0   ? stretch.top
                            : row == 2 ? stretch.bottom
                                       : height - stretch.top - stretch.bottom,
                            0.001f, "stretching preserves fixed vertical caps");
            }
        }
        reach_render_group patch = {};
        reach_motion_set(&stretch.width, source.bounds.width);
        reach_motion_set(&stretch.height, source.bounds.height);
        expect_true(reach_render_stretch_patch(&source, &stretch, 4, &patch) == REACH_OK &&
                        patch.scale.to == 1.0f && patch.scale_y.to == 1.0f,
                    "unstretched axes remain exactly unit scale at every DPI");
        reach_motion_set(&stretch.width, stretch.left + stretch.right);
        expect_true(reach_render_stretch_patch(&source, &stretch, 0, &patch) ==
                        REACH_INVALID_ARGUMENT,
                    "unsupported compact geometry is rejected instead of squeezing corners");
        reach_motion_start_at(&stretch.width, 0, 300 * dpi, 2, REACH_EASING_EASE_IN_OUT, 10);
        stretch.width = reach_motion_clamp(&stretch.width, 120 * dpi, 240 * dpi);
        reach_motion_start_at(&stretch.height, 40 * dpi, 220 * dpi, 2, REACH_EASING_EASE_OUT, 10);
        stretch.height = reach_motion_clamp(&stretch.height, 100 * dpi, 180 * dpi);
        reach_motion_set(&source.y, 50 * dpi);
        source.y = reach_motion_clamp(&source.y, 10 * dpi, 20 * dpi);
        expect_true(reach_render_stretch_patch(&source, &stretch, 4, &patch) == REACH_OK,
                    "bounded dimensions validate their visible sizes");
        for (double now : {10.0, 10.5, 11.0, 12.0})
        {
            reach_point_f32 scale = reach_render_group_scale_at(&patch, now);
            float height = reach_motion_value_at(&stretch.height, now);
            expect_near(patch.bounds.width * scale.x,
                        reach_motion_value_at(&stretch.width, now) - stretch.left - stretch.right,
                        0.001f, "bounded width scales retain their limits after normalization");
            expect_near(patch.bounds.height * scale.y, height - stretch.top - stretch.bottom,
                        0.001f, "bounded height scales retain their limits after normalization");
            expect_near(patch.bounds.y + reach_motion_value_at(&patch.y, now),
                        20 * dpi - (height - source.bounds.height) + stretch.top, 0.001f,
                        "bounded dimension placement preserves the source offset and anchor");
        }
        reach_motion_set(&stretch.width, 500 * dpi);
        stretch.width = reach_motion_clamp(&stretch.width, 120 * dpi, 120 * dpi);
        reach_motion_set(&stretch.height, 500 * dpi);
        stretch.height = reach_motion_clamp(&stretch.height, 100 * dpi, 100 * dpi);
        expect_true(reach_render_stretch_patch(&source, &stretch, 4, &patch) == REACH_OK &&
                        patch.scale.to == 1.0f && patch.scale_y.to == 1.0f,
                    "bounded constant dimensions keep exact unit scale at every DPI");
    }
    reach_render_group group = {};
    reach_motion_set(&group.scale, 2);
    expect_near(reach_render_group_scale_at(&group, 10).y, 2, 0.001f,
                "existing uniform scale still supplies both axes");
    reach_motion_set(&group.scale_y, 1);
    expect_near(reach_render_group_scale_at(&group, 10).x, 2, 0.001f,
                "independent vertical scaling preserves horizontal intent");
    expect_near(reach_render_group_scale_at(&group, 10).y, 1, 0.001f,
                "explicit vertical scale overrides uniform inheritance");
}

static void test_group_hierarchy_samples_one_geometry_timeline(void)
{
    auto storage = std::make_unique<reach_render_command_buffer>();
    reach_render_command_buffer &buffer = *storage;
    reach_render_group outer = {};
    outer.id = 1;
    outer.bounds = {10, 10, 300, 200};
    outer.has_clip = 1;
    outer.clip = {10, 10, 180, 120};
    reach_motion_set(&outer.scale, 1.2f);
    reach_motion_set(&outer.scale_y, 0.8f);
    reach_motion_set(&outer.opacity, 0.8f);
    reach_motion_start_at(&outer.y, 0, 20, 2, REACH_EASING_EASE_OUT, 10);
    expect_true(reach_render_command_buffer_begin_group(&buffer, &outer) == REACH_OK,
                "outer viewport can be declared without raster content");
    reach_render_command_buffer_end_group(&buffer);
    reach_render_group section = {};
    section.id = 2;
    section.parent_id = 1;
    section.bounds = {20, 20, 150, 100};
    section.has_clip = 1;
    section.clip = {20, 20, 100, 80};
    reach_motion_set(&section.opacity, 0.5f);
    reach_motion_start_at(&section.x, 10, 25, 2, REACH_EASING_EASE_IN_OUT, 10);
    reach_motion_start_at(&section.y, 0, 40, 2, REACH_EASING_EASE_IN_OUT, 10);
    expect_true(reach_render_command_buffer_begin_group(&buffer, &section) == REACH_OK,
                "section viewport belongs to the moving content parent");
    reach_render_command_buffer_end_group(&buffer);
    reach_render_group leaf = {};
    leaf.id = 3;
    leaf.parent_id = 2;
    leaf.bounds = {25, 25, 50, 30};
    leaf.has_clip = 1;
    leaf.clip = {25, 25, 60, 50};
    reach_motion_set(&leaf.opacity, 0.7f);
    reach_motion_start_at(&leaf.y, 8, 0, 2, REACH_EASING_EASE_OUT, 10);
    expect_true(reach_render_command_buffer_begin_group(&buffer, &leaf) == REACH_OK,
                "leaf content has motion independent of its section viewport");
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = leaf.bounds;
    expect_true(reach_render_command_buffer_push(&buffer, &command) == REACH_OK,
                "hierarchical geometry includes real command ownership");
    reach_render_command_buffer_end_group(&buffer);
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_OK,
                "ordered hierarchy is valid");
    for (double now : {10.0, 10.3, 11.0, 12.0})
    {
        float outer_y = reach_motion_value_at(&outer.y, now);
        float section_x = reach_motion_value_at(&section.x, now);
        float section_y = reach_motion_value_at(&section.y, now);
        float leaf_y = reach_motion_value_at(&leaf.y, now);
        reach_transform_f32 transform =
            reach_render_command_buffer_group_transform_at(&buffer, leaf.id, now);
        expect_near(25 * transform.scale_x + transform.offset_x, 10 + (25 + section_x - 10) * 1.2f,
                    0.001f, "nested X movement preserves each parent origin");
        expect_near(25 * transform.scale_y + transform.offset_y,
                    10 + (25 + leaf_y + section_y - 10) * 0.8f + outer_y, 0.001f,
                    "nested Y movement sums independent absolute curves before parent scaling");
        expect_near(reach_render_command_buffer_group_opacity_at(&buffer, leaf.id, now),
                    0.8f * 0.5f * 0.7f, 0.000001f,
                    "descendant opacity includes every ancestor exactly once");
        reach_rect_f32 clip = {};
        expect_true(reach_render_command_buffer_group_clip_at(&buffer, leaf.id, now, &clip),
                    "ancestor viewports contribute to the effective clip");
        float left = 10 + (25 + section_x - 10) * 1.2f;
        float top = 10 + (25 + section_y - 10) * 0.8f + outer_y;
        float right = fminf(left + 60 * 1.2f, 22 + 100 * 1.2f);
        float bottom = fminf(top + 50 * 0.8f, fminf(18 + outer_y + 80 * 0.8f, 130));
        expect_near(clip.x, left, 0.001f, "section clip follows parent movement");
        expect_near(clip.y, top, 0.001f, "content slide does not move its own viewport");
        expect_near(clip.width, right - left, 0.001f,
                    "nested clips intersect in surface coordinates");
        expect_near(clip.height, bottom - top, 0.001f,
                    "outer viewport remains fixed during reveal");
    }
    buffer.groups[2].clip_radius = 5;
    reach_rect_f32 rounded_bounds = {}, rectangular_bounds = {};
    expect_true(reach_render_command_buffer_group_clip_at(&buffer, leaf.id, 10, &rounded_bounds) &&
                    reach_render_command_buffer_group_rectangular_clip_at(&buffer, leaf.id, 10,
                                                                          &rectangular_bounds),
                "rounded viewports keep separate rectangular ancestor clips");
    expect_true(rectangular_bounds.width > rounded_bounds.width,
                "hard ancestor clipping does not truncate the rounded mask's antialiased edge");
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_OK,
                "static rounded body clips are supported");
    reach_motion_start_at(&buffer.groups[2].clip_insets[0], 0, 2, 1, REACH_EASING_EASE_IN_OUT, 10);
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_OK,
                "rounded clips accept canonical animated insets");
    reach_rect_motion clip_motion = {};
    expect_true(reach_render_group_clip_motion(&buffer.groups[2], &clip_motion) == REACH_OK,
                "rounded geometry derives the same canonical viewport as event-time sampling");
    for (double now : {10.0, 10.2, 10.5, 11.0})
    {
        reach_rect_f32 sampled = reach_render_group_clip_at(&buffer.groups[2], now);
        reach_rect_f32 derived = reach_rect_motion_value_at(&clip_motion, now);
        expect_near(derived.x, sampled.x, 0.0001f, "animated rounded clip origin agrees");
        expect_near(derived.width, sampled.width, 0.0001f, "animated rounded clip extent agrees");
    }
    reach_motion_start_at(&buffer.groups[2].clip_insets[REACH_RENDER_CLIP_RIGHT], 0, 5, 1,
                          REACH_EASING_EASE_IN_OUT, 10.1);
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_INVALID_ARGUMENT,
                "rounded axis geometry rejects incompatible inset epochs instead of approximating");
}

static void test_rounded_clip_motion_preserves_compact_axes(void)
{
    for (int easing = REACH_EASING_EASE_IN; easing <= REACH_EASING_EASE_IN_OUT; ++easing)
        for (int reverse = 0; reverse < 2; ++reverse)
        {
            reach_motion progress = {};
            reach_motion_start_at(&progress, reverse ? 1 : 0, reverse ? 0 : 1, 2,
                                  (reach_easing)easing, 10);
            reach_render_group group = {};
            group.has_clip = 1;
            group.clip = {-24, 40, 120, 40};
            group.clip_radius = 12;
            group.clip_insets[0] = reach_motion_affine(&progress, 12, 0);
            group.clip_insets[2] = reach_motion_affine(&progress, 120, 0);
            group.clip_insets[3] = reach_motion_affine(&progress, 50, 0);
            reach_motion_set(&group.clip_insets[1], 5);
            reach_rect_motion clip = {};
            expect_true(reach_render_group_clip_motion(&group, &clip) == REACH_OK,
                        "rounded geometry accepts paired insets sharing one physical epoch");
            for (int index = 0; index <= 80; ++index)
            {
                double now = 10 + index / 40.0;
                float value = reach_motion_value_at(&progress, now);
                reach_rect_f32 sampled = reach_rect_motion_value_at(&clip, now);
                expect_near(sampled.x, -24 + 12 * value, 0.0001f,
                            "rounded clip offsets preserve their viewport origin");
                expect_near(sampled.width, fmaxf(0, 120 - 132 * value), 0.0001f,
                            "rounded widths retain both insets and clamp collapsed extents");
                expect_near(sampled.height, fmaxf(0, 35 - 50 * value), 0.0001f,
                            "rounded heights preserve constant padding and independent collapse");
            }
        }
}

static void test_uniform_clip_radii_share_geometry_motion(void)
{
    for (int easing = REACH_EASING_EASE_IN; easing <= REACH_EASING_EASE_IN_OUT; ++easing)
        for (int reverse = 0; reverse < 2; ++reverse)
            for (int uniform = 0; uniform < 2; ++uniform)
            {
                reach_motion progress = {};
                reach_motion_start_at(&progress, reverse ? 1 : 0, reverse ? 0 : 1, 2,
                                      (reach_easing)easing, 10);
                reach_render_group group = {};
                group.has_clip = 1;
                group.clip = {0, 0, 120, 40};
                group.clip_radius = 12;
                group.clip_uniform_radius = uniform;
                group.clip_insets[REACH_RENDER_CLIP_RIGHT] = reach_motion_affine(&progress, 120, 0);
                group.clip_insets[REACH_RENDER_CLIP_BOTTOM] =
                    reach_motion_affine(&progress, -100, 0);
                reach_motion x = {}, y = {};
                expect_true(reach_render_group_clip_radii_motion(&group, &x, &y) == REACH_OK,
                            "clip radii derive from one canonical geometry epoch");
                for (int index = 0; index <= 80; ++index)
                {
                    double now = 10 + index / 40.0;
                    reach_rect_f32 clip = reach_render_group_clip_at(&group, now);
                    float radius_x = fminf(12, clip.width / 2);
                    float radius_y = fminf(12, clip.height / 2);
                    if (uniform)
                        radius_x = radius_y = fminf(radius_x, radius_y);
                    expect_near(reach_motion_value_at(&x, now), radius_x, 0.0001f,
                                "horizontal radius preserves selected compact semantics");
                    expect_near(reach_motion_value_at(&y, now), radius_y, 0.0001f,
                                "vertical radius preserves selected compact semantics");
                }
                reach_motion_start_at(&group.clip_insets[REACH_RENDER_CLIP_BOTTOM], 0, 30, 2,
                                      (reach_easing)easing, 10.1);
                expect_true(reach_render_group_clip_radii_motion(&group, &x, &y) ==
                                (uniform ? REACH_INVALID_ARGUMENT : REACH_OK),
                            "coupled radii reject incompatible axis epochs without approximation");
                group.clip_uniform_radius = 2;
                expect_true(reach_render_group_clip_radii_motion(&group, &x, &y) ==
                                REACH_INVALID_ARGUMENT,
                            "invalid compact radius policy is rejected");
            }
}

static void test_canonical_clip_radius_ownership(void)
{
    reach_motion progress = {};
    double started = reach_motion_now_seconds();
    reach_motion_start_at(&progress, 0, 1, .2, REACH_EASING_EASE_IN_OUT, started);
    reach_render_group group = {};
    group.id = 1;
    group.bounds = group.clip = {0, 0, 120, 40};
    group.has_clip = group.clip_uniform_radius = 1;
    group.clip_radius_motion = reach_motion_affine(&progress, 48, 0);
    group.clip_insets[REACH_RENDER_CLIP_RIGHT] = reach_motion_affine(&progress, 80, 0);
    group.clip_insets[REACH_RENDER_CLIP_BOTTOM] = reach_motion_affine(&progress, -40, 0);
    reach_motion x = {}, y = {};
    expect_true(reach_render_group_has_rounded_clip(&group) &&
                    reach_render_group_clip_radii_motion(&group, &x, &y) == REACH_OK,
                "a zero-start radius retains its canonical rounded owner");
    for (int index = 0; index <= 200; ++index)
    {
        double now = started + index / 1000.0;
        auto clip = reach_render_group_clip_at(&group, now);
        float expected = fminf(reach_motion_value_at(&group.clip_radius_motion, now),
                               fminf(clip.width, clip.height) * .5f);
        expect_near(reach_motion_value_at(&x, now), expected, .0001f,
                    "radius motion clamps to the same event-time geometry");
        expect_near(reach_motion_value_at(&y, now), expected, .0001f,
                    "uniform radius shares both canonical dimensions");
    }
    for (float dpi : {1.f, 1.25f, 1.5f, 2.f})
    {
        auto scaled = group;
        scaled.bounds = scaled.clip = {20 * dpi, 20 * dpi, 200 * dpi, 80 * dpi};
        scaled.clip_radius_motion = reach_motion_affine(&progress, 60 * dpi, 0);
        scaled.clip_insets[REACH_RENDER_CLIP_RIGHT] = reach_motion_affine(&progress, 100 * dpi, 0);
        scaled.clip_insets[REACH_RENDER_CLIP_BOTTOM] = reach_motion_affine(&progress, -20 * dpi, 0);
        float minimum = 0, maximum = 0;
        expect_true(reach_render_group_clip_radii_motion(&scaled, &x, &y) == REACH_OK &&
                        reach_motion_range(&x, &minimum, &maximum) == REACH_OK && minimum >= 0 &&
                        reach_motion_value_at(&x, started) == 0,
                    "composed native radii cannot drift below their nonnegative envelope");
        for (double phase : {0.0, .23, .5, .73, 1.0})
        {
            double now = started + phase * progress.duration_seconds;
            auto clip = reach_render_group_clip_at(&scaled, now);
            float expected = fminf(reach_motion_value_at(&scaled.clip_radius_motion, now),
                                   fminf(clip.width, clip.height) * .5f);
            auto radii = reach_render_group_clip_radii_at(&scaled, now);
            expect_true(radii.x == expected && radii.y == expected,
                        "CPU radius sampling preserves the canonical scalar arithmetic exactly");
        }
    }
    auto buffer = std::make_unique<reach_render_command_buffer>();
    reach_render_command_buffer_clear(buffer.get());
    expect_true(reach_render_command_buffer_begin_group(buffer.get(), &group) == REACH_OK &&
                    reach_render_command_buffer_motion_active(buffer.get()),
                "canonical radius participates in retained validation and fallback demand");
    reach_render_command_buffer_end_group(buffer.get());
    auto changed = group;
    changed.clip_radius_motion.started_seconds += .1;
    expect_true(!reach_render_group_equal(&group, &changed) &&
                    reach_render_group_clip_radii_motion(&changed, &x, &y) ==
                        REACH_INVALID_ARGUMENT,
                "incompatible radius epochs reject without sampling a replacement");
    changed = group;
    changed.clip_radius_motion = reach_motion_affine(&progress, -1, 0);
    size_t count = buffer->group_count;
    changed.id = 2;
    expect_true(reach_render_command_buffer_begin_group(buffer.get(), &changed) ==
                        REACH_INVALID_ARGUMENT &&
                    buffer->group_count == count,
                "negative radius ranges reject before publishing a group");
    changed = group;
    changed.id = 2;
    changed.has_clip = 0;
    expect_true(reach_render_command_buffer_begin_group(buffer.get(), &changed) ==
                        REACH_INVALID_ARGUMENT &&
                    buffer->group_count == count,
                "a radius driver requires a clip owner");
    changed = group;
    changed.clip_radius = 12;
    reach_motion_set(&changed.clip_radius_motion, 0);
    expect_true(!reach_render_group_has_rounded_clip(&changed),
                "a constant zero driver overrides legacy radius and keeps an aliased rectangle");
    reach_render_command_buffer_clear(buffer.get());
    reach_render_group source = {};
    source.id = 10;
    source.bounds = {0, 0, 120, 80};
    source.clip_radius_motion = group.clip_radius_motion;
    reach_rect_motion bounds = {};
    bounds.width = reach_motion_affine(&progress, 100, 20);
    bounds.height = reach_motion_affine(&progress, 40, 40);
    reach_color color = {1, 1, 1, 1};
    expect_true(reach_render_push_rect_motion(buffer.get(), &source, &bounds, 0, color) ==
                        REACH_OK &&
                    reach_render_command_buffer_validate_groups(buffer.get()) == REACH_OK,
                "fitted solid paint preserves its optional canonical radius");
    reach_render_command image = {};
    image.type = REACH_RENDER_COMMAND_ICON;
    image.icon_id = 1;
    source.id = 20;
    count = buffer->group_count;
    size_t commands = buffer->count;
    expect_true(reach_render_push_image_motion(buffer.get(), &source, &bounds, 0, &image) ==
                        REACH_INVALID_ARGUMENT &&
                    buffer->group_count == count && buffer->count == commands,
                "unsupported fitted image radius rejects atomically");
}

static void test_group_hierarchy_rejects_ambiguous_ownership_and_order(void)
{
    auto storage = std::make_unique<reach_render_command_buffer>();
    reach_render_command_buffer &buffer = *storage;
    reach_render_group groups[4] = {};
    for (size_t index = 0; index < 4; ++index)
    {
        groups[index].id = index + 1;
        groups[index].bounds = {0, 0, 100, 100};
        groups[index].parent_id = index == 1 || index == 2 ? 1 : 0;
        expect_true(reach_render_command_buffer_begin_group(&buffer, &groups[index]) == REACH_OK,
                    "parents precede their children in declarations");
        reach_render_command_buffer_end_group(&buffer);
    }
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = {0, 0, 20, 20};
    buffer.commands[0] = buffer.commands[1] = buffer.commands[2] = command;
    buffer.commands[0].group_id = 2;
    buffer.commands[1].group_id = 3;
    buffer.commands[2].group_id = 4;
    buffer.count = 3;
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_OK,
                "siblings form one contiguous subtree before the next root");
    buffer.commands[1].group_id = 4;
    buffer.commands[2].group_id = 3;
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_INVALID_ARGUMENT,
                "interleaved subtrees cannot silently change painter order");
    buffer.commands[1].group_id = 1;
    buffer.count = 2;
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_INVALID_ARGUMENT,
                "parent raster content cannot be painted after its descendants");
    buffer.count = 1;
    buffer.groups[0].parent_id = 2;
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_INVALID_ARGUMENT,
                "forward references and cycles are rejected before native mutation");
    buffer.groups[0].parent_id = 0;
    buffer.groups[0].has_tint = 1;
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_INVALID_ARGUMENT,
                "coverage tint cannot be inherited through a parent viewport");
    buffer.groups[0].has_tint = 0;
    buffer.commands[0].group_id = 99;
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_INVALID_ARGUMENT,
                "missing command owners are rejected on both execution paths");
}

static void test_retained_command_preflight_preserves_batch_atomicity(void)
{
    auto storage = std::make_unique<reach_render_command_buffer>();
    reach_render_command_buffer &buffer = *storage;
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = {10.5f, 20.5f, 80, 16};
    command.text_size = 14;
    reach_render_command_buffer_push(&buffer, &command);
    command.rect.x = NAN;
    reach_render_command_buffer_push(&buffer, &command);
    reach_motion opacity = {}, y = {};
    reach_motion_set(&opacity, 0.5f);
    expect_true(reach_render_retain_commands(&buffer, 0, 10, 0, &opacity, &y) ==
                        REACH_INVALID_ARGUMENT &&
                    buffer.group_count == 0 && buffer.commands[0].group_id == 0,
                "invalid source geometry cannot partially assign retained paint owners");
    buffer.commands[1].rect.x = 10.5f;
    expect_true(reach_render_retain_commands(&buffer, 0, 10, 0, &opacity, &y) == REACH_OK &&
                    buffer.group_count == 2 && buffer.commands[0].group_id == 10 &&
                    buffer.commands[1].group_id == 11,
                "overlapping commands retain independent alpha surfaces");
    const reach_rect_f32 &bounds = buffer.groups[0].bounds;
    expect_true(bounds.x == -4 && bounds.y == 6 && bounds.width == 109 && bounds.height == 45,
                "text source padding is pixel aligned and contains glyph overhang");
}

static void test_popup_width_motion_matches_constrained_placement(void)
{
    for (float dpi : {1.0f, 1.25f, 1.5f, 2.0f})
        for (int direction : {REACH_POPUP_DROP_DOWN, REACH_POPUP_DROP_UP})
            for (float owner : {0.0f, 110.0f, 180.0f, 490.0f, 810.0f, 890.0f, 1100.0f})
                for (int reverse = 0; reverse < 2; ++reverse)
                {
                    reach_popup_anchor anchor = {};
                    anchor.monitor = {100 * dpi, 50 * dpi, 800 * dpi, 700 * dpi};
                    anchor.button = {(owner - 20) * dpi, 0, 40 * dpi, 30 * dpi};
                    anchor.bar_edge_y = 600 * dpi;
                    anchor.direction = direction;
                    reach_motion width = {}, notch = {};
                    reach_motion_start_at(&width, (reverse ? 1000 : 20) * dpi,
                                          (reverse ? 20 : 1000) * dpi, 2, REACH_EASING_EASE_IN_OUT,
                                          10);
                    width = reach_motion_clamp(&width, 30 * dpi, 900 * dpi);
                    reach_rect_motion bounds = {};
                    expect_true(reach_popup_width_motion(&anchor, &width, 180 * dpi, 8 * dpi, dpi,
                                                         &bounds, &notch) == REACH_OK,
                                "popup width derives physical placement and notch intent");
                    for (int sample = 0; sample <= 80; ++sample)
                    {
                        double now = 10 + sample / 40.0;
                        reach_rect_f32 expected =
                            reach_popup_place(&anchor, reach_motion_value_at(&width, now),
                                              180 * dpi, 8 * dpi)
                                .bounds;
                        reach_rect_f32 actual = reach_rect_motion_value_at(&bounds, now);
                        expect_near(actual.x, expected.x, 0.001f,
                                    "native and input X retain existing monitor clamping");
                        expect_near(actual.width, expected.width, 0.001f,
                                    "width intent preserves physical monitor capacity");
                        expect_near(actual.y, expected.y, 0.001f,
                                    "width resize preserves upward and downward placement");
                        expect_near(reach_motion_value_at(&notch, now),
                                    reach_popup_clamp_notch_center_scaled(owner * dpi - expected.x,
                                                                          expected.width, dpi),
                                    0.001f,
                                    "notch follows the owner and existing corner clearance");
                    }
                }
}

static void test_popup_height_motion_matches_constrained_placement(void)
{
    for (float dpi : {1.0f, 1.25f, 1.5f, 2.0f})
        for (int direction : {REACH_POPUP_DROP_DOWN, REACH_POPUP_DROP_UP})
            for (float edge : {-100.0f, 30.0f, 350.0f, 780.0f, 1200.0f})
            {
                reach_popup_anchor anchor = {};
                anchor.monitor = {100 * dpi, 50 * dpi, 800 * dpi, 700 * dpi};
                anchor.button = {110 * dpi, edge * dpi, 40 * dpi, 30 * dpi};
                anchor.bar_edge_y = edge * dpi;
                anchor.direction = direction;
                reach_motion height = {};
                reach_motion_start_at(&height, 180 * dpi, 900 * dpi, 2, REACH_EASING_EASE_OUT, 10);
                reach_rect_motion bounds = {};
                expect_true(reach_popup_height_motion(&anchor, 950 * dpi, &height, 8 * dpi,
                                                      &bounds) == REACH_OK,
                            "popup dimension motion resolves monitor constraints");
                reach_transform_f32 transform = {1.25f, 0.75f, 17, -8};
                reach_rect_motion projected = reach_rect_motion_transform(&bounds, transform);
                for (double now : {10.0, 10.1, 10.5, 11.0, 12.0})
                {
                    reach_rect_f32 expected =
                        reach_popup_place(&anchor, 950 * dpi, reach_motion_value_at(&height, now),
                                          8 * dpi)
                            .bounds;
                    reach_rect_f32 actual = reach_rect_motion_value_at(&bounds, now);
                    expect_near(actual.x, expected.x, 0.001f,
                                "motion preserves horizontal constraints");
                    expect_near(actual.y, expected.y, 0.001f,
                                "motion preserves anchored and clamped Y placement");
                    expect_near(actual.width, expected.width, 0.001f,
                                "motion preserves constrained popup width");
                    expect_near(actual.height, expected.height, 0.001f,
                                "motion preserves constrained popup height");
                    reach_rect_f32 mapped = reach_rect_motion_value_at(&projected, now);
                    expect_near(mapped.y, actual.y * transform.scale_y + transform.offset_y, 0.001f,
                                "native input projection shares the canonical rectangle");
                    expect_near(mapped.height, actual.height * transform.scale_y, 0.001f,
                                "native input projection preserves bounded dimension motion");
                }
            }
}

static void test_group_equality_preserves_intent_without_padding(void)
{
    reach_render_group first = {};
    first.id = 10;
    first.bounds = {0, 0, 100, 50};
    reach_motion_set(&first.opacity, 1);
    reach_render_group second = first;
    unsigned char *bytes = reinterpret_cast<unsigned char *>(&second);
    for (size_t at =
             offsetof(reach_render_group, clip_uniform_radius) + sizeof(second.clip_uniform_radius);
         at < offsetof(reach_render_group, clip_insets); ++at)
        bytes[at] = 0xa5;
    for (size_t at = offsetof(reach_render_group, has_tint) + sizeof(second.has_tint);
         at < offsetof(reach_render_group, tint); ++at)
        bytes[at] = 0xa5;
    for (size_t at = offsetof(reach_render_group, shadow) + sizeof(second.shadow);
         at < offsetof(reach_render_group, clip_radius_motion); ++at)
        bytes[at] = 0xa5;
    for (size_t at =
             offsetof(reach_render_group, clip_radius_motion) + sizeof(second.clip_radius_motion);
         at < sizeof(second); ++at)
        bytes[at] = 0xa5;
    expect_true(reach_render_group_equal(&first, &second),
                "group equality ignores representation padding");
    second.parent_id = 20;
    expect_true(!reach_render_group_equal(&first, &second),
                "changed hierarchy keeps distinct ownership");
    second = first;
    second.clip_uniform_radius = 1;
    expect_true(!reach_render_group_equal(&first, &second),
                "compact radius policy remains distinct");
    second = first;
    reach_motion_set(&second.clip_insets[REACH_RENDER_CLIP_BOTTOM], 12);
    expect_true(!reach_render_group_equal(&first, &second), "clip intent remains distinct");
    second = first;
    second.has_tint = 1;
    reach_motion_set(&second.tint[REACH_RENDER_COLOR_RED], 0.4f);
    expect_true(!reach_render_group_equal(&first, &second), "tint intent remains distinct");
    second = first;
    second.tint_rgb_target.r = 0.5f;
    expect_true(!reach_render_group_equal(&first, &second), "RGB mix target remains distinct");
    second = first;
    reach_motion_set(&second.tint_rgb_mix, 0.5f);
    expect_true(!reach_render_group_equal(&first, &second), "RGB mix intent remains distinct");
    second = first;
    reach_motion_repeat_at(&second.x, 0, 10, 2, 1, 1, REACH_EASING_EASE_IN_OUT, 10);
    expect_true(!reach_render_group_equal(&first, &second), "repeat intent remains distinct");
}

static void test_textbox_caret_reference_ownership(void)
{
    auto buffer_storage = std::make_unique<reach_render_command_buffer>();
    reach_render_command_buffer &buffer = *buffer_storage;
    reach_render_command textbox = {};
    textbox.type = REACH_RENDER_COMMAND_TEXTBOX;
    textbox.rect = {10, 10, 100, 30};
    reach_motion opacity = {};
    reach_motion_repeat_at(&opacity, 1, 0, 0, 0.55, 0.45, REACH_EASING_EASE_IN_OUT, 10);
    expect_true(reach_render_push_textbox(&buffer, &textbox, 100, &opacity, nullptr) == REACH_OK &&
                    buffer.current_group_id == 0 &&
                    reach_render_command_buffer_validate_groups(&buffer) == REACH_OK,
                "textbox part references one retained opacity owner without opening a paint group");
    buffer.commands[0].caret_group_id = 101;
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_INVALID_ARGUMENT,
                "a missing caret owner is rejected before renderer dereference");
    buffer.commands[0].caret_group_id = 100;
    buffer.commands[0].group_id = 100;
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_INVALID_ARGUMENT,
                "caret ownership cannot also paint the editor body");
    buffer.commands[0].group_id = 0;
    reach_render_group body = {};
    body.id = 200;
    body.bounds = textbox.rect;
    expect_true(reach_render_command_buffer_begin_group(&buffer, &body) == REACH_OK,
                "body owner registration succeeds");
    reach_render_command_buffer_end_group(&buffer);
    buffer.commands[0].group_id = 200;
    reach_render_command sibling = {};
    sibling.type = REACH_RENDER_COMMAND_RECT;
    sibling.rect = textbox.rect;
    sibling.group_id = 200;
    expect_true(reach_render_command_buffer_push(&buffer, &sibling) == REACH_OK &&
                    reach_render_command_buffer_validate_groups(&buffer) == REACH_INVALID_ARGUMENT,
                "a caret body stays a single source so splitting preserves painter order");
    reach_render_command_buffer_clear(&buffer);
    reach_render_group color = {};
    color.id = 300;
    color.bounds = textbox.rect;
    color.has_tint = 1;
    for (size_t channel = 0; channel < 4; ++channel)
        reach_motion_set(&color.tint[channel], 1);
    expect_true(reach_render_push_textbox(&buffer, &textbox, 100, &opacity, &color) == REACH_OK &&
                    reach_render_command_buffer_validate_groups(&buffer) == REACH_OK &&
                    buffer.commands[0].text_color_group_id == color.id,
                "textbox ink and caret retain independent exclusive property owners");
    buffer.commands[0].text_color_group_id = 301;
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_INVALID_ARGUMENT,
                "missing textbox ink ownership is rejected");
    buffer.commands[0].text_color_group_id = 100;
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_INVALID_ARGUMENT,
                "textbox ink cannot alias caret ownership");
    reach_render_command_buffer_clear(&buffer);
    color.id = 100;
    expect_true(reach_render_push_textbox(&buffer, &textbox, 100, &opacity, &color) ==
                        REACH_INVALID_ARGUMENT &&
                    buffer.count == 0 && buffer.group_count == 0,
                "conflicting textbox property ownership fails before append");
    color.id = 300;
    reach_render_command_buffer_clear(&buffer);
    expect_true(reach_render_command_buffer_begin_group(&buffer, &body) == REACH_OK &&
                    reach_render_push_textbox(&buffer, &textbox, 100, &opacity, &color) ==
                        REACH_OK &&
                    buffer.current_group_id == body.id &&
                    reach_render_command_buffer_validate_groups(&buffer) == REACH_OK,
                "textbox property ownership preserves an existing placement parent");
}

static void test_shadow_source_ownership(void)
{
    auto storage = std::make_unique<reach_render_command_buffer>();
    auto &buffer = *storage;
    reach_render_group shadow = {};
    shadow.id = 900;
    shadow.bounds = {0, 0, 120, 90};
    shadow.has_shadow = 1;
    shadow.shadow = {0, 2, 6, {0.1f, 0.2f, 0.3f, 0.5f}};
    expect_true(reach_render_command_buffer_begin_group(&buffer, &shadow) == REACH_OK,
                "shadow parent declares a bounded source envelope");
    reach_render_command_buffer_end_group(&buffer);
    reach_render_group source = {};
    source.id = 901;
    source.parent_id = 900;
    source.bounds = {10, 10, 100, 60};
    expect_true(reach_render_command_buffer_begin_group(&buffer, &source) == REACH_OK,
                "source geometry remains an ordinary child");
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = source.bounds;
    command.color = {1, 1, 1, 1};
    expect_true(reach_render_command_buffer_push(&buffer, &command) == REACH_OK,
                "shadow geometry retains normal painter ownership");
    reach_render_command_buffer_end_group(&buffer);
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_OK &&
                    reach_render_command_buffer_group_shadow(&buffer, 901) == &buffer.groups[0],
                "source descendants resolve one filter owner");
    buffer.commands[0].group_id = 900;
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_INVALID_ARGUMENT,
                "a filter owner cannot also paint its source");
    buffer.commands[0].group_id = 901;
    buffer.groups[1].has_shadow = 1;
    buffer.groups[1].shadow = shadow.shadow;
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_INVALID_ARGUMENT,
                "nested filters cannot silently alter source ownership");
    buffer.groups[1].has_shadow = 0;
    buffer.commands[0].type = REACH_RENDER_COMMAND_TEXT;
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_INVALID_ARGUMENT,
                "the shape source contract rejects unsupported paint kinds");
    buffer.commands[0].type = REACH_RENDER_COMMAND_RECT;
    buffer.groups[0].shadow.blur = -1;
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_INVALID_ARGUMENT,
                "invalid blur is rejected before backend allocation");
    buffer.groups[0].shadow = shadow.shadow;
    buffer.groups[0].has_tint = 1;
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_INVALID_ARGUMENT,
                "filter output tint is not confused with source alpha");
    buffer.groups[0].has_tint = 0;
    auto changed = buffer.groups[0];
    changed.shadow.color.a = 0.7f;
    expect_true(!reach_render_group_equal(&buffer.groups[0], &changed),
                "shadow material changes invalidate retained intent");
    buffer.groups[0].has_shadow = changed.has_shadow = 0;
    expect_true(reach_render_group_equal(&buffer.groups[0], &changed),
                "disabled shadow material does not cause property churn");
}

static void test_rect_motion_source_ownership(void)
{
    auto buffer = std::make_unique<reach_render_command_buffer>();
    reach_render_command_buffer_clear(buffer.get());
    reach_render_group source = {};
    source.id = 20;
    source.bounds = {0, 0, 100, 60};
    reach_rect_motion rect = {};
    reach_motion_start_at(&rect.x, 0, 40, .2, REACH_EASING_EASE_IN_OUT, 10);
    reach_motion_set(&rect.y, 0);
    reach_motion_start_at(&rect.width, 0, 100, .2, REACH_EASING_EASE_IN_OUT, 10);
    reach_motion_set(&rect.height, 60);
    reach_render_command_buffer_set_scissor(buffer.get(), {20, 10, 30, 20});
    expect_true(reach_render_push_rect_motion(buffer.get(), &source, &rect, 12, {1, 1, 1, 1}) ==
                        REACH_OK &&
                    reach_render_command_buffer_validate_groups(buffer.get()) == REACH_OK &&
                    buffer->has_scissor && !buffer->commands[0].has_scissor &&
                    buffer->commands[0].rect_fit_to_clip,
                "motion fill retains two owned sources and an output scissor");
    buffer->commands[1] = buffer->commands[0];
    ++buffer->count;
    expect_true(reach_render_command_buffer_validate_groups(buffer.get()) == REACH_INVALID_ARGUMENT,
                "solid clip fill owns exactly one raster command");
    --buffer->count;
    reach_render_group child = {};
    child.id = 30;
    child.parent_id = 21;
    child.bounds = {0, 0, 10, 10};
    expect_true(reach_render_command_buffer_begin_group(buffer.get(), &child) == REACH_OK,
                "solid clip ownership preflight accepts an ordinary descendant group");
    reach_render_command_buffer_end_group(buffer.get());
    expect_true(reach_render_command_buffer_validate_groups(buffer.get()) == REACH_INVALID_ARGUMENT,
                "solid clip fill cannot also own independently clipped descendants");
    --buffer->group_count;
    reach_rect_f32 clip = {};
    expect_true(reach_render_command_buffer_group_clip_at(buffer.get(), 21, 10.2, &clip) &&
                    clip.x == 40 && clip.y == 10 && clip.width == 10 && clip.height == 20,
                "motion fill output scissor stays stationary while its source moves");
    expect_true(reach_render_push_rect_motion(buffer.get(), &source, &rect, 12, {1, 1, 1, 1}) ==
                        REACH_INVALID_ARGUMENT &&
                    buffer->count == 1 && buffer->group_count == 2,
                "motion fill source collision rejects atomically");
    reach_render_command_buffer_clear(buffer.get());
    source.parent_id = 999;
    expect_true(reach_render_push_rect_motion(buffer.get(), &source, &rect, 12, {1, 1, 1, 1}) ==
                        REACH_INVALID_ARGUMENT &&
                    buffer->count == 0 && buffer->group_count == 0,
                "motion fill rejects a missing parent before append");
    source.parent_id = 0;
    reach_motion_set(&rect.width, 101);
    expect_true(reach_render_push_rect_motion(buffer.get(), &source, &rect, 12, {1, 1, 1, 1}) ==
                        REACH_INVALID_ARGUMENT &&
                    buffer->count == 0 && buffer->group_count == 0,
                "motion fill cannot exceed its source envelope");
    reach_motion_set(&rect.width, 80.25f);
    reach_motion_set(&rect.height, 40.25f);
    reach_motion_set(&rect.x, 13.25f);
    reach_render_command image = {};
    image.type = REACH_RENDER_COMMAND_ICON;
    image.icon_id = 100;
    image.color.a = 1;
    image.icon_crop_to_fill = 1;
    expect_true(reach_render_push_image_motion(buffer.get(), &source, &rect, 12, &image) ==
                        REACH_OK &&
                    reach_render_command_buffer_validate_groups(buffer.get()) == REACH_OK &&
                    buffer->commands[0].type == REACH_RENDER_COMMAND_ICON &&
                    buffer->commands[0].rect_fit_to_clip && !buffer->commands[0].has_scissor,
                "fitted image retains exclusive ownership and the inherited output clip");
    expect_true(reach_render_group_clip_at(&buffer->groups[1], 10).width == 80 &&
                    reach_motion_value_at(&buffer->groups[0].x, 10) == 13,
                "fitted image preserves ordinary bitmap snapping for constant axes");
    reach_render_command_buffer_clear(buffer.get());
    source.bounds.x = -.5f;
    source.bounds.y = .25f;
    expect_true(
        reach_render_push_image_motion(buffer.get(), &source, &rect, 12, &image) == REACH_OK &&
            buffer->groups[1].bounds.x == -1 && buffer->groups[1].bounds.y == 0 &&
            reach_motion_value_at(&buffer->groups[0].x, 10) + buffer->groups[1].bounds.x == 13,
        "fractional image capture origin keeps bitmap snapping at the requested placement");
    reach_render_command_buffer_clear(buffer.get());
    image.icon_fade_start = .5f;
    expect_true(reach_render_push_image_motion(buffer.get(), &source, &rect, 12, &image) ==
                        REACH_INVALID_ARGUMENT &&
                    buffer->count == 0 && buffer->group_count == 0,
                "fitted image rejects destination-dependent fade before append");
    image.icon_fade_start = 0;
    reach_render_group shadow = {};
    shadow.id = 10;
    shadow.bounds = source.bounds;
    shadow.has_shadow = 1;
    shadow.shadow = {0, 0, 4, {0, 0, 0, 1}};
    expect_true(reach_render_command_buffer_begin_group(buffer.get(), &shadow) == REACH_OK,
                "fitted image shadow-parent rejection fixture");
    reach_render_command_buffer_end_group(buffer.get());
    source.parent_id = shadow.id;
    expect_true(reach_render_push_image_motion(buffer.get(), &source, &rect, 12, &image) ==
                        REACH_INVALID_ARGUMENT &&
                    buffer->count == 0 && buffer->group_count == 1,
                "fitted image rejects unsupported shape-shadow ancestry before append");
}

static void test_width_text_source_ownership(void)
{
    auto buffer = std::make_unique<reach_render_command_buffer>();
    reach_render_command_buffer_clear(buffer.get());
    reach_render_command text = {};
    text.type = REACH_RENDER_COMMAND_TEXT;
    text.rect = {0, 0, 200, 24};
    text.text_size = 13;
    text.text[0] = 'A';
    reach_render_group viewport = {};
    viewport.id = 100;
    viewport.bounds = viewport.clip = text.rect;
    viewport.has_clip = 1;
    reach_motion width = {};
    reach_motion_start_at(&width, 20, 200, .18, REACH_EASING_EASE_IN_OUT, 10);
    viewport.clip_insets[REACH_RENDER_CLIP_RIGHT] = reach_motion_affine(&width, -1, 200);
    expect_true(reach_render_push_width_text(buffer.get(), &text, &viewport) == REACH_OK &&
                    reach_render_command_buffer_validate_groups(buffer.get()) == REACH_OK &&
                    buffer->commands[0].text_fit_to_clip && !buffer->commands[0].text_ellipsis,
                "width text preserves source trimming with one canonical viewport");
    expect_true(reach_render_push_width_text(buffer.get(), &text, &viewport) ==
                        REACH_INVALID_ARGUMENT &&
                    buffer->count == 1 && buffer->group_count == 1,
                "width text identity collision leaves the destination unchanged");
    buffer->commands[1] = buffer->commands[0];
    ++buffer->count;
    expect_true(reach_render_command_buffer_validate_groups(buffer.get()) == REACH_INVALID_ARGUMENT,
                "width text cannot share its owning viewport with another raster source");
    reach_render_command_buffer_clear(buffer.get());
    viewport.parent_id = 999;
    expect_true(reach_render_push_width_text(buffer.get(), &text, &viewport) ==
                        REACH_INVALID_ARGUMENT &&
                    buffer->count == 0 && buffer->group_count == 0,
                "missing width text parent rejects before append");
    viewport.parent_id = 0;
    reach_motion_set(&viewport.clip_insets[REACH_RENDER_CLIP_LEFT], 1);
    expect_true(reach_render_push_width_text(buffer.get(), &text, &viewport) ==
                        REACH_INVALID_ARGUMENT &&
                    buffer->count == 0 && buffer->group_count == 0,
                "unsupported title origin clipping rejects before append");
    reach_motion_set(&viewport.clip_insets[REACH_RENDER_CLIP_LEFT], 0);
    reach_motion_set(&viewport.clip_insets[REACH_RENDER_CLIP_RIGHT], -1);
    expect_true(reach_render_push_width_text(buffer.get(), &text, &viewport) ==
                    REACH_INVALID_ARGUMENT,
                "title width cannot silently exceed its retained source envelope");
    viewport.clip_insets[REACH_RENDER_CLIP_RIGHT] = reach_motion_affine(&width, -1, 200);
    reach_render_group shadow = {};
    shadow.id = 99;
    shadow.bounds = text.rect;
    shadow.has_shadow = 1;
    shadow.shadow = reach_theme_default()->popup_shadow;
    expect_true(reach_render_command_buffer_begin_group(buffer.get(), &shadow) == REACH_OK,
                "width text preflight starts from a valid shadow owner");
    reach_render_command_buffer_end_group(buffer.get());
    viewport.parent_id = shadow.id;
    expect_true(reach_render_push_width_text(buffer.get(), &text, &viewport) ==
                        REACH_INVALID_ARGUMENT &&
                    buffer->count == 0 && buffer->group_count == 1,
                "width text rejects shape-shadow ancestry before appending raster content");
    reach_render_command_buffer_clear(buffer.get());
    viewport.parent_id = 0;
    text.text_fit_to_clip = REACH_RENDER_TEXT_FIT_VERTICAL_OVERFLOW;
    text.text_ellipsis = 1;
    expect_true(reach_render_push_width_text(buffer.get(), &text, &viewport) == REACH_OK &&
                    reach_render_command_buffer_validate_groups(buffer.get()) == REACH_OK &&
                    buffer->commands[0].text_fit_to_clip ==
                        REACH_RENDER_TEXT_FIT_VERTICAL_OVERFLOW &&
                    buffer->commands[0].text_ellipsis,
                "width text retains explicit ink-overflow and source trimming intent");
}

static void test_popup_width_background_preflight(void)
{
    auto storage = std::make_unique<reach_render_command_buffer>();
    auto &buffer = *storage;
    reach_render_command_buffer_clear(&buffer);
    reach_popup_background_input input = {};
    input.theme = reach_theme_default();
    input.bounds = {0, 0, 180, 120};
    input.dpi_scale = 1;
    input.notch_side = REACH_NOTCH_SIDE_BOTTOM;
    reach_motion width = {};
    reach_motion_start_at(&width, 20, 180, .18, REACH_EASING_EASE_IN_OUT, 10);
    auto notch = reach_motion_affine(&width, .5f, 0);
    expect_true(reach_popup_push_width_background(&input, &width, &notch, 0, 100, &buffer) ==
                    REACH_OK,
                "compact width background composes canonical shape motion");
    expect_true(reach_render_command_buffer_validate_groups(&buffer) == REACH_OK,
                "width background preserves neutral source/filter ownership");
    bool shadow = false;
    for (size_t index = 0; index < buffer.group_count; ++index)
        shadow = shadow || buffer.groups[index].has_shadow;
    expect_true(shadow, "compact width uses shape shadow rather than overlapping rigid caps");
    size_t count = buffer.count, groups = buffer.group_count;
    expect_true(reach_popup_push_width_background(&input, &width, &notch, 0, 100, &buffer) ==
                        REACH_INVALID_ARGUMENT &&
                    buffer.count == count && buffer.group_count == groups,
                "group identity collisions leave the append buffer untouched");
    reach_render_command_buffer_clear(&buffer);
    expect_true(reach_popup_push_width_background(&input, &width, &notch, 999, 100, &buffer) ==
                        REACH_INVALID_ARGUMENT &&
                    buffer.count == 0 && buffer.group_count == 0,
                "missing parent rejects the entire width background append");
    reach_render_group parent = {};
    parent.id = 99;
    parent.bounds = {0, 0, 220, 160};
    parent.has_shadow = 1;
    parent.shadow = input.theme->popup_shadow;
    expect_true(reach_render_command_buffer_begin_group(&buffer, &parent) == REACH_OK,
                "shadow parent is valid before attaching compact chrome");
    reach_render_command_buffer_end_group(&buffer);
    expect_true(reach_popup_push_width_background(&input, &width, &notch, 99, 100, &buffer) ==
                        REACH_INVALID_ARGUMENT &&
                    buffer.count == 0 && buffer.group_count == 1,
                "nested shadow rejects the entire width background append");
    reach_render_command_buffer_clear(&buffer);
    auto theme = *input.theme;
    theme.popup_shadow.color.a = 0;
    input.theme = &theme;
    expect_true(reach_popup_push_width_background(&input, &width, &notch, 0, 100, &buffer) ==
                        REACH_OK &&
                    reach_render_command_buffer_validate_groups(&buffer) == REACH_OK,
                "disabled shadow keeps compact width chrome valid");
    shadow = false;
    for (size_t index = 0; index < buffer.group_count; ++index)
        shadow = shadow || buffer.groups[index].has_shadow;
    expect_true(!shadow, "disabled shadow allocates no retained filter source");
    input.theme = reach_theme_default();
    reach_render_command_buffer_clear(&buffer);
    reach_rect_f32 scissor = {10, 10, 50, 50};
    reach_render_command_buffer_set_scissor(&buffer, scissor);
    expect_true(reach_popup_push_width_background(&input, &width, &notch, 0, 100, &buffer) ==
                        REACH_OK &&
                    reach_render_command_buffer_validate_groups(&buffer) == REACH_OK,
                "scissored width chrome preserves valid retained ownership");
    const auto *clip = reach_render_command_buffer_group(&buffer, 115);
    expect_true(clip != nullptr && clip->has_clip && reach_rect_equal(clip->clip, scissor),
                "scissor clips complete popup output including its shadow");
    reach_render_command_buffer_clear(&buffer);
    input.dpi_scale = 0;
    expect_true(reach_popup_push_width_background(&input, &width, &notch, 0, 100, &buffer) ==
                        REACH_INVALID_ARGUMENT &&
                    buffer.count == 0 && buffer.group_count == 0,
                "invalid DPI rejects width chrome without appending");
    input.dpi_scale = 1;
    reach_motion_start_at(&width, 220, 400, .18, REACH_EASING_EASE_IN_OUT, 10);
    notch = reach_motion_affine(&width, .5f, 0);
    expect_true(reach_popup_push_width_background(&input, &width, &notch, 0, 100, &buffer) ==
                        REACH_OK &&
                    reach_render_command_buffer_validate_groups(&buffer) == REACH_OK,
                "ordinary width background keeps rigid cap sources");
    shadow = false;
    for (size_t index = 0; index < buffer.group_count; ++index)
        shadow = shadow || buffer.groups[index].has_shadow;
    expect_true(!shadow, "covered width range avoids an unnecessary live shadow filter");
}

struct overshoot_extremes
{
    float minimum;
    float maximum;
};

static overshoot_extremes overshoot_sample(const reach_motion &motion)
{
    overshoot_extremes result = {INFINITY, -INFINITY};
    double deadline = reach_motion_deadline(&motion);
    for (double at = motion.started_seconds; at <= deadline + .0005; at += .0005)
    {
        float value = reach_motion_value_at(&motion, at);
        result.minimum = fminf(result.minimum, value);
        result.maximum = fmaxf(result.maximum, value);
    }
    return result;
}

static reach_motion overshoot_spring(float from, float to, float push, reach_motion_preset preset)
{
    reach_motion_spec spec = reach_theme_motion_preset(reach_theme_default(), preset);
    float velocity = push * (to - from) * 6.2831853f / spec.response_seconds;
    reach_motion motion = {};
    reach_motion_start_spec_at(&motion, from, to, velocity, spec, 10);
    return motion;
}

static bool overshoot_compiles(const reach_motion &motion)
{
    reach_motion_segment segments[REACH_MOTION_MAX_SEGMENTS] = {};
    return motion.generation == 0 || motion.repeating || motion.duration_seconds <= 0 ||
           reach_motion_compile_at(&motion, motion.started_seconds, segments) > 0;
}

static bool overshoot_groups_compile(const reach_render_command_buffer &buffer)
{
    for (size_t index = 0; index < buffer.group_count; ++index)
    {
        const reach_render_group &group = buffer.groups[index];
        const reach_motion motions[] = {group.x, group.y, reach_render_unit_motion(&group.opacity),
                                        group.scale, reach_render_group_y_scale(&group)};
        for (const reach_motion &motion : motions)
            if (!overshoot_compiles(motion))
                return false;
        if (!group.has_clip)
            continue;
        reach_rect_motion clip = {};
        if (reach_render_group_clip_motion(&group, &clip) != REACH_OK ||
            !overshoot_compiles(clip.width) || !overshoot_compiles(clip.height))
            return false;
        for (const reach_motion &inset : group.clip_insets)
            if (!overshoot_compiles(inset))
                return false;
        reach_motion radius_x = {}, radius_y = {};
        if (reach_render_group_has_rounded_clip(&group) &&
            (reach_render_group_clip_radii_motion(&group, &radius_x, &radius_y) != REACH_OK ||
             !overshoot_compiles(radius_x) || !overshoot_compiles(radius_y)))
            return false;
    }
    return true;
}

static bool overshoot_clip_covers(const reach_render_group *group, const reach_motion &width,
                                  float limit)
{
    if (group == nullptr)
        return false;
    double deadline = reach_motion_deadline(&width);
    for (double at = width.started_seconds; at <= deadline + .001; at += .001)
    {
        float value = reach_motion_value_at(&width, at);
        float clip = reach_render_group_clip_at(group, at).width;
        float expected = fminf(fmaxf(0, value), limit);
        if (clip < 0 || clip > limit + .01f || fabsf(clip - expected) > .05f)
            return false;
    }
    return true;
}

static void test_overshoot_render_envelopes(void)
{
    const float dpis[] = {1.0f, 1.25f, 1.5f, 2.0f};
    const reach_motion_preset presets[] = {REACH_MOTION_PRESET_BOUNCY, REACH_MOTION_PRESET_RELEASE};
    auto storage = std::make_unique<reach_render_command_buffer>();
    auto &buffer = *storage;
    bool rect_ok = true, rect_covers = true, image_ok = true, text_ok = true;
    bool popup_ok = true, popup_covers = true, popup_motion_ok = true, height_ok = true;
    bool radius_ok = true, unit_ok = true, compile_ok = true, stretch_ok = true;
    bool overshoots = false, undershoots = false;
    for (float dpi : dpis)
        for (reach_motion_preset preset : presets)
            for (float push : {0.0f, 0.5f})
            {
                float small = 24 * dpi, large = 1600 * dpi, height = 40 * dpi;
                reach_motion grow = overshoot_spring(small, large, push, preset);
                reach_motion shrink = overshoot_spring(large, small, push, preset);
                overshoot_extremes grown = overshoot_sample(grow);
                overshoot_extremes shrunk = overshoot_sample(shrink);
                overshoots = overshoots || grown.maximum > large + 1;
                undershoots = undershoots || shrunk.minimum < 0;
                for (const reach_motion *width : {&grow, &shrink})
                {
                    reach_render_command_buffer_clear(&buffer);
                    reach_render_group source = {};
                    source.id = 10;
                    source.bounds = {0, 0, ceilf(large), ceilf(height)};
                    reach_rect_motion bounds = {};
                    reach_motion_set(&bounds.x, 0);
                    reach_motion_set(&bounds.y, 0);
                    bounds.width = *width;
                    reach_motion_set(&bounds.height, height);
                    overshoot_extremes sampled = overshoot_sample(*width);
                    bool pushed = reach_render_push_rect_motion(&buffer, &source, &bounds, 8 * dpi,
                                                                {1, 1, 1, 1}) == REACH_OK &&
                                  reach_render_command_buffer_validate_groups(&buffer) == REACH_OK;
                    rect_ok = rect_ok && pushed;
                    const auto *paint = reach_render_command_buffer_group(&buffer, 11);
                    rect_covers = rect_covers && pushed && paint != nullptr &&
                                  paint->bounds.width >= sampled.maximum &&
                                  overshoot_clip_covers(paint, *width, paint->bounds.width);
                    compile_ok = compile_ok && overshoot_groups_compile(buffer);

                    reach_render_command_buffer_clear(&buffer);
                    reach_render_command image = {};
                    image.type = REACH_RENDER_COMMAND_ICON;
                    image.icon_id = 7;
                    pushed = reach_render_push_image_motion(&buffer, &source, &bounds, 0, &image) ==
                                 REACH_OK &&
                             reach_render_command_buffer_validate_groups(&buffer) == REACH_OK;
                    image_ok = image_ok && pushed &&
                               overshoot_clip_covers(reach_render_command_buffer_group(&buffer, 11),
                                                     *width, source.bounds.width);

                    reach_render_command_buffer_clear(&buffer);
                    reach_render_command text = {};
                    text.type = REACH_RENDER_COMMAND_TEXT;
                    text.rect = {0, 0, ceilf(large), height};
                    text.text_size = 13 * dpi;
                    text.text[0] = 'A';
                    reach_render_group viewport = {};
                    viewport.id = 20;
                    viewport.bounds = viewport.clip = text.rect;
                    viewport.has_clip = 1;
                    viewport.clip_insets[REACH_RENDER_CLIP_RIGHT] =
                        reach_motion_affine(width, -1, text.rect.width);
                    pushed = reach_render_push_width_text(&buffer, &text, &viewport) == REACH_OK &&
                             reach_render_command_buffer_validate_groups(&buffer) == REACH_OK;
                    text_ok = text_ok && pushed &&
                              overshoot_clip_covers(reach_render_command_buffer_group(&buffer, 20),
                                                    *width, text.rect.width);
                }

                reach_popup_anchor anchor = {};
                anchor.monitor = {0, 0, 2560 * dpi, 1440 * dpi};
                anchor.button = {300 * dpi, 0, 40 * dpi, 32 * dpi};
                anchor.bar_edge_y = 32 * dpi;
                anchor.direction = REACH_POPUP_DROP_DOWN;
                reach_popup_background_input input = {};
                input.theme = reach_theme_default();
                input.dpi_scale = dpi;
                input.notch_side = REACH_NOTCH_SIDE_TOP;
                const float spans[][2] = {{20, 180}, {220, 400}, {900, 40}, {40, 900}};
                for (const auto &span : spans)
                {
                    reach_motion width =
                        overshoot_spring(span[0] * dpi, span[1] * dpi, push, preset);
                    reach_rect_motion body = {};
                    reach_motion notch = {};
                    bool placed = reach_popup_width_motion(&anchor, &width, 180 * dpi, 8 * dpi, dpi,
                                                           &body, &notch) == REACH_OK;
                    float minimum = 0, maximum = 0;
                    placed = placed &&
                             reach_motion_range(&body.width, &minimum, &maximum) == REACH_OK &&
                             minimum > 0;
                    popup_motion_ok = popup_motion_ok && placed;
                    reach_render_command_buffer_clear(&buffer);
                    overshoot_extremes sampled = overshoot_sample(width);
                    input.bounds = {0, 0, fmaxf(span[0], span[1]) * dpi, 180 * dpi};
                    auto center = reach_motion_affine(&width, .5f, 0);
                    bool pushed = reach_popup_push_width_background(&input, &width, &center, 0, 100,
                                                                    &buffer) == REACH_OK &&
                                  reach_render_command_buffer_validate_groups(&buffer) == REACH_OK;
                    popup_ok = popup_ok && pushed;
                    compile_ok = compile_ok && overshoot_groups_compile(buffer);
                    const auto *caster = reach_render_command_buffer_group(&buffer, 100);
                    if (pushed && caster != nullptr && caster->has_shadow)
                        popup_covers = popup_covers && caster->bounds.width >= sampled.maximum;

                    reach_motion tall =
                        overshoot_spring(span[1] * dpi, span[0] * dpi, push, preset);
                    reach_rect_motion screen = {};
                    reach_render_command_buffer_clear(&buffer);
                    input.bounds = {0, 0, 320 * dpi, 600 * dpi};
                    height_ok =
                        height_ok &&
                        reach_popup_height_motion(&anchor, 320 * dpi, &tall, 8 * dpi, &screen) ==
                            REACH_OK &&
                        reach_motion_range(&screen.height, &minimum, &maximum) == REACH_OK &&
                        minimum > 0 &&
                        reach_popup_push_height_background(&input, &tall, 0, 200, &buffer) ==
                            REACH_OK &&
                        reach_render_command_buffer_validate_groups(&buffer) == REACH_OK;
                }

                reach_render_command_buffer_clear(&buffer);
                reach_render_group rounded = {};
                rounded.id = 30;
                rounded.bounds = rounded.clip = {0, 0, 200 * dpi, 120 * dpi};
                rounded.has_clip = 1;
                rounded.clip_radius_motion = overshoot_spring(24 * dpi, 0, push, preset);
                reach_motion radius_x = {}, radius_y = {};
                float minimum = 0, maximum = 0;
                bool rounded_ok =
                    reach_render_command_buffer_begin_group(&buffer, &rounded) == REACH_OK &&
                    reach_render_group_clip_radii_motion(&rounded, &radius_x, &radius_y) ==
                        REACH_OK &&
                    reach_motion_range(&radius_x, &minimum, &maximum) == REACH_OK && minimum >= 0 &&
                    overshoot_compiles(radius_x);
                reach_render_command_buffer_end_group(&buffer);
                for (double at = 10; rounded_ok && at < 10 + 3; at += .002)
                    rounded_ok = reach_render_group_clip_radii_at(&rounded, at).x >= 0;
                radius_ok = radius_ok && rounded_ok;

                reach_motion fade = overshoot_spring(0, 1, push, preset);
                reach_motion unit = reach_render_unit_motion(&fade);
                bool bounded = reach_motion_range(&unit, &minimum, &maximum) == REACH_OK &&
                               minimum >= 0 && maximum <= 1 && overshoot_compiles(unit);
                reach_render_command_buffer_clear(&buffer);
                reach_render_group tinted = {};
                tinted.id = 40;
                tinted.bounds = {0, 0, 10, 10};
                tinted.opacity = fade;
                tinted.has_tint = 1;
                for (auto &channel : tinted.tint)
                    channel = fade;
                bounded = bounded &&
                          reach_render_command_buffer_begin_group(&buffer, &tinted) == REACH_OK;
                reach_render_command_buffer_end_group(&buffer);
                for (double at = 10; bounded && at < 10 + 3; at += .002)
                {
                    float opacity = reach_render_command_buffer_group_opacity_at(&buffer, 40, at);
                    reach_color tint = reach_render_group_tint_at(&tinted, at);
                    bounded = opacity >= 0 && opacity <= 1 && tint.r >= 0 && tint.r <= 1 &&
                              tint.a >= 0 && tint.a <= 1;
                }
                unit_ok = unit_ok && bounded;

                reach_render_group chrome = {};
                chrome.id = 50;
                chrome.bounds = {0, 0, 81, 81};
                reach_render_stretch stretch = {};
                stretch.left = stretch.right = stretch.top = stretch.bottom = 30;
                stretch.width = overshoot_spring(200 * dpi, 900 * dpi, push, preset);
                reach_motion_set(&stretch.height, 81);
                reach_render_group center_patch = {}, right_patch = {};
                bool patched =
                    reach_render_stretch_patch(&chrome, &stretch, 1, &center_patch) == REACH_OK &&
                    reach_render_stretch_patch(&chrome, &stretch, 2, &right_patch) == REACH_OK;
                for (double at = 10; patched && at < 10 + 3; at += .002)
                {
                    float scale = reach_motion_value_at(&center_patch.scale, at);
                    float center_right = center_patch.bounds.x +
                                         reach_motion_value_at(&center_patch.x, at) +
                                         center_patch.bounds.width * scale;
                    float right_left =
                        right_patch.bounds.x + reach_motion_value_at(&right_patch.x, at);
                    patched = fabsf(center_right - right_left) < .05f;
                }
                stretch_ok = stretch_ok && patched;
            }
    expect_true(overshoots && undershoots,
                "bouncy presets overshoot their target and undershoot zero at worst-case spans");
    expect_true(rect_ok, "fitted rects accept bouncy widths at every DPI");
    expect_true(rect_covers, "fitted rect sources cover every sampled overshoot extreme");
    expect_true(image_ok, "fitted images clamp bouncy widths to their raster source");
    expect_true(text_ok, "width text clamps overshoot to its retained text envelope");
    expect_true(popup_motion_ok, "popup width placement keeps overshooting widths positive");
    expect_true(popup_ok, "rigid and narrow popup chrome accept bouncy widths at every DPI");
    expect_true(popup_covers, "narrow popup shadow sources cover the sampled width maximum");
    expect_true(height_ok, "popup height chrome accepts bouncy heights below its caps");
    expect_true(radius_ok, "bouncy corner radii clamp at zero instead of rejecting the group");
    expect_true(unit_ok, "opacity and tint stay inside [0, 1] while their springs overshoot");
    expect_true(compile_ok, "overshoot-safe render motions still compile natively");
    expect_true(stretch_ok, "stretched center patches follow spring velocity without seams");

    reach_motion monotone = {};
    reach_motion_start_at(&monotone, 0, 1, .2, REACH_EASING_EASE_OUT, 10);
    reach_motion unit = reach_render_unit_motion(&monotone);
    expect_true(reach_motion_equal(&unit, &monotone),
                "monotone opacity descriptors are submitted unchanged");
}

static void test_presentation_center_uses_canonical_motion(void)
{
    auto buffer = std::make_unique<reach_render_command_buffer>();
    reach_render_command_buffer_clear(buffer.get());
    buffer->has_presentation = 1;
    buffer->presentation.bounds = {10, 20, 100, 200};
    auto center = reach_render_presentation_center_at(buffer.get(), 10);
    expect_true(center.x == 60 && center.y == 120,
                "unspecified presentation center follows the static content bounds");
    reach_motion_start_at(&buffer->presentation_center_y, 40, 200, .2, REACH_EASING_EASE_IN_OUT,
                          10);
    center = reach_render_presentation_center_at(buffer.get(), 10.1);
    expect_true(center.x == 60 && fabsf(center.y - 120) < .001f &&
                    reach_render_command_buffer_validate_groups(buffer.get()) == REACH_OK,
                "one canonical center axis retains the other axis default");
    buffer->has_presentation = 0;
    expect_true(reach_render_command_buffer_validate_groups(buffer.get()) == REACH_INVALID_ARGUMENT,
                "a center motion requires its root presentation owner");
    buffer->has_presentation = 1;
    buffer->presentation_center_y.repeating = 1;
    expect_true(reach_render_command_buffer_validate_groups(buffer.get()) == REACH_INVALID_ARGUMENT,
                "root center repetition cannot prevent finite presentation completion");
    reach_render_command_buffer_clear(buffer.get());
    expect_true(!buffer->presentation_center_y.generation &&
                    !reach_render_command_buffer_motion_active(buffer.get()),
                "clearing the render buffer retires root center ownership");
}

int main(void)
{
    test_presentation_center_uses_canonical_motion();
    test_rect_motion_source_ownership();
    test_width_text_source_ownership();
    test_popup_width_background_preflight();
    test_overshoot_render_envelopes();
    test_shadow_source_ownership();
    test_textbox_caret_reference_ownership();
    test_group_equality_preserves_intent_without_padding();
    test_stretch_geometry_preserves_caps_and_anchor();
    test_focal_stretch_geometry_preserves_fixed_features();
    test_popup_height_motion_matches_constrained_placement();
    test_popup_width_motion_matches_constrained_placement();
    test_group_hierarchy_samples_one_geometry_timeline();
    test_retained_command_preflight_preserves_batch_atomicity();
    test_group_hierarchy_rejects_ambiguous_ownership_and_order();
    test_rounded_clip_motion_preserves_compact_axes();
    test_uniform_clip_radii_share_geometry_motion();
    test_canonical_clip_radius_ownership();
    test_retained_tint_samples_one_timeline();
    test_animated_clip_samples_absolute_geometry();
    test_dock_reserves_side_safe_margins();
    test_launcher_border_wraps_stable_content_geometry();
    test_resolve_orders_descending_by_layer();
    test_override_precedence();
    test_set_condition_order_independence();
    test_tie_break_is_registration_order();
    test_hidden_participants();
    test_layer_zero_resolves_as_exit();
    test_plan_equality_detects_changes();
    test_registration_limits();

    if (failures != 0)
    {
        fprintf(stderr, "%d layout test failure(s)\n", failures);
        return 1;
    }
    printf("layout tests passed\n");
    return 0;
}
