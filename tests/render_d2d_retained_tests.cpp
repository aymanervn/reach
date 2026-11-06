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

static const uint64_t BUDGET = UINT64_C(128) * 1024 * 1024;
static const reach_d2d_surface_size UNLIMITED = {16384, 16384};

static bool same_size(reach_d2d_surface_size left, reach_d2d_surface_size right)
{
    return left.width == right.width && left.height == right.height;
}

static uint64_t area(reach_d2d_surface_size size)
{
    return (uint64_t)size.width * size.height;
}

static int simulate_allocations(const std::vector<reach_d2d_surface_size> &needs)
{
    reach_d2d_surface_size capacity = {};
    int allocations = 0;
    for (const reach_d2d_surface_size &need : needs)
    {
        reach_d2d_surface_size next = reach_d2d_surface_capacity(capacity, need, BUDGET, UNLIMITED);
        expect(next.width >= need.width && next.height >= need.height,
               "the capacity always covers the needed size");
        expect(area(need) * 2 >= area(next), "the capacity never exceeds twice the needed area");
        if (!same_size(next, capacity))
            ++allocations;
        capacity = next;
    }
    return allocations;
}

static void test_first_allocation_is_exact(void)
{
    reach_d2d_surface_size next = reach_d2d_surface_capacity({0, 0}, {137, 59}, BUDGET, UNLIMITED);
    expect(same_size(next, {137, 59}), "a static layer gets no padding");
}

static void test_growth_reuses_surfaces(void)
{
    expect(simulate_allocations({{100, 100}, {180, 180}, {260, 260}, {400, 400}}) <= 4,
           "a coarse growth sequence reallocates at most three times after the first surface");
    std::vector<reach_d2d_surface_size> animation;
    for (uint32_t size = 100; size <= 400; size += 4)
        animation.push_back({size, size});
    expect(simulate_allocations(animation) <= 8,
           "a continuous resize reuses surfaces between growth steps");
    std::vector<reach_d2d_surface_size> there_and_back = animation;
    for (uint32_t size = 400; size >= 100; size -= 4)
        there_and_back.push_back({size, size});
    expect(simulate_allocations(there_and_back) <= 12,
           "closing back down does not reallocate every frame");
}

static void test_reuse_and_shrink(void)
{
    expect(same_size(reach_d2d_surface_capacity({400, 400}, {400, 400}, BUDGET, UNLIMITED),
                     {400, 400}),
           "an unchanged size reuses the surface");
    expect(same_size(reach_d2d_surface_capacity({400, 400}, {300, 300}, BUDGET, UNLIMITED),
                     {400, 400}),
           "a size above half the area reuses the surface");
    expect(same_size(reach_d2d_surface_capacity({400, 400}, {200, 200}, BUDGET, UNLIMITED),
                     {200, 200}),
           "a size below half the area shrinks to the exact size");
    expect(same_size(reach_d2d_surface_capacity({400, 400}, {400, 100}, BUDGET, UNLIMITED),
                     {400, 100}),
           "a thin size shrinks to the exact size");
}

static void test_growth_respects_budget_and_limits(void)
{
    expect(same_size(reach_d2d_surface_capacity({100, 100}, {200, 200}, BUDGET, UNLIMITED),
                     {256, 256}),
           "growth rounds up to the step");
    expect(
        same_size(reach_d2d_surface_capacity({100, 100}, {200, 200}, 200 * 200 * 4 + 1, UNLIMITED),
                  {200, 200}),
        "growth falls back to the exact size when the padded surface exceeds the budget");
    expect(same_size(reach_d2d_surface_capacity({100, 100}, {200, 200}, 0, UNLIMITED), {200, 200}),
           "an exhausted budget still yields the exact size");
    expect(same_size(reach_d2d_surface_capacity({16000, 100}, {16300, 100}, BUDGET, UNLIMITED),
                     {16384, 100}),
           "growth is clamped to the maximum surface extent");
    expect(same_size(reach_d2d_surface_capacity({10, 10}, {20, 10}, BUDGET, UNLIMITED), {20, 10}),
           "padding that would exceed twice the needed area is dropped");
    expect(same_size(reach_d2d_surface_capacity({100, 100}, {200, 200}, BUDGET, {220, 220}),
                     {220, 220}),
           "growth is clamped to the growth limit");
    expect(same_size(reach_d2d_surface_capacity({100, 100}, {200, 200}, BUDGET, {200, 200}),
                     {200, 200}),
           "a growth limit equal to the need allocates the exact size");
    expect(same_size(reach_d2d_surface_capacity({100, 100}, {300, 200}, BUDGET, {250, 250}),
                     {300, 250}),
           "an axis that needs more than the limit keeps its needed size");
    expect(
        same_size(reach_d2d_surface_capacity({100, 100}, {200, 200}, BUDGET, {0, 0}), {200, 200}),
        "an empty growth limit allocates the exact size");
}

static void test_planned_bytes_count_kept_capacity(void)
{
    expect(same_size(reach_d2d_planned_surface({400, 400}, {300, 300}), {400, 400}),
           "a reused surface is planned at its kept capacity");
    expect(same_size(reach_d2d_planned_surface({400, 400}, {200, 200}), {200, 200}),
           "a surface below half the area is planned at the exact size");
    expect(same_size(reach_d2d_planned_surface({400, 400}, {500, 300}), {500, 300}),
           "a surface that must grow is planned at the exact size");
    expect(same_size(reach_d2d_planned_surface({0, 0}, {300, 300}), {300, 300}),
           "a new surface is planned at the exact size");
    expect(reach_d2d_planned_layer_bytes(300.0f, 300.0f, true, false, {400, 400}) ==
               400u * 400u * 4u,
           "planned bytes count the kept capacity instead of the needed size");
    expect(reach_d2d_planned_layer_bytes(300.0f, 300.0f, true, true, {400, 400}) ==
               400u * 400u * 4u + 300u * 300u * 4u,
           "a shadow is planned at the needed size beside the kept paint surface");
    expect(reach_d2d_planned_layer_bytes(300.2f, 300.0f, true, false, {0, 0}) == 301u * 300u * 4u,
           "a layer without a surface is planned at the rounded needed size");
    expect(reach_d2d_planned_layer_bytes(300.0f, 300.0f, false, true, {400, 400}) ==
               300u * 300u * 4u,
           "a shadow-only layer ignores the paint capacity");
    expect(reach_d2d_planned_layer_bytes(0.0f, 300.0f, true, true, {400, 400}) == 0,
           "empty bounds cost nothing");
}

static reach_render_command solid_rect(float x, float y, float width, float height)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = {x, y, width, height};
    command.color = {0.25f, 0.5f, 0.75f, 1.0f};
    return command;
}

static void test_solid_fill_detection(void)
{
    reach_color color = {};
    reach_render_command fill = solid_rect(10.0f, 20.0f, 40.0f, 30.0f);
    expect(reach_d2d_solid_fill(&fill, 1, {10.0f, 20.0f, 40.0f, 30.0f}, &color),
           "a single covering fill is solid");
    expect(color.r == 0.25f && color.g == 0.5f && color.b == 0.75f && color.a == 1.0f,
           "the solid color is the fill color");

    reach_render_command fitted = fill;
    fitted.rect_fit_to_clip = 1;
    expect(!reach_d2d_solid_fill(&fitted, 1, {10.0f, 20.0f, 40.0f, 30.0f}, &color),
           "a fitted fill needs its raster");

    reach_render_command cornered = fill;
    cornered.corner_mask = REACH_RENDER_CORNER_TOP_LEFT;
    expect(!reach_d2d_solid_fill(&cornered, 1, {10.0f, 20.0f, 40.0f, 30.0f}, &color),
           "a corner-masked fill needs its raster");

    reach_render_command pair[2] = {fill, fill};
    expect(!reach_d2d_solid_fill(pair, 2, {10.0f, 20.0f, 40.0f, 30.0f}, &color),
           "two commands are not solid");
    expect(!reach_d2d_solid_fill(nullptr, 0, {10.0f, 20.0f, 40.0f, 30.0f}, &color),
           "no commands are not solid");

    reach_render_command rounded = fill;
    rounded.radius = 4.0f;
    expect(!reach_d2d_solid_fill(&rounded, 1, {10.0f, 20.0f, 40.0f, 30.0f}, &color),
           "a rounded fill needs its raster");

    reach_render_command stroked = fill;
    stroked.stroke_width = 1.0f;
    expect(!reach_d2d_solid_fill(&stroked, 1, {10.0f, 20.0f, 40.0f, 30.0f}, &color),
           "a stroked rect needs its raster");

    reach_render_command scissored = fill;
    scissored.has_scissor = 1;
    scissored.scissor_rect = {10.0f, 20.0f, 10.0f, 10.0f};
    expect(!reach_d2d_solid_fill(&scissored, 1, {10.0f, 20.0f, 40.0f, 30.0f}, &color),
           "a scissored fill needs its raster");

    reach_render_command clipped = fill;
    clipped.has_clip_rect = 1;
    expect(!reach_d2d_solid_fill(&clipped, 1, {10.0f, 20.0f, 40.0f, 30.0f}, &color),
           "a clipped fill needs its raster");

    reach_render_command text = fill;
    text.type = REACH_RENDER_COMMAND_TEXT;
    expect(!reach_d2d_solid_fill(&text, 1, {10.0f, 20.0f, 40.0f, 30.0f}, &color),
           "text is not solid");

    expect(!reach_d2d_solid_fill(&fill, 1, {10.0f, 20.0f, 41.0f, 30.0f}, &color),
           "a fill narrower than its surface is not solid");
    expect(!reach_d2d_solid_fill(&fill, 1, {9.0f, 20.0f, 40.0f, 30.0f}, &color),
           "a fill offset from its surface is not solid");

    reach_render_command fractional = solid_rect(0.0f, 0.0f, 11.0f, 7.0f);
    expect(reach_d2d_solid_fill(&fractional, 1, {0.0f, 0.0f, 10.5f, 6.25f}, &color),
           "a fill covering the rounded-up surface is solid");
    reach_render_command partial = solid_rect(0.0f, 0.0f, 10.5f, 6.25f);
    expect(!reach_d2d_solid_fill(&partial, 1, {0.0f, 0.0f, 10.5f, 6.25f}, &color),
           "a fill with antialiased edges is not solid");
}

static void test_solid_layers_cost_no_surface_bytes(void)
{
    reach_render_command fill = solid_rect(0.0f, 0.0f, 640.0f, 480.0f);
    reach_color color = {};
    bool solid = reach_d2d_solid_fill(&fill, 1, {0.0f, 0.0f, 640.0f, 480.0f}, &color);
    expect(solid, "a full-size fill is solid");
    expect(reach_d2d_planned_layer_bytes(640.0f, 480.0f, !solid, false, {640, 480}) == 0,
           "a solid layer plans no surface bytes even with a kept capacity");
    expect(reach_d2d_planned_layer_bytes(640.0f, 480.0f, true, false, {}) == 640u * 480u * 4u,
           "a painted layer plans its surface bytes");
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
    test_first_allocation_is_exact();
    test_growth_reuses_surfaces();
    test_reuse_and_shrink();
    test_growth_respects_budget_and_limits();
    test_planned_bytes_count_kept_capacity();
    test_solid_fill_detection();
    test_solid_layers_cost_no_surface_bytes();
    test_ungrouped_run_survives_unrelated_insertions();
    test_textbox_runs_split_and_stay_distinct();
    test_ungrouped_run_ids_stay_in_the_reserved_range();
    return failed == 0 ? 0 : 1;
}
