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

static reach_clipboard_item make_item(uint64_t id, uint64_t hash)
{
    reach_clipboard_item item = {};
    item.id = id;
    item.content_hash = hash;
    item.kind = REACH_CLIPBOARD_ITEM_TEXT;
    item.preview[0] = (uint16_t)('A' + id % 26);
    return item;
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
    const uint16_t text[] = {' ', 'a', '\t', ' ', 'b', '\r', '\n', 'c', '\n', 'd',
                             '\n', 'e', 0};
    reach_clipboard_build_text_preview(text, preview, 12);
    const uint16_t expected[] = {'a', ' ', 'b', ' ', 'c', ' ', 'd', 0x2026, 0};
    size_t index = 0;
    while (expected[index] != 0 && preview[index] == expected[index])
    {
        ++index;
    }
    expect_true(expected[index] == preview[index], "preview normalizes whitespace and line limit");

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

    reach_scrollbar_layout layout = reach_scrollbar_compute_layout(
        &model, {0.0f, 0.0f, 4.0f, 100.0f}, 200.0f, 1000.0f, 20.0f);
    expect_true(layout.thumb.height == 20.0f, "minimum thumb size applies");
    reach_scrollbar_drag drag = {};
    reach_scrollbar_begin_drag(&model, &drag, &layout, 100.0f, 0);
    expect_true(model.offset == model.maximum, "track drag reaches end");
    reach_scrollbar_end_drag(&drag);
    expect_true(!drag.active, "drag ends cleanly");
}

int main()
{
    test_capacity_and_order();
    test_duplicate_and_promotion();
    test_remove();
    test_preview();
    test_scrollbar_edges();
    return failures == 0 ? 0 : 1;
}
