#ifndef REACH_FEATURES_DOCK_NOW_PLAYING_H
#define REACH_FEATURES_DOCK_NOW_PLAYING_H

#include "reach/core/render_commands.h"
#include "reach/services/now_playing.h"

typedef struct reach_dock_now_playing reach_dock_now_playing;

typedef struct reach_dock_now_playing_model
{
    int32_t visible;
    uint16_t title[260];
    uint16_t artist[260];
    uint64_t cover_image_id;
    reach_color cover_accent;
    reach_media_playback_state playback;
    int32_t previous_enabled;
    int32_t play_pause_enabled;
    int32_t next_enabled;
} reach_dock_now_playing_model;

typedef struct reach_dock_now_playing_layout
{
    reach_rect_f32 bounds;
    reach_rect_f32 cover;
    reach_rect_f32 title;
    reach_rect_f32 artist;
    reach_rect_f32 previous_button;
    reach_rect_f32 play_pause_button;
    reach_rect_f32 next_button;
} reach_dock_now_playing_layout;

typedef struct reach_dock_now_playing_update_result
{
    int32_t changed;
    int32_t visibility_changed;
} reach_dock_now_playing_update_result;

typedef struct reach_dock_now_playing_render_input
{
    const reach_theme *theme;
    const reach_dock_now_playing_model *model;
    const reach_dock_now_playing_layout *layout;
    float reveal_width;
} reach_dock_now_playing_render_input;

typedef struct reach_dock_now_playing_render_context
{
    const reach_theme *theme;
    float dpi_scale;

    float reveal_width;
} reach_dock_now_playing_render_context;

reach_result reach_dock_now_playing_create(reach_dock_now_playing **out_now_playing);
void reach_dock_now_playing_destroy(reach_dock_now_playing *now_playing);
void reach_dock_now_playing_reset(reach_dock_now_playing *now_playing);

void reach_dock_now_playing_model_init(reach_dock_now_playing_model *model);
float reach_dock_now_playing_model_desired_width(const reach_dock_now_playing_model *model,
                                                 const reach_theme *theme, float dpi_scale);
reach_dock_now_playing_layout
reach_dock_now_playing_compute_layout(const reach_dock_now_playing_model *model,
                                      const reach_theme *theme, reach_rect_f32 bounds,
                                      float dpi_scale);
reach_now_playing_action
reach_dock_now_playing_hit_test(const reach_dock_now_playing_model *model,
                                const reach_dock_now_playing_layout *layout, int32_t x, int32_t y);
reach_result
reach_dock_now_playing_build_render_commands(const reach_dock_now_playing_render_input *input,
                                             reach_render_command_buffer *out_commands);

void reach_dock_now_playing_sync(reach_dock_now_playing *now_playing,
                                 reach_now_playing_service *service,
                                 reach_dock_now_playing_update_result *out);
int32_t reach_dock_now_playing_visible(const reach_dock_now_playing *now_playing);
float reach_dock_now_playing_desired_width(const reach_dock_now_playing *now_playing,
                                           const reach_theme *theme, float dpi_scale);
void reach_dock_now_playing_relayout(reach_dock_now_playing *now_playing, const reach_theme *theme,
                                     reach_rect_f32 bounds, float dpi_scale);

int32_t reach_dock_now_playing_pointer_down(reach_dock_now_playing *now_playing, int32_t x,
                                            int32_t y);
int32_t reach_dock_now_playing_pointer_up(reach_dock_now_playing *now_playing, int32_t x, int32_t y,
                                          reach_now_playing_action *out_action);
int32_t reach_dock_now_playing_pointer_cancel(reach_dock_now_playing *now_playing);

reach_result
reach_dock_now_playing_append_render_commands(reach_dock_now_playing *now_playing,
                                              const reach_dock_now_playing_render_context *ctx,
                                              reach_render_command_buffer *out_commands);

#endif
