#ifndef REACH_FEATURES_SYSTEM_HUD_H
#define REACH_FEATURES_SYSTEM_HUD_H

#include "reach/core/render_commands.h"
#include "reach/core/theme.h"
#include "reach/features/feature_capsule.h"
#include "reach/ports/audio_volume.h"
#include "reach/ports/system_controls.h"
#include "reach/services/now_playing.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_system_hud_kind
    {
        REACH_SYSTEM_HUD_NONE = 0,
        REACH_SYSTEM_HUD_MEDIA = 1,
        REACH_SYSTEM_HUD_VOLUME = 2,
        REACH_SYSTEM_HUD_BRIGHTNESS = 3
    } reach_system_hud_kind;

    typedef struct reach_system_hud_layout
    {
        reach_rect_f32 bounds;
        reach_rect_f32 icon;
        reach_rect_f32 title;
        reach_rect_f32 subtitle;
        reach_rect_f32 value;
        reach_rect_f32 track;
        reach_rect_f32 fill;
        reach_rect_f32 media_cover;
        reach_rect_f32 media_action;
    } reach_system_hud_layout;

    typedef struct reach_system_hud_state
    {
        reach_system_hud_kind kind;
        int32_t open;
        int32_t hovered;
        reach_system_hud_layout layout;
        reach_audio_volume_state volume;
        reach_brightness_state brightness;
        reach_now_playing_snapshot media;
        reach_now_playing_action media_action;
        double visible_seconds;
    } reach_system_hud_state;

    typedef struct reach_system_hud_arrange_context
    {
        const reach_theme *theme;
        reach_rect_f32 monitor_bounds;
        reach_rect_f32 dock_shown_bounds;
        float dpi_scale;
    } reach_system_hud_arrange_context;

    typedef struct reach_system_hud_render_context
    {
        const reach_theme *theme;
        float dpi_scale;
    } reach_system_hud_render_context;

    typedef struct reach_system_hud reach_system_hud;

    reach_result reach_system_hud_create(reach_system_hud **out_hud);
    void reach_system_hud_destroy(reach_system_hud *hud);
    void reach_system_hud_reset(reach_system_hud *hud);
    void reach_system_hud_force_close(reach_system_hud *hud);

    const reach_feature_capsule_ops *reach_system_hud_capsule_ops(void);
    const reach_system_hud_state *reach_system_hud_state_ptr(const reach_system_hud *hud);

    void reach_system_hud_attach_now_playing(reach_system_hud *hud,
                                             reach_now_playing_service *service);
    void reach_system_hud_show_media(reach_system_hud *hud, reach_now_playing_action action);
    void reach_system_hud_show_volume(reach_system_hud *hud, const reach_audio_volume_state *state);
    void reach_system_hud_show_brightness(reach_system_hud *hud,
                                          const reach_brightness_state *state);
    void reach_system_hud_hide(reach_system_hud *hud);
    void reach_system_hud_refresh_media(reach_system_hud *hud);
    int32_t reach_system_hud_arrange(reach_system_hud *hud,
                                     const reach_system_hud_arrange_context *ctx);
    float reach_system_hud_opacity(const reach_system_hud *hud);
    reach_result reach_system_hud_append_render_commands(const reach_system_hud *hud,
                                                         const reach_system_hud_render_context *ctx,
                                                         reach_render_command_buffer *out_commands);

#ifdef __cplusplus
}
#endif

#endif
