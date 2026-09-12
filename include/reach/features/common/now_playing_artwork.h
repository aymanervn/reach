#ifndef REACH_FEATURES_COMMON_NOW_PLAYING_ARTWORK_H
#define REACH_FEATURES_COMMON_NOW_PLAYING_ARTWORK_H

#include "reach/core/render_commands.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_now_playing_artwork_render_input
    {
        reach_rect_f32 bounds;
        reach_rect_f32 cover_bounds;
        uint64_t cover_image_id;
        reach_color base_background;
        reach_color overlay;
        float radius;
        float cover_radius;
        float cover_fade_start;
        int32_t cover_corner_mask;
    } reach_now_playing_artwork_render_input;

    reach_result reach_now_playing_artwork_build_render_commands(
        const reach_now_playing_artwork_render_input *input,
        reach_render_command_buffer *out_commands);

#ifdef __cplusplus
}
#endif

#endif
