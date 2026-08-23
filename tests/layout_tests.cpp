#include "reach/features/common/layout.h"
#include "reach/core/ui_layout.h"

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

int main(void)
{
    test_dock_reserves_side_safe_margins();
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
