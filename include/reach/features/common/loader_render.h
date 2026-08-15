#ifndef REACH_FEATURES_COMMON_LOADER_RENDER_H
#define REACH_FEATURES_COMMON_LOADER_RENDER_H

#include "reach/core/geometry.h"
#include "reach/core/render_commands.h"

#ifdef __cplusplus
extern "C"
{
#endif

    reach_result reach_loader_build_render_commands(reach_rect_f32 container, reach_rect_f32 bar,
                                                    reach_rect_f32 origin, reach_color bar_color,
                                                    reach_render_command_buffer *out);

#ifdef __cplusplus
}
#endif

#endif
