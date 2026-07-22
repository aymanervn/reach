#include "reach/features/dock.h"

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

static void copy_ascii(uint16_t *dst, size_t cap, const char *src)
{
    size_t index = 0;
    while (src != nullptr && src[index] != 0 && index + 1 < cap)
    {
        dst[index] = (uint16_t)(unsigned char)src[index];
        ++index;
    }
    dst[index] = 0;
}

static reach_window_snapshot make_window(uintptr_t id, const char *path, const char *aumid)
{
    reach_window_snapshot window = {};
    window.id = id;
    window.visible = 1;
    copy_ascii(window.title, 260, "window");
    copy_ascii(window.path, 260, path);
    copy_ascii(window.app_user_model_id, 260, aumid);
    return window;
}

static reach_pinned_app_model make_pin(uint32_t id, const char *path)
{
    reach_pinned_app_model app = {};
    app.id = id;
    copy_ascii(app.path, 260, path);
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

int main(void)
{
    test_unpinned_windows_group_into_one_item();
    test_pinned_app_claims_matching_windows();
    test_key_stable_when_representative_closes();
    test_order_preserved_and_new_groups_append();
    test_same_path_different_aumid_stays_split();
    test_key_equality_semantics();
    return failures == 0 ? 0 : 1;
}
