#ifndef REACH_FEATURES_COMMON_PROGRESS_BAR_RENDER_H
#define REACH_FEATURES_COMMON_PROGRESS_BAR_RENDER_H

#include "reach/core/geometry.h"
#include "reach/core/render_commands.h"

#ifdef __cplusplus
extern "C"
{
#endif

    reach_rect_f32 reach_progress_bar_fill_rect(reach_rect_f32 track, float level);
    reach_result reach_progress_bar_build_render_commands(reach_rect_f32 track, reach_rect_f32 fill,
                                                          reach_rect_f32 origin,
                                                          reach_color track_color,
                                                          reach_color fill_color,
                                                          reach_render_command_buffer *out);

#ifdef __cplusplus
}
#endif

#endif
