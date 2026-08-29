#include "reach/support/util.h"
#include "reach/features/dock.h"
#include "test_utf16.h"

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

static reach_window_snapshot make_window(uintptr_t id, const char *path, const char *aumid)
{
    reach_window_snapshot window = {};
    window.id = id;
    window.visible = 1;
    reach_copy_ascii_to_utf16(window.title, 260, "window");
    reach_copy_ascii_to_utf16(window.path, 260, path);
    reach_copy_ascii_to_utf16(window.app_user_model_id, 260, aumid);
    return window;
}

static reach_pinned_app_model make_pin(uint32_t id, const char *path)
{
    reach_pinned_app_model app = {};
    app.id = id;
    reach_copy_ascii_to_utf16(app.path, 260, path);
    return app;
}

static int32_t matches_thunk(void *user, const reach_pinned_app_model *pinned_app,
                             const reach_window_snapshot *window)
{
    (void)user;
    return reach_window_tracking_window_matches_app(pinned_app, window);
}

static void test_unpinned_windows_group_into_one_item(void)
{
    reach_dock_feature_model model = {};
    reach_dock_feature_model_init(&model);
    uint32_t next_key = 1;

    reach_window_snapshot windows[3] = {
        make_window(101, "C:\\apps\\brave.exe", ""),
        make_window(102, "C:\\apps\\brave.exe", ""),
        make_window(103, "C:\\apps\\brave.exe", ""),
    };
    uint32_t group_ids[3] = {7, 7, 7};

    reach_dock_feature_model_build_items(&model, &next_key, nullptr, 0, windows, group_ids, 3, matches_thunk,
                                         nullptr);

    expect_true(model.item_count == 1, "three same-app windows collapse into one item");
    expect_true(model.items[0].pinned == 0, "grouped item is unpinned");
    expect_true(model.items[0].instance_count == 3, "the item owns all three instances");
    expect_true(model.items[0].window == 101, "first window is the representative");
    expect_true(reach_dock_feature_model_find_order_key(
                    &model, reach_dock_item_key_at(&model, 0)) == 0,
                "the item holds the first slot in the order");
}

static void test_pinned_app_claims_matching_windows(void)
{
    reach_dock_feature_model model = {};
    reach_dock_feature_model_init(&model);
    uint32_t next_key = 1;

    reach_pinned_app_model pins[1] = {make_pin(5, "C:\\apps\\brave.exe")};
    reach_window_snapshot windows[3] = {
        make_window(101, "C:\\apps\\brave.exe", ""),
        make_window(102, "C:\\apps\\brave.exe", ""),
        make_window(103, "C:\\apps\\code.exe", ""),
    };
    uint32_t group_ids[3] = {7, 7, 8};

    reach_dock_feature_model_build_items(&model, &next_key, pins, 1, windows, group_ids, 3, matches_thunk,
                                         nullptr);

    expect_true(model.item_count == 2, "one pinned item plus one unpinned group");
    expect_true(model.items[0].pinned == 1 && model.items[0].pin_id == 5,
                "a pinned entry carries its pin id as a property");
    expect_true(model.items[0].window == 101, "pinned item takes first matching window");
    expect_true(model.items[1].pinned == 0 && model.items[1].pin_id == 0,
                "leftover window forms an unpinned entry");
}

static void test_key_stable_when_representative_closes(void)
{
    reach_dock_feature_model model = {};
    reach_dock_feature_model_init(&model);
    uint32_t next_key = 1;

    reach_window_snapshot windows[3] = {
        make_window(101, "C:\\apps\\brave.exe", ""),
        make_window(102, "C:\\apps\\brave.exe", ""),
        make_window(103, "C:\\apps\\code.exe", ""),
    };
    uint32_t group_ids[3] = {7, 7, 8};
    reach_dock_feature_model_build_items(&model, &next_key, nullptr, 0, windows, group_ids, 3, matches_thunk,
                                         nullptr);
    expect_true(model.item_count == 2, "two groups before churn");

    reach_window_snapshot after[2] = {windows[1], windows[2]};
    uint32_t after_group_ids[2] = {7, 8};
    reach_dock_feature_model_build_items(&model, &next_key, nullptr, 0, after, after_group_ids, 2,
                                         matches_thunk, nullptr);

    expect_true(model.item_count == 2, "groups survive representative closing");
    expect_true(reach_dock_feature_model_find_order_key(
                    &model, reach_dock_item_key_at(&model, 0)) == 0,
                "group keeps its dock position when representative closes");
    expect_true(model.items[0].window == 102,
                "surviving window becomes the representative");
}

static void test_order_preserved_and_new_groups_append(void)
{
    reach_dock_feature_model model = {};
    reach_dock_feature_model_init(&model);
    uint32_t next_key = 1;

    reach_pinned_app_model pins[1] = {make_pin(5, "C:\\apps\\term.exe")};
    reach_window_snapshot windows[2] = {
        make_window(201, "C:\\apps\\brave.exe", ""),
        make_window(202, "C:\\apps\\code.exe", ""),
    };
    uint32_t group_ids[2] = {7, 8};
    reach_dock_feature_model_build_items(&model, &next_key, pins, 1, windows, group_ids, 2, matches_thunk,
                                         nullptr);
    expect_true(model.item_count == 3, "pinned plus two unpinned groups");

    reach_dock_feature_model_move_order(&model, 2, 0);
    reach_window_snapshot with_new[3] = {windows[0], windows[1],
                                         make_window(203, "C:\\apps\\mail.exe", "")};
    uint32_t new_group_ids[3] = {7, 8, 9};
    reach_dock_feature_model_build_items(&model, &next_key, pins, 1, with_new, new_group_ids, 3, matches_thunk,
                                         nullptr);

    expect_true(reach_test_utf16_equals_ascii(model.items[0].path, "C:\\apps\\code.exe"),
                "moved group keeps its position across rebuild");
    expect_true(reach_test_utf16_equals_ascii(model.items[3].path, "C:\\apps\\mail.exe"),
                "new group appends at the end");
}

static void test_same_path_different_aumid_stays_split(void)
{
    reach_dock_feature_model model = {};
    reach_dock_feature_model_init(&model);
    uint32_t next_key = 1;

    reach_window_snapshot windows[2] = {
        make_window(301, "C:\\apps\\brave.exe", "Brave._crx_abc"),
        make_window(302, "C:\\apps\\brave.exe", "Brave._crx_xyz"),
    };
    uint32_t group_ids[2] = {7, 8};
    reach_dock_feature_model_build_items(&model, &next_key, nullptr, 0, windows, group_ids, 2, matches_thunk,
                                         nullptr);

    expect_true(model.item_count == 2, "distinct group ids produce distinct items");
}

/* The property the whole dock rests on: an entry describes an application, and being pinned is
   one of its properties. Everything that drives behaviour must therefore be identical whether the
   app arrived from the config store or from a tracked window. */
static void test_pinned_and_unpinned_entries_are_the_same_kind_of_thing(void)
{
    reach_window_snapshot windows[1] = {make_window(501, "C:\\apps\\brave.exe", "Brave.App")};
    uint32_t group_ids[1] = {11};

    reach_dock_feature_model unpinned = {};
    reach_dock_feature_model_init(&unpinned);
    uint32_t unpinned_key = 1;
    reach_dock_feature_model_build_items(&unpinned, &unpinned_key, nullptr, 0, windows, group_ids,
                                         1, matches_thunk, nullptr);

    reach_pinned_app_model pins[1] = {make_pin(5, "C:\\apps\\brave.exe")};
    reach_copy_ascii_to_utf16(pins[0].app_user_model_id, 260, "Brave.App");
    reach_dock_feature_model pinned = {};
    reach_dock_feature_model_init(&pinned);
    uint32_t pinned_key = 1;
    reach_dock_feature_model_build_items(&pinned, &pinned_key, pins, 1, windows, group_ids, 1,
                                         matches_thunk, nullptr);

    expect_true(unpinned.item_count == 1 && pinned.item_count == 1,
                "the same application is one entry either way");

    const reach_dock_item_model *a = &unpinned.items[0];
    const reach_dock_item_model *b = &pinned.items[0];

    expect_true(a->key != 0 && b->key != 0, "every entry has a usable identity");
    expect_true(reach_dock_item_identity_equal(a, b->path, b->app_user_model_id),
                "both entries describe the same application");
    expect_true(a->instance_count == b->instance_count && a->instance_count == 1,
                "both entries own the running window");
    expect_true(a->window == b->window && a->window == 501,
                "both entries report the same representative");
    expect_true(a->icon_ref[0] != 0 && b->icon_ref[0] != 0,
                "both entries can draw themselves without an external lookup");
    expect_true(a->path[0] != 0 && b->path[0] != 0,
                "both entries can be launched without an external lookup");

    expect_true(!a->pinned && a->pin_id == 0, "the unpinned entry differs only in its properties");
    expect_true(b->pinned && b->pin_id == 5, "the pinned entry differs only in its properties");
}

static void test_identity_survives_pinning_and_unpinning(void)
{
    reach_dock_feature_model model = {};
    reach_dock_feature_model_init(&model);
    uint32_t next_key = 1;

    reach_window_snapshot windows[1] = {make_window(401, "C:\\apps\\brave.exe", "")};
    uint32_t group_ids[1] = {7};
    reach_dock_feature_model_build_items(&model, &next_key, nullptr, 0, windows, group_ids, 1,
                                         matches_thunk, nullptr);
    expect_true(model.item_count == 1 && !model.items[0].pinned,
                "the running app starts unpinned");
    uint32_t running_key = reach_dock_item_key_at(&model, 0);

    reach_pinned_app_model pins[1] = {make_pin(5, "C:\\apps\\brave.exe")};
    reach_dock_feature_model_build_items(&model, &next_key, pins, 1, windows, group_ids, 1,
                                         matches_thunk, nullptr);
    expect_true(model.item_count == 1, "pinning a running app does not duplicate it");
    expect_true(model.items[0].pinned && model.items[0].pin_id == 5,
                "pinning sets the properties on the same entry");
    expect_true(reach_dock_item_key_at(&model, 0) == running_key,
                "an app keeps its identity, and so its dock slot, when it is pinned");

    reach_dock_feature_model_build_items(&model, &next_key, nullptr, 0, windows, group_ids, 1,
                                         matches_thunk, nullptr);
    expect_true(reach_dock_item_key_at(&model, 0) == running_key,
                "and keeps it again when it is unpinned");
    expect_true(!model.items[0].pinned && model.items[0].pin_id == 0,
                "unpinning clears the properties without changing the entry");
}

static void test_capacity_keeps_all_pinned_and_running_groups(void)
{
    static reach_pinned_app_model pins[REACH_MAX_PINNED_APPS];
    static reach_window_snapshot windows[REACH_MAX_DOCK_RUNNING_APPS];
    static uint32_t group_ids[REACH_MAX_DOCK_RUNNING_APPS];
    reach_dock_feature_model model = {};
    reach_dock_feature_model_init(&model);
    uint32_t next_key = 1;

    for (size_t index = 0; index < REACH_MAX_PINNED_APPS; ++index)
    {
        char path[64] = {};
        snprintf(path, sizeof(path), "C:\\pins\\pin_%zu.exe", index);
        pins[index] = make_pin((uint32_t)(index + 1), path);
    }
    for (size_t index = 0; index < REACH_MAX_DOCK_RUNNING_APPS; ++index)
    {
        char path[64] = {};
        snprintf(path, sizeof(path), "C:\\running\\app_%zu.exe", index);
        windows[index] = make_window((uintptr_t)(index + 1000), path, "");
        group_ids[index] = (uint32_t)(index + 1000);
    }

    reach_dock_feature_model_build_items(&model, &next_key, pins, REACH_MAX_PINNED_APPS, windows, group_ids,
                                         REACH_MAX_DOCK_RUNNING_APPS, matches_thunk, nullptr);

    expect_true(REACH_MAX_PINNED_APPS == 96, "configured pin capacity is 96");
    expect_true(REACH_MAX_DOCK_RUNNING_APPS == 96, "running Dock app capacity is 96");
    expect_true(REACH_MAX_DOCK_ITEMS == 192, "combined Dock capacity is 192");
    expect_true(model.item_count == REACH_MAX_DOCK_ITEMS,
                "all pinned and running groups fit together");
    expect_true(model.items[REACH_MAX_PINNED_APPS - 1].pinned == 1, "last pinned item is retained");
    expect_true(model.items[REACH_MAX_PINNED_APPS].pinned == 0,
                "first running item follows the pins");
}

static void test_fit_metrics_keep_native_size_until_overflow(void)
{
    reach_dock_fit_result fit = reach_dock_fit_metrics(64.0f, 40.0f, 12.0f, 1.0f, 600.0f, 10.0f);
    expect_near(fit.scale, 1.0f, 0.0001f, "fitting content stays at native scale");
    expect_near(fit.width, 589.6f, 0.0001f,
                "native width includes border and equal enlarged outer padding");
    expect_near(fit.height, 64.0f, 0.0001f, "native height is unchanged");

    reach_dock_fit_result wider_border =
        reach_dock_fit_metrics(64.0f, 40.0f, 12.0f, 3.0f, 600.0f, 10.0f);
    expect_near(wider_border.width - fit.width, 4.0f, 0.0001f,
                "Dock width derives both border sides from the runtime thickness");
}

static void test_fit_metrics_scale_every_dimension_without_a_minimum(void)
{
    reach_dock_fit_result half = reach_dock_fit_metrics(64.0f, 40.0f, 12.0f, 1.0f, 294.8f, 10.0f);
    expect_near(half.scale, 0.5f, 0.0001f, "overflow resolves to the exact fit scale");
    expect_near(half.width, 294.8f, 0.0001f, "overflow width exactly fits the monitor");
    expect_near(half.height, 32.0f, 0.0001f, "Dock height scales uniformly");
    expect_near(half.icon_size, 20.0f, 0.0001f, "icons scale uniformly");
    expect_near(half.gap, 6.0f, 0.0001f, "gaps scale uniformly");

    reach_dock_fit_result tiny = reach_dock_fit_metrics(64.0f, 40.0f, 12.0f, 1.0f, 1.0f, 10.0f);
    expect_near(tiny.width, 1.0f, 0.0001f, "arbitrarily narrow monitors still fit exactly");
    expect_true(tiny.scale > 0.0f && tiny.scale < half.scale,
                "fit calculation has no minimum scale clamp");
}

static void test_adaptive_layout_rebuild_keeps_native_height(void)
{
    reach_dock *dock = nullptr;
    expect_true(reach_dock_create(&dock) == REACH_OK && dock != nullptr,
                "Dock creation succeeds for adaptive rebuild test");
    if (dock == nullptr)
    {
        return;
    }

    reach_pinned_app_model pins[10] = {};
    for (size_t index = 0; index < 10; ++index)
    {
        char path[64] = {};
        snprintf(path, sizeof(path), "C:\\pins\\pin_%zu.exe", index);
        pins[index] = make_pin((uint32_t)(index + 1), path);
    }

    reach_dock_model model = {};
    reach_dock_model_defaults(&model);
    reach_ui_layout_input input = {};
    input.work_area = {0.0f, 0.0f, 358.8f, 1080.0f};
    input.dpi_scale = 1.0f;
    reach_dock_layout layout = {};
    expect_true(reach_dock_layout_compute(&model, &input, &layout) == REACH_OK,
                "adaptive rebuild test computes native Dock layout");

    reach_dock_build_context context = {};
    context.theme = reach_theme_default();
    context.dpi_scale = 1.0f;
    context.icon_size = model.icon_size;
    context.gap = model.gap;
    context.pinned_apps = pins;
    context.pinned_app_count = 10;

    reach_dock_build_layout(dock, &context, &layout);
    const reach_rect_f32 first_bounds = layout.bounds;
    const reach_rect_f32 first_slot = layout.app_slots[0];
    const float first_scale = layout.content_scale;

    reach_dock_build_layout(dock, &context, &layout);

    expect_near(first_scale, 0.5f, 0.0001f, "test Dock enters adaptive fitting");
    expect_near(layout.native_height, 64.0f, 0.0001f, "adaptive rebuild preserves native height");
    expect_near(layout.bounds.x, first_bounds.x, 0.0001f,
                "adaptive rebuild preserves fitted horizontal position");
    expect_near(layout.bounds.y, first_bounds.y, 0.0001f,
                "adaptive rebuild preserves fitted vertical position");
    expect_near(layout.bounds.width, first_bounds.width, 0.0001f,
                "adaptive rebuild preserves fitted width");
    expect_near(layout.bounds.height, first_bounds.height, 0.0001f,
                "adaptive rebuild does not scale height twice");
    expect_near(layout.app_slots[0].x, first_slot.x, 0.0001f,
                "adaptive rebuild preserves item horizontal geometry");
    expect_near(layout.app_slots[0].y, first_slot.y, 0.0001f,
                "adaptive rebuild preserves item vertical geometry");

    reach_dock_destroy(dock);
}

static void test_dock_metrics_are_spaced_before_any_configuration_arrives(void)
{
    reach_dock *dock = nullptr;
    expect_true(reach_dock_create(&dock) == REACH_OK, "metrics test creates a dock");
    if (dock == nullptr)
    {
        return;
    }

    reach_dock_arrange_context arrange = {};
    arrange.theme = reach_theme_default();
    arrange.monitor_bounds = {0.0f, 0.0f, 1920.0f, 1080.0f};
    arrange.dpi_scale = 1.0f;

    reach_pinned_app_model pins[2] = {make_pin(1, "C:\\apps\\a.exe"),
                                      make_pin(2, "C:\\apps\\b.exe")};
    reach_dock_apply_pinned_apps(dock, pins, 2);
    (void)reach_dock_arrange(dock, &arrange);

    const reach_dock_layout *layout = reach_dock_arranged_layout(dock);
    expect_true(layout != nullptr && layout->app_slot_count == 2,
                "arranging the dock lays out every item");
    expect_true(layout != nullptr && layout->bounds.height > 0.0f,
                "a dock that has never seen configuration still has its default height");
    if (layout != nullptr && layout->app_slot_count == 2)
    {
        expect_true(layout->app_slots[0].width > 0.0f,
                    "dock slots keep their icon size without configuration");
        expect_true(layout->app_slots[1].x > layout->app_slots[0].x + layout->app_slots[0].width,
                    "dock slots keep the gap between them without configuration");
    }

    reach_dock_destroy(dock);
}

static void test_pinned_reorder_survives_the_config_round_trip(void)
{
    reach_dock *dock = nullptr;
    expect_true(reach_dock_create(&dock) == REACH_OK, "reorder test creates a dock");
    if (dock == nullptr)
    {
        return;
    }

    reach_pinned_app_model pins[2] = {make_pin(1, "C:\\apps\\a.exe"),
                                      make_pin(2, "C:\\apps\\b.exe")};
    reach_dock_apply_pinned_apps(dock, pins, 2);

    reach_dock_arrange_context arrange = {};
    arrange.theme = reach_theme_default();
    arrange.monitor_bounds = {0.0f, 0.0f, 1920.0f, 1080.0f};
    arrange.dpi_scale = 1.0f;
    (void)reach_dock_arrange(dock, &arrange);

    /* the user drags the second pin in front of the first */
    uint32_t swapped[2] = {reach_dock_order_key_at(dock, 1), reach_dock_order_key_at(dock, 0)};
    reach_dock_restore_order(dock, swapped, 2);
    reach_dock_mark_items_changed(dock);
    (void)reach_dock_arrange(dock, &arrange);
    expect_true(reach_test_utf16_equals_ascii(reach_dock_item_at(dock, 0)->path,
                                              "C:\\apps\\b.exe"),
                "the dragged pin takes the first slot straight away");

    /* config persists that move and reissues pin ids on the way back */
    reach_pinned_app_model reordered[2] = {make_pin(7, "C:\\apps\\b.exe"),
                                           make_pin(8, "C:\\apps\\a.exe")};
    reach_dock_apply_pinned_apps(dock, reordered, 2);
    (void)reach_dock_arrange(dock, &arrange);
    expect_true(reach_test_utf16_equals_ascii(reach_dock_item_at(dock, 0)->path,
                                              "C:\\apps\\b.exe"),
                "the dragged order survives the config round trip that reissues pin ids");
    expect_true(reach_dock_item_at(dock, 0)->pin_id == 7,
                "and the entry picks up the pin id the config store reissued");

    reach_dock_destroy(dock);
}

int main(void)
{
    test_dock_metrics_are_spaced_before_any_configuration_arrives();
    test_pinned_reorder_survives_the_config_round_trip();
    test_unpinned_windows_group_into_one_item();
    test_pinned_app_claims_matching_windows();
    test_key_stable_when_representative_closes();
    test_order_preserved_and_new_groups_append();
    test_same_path_different_aumid_stays_split();
    test_pinned_and_unpinned_entries_are_the_same_kind_of_thing();
    test_identity_survives_pinning_and_unpinning();
    test_capacity_keeps_all_pinned_and_running_groups();
    test_fit_metrics_keep_native_size_until_overflow();
    test_fit_metrics_scale_every_dimension_without_a_minimum();
    test_adaptive_layout_rebuild_keeps_native_height();
    return failures == 0 ? 0 : 1;
}
