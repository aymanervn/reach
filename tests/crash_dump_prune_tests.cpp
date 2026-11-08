#include "reach/support/crash_dump_prune.h"

#include <stdio.h>

static int failed = 0;

static void expect(int condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        failed += 1;
    }
}

static void test_keeps_the_newest_by_count(void)
{
    reach_crash_dump_entry entries[7] = {{10, 5},  {10, 70}, {10, 20}, {10, 60},
                                         {10, 40}, {10, 30}, {10, 50}};
    int32_t remove[7] = {};
    reach_crash_dump_prune_select(entries, 7, 5, 1000, remove);
    expect(remove[1] == 0 && remove[3] == 0 && remove[6] == 0 && remove[4] == 0 && remove[5] == 0,
           "the five newest dumps are kept");
    expect(remove[0] == 1 && remove[2] == 1, "older dumps are removed");
}

static void test_total_size_bounds_the_kept_set(void)
{
    reach_crash_dump_entry entries[4] = {{40, 4}, {30, 3}, {30, 2}, {5, 1}};
    int32_t remove[4] = {};
    reach_crash_dump_prune_select(entries, 4, 5, 100, remove);
    expect(remove[0] == 0 && remove[1] == 0 && remove[2] == 0,
           "dumps within the byte limit are kept");
    expect(remove[3] == 1, "the dump that would exceed the limit is removed");

    reach_crash_dump_entry oversized[3] = {{500, 3}, {10, 2}, {10, 1}};
    int32_t oversized_remove[3] = {};
    reach_crash_dump_prune_select(oversized, 3, 5, 100, oversized_remove);
    expect(oversized_remove[0] == 0, "the newest dump is always kept");
    expect(oversized_remove[1] == 1 && oversized_remove[2] == 1,
           "older dumps are removed once the limit is exceeded");
}

static void test_empty_and_small_sets(void)
{
    int32_t remove[1] = {};
    reach_crash_dump_prune_select(nullptr, 1, 5, 100, remove);
    reach_crash_dump_entry one[1] = {{1, 1}};
    reach_crash_dump_prune_select(one, 1, 5, 100, remove);
    expect(remove[0] == 0, "a single small dump is kept");
}

int main(void)
{
    test_keeps_the_newest_by_count();
    test_total_size_bounds_the_kept_set();
    test_empty_and_small_sets();
    return failed == 0 ? 0 : 1;
}
