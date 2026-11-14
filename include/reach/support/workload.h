#ifndef REACH_SUPPORT_WORKLOAD_H
#define REACH_SUPPORT_WORKLOAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_workload_counter
    {
        REACH_WORKLOAD_WAKEUPS,
        REACH_WORKLOAD_APP_UPDATES,
        REACH_WORKLOAD_CAPSULE_TICKS,
        REACH_WORKLOAD_SCALAR_ADVANCES,
        REACH_WORKLOAD_LOADER_ADVANCES,
        REACH_WORKLOAD_LAYOUTS,
        REACH_WORKLOAD_COMMAND_BUILDS,
        REACH_WORKLOAD_RENDER_EXECUTIONS,
        REACH_WORKLOAD_RASTER_PASSES,
        REACH_WORKLOAD_SWAP_CHAIN_PRESENTS,
        REACH_WORKLOAD_DCOMP_COMMITS,
        REACH_WORKLOAD_PROPERTY_SUBMISSIONS,
        REACH_WORKLOAD_NATIVE_PLACEMENT_REQUESTS,
        REACH_WORKLOAD_DWM_UPDATES,
        REACH_WORKLOAD_THUMBNAIL_REGISTRATIONS,
        REACH_WORKLOAD_THUMBNAIL_RELEASES,
        REACH_WORKLOAD_WUC_EXECUTIONS,
        REACH_WORKLOAD_DCOMP_EXECUTIONS,
        REACH_WORKLOAD_FALLBACK_EXECUTIONS,
        REACH_WORKLOAD_VISUAL_TREE_MUTATIONS,
        REACH_WORKLOAD_SCENE_TRANSFORM_SUBMISSIONS,
        REACH_WORKLOAD_COUNTER_COUNT
    } reach_workload_counter;

    typedef struct reach_workload_snapshot
    {
        uint64_t counters[REACH_WORKLOAD_COUNTER_COUNT];
        uint64_t retained_surface_bytes;
        uint64_t peak_retained_surface_bytes;
    } reach_workload_snapshot;

    int32_t reach_workload_enabled(void);
    void reach_workload_add(reach_workload_counter counter, uint64_t amount);
    void reach_workload_adjust_retained_bytes(int64_t difference);
    void reach_workload_read(reach_workload_snapshot *out);

#ifdef __cplusplus
}

#include <string>

class reach_workload_session
{
  public:
    explicit reach_workload_session(const char *application);
    ~reach_workload_session();
    reach_workload_session(const reach_workload_session &) = delete;
    reach_workload_session &operator=(const reach_workload_session &) = delete;

  private:
    std::string report_path;
    double started_seconds = 0.0;
};
#endif

#endif
