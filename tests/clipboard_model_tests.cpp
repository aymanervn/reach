#include "reach/features/clipboard.h"

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

static reach_clipboard_item make_item(uint64_t id, uint64_t hash)
{
    reach_clipboard_item item = {};
    item.id = id;
    item.content_hash = hash;
    item.kind = REACH_CLIPBOARD_ITEM_TEXT;
    item.preview[0] = (uint16_t)('A' + id % 26);
    return item;
}

struct fake_clipboard_provider
{
    reach_clipboard_changed_callback changed;
    void *changed_user;
    reach_clipboard_item next;
    uint64_t released_id;
    int start_count;
    int stop_count;
    int update_count;
};

static reach_result fake_clipboard_start(reach_clipboard_provider *provider,
                                         reach_clipboard_changed_callback callback, void *user)
{
    fake_clipboard_provider *fake = reinterpret_cast<fake_clipboard_provider *>(provider);
    fake->changed = callback;
    fake->changed_user = user;
    ++fake->start_count;
    return REACH_OK;
}

static reach_result fake_clipboard_stop(reach_clipboard_provider *provider)
{
    ++reinterpret_cast<fake_clipboard_provider *>(provider)->stop_count;
    return REACH_OK;
}

static reach_result fake_clipboard_capture(reach_clipboard_provider *provider,
                                           reach_clipboard_item *out)
{
    *out = reinterpret_cast<fake_clipboard_provider *>(provider)->next;
    return REACH_OK;
}

static void fake_clipboard_release(reach_clipboard_provider *provider, uint64_t item_id)
{
    reinterpret_cast<fake_clipboard_provider *>(provider)->released_id = item_id;
}

static void fake_clipboard_request_update(void *user)
{
    ++static_cast<fake_clipboard_provider *>(user)->update_count;
}

static void test_capsule_owns_capture_and_resource_retirement()
{
    fake_clipboard_provider fake = {};
    reach_clipboard_port port = {};
    port.provider = reinterpret_cast<reach_clipboard_provider *>(&fake);
    port.ops.start = fake_clipboard_start;
    port.ops.stop = fake_clipboard_stop;
    port.ops.capture_current = fake_clipboard_capture;
    port.ops.release = fake_clipboard_release;

    reach_clipboard_feature *clipboard = nullptr;
    expect_true(reach_clipboard_feature_create(&clipboard) == REACH_OK,
                "clipboard capsule is created");
    reach_clipboard_feature_attach_port(clipboard, &port, fake_clipboard_request_update, &fake);
    expect_true(reach_clipboard_feature_start(clipboard) == REACH_OK,
                "clipboard capsule starts its provider");
    expect_true(fake.start_count == 1, "provider starts exactly once");

    fake.next = make_item(1, 100);
    fake.next.thumbnail_id = 11;
    fake.changed(fake.changed_user);
    expect_true(fake.update_count == 1, "provider change requests a host update");

    reach_feature_tick_result tick = {};
    reach_clipboard_feature_capsule_ops()->tick(clipboard, 0.0, &tick);
    expect_true(reach_clipboard_item_count(clipboard) == 1,
                "capsule captures the provider item during its tick");
    expect_true(tick.redraw && tick.relayout && tick.request_update,
                "accepted capture reports its generic runtime effects");

    fake.next = make_item(2, 100);
    fake.next.thumbnail_id = 22;
    fake.changed(fake.changed_user);
    tick = {};
    reach_clipboard_feature_capsule_ops()->tick(clipboard, 0.0, &tick);

    reach_clipboard_retired_resource retired = {};
    expect_true(reach_clipboard_feature_take_retired_resources(clipboard, &retired, 1) == 1,
                "duplicate capture exposes one retired resource");
    expect_true(retired.item_id == 2 && retired.thumbnail_id == 22,
                "retired resource keeps provider and renderer identities together");
    reach_clipboard_feature_release_resource(clipboard, &retired);
    expect_true(fake.released_id == 2, "resource release returns ownership to the provider");

    reach_clipboard_feature_stop(clipboard);
    expect_true(fake.stop_count == 1, "provider stops exactly once");
    reach_clipboard_feature_destroy(clipboard);
}

static void test_capacity_and_order()
{
    reach_clipboard_model model = {};
    reach_clipboard_model_init(&model);
    for (uint64_t id = 1; id <= REACH_CLIPBOARD_MAX_ITEMS; ++id)
    {
        reach_clipboard_insert_result result =
            reach_clipboard_model_insert(&model, make_item(id, id));
        expect_true(result.inserted, "unique item inserts");
        expect_true(result.evicted_id == 0, "no early eviction");
    }
    expect_true(model.count == REACH_CLIPBOARD_MAX_ITEMS, "capacity reaches twenty");
    expect_true(model.items[0].id == REACH_CLIPBOARD_MAX_ITEMS, "newest item is first");
    expect_true(model.items[REACH_CLIPBOARD_MAX_ITEMS - 1].id == 1, "oldest item is last");

    reach_clipboard_insert_result overflow =
        reach_clipboard_model_insert(&model, make_item(21, 21));
    expect_true(overflow.evicted_id == 1, "overflow evicts oldest item");
    expect_true(model.items[0].id == 21, "overflow item is first");
}

static void test_duplicate_and_promotion()
{
    reach_clipboard_model model = {};
    reach_clipboard_model_init(&model);
    reach_clipboard_model_insert(&model, make_item(1, 100));
    reach_clipboard_model_insert(&model, make_item(2, 200));
    reach_clipboard_model_insert(&model, make_item(3, 300));

    reach_clipboard_insert_result duplicate =
        reach_clipboard_model_insert(&model, make_item(4, 100));
    expect_true(!duplicate.inserted, "duplicate does not grow history");
    expect_true(duplicate.rejected_id == 4, "duplicate payload is returned for release");
    expect_true(duplicate.promoted_existing, "existing duplicate is promoted");
    expect_true(model.count == 3 && model.items[0].id == 1, "duplicate promotion preserves item");

    expect_true(reach_clipboard_model_promote(&model, 2), "valid index promotes");
    expect_true(model.items[0].id == 2, "clicked item moves to first");
    expect_true(!reach_clipboard_model_promote(&model, 9), "invalid promotion is rejected");
}

static void test_remove()
{
    reach_clipboard_model model = {};
    reach_clipboard_model_init(&model);
    reach_clipboard_model_insert(&model, make_item(1, 1));
    reach_clipboard_model_insert(&model, make_item(2, 2));
    reach_clipboard_model_insert(&model, make_item(3, 3));
    reach_clipboard_model_remove(&model, 1);
    expect_true(model.count == 2, "remove shrinks history");
    expect_true(model.items[0].id == 3 && model.items[1].id == 1, "remove closes gap");
    reach_clipboard_model_remove(&model, 9);
    expect_true(model.count == 2, "invalid remove is ignored");
}

static void test_preview()
{
    uint16_t preview[12] = {};
    const uint16_t text[] = {' ', 'a', '\t', ' ', 'b', '\r', '\n', 'c', '\n', 'd', '\n', 'e', 0};
    reach_clipboard_build_text_preview(text, preview, 12);
    const uint16_t expected[] = {' ', 'a', '\t', ' ', 'b', '\n', 'c', '\n', 'd', '\n', 'e', 0};
    size_t index = 0;
    while (expected[index] != 0 && preview[index] == expected[index])
    {
        ++index;
    }
    expect_true(expected[index] == preview[index], "preview preserves whitespace and line breaks");

    uint16_t tiny[5] = {};
    const uint16_t long_text[] = {'a', 'b', 'c', 'd', 'e', 'f', 0};
    reach_clipboard_build_text_preview(long_text, tiny, 5);
    expect_true(tiny[3] == 0x2026 && tiny[4] == 0, "preview reserves space for ellipsis");
}

static void test_scrollbar_edges()
{
    reach_scrollbar_model model = {};
    reach_scrollbar_model_init(&model, REACH_SCROLLBAR_DRAG_FREE, 0.0f);
    reach_scrollbar_set_extents(&model, 1000.0f, 200.0f);
    reach_scrollbar_set_target(&model, -10.0f);
    expect_true(model.target == 0.0f, "free scrollbar clamps low");
    reach_scrollbar_set_target(&model, 900.0f);
    expect_true(model.target == 800.0f, "free scrollbar clamps high");

    reach_scrollbar_model stepped = {};
    reach_scrollbar_model_init(&stepped, REACH_SCROLLBAR_DRAG_STEPPED, 50.0f);
    reach_scrollbar_set_extents(&stepped, 500.0f, 200.0f);
    reach_scrollbar_set_target(&stepped, 126.0f);
    expect_true(stepped.target == 150.0f, "stepped scrollbar quantizes target");
}

static void test_layout_wraps_runtime_border_around_stable_content(void)
{
    reach_clipboard_model model = {};
    reach_clipboard_model_init(&model);
    (void)reach_clipboard_model_insert(&model, make_item(1, 1));
    reach_rect_f32 monitor = {0.0f, 0.0f, 1920.0f, 1080.0f};

    reach_clipboard_layout narrow = reach_clipboard_compute_layout(&model, monitor, {}, 1.0f, 1.0f);
    reach_clipboard_layout wide = reach_clipboard_compute_layout(&model, monitor, {}, 1.0f, 3.0f);

    expect_near(wide.bounds.width - narrow.bounds.width, 4.0f, 0.001f,
                "clipboard outer width derives both border sides");
    expect_near(wide.bounds.height - narrow.bounds.height, 4.0f, 0.001f,
                "clipboard outer height derives both border sides");
    expect_near(wide.viewport.width, narrow.viewport.width, 0.001f,
                "clipboard content width stays stable across border widths");
    expect_near(narrow.title.x - narrow.bounds.x - 1.0f, 8.0f, 0.001f,
                "clipboard title padding starts inside the 1dp border");
    expect_near(wide.title.x - wide.bounds.x - 3.0f, 8.0f, 0.001f,
                "clipboard title padding starts inside an arbitrary border");
}

int main()
{
    test_capacity_and_order();
    test_duplicate_and_promotion();
    test_remove();
    test_preview();
    test_scrollbar_edges();
    test_layout_wraps_runtime_border_around_stable_content();
    test_capsule_owns_capture_and_resource_retirement();
    return failures == 0 ? 0 : 1;
}
