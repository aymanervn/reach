#include "host_internal.h"

void reach_host_clear_sticky_dock_feedback(reach_host *host)
{
    if (host != nullptr && reach_dock_feedback_clear_sticky(host->dock_capsule))
    {
        host->dock.dirty_flags = 1;
    }
}
