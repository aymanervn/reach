#ifndef REACH_FEATURES_COMMON_SECTION_REVEAL_H
#define REACH_FEATURES_COMMON_SECTION_REVEAL_H

#include "reach/core/render_commands.h"

#define REACH_SECTION_REVEAL_SECONDS 0.16

#ifdef __cplusplus
extern "C"
{
#endif

    void reach_section_reveal_apply(reach_render_command_buffer *buffer, size_t first_command,
                                    float progress, float maximum_y_offset);

#ifdef __cplusplus
}
#endif

#endif
