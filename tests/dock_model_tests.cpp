#include "reach/support/util.h"
#include "reach/core/config.h"
#include "reach/features/dock.h"
#include "test_utf16.h"
#include "../src/features/dock/dock_interaction.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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
    uint16_t runtime_path[260] = {};
    reach_copy_ascii_to_utf16(runtime_path, 260, path);
    reach_application_identity_add_runtime_path(&window.identity, runtime_path);
    reach_copy_ascii_to_utf16(window.identity.app_user_model_id, 260, aumid);
    return window;
}

static reach_window_snapshot dock_test_window;
static reach_rect_f32 dock_test_window_bounds;
static size_t dock_test_window_count;

static size_t dock_test_window_count_op(const reach_window_manager *manager)
{
    (void)manager;
    return dock_test_window_count;
}

static reach_result dock_test_window_at_op(const reach_window_manager *manager, size_t index,
                                           reach_window_snapshot *out_window)
{
    (void)manager;
    if (index != 0 || dock_test_window_count == 0 || out_window == nullptr)
    {
        return REACH_ERROR;
    }
    *out_window = dock_test_window;
    return REACH_OK;
}

static reach_result dock_test_outer_bounds_op(const reach_window_manager *manager,
                                              reach_window_id window_id, reach_rect_f32 *out_bounds)
{
    (void)manager;
    if (window_id != dock_test_window.id || out_bounds == nullptr)
    {
        return REACH_ERROR;
    }
    *out_bounds = dock_test_window_bounds;
    return REACH_OK;
}

static reach_window_tracking *make_dock_test_window_service(void)
{
    reach_window_manager_port port = {};
    port.ops.window_count = dock_test_window_count_op;
    port.ops.window_at = dock_test_window_at_op;
    port.ops.outer_bounds = dock_test_outer_bounds_op;
    reach_window_tracking *service = nullptr;
    if (reach_window_tracking_create(port, &service) != REACH_OK)
    {
        return nullptr;
    }
    return service;
}

static void refresh_dock_test_window(reach_window_tracking *service, float x, float width)
{
    dock_test_window_count = 1;
    dock_test_window_bounds = {x, 500.0f, width, 600.0f};
    (void)reach_window_tracking_refresh(service, nullptr);
}

static reach_pinned_app_model make_pin(uint32_t id, const char *path)
{
    reach_pinned_app_model app = {};
    app.id = id;
    app.application.launch.kind = REACH_APPLICATION_LAUNCH_EXECUTABLE;
    reach_copy_ascii_to_utf16(app.application.launch.path, 260, path);
    uint16_t runtime_path[260] = {};
    reach_copy_ascii_to_utf16(runtime_path, 260, path);
    reach_application_identity_add_runtime_path(&app.application.identity, runtime_path);
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

    reach_dock_feature_model_build_items(&model, &next_key, nullptr, 0, windows, group_ids, 3,
                                         matches_thunk, nullptr);

    expect_true(model.item_count == 1, "three same-app windows collapse into one item");
    expect_true(model.items[0].pinned == 0, "grouped item is unpinned");
    expect_true(model.items[0].instance_count == 3, "the item owns all three instances");
    expect_true(model.items[0].window == 101, "first window is the representative");
    expect_true(
        reach_dock_feature_model_find_order_key(&model, reach_dock_item_key_at(&model, 0)) == 0,
        "the item holds the first slot in the order");
    reach_dock_feature_model_destroy(&model);
}

static void test_item_keeps_every_open_window_as_an_instance(void)
{
    reach_dock_feature_model model = {};
    reach_dock_feature_model_init(&model);
    uint32_t next_key = 1;

    static reach_window_snapshot windows[REACH_MAX_OPEN_WINDOWS];
    uint32_t group_ids[REACH_MAX_OPEN_WINDOWS] = {};
    for (size_t index = 0; index < REACH_MAX_OPEN_WINDOWS; ++index)
    {
        windows[index] = make_window(1000 + index, "C:\\apps\\brave.exe", "");
        group_ids[index] = 7;
    }

    reach_dock_feature_model_build_items(&model, &next_key, nullptr, 0, windows, group_ids,
                                         REACH_MAX_OPEN_WINDOWS, matches_thunk, nullptr);

    expect_true(model.item_count == 1 && model.items[0].instance_count == REACH_MAX_OPEN_WINDOWS,
                "one app keeps every open window so close all reaches them all");
    reach_dock_feature_model_destroy(&model);
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

    reach_dock_feature_model_build_items(&model, &next_key, pins, 1, windows, group_ids, 3,
                                         matches_thunk, nullptr);

    expect_true(model.item_count == 2, "one pinned item plus one unpinned group");
    expect_true(model.items[0].pinned == 1 && model.items[0].pin_id == 5,
                "a pinned entry carries its pin id as a property");
    expect_true(model.items[0].window == 101, "pinned item takes first matching window");
    expect_true(model.items[1].pinned == 0 && model.items[1].pin_id == 0,
                "leftover window forms an unpinned entry");
    reach_dock_feature_model_destroy(&model);
}

static void test_shortcut_pin_matches_executable(void)
{
    reach_dock_feature_model model = {};
    reach_dock_feature_model_init(&model);
    uint32_t next_key = 1;
    reach_pinned_app_model pin = make_pin(6, "C:\\apps\\zed.exe");
    pin.application.launch.kind = REACH_APPLICATION_LAUNCH_SHORTCUT;
    reach_copy_ascii_to_utf16(pin.application.launch.path, 260, "C:\\Pins\\Zed.lnk");
    reach_copy_ascii_to_utf16(pin.application.identity.app_user_model_id, 260, "ZedIndustries.Zed");
    reach_window_snapshot window = make_window(104, "C:\\apps\\zed.exe", "");
    uint32_t group_id = 9;
    reach_dock_feature_model_build_items(&model, &next_key, &pin, 1, &window, &group_id, 1,
                                         matches_thunk, nullptr);
    expect_true(model.item_count == 1 && model.items[0].window == 104,
                "resolved shortcut executable claims the running window");
    reach_dock_feature_model_destroy(&model);
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
    reach_dock_feature_model_build_items(&model, &next_key, nullptr, 0, windows, group_ids, 3,
                                         matches_thunk, nullptr);
    expect_true(model.item_count == 2, "two groups before churn");

    reach_window_snapshot after[2] = {windows[1], windows[2]};
    uint32_t after_group_ids[2] = {7, 8};
    reach_dock_feature_model_build_items(&model, &next_key, nullptr, 0, after, after_group_ids, 2,
                                         matches_thunk, nullptr);

    expect_true(model.item_count == 2, "groups survive representative closing");
    expect_true(
        reach_dock_feature_model_find_order_key(&model, reach_dock_item_key_at(&model, 0)) == 0,
        "group keeps its dock position when representative closes");
    expect_true(model.items[0].window == 102, "surviving window becomes the representative");
    reach_dock_feature_model_destroy(&model);
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
    reach_dock_feature_model_build_items(&model, &next_key, pins, 1, windows, group_ids, 2,
                                         matches_thunk, nullptr);
    expect_true(model.item_count == 3, "pinned plus two unpinned groups");

    reach_dock_feature_model_move_order(&model, 2, 0);
    reach_window_snapshot with_new[3] = {windows[0], windows[1],
                                         make_window(203, "C:\\apps\\mail.exe", "")};
    uint32_t new_group_ids[3] = {7, 8, 9};
    reach_dock_feature_model_build_items(&model, &next_key, pins, 1, with_new, new_group_ids, 3,
                                         matches_thunk, nullptr);

    expect_true(
        reach_test_utf16_equals_ascii(model.items[0].application.launch.path, "C:\\apps\\code.exe"),
        "moved group keeps its position across rebuild");
    expect_true(
        reach_test_utf16_equals_ascii(model.items[3].application.launch.path, "C:\\apps\\mail.exe"),
        "new group appends at the end");
    reach_dock_feature_model_destroy(&model);
}

static void test_same_path_different_aumid_groups(void)
{
    reach_dock_feature_model model = {};
    reach_dock_feature_model_init(&model);
    uint32_t next_key = 1;

    reach_window_snapshot windows[2] = {
        make_window(301, "C:\\apps\\brave.exe", "Brave._crx_abc"),
        make_window(302, "C:\\apps\\brave.exe", "Brave._crx_xyz"),
    };
    uint32_t group_ids[2] = {7, 7};
    reach_dock_feature_model_build_items(&model, &next_key, nullptr, 0, windows, group_ids, 2,
                                         matches_thunk, nullptr);

    expect_true(model.item_count == 1, "shared executable produces one item");
    reach_dock_feature_model_destroy(&model);
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
    reach_copy_ascii_to_utf16(pins[0].application.identity.app_user_model_id, 260, "Brave.App");
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
    expect_true(reach_dock_item_identity_matches(a, &b->application.identity),
                "both entries describe the same application");
    expect_true(a->instance_count == b->instance_count && a->instance_count == 1,
                "both entries own the running window");
    expect_true(a->window == b->window && a->window == 501,
                "both entries report the same representative");
    expect_true(a->application.icon_ref[0] != 0 && b->application.icon_ref[0] != 0,
                "both entries can draw themselves without an external lookup");
    expect_true(a->application.launch.path[0] != 0 && b->application.launch.path[0] != 0,
                "both entries can be launched without an external lookup");

    expect_true(!a->pinned && a->pin_id == 0, "the unpinned entry differs only in its properties");
    expect_true(b->pinned && b->pin_id == 5, "the pinned entry differs only in its properties");
    reach_dock_feature_model_destroy(&unpinned);
    reach_dock_feature_model_destroy(&pinned);
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
    expect_true(model.item_count == 1 && !model.items[0].pinned, "the running app starts unpinned");
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
    reach_dock_feature_model_destroy(&model);
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

    reach_dock_feature_model_build_items(&model, &next_key, pins, REACH_MAX_PINNED_APPS, windows,
                                         group_ids, REACH_MAX_DOCK_RUNNING_APPS, matches_thunk,
                                         nullptr);

    expect_true(REACH_MAX_PINNED_APPS == 96, "configured pin capacity is 96");
    expect_true(REACH_MAX_DOCK_RUNNING_APPS == 96, "running Dock app capacity is 96");
    expect_true(REACH_MAX_DOCK_ITEMS == 192, "combined Dock capacity is 192");
    expect_true(model.item_count == REACH_MAX_DOCK_ITEMS,
                "all pinned and running groups fit together");
    expect_true(model.items[REACH_MAX_PINNED_APPS - 1].pinned == 1, "last pinned item is retained");
    expect_true(model.items[REACH_MAX_PINNED_APPS].pinned == 0,
                "first running item follows the pins");
    reach_dock_feature_model_destroy(&model);
}

static void test_fit_metrics_keep_native_size_until_overflow(void)
{
    reach_dock_fit_result fit =
        reach_dock_fit_metrics(64.0f, 40.0f, 12.0f, 1.0f, 600.0f, 10.0f, 1.0f);
    expect_near(fit.scale, 1.0f, 0.0001f, "fitting content stays at native scale");
    expect_near(fit.width, 590.0f, 0.0001f,
                "native width encloses the Dock on a whole physical pixel");
    expect_near(fit.outer_padding, 14.0f, 0.0001f,
                "pixel alignment slack is shared by both outer paddings");
    expect_near(fit.height, 64.0f, 0.0001f, "native height is unchanged");

    reach_dock_fit_result wider_border =
        reach_dock_fit_metrics(64.0f, 40.0f, 12.0f, 3.0f, 600.0f, 10.0f, 1.0f);
    expect_near(wider_border.width - fit.width, 4.0f, 0.0001f,
                "Dock width derives both border sides from the runtime thickness");
}

static void test_fit_metrics_scale_every_dimension_without_a_minimum(void)
{
    reach_dock_fit_result half =
        reach_dock_fit_metrics(64.0f, 40.0f, 12.0f, 1.0f, 294.8f, 10.0f, 1.0f);
    expect_near(half.scale, 0.5f, 0.0001f, "overflow resolves to the exact fit scale");
    expect_near(half.width, 294.8f, 0.0001f, "overflow width exactly fits the monitor");
    expect_near(half.height, 32.0f, 0.0001f, "Dock height scales uniformly");
    expect_near(half.icon_size, 20.0f, 0.0001f, "icons scale uniformly");
    expect_near(half.gap, 6.0f, 0.0001f, "gaps scale uniformly");

    reach_dock_fit_result tiny =
        reach_dock_fit_metrics(64.0f, 40.0f, 12.0f, 1.0f, 1.0f, 10.0f, 1.0f);
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
        float border = reach_theme_border_thickness(arrange.theme, arrange.dpi_scale);
        float left_padding = layout->trigger_button.x - border;
        const reach_rect_f32 *last = &layout->app_slots[layout->app_slot_count - 1];
        float right_padding = layout->bounds.width - border - last->x - last->width;
        expect_near(right_padding, left_padding, 0.0001f,
                    "Dock content has identical left and right padding");
    }

    reach_dock_destroy(dock);
}

static const reach_dock_layout *arrange_one_pin_dock(reach_dock *dock, float size)
{
    reach_pinned_app_model pin = make_pin(7, "C:\\apps\\a.exe");
    reach_dock_apply_pinned_apps(dock, &pin, 1);
    if (size > 0.0f)
    {
        reach_dock_apply_config(dock, 0.0f, (int32_t)size);
    }

    reach_dock_arrange_context arrange = {};
    arrange.theme = reach_theme_default();
    arrange.monitor_bounds = {0.0f, 0.0f, 1920.0f, 1080.0f};
    arrange.dpi_scale = 1.0f;
    (void)reach_dock_arrange(dock, &arrange);

    reach_feature_tick_result tick = {};
    reach_dock_capsule_ops()->tick(dock, 1.0, &tick);
    const reach_dock_layout *layout = reach_dock_arranged_layout(dock);
    return layout != nullptr && layout->app_slot_count == 1 ? layout : nullptr;
}

static void test_resize_edge_hits_the_top_rim(void)
{
    reach_dock *dock = nullptr;
    expect_true(reach_dock_create(&dock) == REACH_OK, "resize edge test creates a dock");
    if (dock == nullptr)
    {
        return;
    }
    const reach_dock_layout *layout = arrange_one_pin_dock(dock, 0.0f);
    expect_true(layout != nullptr, "resize edge test arranges one pinned item");
    if (layout != nullptr)
    {
        reach_rect_f32 edge = reach_dock_resize_edge_rect(layout);
        expect_true(edge.height > 0.0f, "the dock has a resize band along its top rim");
        reach_dock_hit_result hit = reach_dock_hit_test(
            layout, (int32_t)(edge.x + edge.width * 0.5f), (int32_t)(edge.y + edge.height * 0.5f));
        expect_true(hit.type == REACH_DOCK_HIT_RESIZE_EDGE, "the band center is a resize edge hit");
        hit = reach_dock_hit_test(layout, (int32_t)(edge.x + edge.width * 0.5f),
                                  (int32_t)(edge.y + edge.height) + 1);
        expect_true(hit.type != REACH_DOCK_HIT_RESIZE_EDGE,
                    "a point just below the band is not a resize edge hit");
    }
    reach_dock_destroy(dock);
}

static void test_resize_edge_never_overlaps_icons(void)
{
    const float sizes[2] = {100.0f, 50.0f};
    for (size_t index = 0; index < 2; ++index)
    {
        reach_dock *dock = nullptr;
        expect_true(reach_dock_create(&dock) == REACH_OK, "resize overlap test creates a dock");
        if (dock == nullptr)
        {
            return;
        }
        const reach_dock_layout *layout = arrange_one_pin_dock(dock, sizes[index]);
        expect_true(layout != nullptr, "resize overlap test arranges one pinned item");
        if (layout != nullptr)
        {
            reach_rect_f32 edge = reach_dock_resize_edge_rect(layout);
            expect_true(edge.height > 0.0f, "the band is not empty at this size");
            const reach_rect_f32 *slot = &layout->app_slots[0];
            expect_true(edge.y + edge.height <= slot->y, "the band ends above the app slot");
            expect_true(edge.y + edge.height <= layout->trigger_button.y,
                        "the band ends above the trigger button");

            reach_dock_hit_result item = reach_dock_hit_test(
                layout, (int32_t)(slot->x + slot->width * 0.5f), (int32_t)ceilf(slot->y));
            expect_true(item.type == REACH_DOCK_HIT_ITEM && item.index == 0,
                        "the top of an app slot still hits the item");
            reach_dock_hit_result trigger = reach_dock_hit_test(
                layout, (int32_t)(layout->trigger_button.x + layout->trigger_button.width * 0.5f),
                (int32_t)ceilf(layout->trigger_button.y));
            expect_true(trigger.type == REACH_DOCK_HIT_TRIGGER,
                        "the top of the trigger button still hits the trigger");
        }
        reach_dock_destroy(dock);
    }
}

static void test_resize_edge_skips_rounded_ends(void)
{
    reach_dock *dock = nullptr;
    expect_true(reach_dock_create(&dock) == REACH_OK, "resize ends test creates a dock");
    if (dock == nullptr)
    {
        return;
    }
    const reach_dock_layout *layout = arrange_one_pin_dock(dock, 0.0f);
    expect_true(layout != nullptr, "resize ends test arranges one pinned item");
    if (layout != nullptr)
    {
        reach_rect_f32 edge = reach_dock_resize_edge_rect(layout);
        float radius = layout->bounds.height * reach_theme_default()->dock_corner_radius_ratio;
        /* The band lives in local space, where the pill starts at x = 0. */
        expect_true(edge.x >= radius - 0.5f, "the band starts after the left rounded end");
        expect_true(edge.x + edge.width <= layout->bounds.width - radius + 0.5f,
                    "the band stops before the right rounded end");
    }
    reach_dock_destroy(dock);
}

struct resize_route_log
{
    int32_t calls;
    int32_t last_percent;
};

static void record_size_committed(void *user, int32_t size_percent)
{
    resize_route_log *log = static_cast<resize_route_log *>(user);
    ++log->calls;
    log->last_percent = size_percent;
}

static reach_dock_arrange_context resize_arrange_context(float monitor_width)
{
    reach_dock_arrange_context arrange = {};
    arrange.theme = reach_theme_default();
    arrange.monitor_bounds = {0.0f, 0.0f, monitor_width, 1080.0f};
    arrange.dpi_scale = 1.0f;
    return arrange;
}

static reach_dock *make_resize_dock(resize_route_log *log, int32_t size, float monitor_width,
                                    size_t pin_count)
{
    reach_dock *dock = nullptr;
    if (reach_dock_create(&dock) != REACH_OK)
        return nullptr;
    reach_pinned_app_model pins[16] = {};
    for (size_t index = 0; index < pin_count; ++index)
    {
        char path[32];
        snprintf(path, sizeof(path), "C:\\apps\\app%zu.exe", index);
        pins[index] = make_pin((uint32_t)(index + 1), path);
    }
    reach_dock_apply_pinned_apps(dock, pins, pin_count);
    reach_dock_apply_config(dock, 0.0f, size);
    reach_dock_routes routes = {};
    routes.user = log;
    routes.size_committed = record_size_committed;
    reach_dock_set_routes(dock, &routes);

    reach_dock_arrange_context arrange = resize_arrange_context(monitor_width);
    (void)reach_dock_arrange(dock, &arrange);
    reach_feature_tick_result tick = {};
    reach_dock_capsule_ops()->tick(dock, 1.0, &tick);
    return dock;
}

static float resize_scale_now(const reach_dock *dock)
{
    return reach_dock_arranged_layout(dock)->content_scale;
}

static void resize_event(reach_dock *dock, reach_pointer_event_kind kind, int32_t x, int32_t y,
                         reach_capsule_pointer_result *result)
{
    reach_pointer_event pointer = {};
    pointer.kind = kind;
    pointer.button = REACH_POINTER_BUTTON_PRIMARY;
    pointer.x = x;
    pointer.y = y;
    reach_dock_capsule_ops()->handle_pointer(dock, &pointer, result);
}

struct resize_edge_point
{
    int32_t x;
    int32_t y;
    float native_height;
};

static resize_edge_point resize_edge_point_of(const reach_dock *dock)
{
    const reach_dock_layout *layout = reach_dock_arranged_layout(dock);
    reach_rect_f32 edge = reach_dock_resize_edge_rect(layout);
    resize_edge_point point = {};
    point.x = (int32_t)(layout->bounds.x + edge.x + edge.width * 0.5f);
    point.y = (int32_t)(layout->bounds.y + edge.y + edge.height * 0.5f);
    point.native_height = layout->native_height;
    return point;
}

/* Press the edge, drag to each dy (positive is upward) in turn, then release where the last
   move ended. */
static reach_capsule_pointer_result press_edge(reach_dock *dock, const float *dy_list, size_t count,
                                               int32_t dx = 0)
{
    resize_edge_point start = resize_edge_point_of(dock);
    reach_capsule_pointer_result result = {};
    resize_event(dock, REACH_POINTER_EVENT_DOWN, start.x, start.y, &result);
    int32_t y = start.y;
    for (size_t index = 0; index < count; ++index)
    {
        y = start.y - (int32_t)lroundf(dy_list[index]);
        resize_event(dock, REACH_POINTER_EVENT_MOVE, start.x + dx, y, &result);
    }
    resize_event(dock, REACH_POINTER_EVENT_UP, start.x + dx, y, &result);
    return result;
}

static void test_resize_drag_up_and_down_scales_proportionally(void)
{
    resize_route_log log = {};
    reach_dock *dock = make_resize_dock(&log, 80, 1920.0f, 1);
    expect_true(dock != nullptr, "resize drag test creates a dock");
    if (dock == nullptr)
        return;
    resize_edge_point start = resize_edge_point_of(dock);
    int32_t down_y = start.y + (int32_t)lroundf(0.10f * start.native_height);
    reach_capsule_pointer_result result = {};
    resize_event(dock, REACH_POINTER_EVENT_DOWN, start.x, start.y, &result);
    expect_true(result.handled && result.capture == 1 && result.sync_pointer_subscriptions,
                "pressing the edge is handled and captures the pointer");
    resize_event(dock, REACH_POINTER_EVENT_MOVE, start.x, down_y, &result);
    expect_true(result.relayout && result.redraw, "a resize move asks for relayout and redraw");
    reach_dock_arrange_context arrange = resize_arrange_context(1920.0f);
    (void)reach_dock_arrange(dock, &arrange);
    expect_near(resize_scale_now(dock), 0.70f, 0.011f, "dragging down by 10% shrinks to 70%");
    resize_event(dock, REACH_POINTER_EVENT_UP, start.x, down_y, &result);
    expect_true(result.capture == -1, "release lets go of the pointer");
    /* The pointer moves in whole pixels, so the percent is the 10% drag rounded to one. */
    expect_true(log.calls == 1 && abs(log.last_percent - 70) <= 1, "release commits about 70");
    reach_dock_destroy(dock);
}

static void test_resize_clamps_to_min_and_max(void)
{
    resize_route_log log = {};
    reach_dock *dock = make_resize_dock(&log, 80, 1920.0f, 1);
    expect_true(dock != nullptr, "resize clamp test creates a dock");
    if (dock == nullptr)
        return;
    float height = resize_edge_point_of(dock).native_height;
    float down[1] = {-10.0f * height};
    (void)press_edge(dock, down, 1);
    expect_true(log.calls == 1 && log.last_percent == 50, "dragging far down clamps to 50");
    reach_dock_destroy(dock);

    log = {};
    dock = make_resize_dock(&log, 80, 1920.0f, 1);
    if (dock == nullptr)
        return;
    float up[1] = {10.0f * height};
    (void)press_edge(dock, up, 1);
    expect_true(log.calls == 1 && log.last_percent == 100, "dragging far up clamps to 100");
    reach_dock_destroy(dock);
}

static void test_resize_click_without_move_saves_nothing(void)
{
    resize_route_log log = {};
    reach_dock *dock = make_resize_dock(&log, 80, 1920.0f, 1);
    expect_true(dock != nullptr, "resize click test creates a dock");
    if (dock == nullptr)
        return;
    float before = resize_scale_now(dock);
    reach_capsule_pointer_result result = press_edge(dock, nullptr, 0);
    expect_true(log.calls == 0, "a click on the edge saves nothing");
    reach_dock_arrange_context arrange = resize_arrange_context(1920.0f);
    (void)reach_dock_arrange(dock, &arrange);
    expect_near(resize_scale_now(dock), before, 0.0001f, "a click on the edge keeps the size");
    expect_true(result.action.kind == REACH_FEATURE_ACTION_NONE,
                "a click on the edge does nothing");
    expect_true(!reach_draggable_tracking(&reach_dock_state_ptr(dock)->drag.gesture),
                "a click on the edge does not start a reorder");
    reach_dock_destroy(dock);
}

static void test_resize_press_never_activates_or_reorders(void)
{
    resize_route_log log = {};
    reach_dock *dock = make_resize_dock(&log, 80, 1920.0f, 2);
    expect_true(dock != nullptr, "resize isolation test creates a dock");
    if (dock == nullptr)
        return;
    resize_edge_point start = resize_edge_point_of(dock);
    reach_capsule_pointer_result result = {};
    int32_t unexpected_actions = 0;
    resize_event(dock, REACH_POINTER_EVENT_DOWN, start.x, start.y, &result);
    unexpected_actions += result.action.kind != REACH_FEATURE_ACTION_NONE;
    resize_event(dock, REACH_POINTER_EVENT_MOVE, start.x + 200, start.y, &result);
    unexpected_actions += result.action.kind != REACH_FEATURE_ACTION_NONE;
    expect_true(!reach_draggable_tracking(&reach_dock_state_ptr(dock)->drag.gesture),
                "a horizontal drag on the edge never starts a reorder");
    resize_event(dock, REACH_POINTER_EVENT_UP, start.x + 200, start.y, &result);
    unexpected_actions += result.action.kind != REACH_FEATURE_ACTION_NONE;
    expect_true(unexpected_actions == 0, "pressing the edge never opens a target or moves a pin");
    expect_true(log.calls == 0, "a horizontal drag changes no size");
    reach_dock_destroy(dock);
}

static void test_resize_cancel_restores_start_size(void)
{
    for (int32_t by_game_mode = 0; by_game_mode < 2; ++by_game_mode)
    {
        resize_route_log log = {};
        reach_dock *dock = make_resize_dock(&log, 80, 1920.0f, 1);
        expect_true(dock != nullptr, "resize cancel test creates a dock");
        if (dock == nullptr)
            return;
        const reach_feature_capsule_ops *ops = reach_dock_capsule_ops();
        resize_edge_point start = resize_edge_point_of(dock);
        reach_capsule_pointer_result result = {};
        resize_event(dock, REACH_POINTER_EVENT_DOWN, start.x, start.y, &result);
        resize_event(dock, REACH_POINTER_EVENT_MOVE, start.x,
                     start.y + (int32_t)lroundf(0.20f * start.native_height), &result);
        reach_dock_arrange_context arrange = resize_arrange_context(1920.0f);
        (void)reach_dock_arrange(dock, &arrange);
        expect_near(resize_scale_now(dock), 0.60f, 0.011f, "the drag changed the live size");
        if (by_game_mode)
            ops->on_game_mode(dock, 1);
        else
            resize_event(dock, REACH_POINTER_EVENT_CANCEL, start.x, start.y, &result);
        (void)reach_dock_arrange(dock, &arrange);
        expect_near(resize_scale_now(dock), 0.80f, 0.011f, "cancelling restores the start size");
        expect_true(log.calls == 0, "cancelling commits nothing");
        expect_true(!ops->pointer_sequence_active(dock), "cancelling ends the resize");
        reach_dock_destroy(dock);
    }
}

static void test_resize_width_capped_never_shrinks_setting(void)
{
    resize_route_log log = {};
    reach_dock *dock = make_resize_dock(&log, 100, 400.0f, 8);
    expect_true(dock != nullptr, "resize cap test creates a dock");
    if (dock == nullptr)
        return;
    float capped = resize_scale_now(dock);
    expect_true(capped < 0.99f, "the narrow monitor caps the dock below 100%");
    float height = resize_edge_point_of(dock).native_height;
    float up[1] = {2.0f * height};
    (void)press_edge(dock, up, 1);
    expect_true(log.calls == 0, "dragging up at the width cap saves nothing");
    expect_near(resize_scale_now(dock), capped, 0.011f, "the width cap holds the scale");

    float down[1] = {-0.5f * height};
    (void)press_edge(dock, down, 1);
    expect_true(log.calls == 1 && log.last_percent < 100 && log.last_percent >= 50,
                "dragging below the cap saves the lower percent");
    reach_dock_destroy(dock);
}

static void test_resize_below_minimum_cap_never_saves_out_of_range(void)
{
    resize_route_log log = {};
    reach_dock *dock = make_resize_dock(&log, 100, 400.0f, 12);
    expect_true(dock != nullptr, "resize low cap test creates a dock");
    if (dock == nullptr)
        return;
    expect_true(resize_scale_now(dock) < 0.5f, "the narrow monitor caps the dock below 50%");
    float height = resize_edge_point_of(dock).native_height;
    resize_edge_point click = resize_edge_point_of(dock);
    reach_capsule_pointer_result pressed = {};
    resize_event(dock, REACH_POINTER_EVENT_DOWN, click.x, click.y, &pressed);
    expect_true(pressed.capture == 1, "the click lands on the resize edge");
    resize_event(dock, REACH_POINTER_EVENT_UP, click.x, click.y, &pressed);
    expect_true(log.calls == 0, "a click at a sub-minimum cap saves nothing");
    reach_dock_arrange_context wide = resize_arrange_context(1920.0f);
    (void)reach_dock_arrange(dock, &wide);
    expect_near(resize_scale_now(dock), 1.0f, 0.0001f,
                "a click leaves the configured size, so the dock grows back on a wider monitor");
    reach_dock_arrange_context narrow = resize_arrange_context(400.0f);
    (void)reach_dock_arrange(dock, &narrow);

    float moves[3] = {3.0f, -3.0f, -0.5f * height};
    for (float dy : moves)
    {
        (void)press_edge(dock, &dy, 1);
        expect_true(log.calls == 0 || (log.last_percent >= REACH_CONFIG_DOCK_SIZE_MIN &&
                                       log.last_percent <= REACH_CONFIG_DOCK_SIZE_MAX),
                    "a sub-minimum cap never commits an out-of-range size");
    }
    reach_dock_destroy(dock);
}

static void test_resize_commit_then_config_echo_is_silent(void)
{
    resize_route_log log = {};
    reach_dock *dock = make_resize_dock(&log, 80, 1920.0f, 1);
    expect_true(dock != nullptr, "resize echo test creates a dock");
    if (dock == nullptr)
        return;
    float height = resize_edge_point_of(dock).native_height;
    float up[1] = {0.10f * height};
    (void)press_edge(dock, up, 1);
    expect_true(log.calls == 1 && abs(log.last_percent - 90) <= 1, "the drag commits about 90");
    reach_dock_apply_config(dock, 0.0f, log.last_percent);
    reach_dock_arrange_context arrange = resize_arrange_context(1920.0f);
    (void)reach_dock_arrange(dock, &arrange);
    reach_feature_tick_result tick = {};
    reach_dock_capsule_ops()->tick(dock, 1.0, &tick);
    expect_true(!reach_dock_capsule_ops()->needs_frame(dock),
                "echoing the committed size starts no animation");
    expect_near(resize_scale_now(dock), (float)log.last_percent / 100.0f, 0.001f,
                "the echoed size matches the live size");
    reach_dock_destroy(dock);
}

static void test_resize_config_echo_mid_drag_leaves_live_scale_alone(void)
{
    resize_route_log log = {};
    reach_dock *dock = make_resize_dock(&log, 80, 1920.0f, 1);
    expect_true(dock != nullptr, "mid-drag config test creates a dock");
    if (dock == nullptr)
        return;
    resize_edge_point start = resize_edge_point_of(dock);
    int32_t down_y = start.y + (int32_t)lroundf(0.10f * start.native_height);
    reach_capsule_pointer_result result = {};
    resize_event(dock, REACH_POINTER_EVENT_DOWN, start.x, start.y, &result);
    resize_event(dock, REACH_POINTER_EVENT_MOVE, start.x, down_y, &result);
    reach_dock_arrange_context arrange = resize_arrange_context(1920.0f);
    (void)reach_dock_arrange(dock, &arrange);
    const float live = resize_scale_now(dock);
    reach_dock_apply_config(dock, 0.0f, 60);
    reach_feature_tick_result tick = {};
    reach_dock_capsule_ops()->tick(dock, 1.0, &tick);
    (void)reach_dock_arrange(dock, &arrange);
    expect_near(resize_scale_now(dock), live, 0.001f,
                "a config change mid-drag does not move the live scale");
    /* Releasing at 60 equals the configured 60, so nothing is reported. */
    int32_t end_y = start.y + (int32_t)lroundf(0.20f * start.native_height);
    resize_event(dock, REACH_POINTER_EVENT_MOVE, start.x, end_y, &result);
    resize_event(dock, REACH_POINTER_EVENT_UP, start.x, end_y, &result);
    expect_true(log.calls == 0, "the release compares against the size configured mid-drag");
    reach_dock_destroy(dock);
}

static void test_resize_keeps_dock_revealed(void)
{
    resize_route_log log = {};
    reach_dock *dock = make_resize_dock(&log, 80, 1920.0f, 1);
    expect_true(dock != nullptr, "resize reveal test creates a dock");
    if (dock == nullptr)
        return;
    const reach_feature_capsule_ops *ops = reach_dock_capsule_ops();
    resize_edge_point start = resize_edge_point_of(dock);
    reach_capsule_pointer_result result = {};
    resize_event(dock, REACH_POINTER_EVENT_DOWN, start.x, start.y, &result);
    expect_true(ops->pointer_sequence_active(dock) && ops->wants_pointer_move(dock) &&
                    ops->pointer_capture_active(dock),
                "an active resize holds the pointer sequence, moves and capture");
    resize_event(dock, REACH_POINTER_EVENT_UP, start.x, start.y, &result);
    expect_true(!ops->pointer_sequence_active(dock) && !ops->wants_pointer_move(dock) &&
                    !ops->pointer_capture_active(dock),
                "releasing ends the resize sequence");
    reach_dock_destroy(dock);
}

static void test_pinned_item_pressable_release_opens_its_target(void)
{
    reach_dock *dock = nullptr;
    expect_true(reach_dock_create(&dock) == REACH_OK, "activation test creates a dock");
    if (dock == nullptr)
    {
        return;
    }

    reach_pinned_app_model pin = make_pin(7, "C:\\apps\\brave.exe");
    reach_copy_ascii_to_utf16(pin.application.launch.arguments, 260, "--profile-directory=Default");
    reach_copy_ascii_to_utf16(pin.application.identity.app_user_model_id, 260, "Brave.App");
    pin.application.launch.kind = REACH_APPLICATION_LAUNCH_SHORTCUT;
    reach_copy_ascii_to_utf16(pin.application.launch.path, 260, "C:\\Pins\\Brave.lnk");
    reach_dock_apply_pinned_apps(dock, &pin, 1);

    reach_dock_arrange_context arrange = {};
    arrange.theme = reach_theme_default();
    arrange.monitor_bounds = {0.0f, 0.0f, 1920.0f, 1080.0f};
    arrange.dpi_scale = 1.0f;
    (void)reach_dock_arrange(dock, &arrange);

    const reach_feature_capsule_ops *ops = reach_dock_capsule_ops();
    reach_feature_tick_result tick = {};
    ops->tick(dock, 1.0, &tick);
    const reach_dock_layout *layout = reach_dock_arranged_layout(dock);
    expect_true(layout != nullptr && layout->app_slot_count == 1,
                "activation test has one arranged pinned item");
    if (layout == nullptr || layout->app_slot_count != 1)
    {
        reach_dock_destroy(dock);
        return;
    }

    reach_pointer_event pointer = {};
    pointer.kind = REACH_POINTER_EVENT_DOWN;
    pointer.button = REACH_POINTER_BUTTON_PRIMARY;
    pointer.x =
        (int32_t)(layout->bounds.x + layout->app_slots[0].x + layout->app_slots[0].width * 0.5f);
    pointer.y =
        (int32_t)(layout->bounds.y + layout->app_slots[0].y + layout->app_slots[0].height * 0.5f);
    reach_capsule_pointer_result result = {};
    ops->handle_pointer(dock, &pointer, &result);
    expect_true(result.handled && ops->pointer_sequence_active(dock),
                "primary down starts one pressable sequence");

    pointer.kind = REACH_POINTER_EVENT_UP;
    ops->handle_pointer(dock, &pointer, &result);
    expect_true(result.handled && !ops->pointer_sequence_active(dock),
                "primary release completes the pressable sequence");
    expect_true(result.action.kind == REACH_FEATURE_ACTION_OPEN_TARGET &&
                    result.action.target.kind == REACH_FEATURE_TARGET_APP,
                "primary release publishes the generic app target action");
    expect_true(reach_test_utf16_equals_ascii(result.action.target.path, "C:\\Pins\\Brave.lnk"),
                "the target launches the pin's original shortcut");
    expect_true(reach_test_utf16_equals_ascii(result.action.target.arguments,
                                              "--profile-directory=Default"),
                "the target carries the pinned app arguments");
    expect_true(reach_test_utf16_equals_ascii(result.action.target.app_user_model_id, "Brave.App"),
                "the target carries the pinned app identity");
    expect_true((result.action.flags & REACH_FEATURE_ACTION_FLAG_NEW_INSTANCE) == 0,
                "primary release uses normal open-or-focus behavior");

    pointer.kind = REACH_POINTER_EVENT_DOWN;
    pointer.button = REACH_POINTER_BUTTON_MIDDLE;
    ops->handle_pointer(dock, &pointer, &result);
    pointer.kind = REACH_POINTER_EVENT_UP;
    ops->handle_pointer(dock, &pointer, &result);
    expect_true(result.action.kind == REACH_FEATURE_ACTION_OPEN_TARGET &&
                    (result.action.flags & REACH_FEATURE_ACTION_FLAG_NEW_INSTANCE) != 0,
                "middle release reuses the target path and requests a new instance");

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
    expect_true(reach_test_utf16_equals_ascii(reach_dock_item_at(dock, 0)->application.launch.path,
                                              "C:\\apps\\b.exe"),
                "the dragged pin takes the first slot straight away");

    /* config persists that move and reissues pin ids on the way back */
    reach_pinned_app_model reordered[2] = {make_pin(7, "C:\\apps\\b.exe"),
                                           make_pin(8, "C:\\apps\\a.exe")};
    reach_dock_apply_pinned_apps(dock, reordered, 2);
    (void)reach_dock_arrange(dock, &arrange);
    expect_true(reach_test_utf16_equals_ascii(reach_dock_item_at(dock, 0)->application.launch.path,
                                              "C:\\apps\\b.exe"),
                "the dragged order survives the config round trip that reissues pin ids");
    expect_true(reach_dock_item_at(dock, 0)->pin_id == 7,
                "and the entry picks up the pin id the config store reissued");

    reach_dock_destroy(dock);
}

static void test_dock_trespass_uses_current_horizontal_span(void)
{
    reach_dock *dock = nullptr;
    expect_true(reach_dock_create(&dock) == REACH_OK, "trespass test creates a dock");
    if (dock == nullptr)
    {
        return;
    }

    dock_test_window = make_window(1, "C:\\apps\\window.exe", "");
    reach_window_tracking *windows = make_dock_test_window_service();
    expect_true(windows != nullptr, "trespass test creates a window service");
    if (windows == nullptr)
    {
        reach_dock_destroy(dock);
        return;
    }

    refresh_dock_test_window(windows, 100.0f, 200.0f);
    reach_dock_attach_services(dock, nullptr, windows);

    reach_bar_visibility_request request = {};
    request.shown_bounds = {400.0f, 1016.0f, 200.0f, 64.0f};
    request.monitor_bounds = {0.0f, 0.0f, 1000.0f, 1080.0f};
    request.reveal = reach_motion_spec_curve(0.25, REACH_EASING_EASE_IN_OUT);
    const reach_bar_reveal_ops *ops = reach_dock_reveal_ops();
    reach_bar_visibility_result result = ops->update_visibility(dock, &request);
    expect_true(!result.reveal_edge_shown,
                "a window below the monitor outside the Dock span does not hide the Dock");

    refresh_dock_test_window(windows, 350.0f, 200.0f);
    ops->invalidate_coverage(dock);
    result = ops->update_visibility(dock, &request);
    expect_true(result.reveal_edge_shown, "a window overlapping the Dock span hides the Dock");

    refresh_dock_test_window(windows, 250.0f, 100.0f);
    ops->invalidate_coverage(dock);
    result = ops->update_visibility(dock, &request);
    expect_true(!result.reveal_edge_shown,
                "a window outside the current Dock span stops hiding the Dock");

    request.shown_bounds = {300.0f, 1016.0f, 300.0f, 64.0f};
    result = ops->update_visibility(dock, &request);
    expect_true(result.reveal_edge_shown,
                "a changed Dock position and width update the trespass span");

    reach_window_tracking_destroy(windows);
    reach_dock_destroy(dock);
}

int main(void)
{
    test_dock_metrics_are_spaced_before_any_configuration_arrives();
    test_pinned_item_pressable_release_opens_its_target();
    test_resize_edge_hits_the_top_rim();
    test_resize_edge_never_overlaps_icons();
    test_resize_edge_skips_rounded_ends();
    test_resize_drag_up_and_down_scales_proportionally();
    test_resize_clamps_to_min_and_max();
    test_resize_click_without_move_saves_nothing();
    test_resize_press_never_activates_or_reorders();
    test_resize_cancel_restores_start_size();
    test_resize_width_capped_never_shrinks_setting();
    test_resize_below_minimum_cap_never_saves_out_of_range();
    test_resize_commit_then_config_echo_is_silent();
    test_resize_config_echo_mid_drag_leaves_live_scale_alone();
    test_resize_keeps_dock_revealed();
    test_pinned_reorder_survives_the_config_round_trip();
    test_unpinned_windows_group_into_one_item();
    test_item_keeps_every_open_window_as_an_instance();
    test_pinned_app_claims_matching_windows();
    test_shortcut_pin_matches_executable();
    test_key_stable_when_representative_closes();
    test_order_preserved_and_new_groups_append();
    test_same_path_different_aumid_groups();
    test_pinned_and_unpinned_entries_are_the_same_kind_of_thing();
    test_identity_survives_pinning_and_unpinning();
    test_capacity_keeps_all_pinned_and_running_groups();
    test_fit_metrics_keep_native_size_until_overflow();
    test_fit_metrics_scale_every_dimension_without_a_minimum();
    test_adaptive_layout_rebuild_keeps_native_height();
    test_dock_trespass_uses_current_horizontal_span();
    return failures == 0 ? 0 : 1;
}
