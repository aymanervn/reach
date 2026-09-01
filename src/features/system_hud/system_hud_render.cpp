#include "system_hud_common.h"

#include "reach/core/typography.h"
#include "reach/features/common/level_presentation.h"
#include "reach/features/common/progress_bar_render.h"

static void reach_system_hud_push_text(reach_render_command_buffer *commands, reach_rect_f32 rect,
                                       const uint16_t *text, float size, int32_t weight,
                                       int32_t alignment, reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = rect;
    command.text_size = size;
    command.text_weight = weight;
    command.text_alignment = alignment;
    command.text_ellipsis = 1;
    command.color = color;
    reach_copy_utf16(command.text, 260, text);
    (void)reach_render_command_buffer_push(commands, &command);
}

static void reach_system_hud_push_vector_icon(reach_render_command_buffer *commands,
                                              reach_rect_f32 rect, reach_vector_icon_id icon_id,
                                              reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    command.rect = rect;
    command.icon_id = icon_id;
    command.color = color;
    (void)reach_render_command_buffer_push(commands, &command);
}

static reach_vector_icon_id reach_system_hud_media_action_icon(reach_now_playing_action action,
                                                               reach_media_playback_state playback)
{
    if (action == REACH_NOW_PLAYING_ACTION_PREVIOUS)
    {
        return REACH_VECTOR_ICON_PREVIOUS;
    }
    if (action == REACH_NOW_PLAYING_ACTION_NEXT)
    {
        return REACH_VECTOR_ICON_NEXT;
    }
    return playback == REACH_MEDIA_PLAYBACK_PLAYING ? REACH_VECTOR_ICON_PAUSE
                                                    : REACH_VECTOR_ICON_PLAY;
}

static void reach_system_hud_render_media(const reach_system_hud_state *state,
                                          const reach_system_hud_render_context *ctx,
                                          reach_render_command_buffer *commands)
{
    const reach_theme *theme = ctx->theme;
    if (state->media.cover_image_id != 0)
    {
        reach_render_command cover = {};
        cover.type = REACH_RENDER_COMMAND_ICON;
        cover.rect = state->layout.media_cover;
        cover.icon_id = state->media.cover_image_id;
        cover.icon_crop_to_fill = 1;
        cover.radius = 12.0f * ctx->dpi_scale;
        cover.color.a = 1.0f;
        (void)reach_render_command_buffer_push(commands, &cover);
    }
    else
    {
        reach_render_command placeholder = {};
        placeholder.type = REACH_RENDER_COMMAND_RECT;
        placeholder.rect = state->layout.media_cover;
        placeholder.radius = 12.0f * ctx->dpi_scale;
        placeholder.color = theme->system_hud_icon_background;
        (void)reach_render_command_buffer_push(commands, &placeholder);
        reach_rect_f32 glyph = state->layout.media_cover;
        glyph.x += glyph.width * 0.25f;
        glyph.y += glyph.height * 0.25f;
        glyph.width *= 0.5f;
        glyph.height *= 0.5f;
        reach_system_hud_push_vector_icon(commands, glyph, REACH_VECTOR_ICON_MUSIC_NOTE,
                                          theme->system_hud_glyph);
    }

    static const uint16_t fallback_title[] = {'M', 'e', 'd', 'i', 'a', 0};
    const uint16_t *title = state->media.title[0] != 0 ? state->media.title : fallback_title;
    reach_system_hud_push_text(commands, state->layout.title, title,
                               REACH_TEXT_SIZE_LARGE * ctx->dpi_scale, REACH_TEXT_WEIGHT_DEMIBOLD,
                               REACH_TEXT_ALIGNMENT_LEADING, theme->system_hud_primary_text);
    reach_system_hud_push_text(commands, state->layout.subtitle, state->media.artist,
                               REACH_TEXT_SIZE_SMALL * ctx->dpi_scale, REACH_TEXT_WEIGHT_NORMAL,
                               REACH_TEXT_ALIGNMENT_LEADING, theme->system_hud_secondary_text);
    reach_system_hud_push_vector_icon(
        commands, state->layout.media_action,
        reach_system_hud_media_action_icon(state->media_action, state->media.playback),
        theme->system_hud_glyph);
}

static void reach_system_hud_render_level(const reach_system_hud_state *state,
                                          const reach_system_hud_render_context *ctx,
                                          reach_render_command_buffer *commands)
{
    const reach_theme *theme = ctx->theme;
    int32_t volume = state->kind == REACH_SYSTEM_HUD_VOLUME;
    float level = volume ? state->volume.level : state->brightness.level;
    int32_t muted = volume && state->volume.muted;
    reach_vector_icon_id icon =
        volume ? reach_volume_level_icon(level, muted) : REACH_VECTOR_ICON_BRIGHTNESS;
    reach_system_hud_push_vector_icon(commands, state->layout.icon, icon, theme->system_hud_glyph);

    static const uint16_t volume_label[] = {'V', 'o', 'l', 'u', 'm', 'e', 0};
    static const uint16_t muted_label[] = {'M', 'u', 't', 'e', 'd', 0};
    static const uint16_t brightness_label[] = {'B', 'r', 'i', 'g', 'h', 't',
                                                'n', 'e', 's', 's', 0};
    const uint16_t *label = volume ? (muted ? muted_label : volume_label) : brightness_label;
    reach_system_hud_push_text(commands, state->layout.title, label,
                               REACH_TEXT_SIZE_MEDIUM * ctx->dpi_scale, REACH_TEXT_WEIGHT_DEMIBOLD,
                               REACH_TEXT_ALIGNMENT_LEADING, theme->system_hud_primary_text);

    uint16_t percent[8] = {};
    reach_level_format_percent(percent, 8, level);
    reach_system_hud_push_text(commands, state->layout.value, percent,
                               REACH_TEXT_SIZE_MEDIUM * ctx->dpi_scale, REACH_TEXT_WEIGHT_NORMAL,
                               REACH_TEXT_ALIGNMENT_TRAILING, theme->system_hud_secondary_text);

    reach_color fill = muted ? theme->system_hud_muted_fill : theme->system_hud_fill;
    reach_rect_f32 origin = {};
    (void)reach_progress_bar_build_render_commands(state->layout.track, state->layout.fill, origin,
                                                   theme->system_hud_track, fill, commands);
}

reach_result reach_system_hud_append_render_commands(const reach_system_hud *hud,
                                                     const reach_system_hud_render_context *ctx,
                                                     reach_render_command_buffer *out_commands)
{
    if (hud == nullptr || ctx == nullptr || ctx->theme == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    const reach_system_hud_state *state = &hud->state;
    if (!state->open || state->kind == REACH_SYSTEM_HUD_NONE)
    {
        return REACH_OK;
    }

    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_RECT;
    shape.rect = {0.0f, 0.0f, state->layout.bounds.width, state->layout.bounds.height};
    shape.radius = shape.rect.height * 0.5f;
    reach_result result = reach_render_push_bordered_background(
        out_commands, &shape, ctx->theme->system_hud_background, ctx->theme->system_hud_border,
        reach_theme_border_thickness(ctx->theme, ctx->dpi_scale), &ctx->theme->popup_shadow,
        ctx->dpi_scale);
    if (result != REACH_OK)
    {
        return result;
    }

    if (state->kind == REACH_SYSTEM_HUD_MEDIA)
    {
        reach_system_hud_render_media(state, ctx, out_commands);
    }
    else
    {
        reach_system_hud_render_level(state, ctx, out_commands);
    }
    reach_render_command_buffer_multiply_opacity(out_commands, reach_system_hud_opacity(hud));
    return REACH_OK;
}
