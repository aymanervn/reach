#include "reach/support/workload.h"

#include "reach/support/motion.h"

#include <array>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>

static std::atomic<bool> workload_enabled{false};
static std::array<std::atomic<uint64_t>, REACH_WORKLOAD_COUNTER_COUNT> workload_counters{};
static std::atomic<uint64_t> retained_bytes{0};
static std::atomic<uint64_t> peak_retained_bytes{0};

static const char *const counter_names[] = {"wakeups",
                                            "app_updates",
                                            "capsule_ticks",
                                            "scalar_advances",
                                            "loader_advances",
                                            "scroll_advances",
                                            "layouts",
                                            "command_builds",
                                            "render_executions",
                                            "raster_passes",
                                            "swap_chain_presents",
                                            "dcomp_commits",
                                            "property_submissions",
                                            "native_placement_requests",
                                            "dwm_updates",
                                            "thumbnail_registrations",
                                            "thumbnail_releases",
                                            "wuc_executions",
                                            "dcomp_executions",
                                            "fallback_executions",
                                            "visual_tree_mutations",
                                            "scene_transform_submissions",
                                            "solid_layer_fills"};

static_assert(sizeof(counter_names) / sizeof(counter_names[0]) == REACH_WORKLOAD_COUNTER_COUNT);

int32_t reach_workload_enabled(void)
{
    return workload_enabled.load(std::memory_order_relaxed) ? 1 : 0;
}

void reach_workload_add(reach_workload_counter counter, uint64_t amount)
{
    if (reach_workload_enabled() && counter >= 0 && counter < REACH_WORKLOAD_COUNTER_COUNT)
        workload_counters[counter].fetch_add(amount, std::memory_order_relaxed);
}

void reach_workload_adjust_retained_bytes(int64_t difference)
{
    if (!reach_workload_enabled())
        return;
    uint64_t current =
        difference >= 0
            ? retained_bytes.fetch_add((uint64_t)difference, std::memory_order_relaxed) + difference
            : retained_bytes.fetch_sub((uint64_t)-difference, std::memory_order_relaxed) +
                  difference;
    uint64_t peak = peak_retained_bytes.load(std::memory_order_relaxed);
    while (current > peak &&
           !peak_retained_bytes.compare_exchange_weak(peak, current, std::memory_order_relaxed))
    {
    }
}

void reach_workload_read(reach_workload_snapshot *out)
{
    if (out == nullptr)
        return;
    for (size_t index = 0; index < REACH_WORKLOAD_COUNTER_COUNT; ++index)
        out->counters[index] = workload_counters[index].load(std::memory_order_relaxed);
    out->retained_surface_bytes = retained_bytes.load(std::memory_order_relaxed);
    out->peak_retained_surface_bytes = peak_retained_bytes.load(std::memory_order_relaxed);
}

reach_workload_session::reach_workload_session(const char *application)
{
    const char *prefix = std::getenv("REACH_WORKLOAD_REPORT");
    if (prefix == nullptr || *prefix == '\0' || application == nullptr)
        return;
    report_path = std::string(prefix) + "." + application + ".json";
    started_seconds = reach_motion_now_seconds();
    workload_enabled.store(true, std::memory_order_relaxed);
}

reach_workload_session::~reach_workload_session()
{
    if (report_path.empty())
        return;
    try
    {
        reach_workload_snapshot snapshot = {};
        reach_workload_read(&snapshot);
        std::ofstream report(std::filesystem::u8path(report_path));
        report << "{\n  \"schema\": 1,\n  \"elapsed_seconds\": "
               << reach_motion_now_seconds() - started_seconds << ",\n  \"counters\": {\n";
        for (size_t index = 0; index < REACH_WORKLOAD_COUNTER_COUNT; ++index)
            report << "    \"" << counter_names[index] << "\": " << snapshot.counters[index]
                   << (index + 1 < REACH_WORKLOAD_COUNTER_COUNT ? ",\n" : "\n");
        report << "  },\n  \"retained_surface_bytes\": " << snapshot.retained_surface_bytes
               << ",\n  \"peak_retained_surface_bytes\": " << snapshot.peak_retained_surface_bytes
               << "\n}\n";
    }
    catch (...)
    {
    }
}
