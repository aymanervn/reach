#ifndef REACH_FEATURES_COMMON_SCROLLBAR_RENDER_H
#define REACH_FEATURES_COMMON_SCROLLBAR_RENDER_H

#include "reach/core/geometry.h"
#include "reach/core/render_commands.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * Emits the scrollbar track and thumb as rounded RECT render commands
     * (corner radius = half the rect width), with coordinates made local to
     * `origin`. Shared by the launcher and clipboard scrollbars; the neutral
     * scroll model lives in core (reach/core/scrollbar.h).
     *
     * Callers decide *whether* to draw (their own visibility guard) and pass the
     * track/thumb colors; the geometry and rounded-rect emission are identical
     * across callers, so only that is consolidated here.
     */
    reach_result reach_scrollbar_build_render_commands(reach_rect_f32 track, reach_rect_f32 thumb,
                                                       reach_rect_f32 origin, reach_color track_color,
                                                       reach_color thumb_color,
                                                       reach_render_command_buffer *out);

#ifdef __cplusplus
}
#endif

#endif
