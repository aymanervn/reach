#ifndef REACH_FEATURES_COMMON_SCROLLBAR_RENDER_H
#define REACH_FEATURES_COMMON_SCROLLBAR_RENDER_H

#include "reach/core/geometry.h"
#include "reach/core/render_commands.h"

#ifdef __cplusplus
extern "C"
{
#endif

    reach_result reach_scrollbar_build_render_commands(reach_rect_f32 track, reach_rect_f32 thumb,
                                                       reach_rect_f32 origin,
                                                       reach_color track_color,
                                                       reach_color thumb_color,
                                                       reach_render_command_buffer *out);

#ifdef __cplusplus
}
#endif

#endif
