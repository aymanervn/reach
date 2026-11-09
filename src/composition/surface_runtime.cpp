#include "reach/composition/surface_runtime.h"

int32_t reach_close_handoff_ready(int32_t completed, reach_result result, int32_t target_minimized)
{
    if (!completed)
    {
        return 0;
    }
    return result != REACH_OK || !target_minimized;
}

void reach_surface_runtime_init(reach_surface_runtime *runtime)
{
    if (runtime == nullptr)
    {
        return;
    }

    *runtime = {};
}

void reach_surface_runtime_mark_dirty(reach_surface_runtime *runtime, uint32_t dirty_flags)
{
    if (runtime == nullptr)
    {
        return;
    }

    runtime->dirty_flags |= dirty_flags;
}

void reach_surface_runtime_clear_dirty(reach_surface_runtime *runtime, uint32_t dirty_flags)
{
    if (runtime == nullptr)
    {
        return;
    }

    runtime->dirty_flags &= ~dirty_flags;
}
