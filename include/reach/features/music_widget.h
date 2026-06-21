#ifndef REACH_FEATURES_MUSIC_WIDGET_H
#define REACH_FEATURES_MUSIC_WIDGET_H

#include <stdint.h>

#include "reach/core/render_commands.h"
#include "reach/core/theme.h"
#include "reach/ports/media_controls.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_music_widget_action_type
    {
        REACH_MUSIC_WIDGET_ACTION_NONE = 0,
        REACH_MUSIC_WIDGET_ACTION_PREVIOUS = 1,
        REACH_MUSIC_WIDGET_ACTION_PLAY_PAUSE = 2,
        REACH_MUSIC_WIDGET_ACTION_NEXT = 3
    } reach_music_widget_action_type;

    typedef struct reach_music_widget_model
    {
        int32_t visible;
        uint16_t title[260];
        uint16_t artist[260];
        uint64_t cover_icon_id;
        reach_color cover_accent;
        reach_media_playback_state playback;
        int32_t previous_enabled;
        int32_t next_enabled;
    } reach_music_widget_model;

    typedef struct reach_music_widget_layout
    {
        reach_rect_f32 bounds;
        reach_rect_f32 cover;
        reach_rect_f32 title;
        reach_rect_f32 artist;
        reach_rect_f32 previous_button;
        reach_rect_f32 play_pause_button;
        reach_rect_f32 next_button;
    } reach_music_widget_layout;

    typedef struct reach_music_widget_render_input
    {
        const reach_theme *theme;
        const reach_music_widget_model *model;
        const reach_music_widget_layout *layout;
        float reveal_width;
        int32_t text_alignment_center;
        int32_t text_alignment_leading;
    } reach_music_widget_render_input;

    void reach_music_widget_model_init(reach_music_widget_model *model);
    float reach_music_widget_desired_width(const reach_music_widget_model *model,
                                           const reach_theme *theme, float dpi_scale);
    reach_music_widget_layout
    reach_music_widget_compute_layout(const reach_music_widget_model *model,
                                      const reach_theme *theme, reach_rect_f32 bounds,
                                      float dpi_scale);
    reach_music_widget_action_type
    reach_music_widget_hit_test(const reach_music_widget_model *model,
                                const reach_music_widget_layout *layout, int32_t x, int32_t y);
    reach_result
    reach_music_widget_build_render_commands(const reach_music_widget_render_input *input,
                                             reach_render_command_buffer *out_commands);

#ifdef __cplusplus
}
#endif

#endif
