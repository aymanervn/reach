#include "host_internal.h"

void reach_host_stop_launcher_search_worker(reach_host *host)
{
    if (host != nullptr)
    {
        reach_search_service_stop(host->search_service);
    }
}

void reach_host_apply_launcher_search_results(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_search_candidate results[REACH_SEARCH_MAX_RESULTS] = {};
    size_t count = 0;
    int32_t error = 0;
    if (!reach_launcher_take_search_results(
            reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER), results,
            &count, &error))
    {
        return;
    }

    (void)reach_launcher_set_results(
        reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER), results,
        count);
    reach_launcher_set_search_error(
        reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER), error);
    host->dirty.layout = 1;
    host->launcher.dirty_flags = 1;
}
