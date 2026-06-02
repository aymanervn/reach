#ifndef REACH_SHELL_SURFACE_RUNTIME_H
#define REACH_SHELL_SURFACE_RUNTIME_H

#include <stdint.h>

#include "reach/core/ui_events.h"
#include "reach/ports/platform_window.h"
#include "reach/ports/render_backend.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_surface_runtime
    {
        reach_platform_window_port window;
        reach_render_backend_port renderer;
        reach_rect_f32 last_bounds;
        float last_opacity;
        int32_t bounds_valid;
        int32_t opacity_valid;
        uint32_t dirty_flags;
    } reach_surface_runtime;

    void reach_surface_runtime_init(reach_surface_runtime *runtime);
    void reach_surface_runtime_mark_dirty(reach_surface_runtime *runtime, uint32_t dirty_flags);
    void reach_surface_runtime_clear_dirty(reach_surface_runtime *runtime, uint32_t dirty_flags);

#ifdef __cplusplus
}
#endif

#endif
