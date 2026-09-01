#ifndef REACH_FEATURES_COMMON_ICON_FEEDBACK_H
#define REACH_FEATURES_COMMON_ICON_FEEDBACK_H

#include <stdint.h>

#include "reach/core/render_commands.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void reach_push_icon_press_feedback(reach_render_command_buffer *commands, reach_rect_f32 rect,
                                        float radius, uint64_t icon_id, reach_color color,
                                        float opacity, float minimum_opacity);

#ifdef __cplusplus
}
#endif

#endif
