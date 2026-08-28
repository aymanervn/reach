#include "host_internal.h"

void reach_host_clear_sticky_dock_feedback(reach_host *host)
{
    if (host != nullptr && reach_dock_clear_context_feedback(
                               reach_host_feature_capsule<reach_dock>(host, REACH_SURFACE_ID_DOCK)))
    {
        host->dock.dirty_flags = 1;
    }
}
