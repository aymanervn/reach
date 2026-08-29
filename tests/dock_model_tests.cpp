#include "reach/support/util.h"
#include "reach/features/dock.h"

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

    reach_window_snapshot windows[3] = {
        make_window(101, "C:\\apps\\brave.exe", ""),
        make_window(102, "C:\\apps\\brave.exe", ""),
        make_window(103, "C:\\apps\\brave.exe", ""),
    };
    uint32_t group_ids[3] = {7, 7, 7};

    reach_dock_feature_model_build_items(&model, nullptr, 0, windows, group_ids, 3, matches_thunk,
                                         nullptr);

    expect_true(model.item_count == 1, "three same-app windows collapse into one item");
    expect_true(model.items[0].pinned == 0, "grouped item is unpinned");
    expect_true(model.items[0].app_id == 7, "item carries the group id");
    expect_true(model.items[0].window == 101, "first window is the representative");

    reach_dock_order_key key = {0, 7};
    expect_true(reach_dock_feature_model_find_order_key(&model, key) == 0,
                "order key is the group id");
}

static void test_pinned_app_claims_matching_windows(void)
{
    reach_dock_feature_model model = {};
    reach_dock_feature_model_init(&model);

    reach_pinned_app_model pins[1] = {make_pin(5, "C:\\apps\\brave.exe")};
    reach_window_snapshot windows[3] = {
        make_window(101, "C:\\apps\\brave.exe", ""),
        make_window(102, "C:\\apps\\brave.exe", ""),
        make_window(103, "C:\\apps\\code.exe", ""),
    };
    uint32_t group_ids[3] = {7, 7, 8};

    reach_dock_feature_model_build_items(&model, pins, 1, windows, group_ids, 3, matches_thunk,
                                         nullptr);

    expect_true(model.item_count == 2, "one pinned item plus one unpinned group");
    expect_true(model.items[0].pinned == 1 && model.items[0].app_id == 5,
                "pinned item keyed by pin id");
    expect_true(model.items[0].window == 101, "pinned item takes first matching window");
    expect_true(model.items[1].pinned == 0 && model.items[1].app_id == 8,
                "leftover window forms unpinned group");
}

static void test_key_stable_when_representative_closes(void)
{
    reach_dock_feature_model model = {};
    reach_dock_feature_model_init(&model);

    reach_window_snapshot windows[3] = {
        make_window(101, "C:\\apps\\brave.exe", ""),
        make_window(102, "C:\\apps\\brave.exe", ""),
        make_window(103, "C:\\apps\\code.exe", ""),
    };
    uint32_t group_ids[3] = {7, 7, 8};
    reach_dock_feature_model_build_items(&model, nullptr, 0, windows, group_ids, 3, matches_thunk,
                                         nullptr);
    expect_true(model.item_count == 2, "two groups before churn");

    reach_window_snapshot after[2] = {windows[1], windows[2]};
    uint32_t after_group_ids[2] = {7, 8};
    reach_dock_feature_model_build_items(&model, nullptr, 0, after, after_group_ids, 2,
                                         matches_thunk, nullptr);

    reach_dock_order_key brave_key = {0, 7};
    expect_true(model.item_count == 2, "groups survive representative closing");
    expect_true(reach_dock_feature_model_find_order_key(&model, brave_key) == 0,
                "group keeps its dock position when representative closes");
    expect_true(model.items[0].app_id == 7 && model.items[0].window == 102,
                "surviving window becomes the representative");
}

static void test_order_preserved_and_new_groups_append(void)
{
    reach_dock_feature_model model = {};
    reach_dock_feature_model_init(&model);

    reach_pinned_app_model pins[1] = {make_pin(5, "C:\\apps\\term.exe")};
    reach_window_snapshot windows[2] = {
        make_window(201, "C:\\apps\\brave.exe", ""),
        make_window(202, "C:\\apps\\code.exe", ""),
    };
    uint32_t group_ids[2] = {7, 8};
    reach_dock_feature_model_build_items(&model, pins, 1, windows, group_ids, 2, matches_thunk,
                                         nullptr);
    expect_true(model.item_count == 3, "pinned plus two unpinned groups");

    reach_dock_feature_model_move_order(&model, 2, 0);
    reach_window_snapshot with_new[3] = {windows[0], windows[1],
                                         make_window(203, "C:\\apps\\mail.exe", "")};
    uint32_t new_group_ids[3] = {7, 8, 9};
    reach_dock_feature_model_build_items(&model, pins, 1, with_new, new_group_ids, 3, matches_thunk,
                                         nullptr);

    reach_dock_order_key code_key = {0, 8};
    reach_dock_order_key mail_key = {0, 9};
    expect_true(reach_dock_feature_model_find_order_key(&model, code_key) == 0,
                "moved group keeps its position across rebuild");
    expect_true(reach_dock_feature_model_find_order_key(&model, mail_key) == 3,
                "new group appends at the end");
}

static void test_same_path_different_aumid_stays_split(void)
{
    reach_dock_feature_model model = {};
    reach_dock_feature_model_init(&model);

    reach_window_snapshot windows[2] = {
        make_window(301, "C:\\apps\\brave.exe", "Brave._crx_abc"),
        make_window(302, "C:\\apps\\brave.exe", "Brave._crx_xyz"),
    };
    uint32_t group_ids[2] = {7, 8};
    reach_dock_feature_model_build_items(&model, nullptr, 0, windows, group_ids, 2, matches_thunk,
                                         nullptr);

    expect_true(model.item_count == 2, "distinct group ids produce distinct items");
}

static void test_key_equality_semantics(void)
{
    reach_dock_order_key pinned_key = {1, 7};
    reach_dock_order_key unpinned_key = {0, 7};
    reach_dock_order_key zero_key = {0, 0};

    expect_true(!reach_dock_key_equal(&pinned_key, &unpinned_key),
                "pin ids and group ids live in separate namespaces");
    expect_true(!reach_dock_key_equal(&zero_key, &zero_key), "zero id never matches");
    expect_true(reach_dock_key_equal(&unpinned_key, &unpinned_key), "same key matches itself");
}

static void test_capacity_keeps_all_pinned_and_running_groups(void)
{
    static reach_pinned_app_model pins[REACH_MAX_PINNED_APPS];
    static reach_window_snapshot windows[REACH_MAX_DOCK_RUNNING_APPS];
    static uint32_t group_ids[REACH_MAX_DOCK_RUNNING_APPS];
    reach_dock_feature_model model = {};
    reach_dock_feature_model_init(&model);

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

    reach_dock_feature_model_build_items(&model, pins, REACH_MAX_PINNED_APPS, windows, group_ids,
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

static void test_pin_snapshot_rebases_the_dock_order_by_path(void)
{
    reach_dock *dock = nullptr;
    expect_true(reach_dock_create(&dock) == REACH_OK, "dock capsule is created");
    if (dock == nullptr)
    {
        return;
    }

    reach_pinned_app_model before[2] = {make_pin(1, "C:\\apps\\a.exe"),
                                        make_pin(2, "C:\\apps\\b.exe")};
    reach_dock_apply_pinned_apps(dock, before, 2);

    reach_dock_order_key order[3] = {};
    order[0].pinned = 1;
    order[0].app_id = 1;
    order[1].pinned = 0;
    order[1].app_id = 90;
    order[2].pinned = 1;
    order[2].app_id = 2;
    reach_dock_restore_order(dock, order, 3);

    reach_pinned_app_model after[2] = {make_pin(7, "C:\\apps\\b.exe"),
                                       make_pin(8, "C:\\apps\\a.exe")};
    reach_dock_apply_pinned_apps(dock, after, 2);

    expect_true(reach_dock_order_count(dock) == 3,
                "rebasing the pin snapshot keeps every dock order slot");
    expect_true(reach_dock_order_key_at(dock, 0).pinned == 1 &&
                    reach_dock_order_key_at(dock, 0).app_id == 8,
                "a still-pinned app keeps its slot under its reissued pin id");
    expect_true(reach_dock_order_key_at(dock, 2).pinned == 1 &&
                    reach_dock_order_key_at(dock, 2).app_id == 7,
                "pin ids are rebased by path, not by position");
    expect_true(reach_dock_order_key_at(dock, 1).pinned == 0 &&
                    reach_dock_order_key_at(dock, 1).app_id == 90,
                "an unpinned running group is left alone by a pin snapshot");
    expect_true(reach_dock_take_items_changed(dock),
                "a pin snapshot marks the dock items for rebuild");

    reach_dock_destroy(dock);
}

int main(void)
{
    test_pin_snapshot_rebases_the_dock_order_by_path();
    test_unpinned_windows_group_into_one_item();
    test_pinned_app_claims_matching_windows();
    test_key_stable_when_representative_closes();
    test_order_preserved_and_new_groups_append();
    test_same_path_different_aumid_stays_split();
    test_key_equality_semantics();
    test_capacity_keeps_all_pinned_and_running_groups();
    test_fit_metrics_keep_native_size_until_overflow();
    test_fit_metrics_scale_every_dimension_without_a_minimum();
    test_adaptive_layout_rebuild_keeps_native_height();
    return failures == 0 ? 0 : 1;
}
