#include "reach/shell/surface_runtime.h"

void reach_surface_runtime_init(reach_surface_runtime *runtime)
{
    if (runtime == nullptr) {
        return;
    }

    *runtime = {};
}

void reach_surface_runtime_mark_dirty(reach_surface_runtime *runtime, uint32_t dirty_flags)
{
    if (runtime == nullptr) {
        return;
    }

    runtime->dirty_flags |= dirty_flags;
}

void reach_surface_runtime_clear_dirty(reach_surface_runtime *runtime, uint32_t dirty_flags)
{
    if (runtime == nullptr) {
        return;
    }

    runtime->dirty_flags &= ~dirty_flags;
}
