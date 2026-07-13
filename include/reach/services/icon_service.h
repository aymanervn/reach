#ifndef REACH_SERVICES_ICON_SERVICE_H
#define REACH_SERVICES_ICON_SERVICE_H

#include <stdint.h>

#include "reach/ports/icon_provider.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * Icon service — the single owner of every app/window icon in the process.
     *
     * Pull model: consumers (dock, switcher, launcher — features attach the
     * service and call it while assembling render commands) ask
     * reach_icon_service_get(path, size_px) each time they draw. A cache hit
     * returns the render icon id immediately; a miss returns 0 (the consumer
     * draws its fallback), queues the load ONCE on the service's worker, and a
     * notify follows when it lands so the surface redraws into a hit. Failed
     * loads are negative-cached with a cooldown so per-frame gets cannot
     * retry-storm. Nobody holds handles and nothing migrates: keys are
     * (path, size), so list reshuffles are irrelevant to icon lifetime.
     *
     * Eviction is recency-based: trim() releases entries unused for max_age
     * through the provider and QUEUES their render ids; the host drains
     * take_evicted() on the main thread and evicts the images from every
     * surface renderer (cross-surface eviction stays host policy). At shutdown
     * the queue is simply dropped — renderer teardown already freed the images.
     *
     * The service stays mechanism; consumers stay policy-free. Optimizations
     * (key hashing, load prioritization, prefetch/pin hints, telemetry) belong
     * INSIDE the service — that centralization is the design's point.
     *
     * Depends only on the icon_provider PORT; loading keeps master's proven
     * crisp path behind it. Takes ownership of the provider (destroyed with
     * the service).
     */
    typedef struct reach_icon_service reach_icon_service;

    reach_result reach_icon_service_create(reach_icon_provider_port provider,
                                           reach_icon_service **out_service);
    void reach_icon_service_destroy(reach_icon_service *service);
    /* Join the load worker early (shutdown ordering); idempotent. */
    void reach_icon_service_stop(reach_icon_service *service);

    /* Fired on the worker thread after a load completes (hit next frame). */
    void reach_icon_service_set_notify(reach_icon_service *service, void (*notify)(void *user),
                                       void *notify_user);

    /* The pull: render icon id, or 0 (fallback) while loading / after a
       recent failure. Touches the entry's recency. */
    uint64_t reach_icon_service_get(reach_icon_service *service, const uint16_t *path,
                                    int32_t size_px);

    /* Recency eviction + the queued render-id drain (main thread). */
    void reach_icon_service_trim(reach_icon_service *service, double max_age_seconds);
    size_t reach_icon_service_take_evicted(reach_icon_service *service, uint64_t *out_icon_ids,
                                           size_t capacity);

    /* One or more loads landed since the last take (success or negative
       cache). The consumer redraws its icon surfaces so the next pull hits;
       drained on the main thread after the completion notify wakes it. */
    int32_t reach_icon_service_take_loads_completed(reach_icon_service *service);

    /* Loads queued/in flight, or evictions not yet drained. */
    int32_t reach_icon_service_work_pending(const reach_icon_service *service);

    /* Force-release everything through the provider and empty the cache;
       queued evictions are dropped (shutdown path). */
    void reach_icon_service_clear(reach_icon_service *service);

#ifdef __cplusplus
}
#endif

#endif
