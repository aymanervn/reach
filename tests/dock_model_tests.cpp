#include "reach/support/util.h"
#include "reach/core/config.h"
#include "reach/features/dock.h"
#include "test_utf16.h"
#include "motion_clock_test_support.h"
#include "../src/features/dock/dock_interaction.h"

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
    reach_pinned_app_model pin = make_pin(7, "C:\apps\a.exe");
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
    request.auto_hide_active = 1;
    const reach_bar_reveal_ops *ops = reach_dock_reveal_ops();
    reach_bar_visibility_result result = ops->update_visibility(dock, &request);
    expect_true(!result.reveal_edge_shown,
                "a window below the monitor outside the Dock span does not hide the Dock");

    refresh_dock_test_window(windows, 350.0f, 200.0f);
    ops->invalidate_coverage(dock);
    request.auto_hide_active = 0;
    result = ops->update_visibility(dock, &request);
    expect_true(!result.reveal_edge_shown,
                "a fixed Dock stays shown when a window overlaps its span");
    request.auto_hide_active = 1;
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

static void test_dock_auto_hide_publishes_work_area(void)
{
    reach_dock *dock = nullptr;
    expect_true(reach_dock_create(&dock) == REACH_OK && dock != nullptr,
                "auto hide test creates a dock");
    if (dock == nullptr)
    {
        return;
    }
    reach_dock_arrange_context arrange = {};
    arrange.theme = reach_theme_default();
    arrange.monitor_bounds = {0.0f, 0.0f, 1920.0f, 1080.0f};
    arrange.dpi_scale = 1.0f;
    (void)reach_dock_arrange(dock, &arrange);

    reach_feature_surface_geometry geometry = {};
    reach_dock_capsule_ops()->surface_geometry(dock, &geometry);
    expect_true(geometry.manage_monitor_work_area == 1,
                "dock always takes part in work-area management");
    expect_true(geometry.reserve_monitor_work_area == 0, "auto hide dock reserves nothing");
    expect_true(geometry.bar_reveal_policy == REACH_FEATURE_BAR_REVEAL_ALWAYS,
                "auto hide dock hides for any overlapping window");

    reach_dock_set_auto_hide(dock, 0);
    (void)reach_dock_arrange(dock, &arrange);
    geometry = {};
    reach_dock_capsule_ops()->surface_geometry(dock, &geometry);
    const reach_dock_layout *layout = reach_dock_arranged_layout(dock);
    expect_true(layout != nullptr, "fixed dock has an arranged layout");
    expect_true(geometry.reserve_monitor_work_area == 1, "fixed dock reserves its strip");
    expect_true(geometry.bar_reveal_policy == REACH_FEATURE_BAR_REVEAL_FULLSCREEN,
                "fixed dock hides only for fullscreen apps");
    if (layout != nullptr)
    {
        float gap_below = 1080.0f - (layout->bounds.y + layout->bounds.height);
        expect_near(geometry.work_area_clearance, gap_below, 0.0001f,
                    "fixed dock keeps the same gap above as below");
        expect_true(gap_below >= 0.0f, "dock sits inside the monitor");
    }

    reach_dock_set_auto_hide(dock, 1);
    geometry = {};
    reach_dock_capsule_ops()->surface_geometry(dock, &geometry);
    expect_true(geometry.reserve_monitor_work_area == 0 &&
                    geometry.bar_reveal_policy == REACH_FEATURE_BAR_REVEAL_ALWAYS,
                "turning auto hide back on releases the strip");

    reach_dock_destroy(dock);
}

static int32_t rect_f32_same(reach_rect_f32 a, reach_rect_f32 b)
{
    return fabsf(a.x - b.x) < 0.001f && fabsf(a.y - b.y) < 0.001f &&
           fabsf(a.width - b.width) < 0.001f && fabsf(a.height - b.height) < 0.001f;
}

static void test_fixed_dock_reserves_settled_bounds_while_resizing(void)
{
    reach_dock *dock = nullptr;
    expect_true(reach_dock_create(&dock) == REACH_OK && dock != nullptr,
                "settled reservation test creates a dock");
    if (dock == nullptr)
    {
        return;
    }
    const reach_feature_capsule_ops *ops = reach_dock_capsule_ops();
    reach_feature_tick_result tick = {};
    reach_dock_arrange_context arrange = {};
    arrange.theme = reach_theme_default();
    arrange.monitor_bounds = {0.0f, 0.0f, 1920.0f, 1080.0f};
    arrange.dpi_scale = 1.0f;

    reach_dock_set_auto_hide(dock, 0);
    reach_dock_apply_config(dock, 64.0f, REACH_CONFIG_DOCK_SIZE_MAX);
    ops->tick(dock, 0.0, &tick);
    (void)reach_dock_arrange(dock, &arrange);
    reach_feature_surface_geometry settled = {};
    ops->surface_geometry(dock, &settled);
    const reach_dock_layout *layout = reach_dock_arranged_layout(dock);
    expect_true(layout != nullptr && settled.reserve_monitor_work_area &&
                    rect_f32_same(settled.work_area_bounds, layout->bounds),
                "settled fixed dock reserves from its own bounds");

    reach_dock_apply_config(dock, 64.0f, REACH_CONFIG_DOCK_SIZE_MIN);
    reach_test_motion_clock_advance(0.05);
    ops->tick(dock, 0.0, &tick);
    (void)reach_dock_arrange(dock, &arrange);
    reach_feature_surface_geometry resizing = {};
    ops->surface_geometry(dock, &resizing);
    layout = reach_dock_arranged_layout(dock);
    expect_true(layout != nullptr && !rect_f32_same(layout->bounds, settled.work_area_bounds),
                "dock bounds move while the size animates");
    expect_true(resizing.reserve_monitor_work_area &&
                    rect_f32_same(resizing.work_area_bounds, settled.work_area_bounds) &&
                    resizing.work_area_clearance == settled.work_area_clearance,
                "animating dock keeps its settled reservation");

    reach_test_motion_clock_advance(5.0);
    ops->tick(dock, 0.0, &tick);
    (void)reach_dock_arrange(dock, &arrange);
    reach_feature_surface_geometry resized = {};
    ops->surface_geometry(dock, &resized);
    layout = reach_dock_arranged_layout(dock);
    expect_true(layout != nullptr && rect_f32_same(resized.work_area_bounds, layout->bounds) &&
                    !rect_f32_same(resized.work_area_bounds, settled.work_area_bounds),
                "settled dock reserves its new size");

    reach_dock_destroy(dock);
}

int main(void)
{
    test_dock_metrics_are_spaced_before_any_configuration_arrives();
    test_pinned_item_pressable_release_opens_its_target();
    test_resize_edge_hits_the_top_rim();
    test_resize_edge_never_overlaps_icons();
    test_resize_edge_skips_rounded_ends();
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
    test_dock_auto_hide_publishes_work_area();
    test_fixed_dock_reserves_settled_bounds_while_resizing();
    return failures == 0 ? 0 : 1;
}
