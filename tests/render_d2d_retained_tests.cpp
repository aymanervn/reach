#include "render_d2d_retained_policy.h"

#include <limits>
#include <memory>
#include <set>
#include <stdio.h>
#include <vector>

static int failed = 0;

static void expect(int condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        failed += 1;
    }
}

static void test_layer_bytes(void)
{
    expect(reach_d2d_layer_bytes(100.0f, 50.0f, true, false) == 100u * 50u * 4u,
           "a painted layer costs four bytes per pixel");
    expect(reach_d2d_layer_bytes(100.2f, 50.5f, true, false) == 101u * 51u * 4u,
           "fractional bounds round up per axis");
    expect(reach_d2d_layer_bytes(100.0f, 50.0f, true, true) == 100u * 50u * 4u * 2u,
           "a painted layer with a shadow costs two surfaces");
    expect(reach_d2d_layer_bytes(100.0f, 50.0f, false, true) == 100u * 50u * 4u,
           "a shadow-only layer costs one surface");
    expect(reach_d2d_layer_bytes(100.0f, 50.0f, false, false) == 0,
           "a paint-less shadow-less layer costs nothing");
    expect(reach_d2d_layer_bytes(0.0f, 50.0f, true, true) == 0, "empty bounds cost nothing");
    expect(reach_d2d_layer_bytes(-4.0f, 50.0f, true, false) == 0, "negative bounds cost nothing");
    expect(reach_d2d_layer_bytes(std::numeric_limits<float>::quiet_NaN(), 50.0f, true, false) == 0,
           "NaN bounds cost nothing");
}

static void test_layer_bytes_saturates(void)
{
    expect(reach_d2d_layer_bytes(1e30f, 1e30f, true, true) > UINT64_C(128) * 1024 * 1024,
           "absurd bounds exceed the retained budget without overflowing");
    expect(reach_d2d_layer_bytes(std::numeric_limits<float>::infinity(), 1000.0f, true, false) >
               UINT64_C(128) * 1024 * 1024,
           "infinite bounds exceed the retained budget");
}

struct buffer_entry
{
    uint64_t group_id;
    reach_render_command_type type;
};

static std::unique_ptr<reach_render_command_buffer>
make_buffer(const std::vector<buffer_entry> &entries)
{
    auto buffer = std::make_unique<reach_render_command_buffer>();
    for (const buffer_entry &entry : entries)
    {
        reach_render_command &command = buffer->commands[buffer->count++];
        command.type = entry.type;
        command.group_id = entry.group_id;
    }
    return buffer;
}

static std::vector<reach_d2d_retained_run> runs_of(const reach_render_command_buffer &buffer)
{
    std::vector<reach_d2d_retained_run> runs;
    reach_d2d_retained_runs(&buffer, &runs);
    return runs;
}

static void test_ungrouped_run_survives_unrelated_insertions(void)
{
    const uint64_t group = 0x1234;
    auto before = make_buffer({{0, REACH_RENDER_COMMAND_RECT},
                               {group, REACH_RENDER_COMMAND_RECT},
                               {0, REACH_RENDER_COMMAND_RECT}});
    auto after = make_buffer({{0, REACH_RENDER_COMMAND_RECT},
                              {0, REACH_RENDER_COMMAND_RECT},
                              {group, REACH_RENDER_COMMAND_RECT},
                              {0, REACH_RENDER_COMMAND_RECT}});
    auto first = runs_of(*before);
    auto second = runs_of(*after);
    expect(first.size() == 3 && second.size() == 3, "ungrouped neighbours merge into one run");
    if (first.size() == 3 && second.size() == 3)
    {
        expect(first[2].id == second[2].id,
               "a trailing ungrouped run keeps its id when commands are inserted before its group");
        expect(first[0].id == second[0].id, "a leading ungrouped run keeps its id");
        expect(second[1].id == group, "a grouped run is keyed by its group");
        expect(second[2].begin == 3 && second[2].end == 4, "run ranges follow the command order");
    }
}

static void test_textbox_runs_split_and_stay_distinct(void)
{
    auto buffer = make_buffer({{0, REACH_RENDER_COMMAND_RECT},
                               {0, REACH_RENDER_COMMAND_TEXTBOX},
                               {0, REACH_RENDER_COMMAND_TEXTBOX},
                               {0, REACH_RENDER_COMMAND_RECT}});
    auto runs = runs_of(*buffer);
    expect(runs.size() == 4, "every textbox is its own ungrouped run");
    std::set<uint64_t> ids;
    for (const auto &run : runs)
        ids.insert(run.id);
    expect(ids.size() == runs.size(), "split ungrouped runs have distinct ids");
    expect(runs.size() == 4 && runs[1].id == reach_d2d_ungrouped_run_id(0, 1),
           "the ordinal counts ungrouped runs since the anchor");
}

static void test_ungrouped_run_ids_stay_in_the_reserved_range(void)
{
    std::set<uint64_t> ids;
    for (uint64_t anchor = 0; anchor < 64; ++anchor)
        for (uint64_t ordinal = 0; ordinal < 32; ++ordinal)
        {
            uint64_t id = reach_d2d_ungrouped_run_id(anchor * 0x10001, ordinal);
            ids.insert(id);
            expect(id >= UINT64_C(0x8000000000000000) && id <= UINT64_C(0xbfffffffffffffff),
                   "ungrouped run ids keep bit 63 set and bit 62 clear");
            uint64_t prefix = id & UINT64_C(0xffffffffffff0000);
            expect(prefix < UINT64_C(0xc000000000000000), "ungrouped ids never match a fit prefix");
        }
    expect(ids.size() == 64u * 32u, "ungrouped run ids are unique across anchors and ordinals");

    std::vector<buffer_entry> entries;
    for (int index = 0; index < 2048; ++index)
        entries.push_back({0, REACH_RENDER_COMMAND_TEXTBOX});
    auto buffer = make_buffer(entries);
    auto runs = runs_of(*buffer);
    std::set<uint64_t> unique;
    for (const auto &run : runs)
        unique.insert(run.id);
    expect(runs.size() == 2048 && unique.size() == 2048, "2048 runs get unique ids");
}

int main(void)
{
    test_layer_bytes();
    test_layer_bytes_saturates();
    test_ungrouped_run_survives_unrelated_insertions();
    test_textbox_runs_split_and_stay_distinct();
    test_ungrouped_run_ids_stay_in_the_reserved_range();
    return failed == 0 ? 0 : 1;
}
