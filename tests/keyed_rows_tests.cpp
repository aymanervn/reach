#include "reach/features/common/keyed_rows.h"

#include "motion_clock_test_support.h"

#include <math.h>
#include <stdio.h>

static int failures = 0;

static void expect_true(int32_t condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

enum
{
    POOL = 8
};

struct pool
{
    reach_keyed_row rows[POOL];
    int keys[POOL];
    reach_keyed_row_change changes[REACH_KEYED_ROWS_MAX_CHANGES(POOL)];
    size_t change_count;
    size_t capacity = POOL;
};

struct identity
{
    const pool *rows;
    const int *items;
};

static int32_t same_key(const void *context, size_t slot, size_t item_index)
{
    const identity *id = static_cast<const identity *>(context);
    return id->rows->keys[slot] == id->items[item_index];
}

static void sync(pool *p, const int *items, size_t count, int32_t animate)
{
    float targets[POOL] = {};
    for (size_t index = 0; index < count; ++index)
        targets[index] = 40.0f * (float)index;
    identity id = {p, items};
    reach_keyed_rows_sync_input input = {};
    input.count = count;
    input.targets = targets;
    input.same = same_key;
    input.context = &id;
    input.animate = animate;
    input.theme = reach_theme_default();
    input.now_seconds = reach_motion_now_seconds();
    p->change_count = reach_keyed_rows_sync(p->rows, p->capacity, &input, p->changes,
                                            REACH_KEYED_ROWS_MAX_CHANGES(POOL));
    for (size_t index = 0; index < p->change_count; ++index)
        if (p->changes[index].kind == REACH_KEYED_ROW_RELEASED)
            p->keys[p->changes[index].slot] = 0;
    for (size_t slot = 0; slot < p->capacity; ++slot)
        if (p->rows[slot].lifecycle == REACH_KEYED_ROW_LIVE)
            p->keys[slot] = items[p->rows[slot].index];
}

static size_t release(pool *p)
{
    p->change_count = reach_keyed_rows_release_settled(p->rows, p->capacity, p->changes, POOL);
    for (size_t index = 0; index < p->change_count; ++index)
        p->keys[p->changes[index].slot] = 0;
    return p->change_count;
}

static size_t slot_of(const pool &p, int key)
{
    for (size_t slot = 0; slot < p.capacity; ++slot)
        if (p.rows[slot].lifecycle != REACH_KEYED_ROW_EMPTY && p.keys[slot] == key)
            return slot;
    return POOL;
}

static size_t count_kind(const pool &p, int32_t kind)
{
    size_t count = 0;
    for (size_t index = 0; index < p.change_count; ++index)
        count += p.changes[index].kind == kind;
    return count;
}

static void settle(pool *p)
{
    reach_test_motion_clock_advance(reach_keyed_rows_deadline(p->rows, p->capacity) -
                                    reach_motion_now_seconds() + 0.01);
}

static void test_survivor_keeps_value_and_velocity_across_resync(void)
{
    pool p = {};
    const int first[] = {1, 2, 3};
    sync(&p, first, 3, 0);
    const int removed[] = {2, 3};
    sync(&p, removed, 2, 1);
    reach_test_motion_clock_advance(0.08);
    size_t slot = slot_of(p, 3);
    double now = reach_motion_now_seconds();
    float value = reach_motion_value_at(&p.rows[slot].y, now);
    float velocity = reach_motion_velocity_at(&p.rows[slot].y, now);
    expect_true(fabsf(velocity) > 1.0f, "survivor is moving before the re-sync");
    const int reordered[] = {3, 2};
    sync(&p, reordered, 2, 1);
    expect_true(slot_of(p, 3) == slot, "survivor keeps its slot");
    expect_true(fabsf(reach_motion_value_at(&p.rows[slot].y, now) - value) <= 0.01f,
                "survivor value is continuous across a re-sync");
    expect_true(fabsf(reach_motion_velocity_at(&p.rows[slot].y, now) - velocity) <= 1.0f,
                "survivor velocity is continuous across a re-sync");
    reach_motion_spec smooth =
        reach_theme_motion_preset(reach_theme_default(), REACH_MOTION_PRESET_SMOOTH);
    expect_true(p.rows[slot].y.spring_response == smooth.response_seconds &&
                    p.rows[slot].y.spring_damping == smooth.damping,
                "survivor retargets with the smooth preset");
}

static void test_insert_starts_transparent_at_its_slot(void)
{
    pool p = {};
    const int first[] = {1};
    sync(&p, first, 1, 0);
    const int inserted[] = {4, 1};
    sync(&p, inserted, 2, 1);
    size_t slot = slot_of(p, 4);
    double now = reach_motion_now_seconds();
    expect_true(slot < POOL && count_kind(p, REACH_KEYED_ROW_ALLOCATED) == 1,
                "insert allocates one entry");
    expect_true(reach_motion_value_at(&p.rows[slot].opacity, now) == 0.0f &&
                    p.rows[slot].opacity.to == 1.0f,
                "insert starts at opacity 0 and fades in");
    expect_true(!reach_motion_active(&p.rows[slot].y) && p.rows[slot].y.to == 0.0f,
                "insert is placed at its slot without moving");
}

static void test_removal_released_only_after_fade(void)
{
    pool p = {};
    const int first[] = {1, 2};
    sync(&p, first, 2, 0);
    const int removed[] = {1};
    sync(&p, removed, 1, 1);
    size_t slot = slot_of(p, 2);
    expect_true(p.rows[slot].lifecycle == REACH_KEYED_ROW_RETIRING && p.rows[slot].opacity.to == 0,
                "removal fades out in place");
    expect_true(release(&p) == 0, "removal is not released while fading");
    settle(&p);
    expect_true(release(&p) == 1 && p.changes[0].slot == slot &&
                    p.changes[0].previous_lifecycle == REACH_KEYED_ROW_RETIRING,
                "removal is released once its fade settles");
    expect_true(p.rows[slot].lifecycle == REACH_KEYED_ROW_EMPTY, "released entry is empty");
}

static void test_returning_identity_revives_its_entry(void)
{
    pool p = {};
    const int first[] = {1, 2};
    sync(&p, first, 2, 0);
    const int removed[] = {1};
    sync(&p, removed, 1, 1);
    size_t slot = slot_of(p, 2);
    reach_test_motion_clock_advance(0.05);
    double now = reach_motion_now_seconds();
    float opacity = reach_motion_value_at(&p.rows[slot].opacity, now);
    sync(&p, first, 2, 1);
    expect_true(count_kind(p, REACH_KEYED_ROW_REVIVED) == 1 && p.changes[0].slot == slot,
                "returning identity reports a revive");
    expect_true(p.rows[slot].lifecycle == REACH_KEYED_ROW_LIVE && p.rows[slot].opacity.to == 1,
                "revived entry is live and fades back in");
    expect_true(fabsf(reach_motion_value_at(&p.rows[slot].opacity, now) - opacity) <= 0.001f,
                "revived opacity continues from its current value");
}

static void test_full_pool_insert_waits_without_eviction(void)
{
    pool p = {};
    p.capacity = 2;
    const int first[] = {1, 2};
    sync(&p, first, 2, 0);
    const int removed[] = {1};
    sync(&p, removed, 1, 1);
    const int inserted[] = {1, 3};
    sync(&p, inserted, 2, 1);
    expect_true(count_kind(p, REACH_KEYED_ROW_ALLOCATED) == 0 && slot_of(p, 3) == POOL,
                "insert waits while the pool is full");
    expect_true(p.rows[slot_of(p, 2)].lifecycle == REACH_KEYED_ROW_RETIRING &&
                    p.rows[slot_of(p, 1)].lifecycle == REACH_KEYED_ROW_LIVE,
                "nothing is evicted for a waiting insert");
    settle(&p);
    expect_true(release(&p) == 1, "retiring entry is released after its fade");
    sync(&p, inserted, 2, 1);
    expect_true(count_kind(p, REACH_KEYED_ROW_ALLOCATED) == 1 && slot_of(p, 3) < POOL,
                "waiting insert takes the released entry");
}

static void test_duplicate_identities_resolve_in_claim_order(void)
{
    pool p = {};
    const int duplicates[] = {5, 5};
    sync(&p, duplicates, 2, 0);
    expect_true(p.rows[0].index == 0 && p.rows[1].index == 1, "duplicates fill slots in order");
    sync(&p, duplicates, 2, 1);
    expect_true(p.rows[0].index == 0 && p.rows[1].index == 1 &&
                    count_kind(p, REACH_KEYED_ROW_ALLOCATED) == 0,
                "duplicates are claimed in slot order");
    const int single[] = {5};
    sync(&p, single, 1, 1);
    expect_true(p.rows[0].lifecycle == REACH_KEYED_ROW_LIVE &&
                    p.rows[1].lifecycle == REACH_KEYED_ROW_RETIRING,
                "the first claimed duplicate survives");
}

static void test_non_animated_sync_sets_without_motion(void)
{
    pool p = {};
    const int first[] = {1, 2, 3};
    sync(&p, first, 3, 1);
    const int reordered[] = {3, 1};
    sync(&p, reordered, 2, 0);
    expect_true(!reach_keyed_rows_active(p.rows, p.capacity), "non-animated sync starts no motion");
    expect_true(count_kind(p, REACH_KEYED_ROW_RELEASED) == 1 && slot_of(p, 2) == POOL,
                "non-animated sync releases removed entries at once");
    size_t slot = slot_of(p, 1);
    expect_true(p.rows[slot].y.to == 40.0f &&
                    reach_motion_value_at(&p.rows[slot].opacity, reach_motion_now_seconds()) ==
                        1.0f,
                "non-animated sync sets position and opacity");
}

int main(void)
{
    test_survivor_keeps_value_and_velocity_across_resync();
    test_insert_starts_transparent_at_its_slot();
    test_removal_released_only_after_fade();
    test_returning_identity_revives_its_entry();
    test_full_pool_insert_waits_without_eviction();
    test_duplicate_identities_resolve_in_claim_order();
    test_non_animated_sync_sets_without_motion();
    return failures == 0 ? 0 : 1;
}
