#ifndef REACH_SERVICES_SEARCH_H
#define REACH_SERVICES_SEARCH_H

#include <stdint.h>

#include "reach/core/ui_state.h"
#include "reach/ports/search_provider.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * Search service — the async query executor over the search-provider port
     * (the worker thread that used to live in composition). Latest-wins: each
     * submit replaces the pending query; `generation` is the caller's
     * correlation id, returned with the completed result set so stale
     * completions can be dropped (the launcher owns the generation counter).
     * `notify` fires on the worker thread after a result set completes; the
     * consumer drains with take_completed on its own thread.
     *
     * The provider port is borrowed (composition owns and destroys it).
     */
    typedef struct reach_search_service reach_search_service;

    reach_result reach_search_service_create(reach_search_provider_port provider,
                                             void (*notify)(void *user), void *notify_user,
                                             reach_search_service **out_service);
    void reach_search_service_destroy(reach_search_service *service);
    /* Join the worker early (shutdown ordering); idempotent. */
    void reach_search_service_stop(reach_search_service *service);

    reach_result reach_search_service_submit(reach_search_service *service, const uint16_t *query,
                                             uint32_t generation);
    /* Drop the pending query and any completed-but-untaken result set. */
    void reach_search_service_cancel(reach_search_service *service);
    /* Returns 1 when a completed result set was taken. */
    int32_t reach_search_service_take_completed(reach_search_service *service,
                                                uint32_t *out_generation,
                                                reach_search_candidate *out_results,
                                                size_t *out_count);
    int32_t reach_search_service_work_pending(const reach_search_service *service);

#ifdef __cplusplus
}
#endif

#endif
