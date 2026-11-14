#include "system_hud_common.h"

#include "reach/features/common/level_presentation.h"

#include <math.h>
#include <new>

static const double REACH_SYSTEM_HUD_LIFETIME_SECONDS = 1.5;
static const double REACH_SYSTEM_HUD_APP_REMOVED_LIFETIME_SECONDS =
    REACH_SYSTEM_HUD_LIFETIME_SECONDS * 2.0;
static const float REACH_SYSTEM_HUD_MEDIA_COVER_SIZE = 48.0f;
static const size_t REACH_SYSTEM_HUD_APP_NAME_MAX_CHARACTERS = 8;

static double reach_system_hud_lifetime(const reach_system_hud *hud);

static int32_t reach_system_hud_rect_equal(reach_rect_f32 left, reach_rect_f32 right)
{
    return fabsf(left.x - right.x) < 0.5f && fabsf(left.y - right.y) < 0.5f &&
           fabsf(left.width - right.width) < 0.5f && fabsf(left.height - right.height) < 0.5f;
}

static void reach_system_hud_begin_show(reach_system_hud *hud, reach_system_hud_kind kind)
{
    if (hud == nullptr)
    {
        return;
    }
    hud->state.kind = kind;
    hud->state.open = 1;
    hud->state.deadline_seconds = reach_motion_now_seconds() + reach_system_hud_lifetime(hud);
    (void)reach_feature_transition_set_open(&hud->transition, 1);
}

const reach_system_hud_state *reach_system_hud_state_ptr(const reach_system_hud *hud)
{
    return hud != nullptr ? &hud->state : nullptr;
}

void reach_system_hud_attach_now_playing(reach_system_hud *hud, reach_now_playing_service *service)
{
    if (hud != nullptr)
    {
        hud->now_playing = service;
    }
}

void reach_system_hud_refresh_media(reach_system_hud *hud)
{
    if (hud != nullptr && hud->now_playing != nullptr)
    {
        reach_now_playing_service_snapshot(hud->now_playing, &hud->state.media);
    }
}

void reach_system_hud_show_media(reach_system_hud *hud, reach_now_playing_action action)
{
    if (hud == nullptr || action == REACH_NOW_PLAYING_ACTION_NONE)
    {
        return;
    }
    reach_system_hud_refresh_media(hud);
    hud->state.media_action = action;
    reach_system_hud_begin_show(hud, REACH_SYSTEM_HUD_MEDIA);
}

void reach_system_hud_show_volume(reach_system_hud *hud, const reach_audio_volume_state *state)
{
    if (hud == nullptr || state == nullptr)
    {
        return;
    }
    hud->state.volume = *state;
    reach_system_hud_begin_show(hud, REACH_SYSTEM_HUD_VOLUME);
}

void reach_system_hud_show_brightness(reach_system_hud *hud, const reach_brightness_state *state)
{
    if (hud == nullptr || state == nullptr || !state->available)
    {
        return;
    }
    hud->state.brightness = *state;
    reach_system_hud_begin_show(hud, REACH_SYSTEM_HUD_BRIGHTNESS);
}

static void reach_system_hud_copy_app_name(uint16_t *destination, const uint16_t *source)
{
    destination[0] = 0;
    if (source == nullptr)
    {
        return;
    }
    size_t length = reach_strlen_utf16(source);
    size_t copy_count = length;
    if (copy_count > REACH_SYSTEM_HUD_APP_NAME_MAX_CHARACTERS)
    {
        copy_count = REACH_SYSTEM_HUD_APP_NAME_MAX_CHARACTERS - 1;
        if (copy_count > 0 && source[copy_count - 1] >= 0xD800 && source[copy_count - 1] <= 0xDBFF)
        {
            --copy_count;
        }
    }
    for (size_t index = 0; index < copy_count; ++index)
    {
        destination[index] = source[index];
    }
    if (length > REACH_SYSTEM_HUD_APP_NAME_MAX_CHARACTERS)
    {
        destination[copy_count++] = 0x2026;
    }
    destination[copy_count] = 0;
}

void reach_system_hud_show_app_removed(reach_system_hud *hud, const uint16_t *app_name)
{
    if (hud == nullptr)
    {
        return;
    }
    reach_system_hud_copy_app_name(hud->state.app_name, app_name);
    reach_system_hud_begin_show(hud, REACH_SYSTEM_HUD_APP_REMOVED);
}

static double reach_system_hud_lifetime(const reach_system_hud *hud)
{
    return hud != nullptr && hud->state.kind == REACH_SYSTEM_HUD_APP_REMOVED
               ? REACH_SYSTEM_HUD_APP_REMOVED_LIFETIME_SECONDS
               : REACH_SYSTEM_HUD_LIFETIME_SECONDS;
}

void reach_system_hud_force_close(reach_system_hud *hud)
{
    if (hud == nullptr)
    {
        return;
    }
    hud->state.open = 0;
    hud->state.kind = REACH_SYSTEM_HUD_NONE;
    hud->state.deadline_seconds = 0.0;
    reach_feature_transition_reset(&hud->transition);
}

void reach_system_hud_reset(reach_system_hud *hud)
{
    if (hud == nullptr)
    {
        return;
    }
    hud->state = {};
    reach_feature_transition_reset(&hud->transition);
}

static void reach_system_hud_capsule_reset(void *capsule)
{
    reach_system_hud_reset(static_cast<reach_system_hud *>(capsule));
}

static void reach_system_hud_capsule_tick(void *capsule, double delta_seconds,
                                          reach_feature_tick_result *out)
{
    if (out != nullptr)
    {
        *out = {};
    }
    reach_system_hud *hud = static_cast<reach_system_hud *>(capsule);
    if (hud == nullptr || out == nullptr || !hud->state.open)
    {
        return;
    }

    (void)reach_feature_transition_tick(&hud->transition, delta_seconds);
    double now = reach_motion_now_seconds();
    if (now >= hud->state.deadline_seconds)
    {
        reach_system_hud_force_close(hud);
        out->redraw = 1;
        return;
    }

    double close_start =
        hud->state.deadline_seconds - reach_feature_transition_exit_seconds(&hud->transition);
    if (now >= close_start && hud->transition.target_open)
    {
        (void)reach_feature_transition_set_open(&hud->transition, 0);
        out->redraw = 1;
    }
}

static int32_t reach_system_hud_capsule_is_open(const void *capsule)
{
    const reach_system_hud *hud = static_cast<const reach_system_hud *>(capsule);
    return hud != nullptr && hud->state.open;
}

static int32_t reach_system_hud_capsule_needs_frame(const void *capsule)
{
    (void)capsule;
    return 0;
}

static void reach_system_hud_capsule_surface_geometry(const void *capsule,
                                                      reach_feature_surface_geometry *out)
{
    if (out == nullptr)
    {
        return;
    }
    *out = {};
    const reach_system_hud *hud = static_cast<const reach_system_hud *>(capsule);
    if (hud == nullptr)
    {
        return;
    }
    out->visible_bounds = hud->state.layout.bounds;
    out->envelope_bounds = hud->state.layout.bounds;
    reach_feature_transition_presentation(&hud->transition, out);
    out->wait_for_presentation = 0;
    out->deadline_seconds = hud->state.open
                                ? hud->state.deadline_seconds -
                                      (hud->transition.target_open
                                           ? reach_feature_transition_exit_seconds(&hud->transition)
                                           : 0.0)
                                : 0.0;
}

const reach_feature_capsule_ops *reach_system_hud_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {
        reach_system_hud_capsule_reset,
        reach_system_hud_capsule_tick,
        reach_system_hud_capsule_is_open,
        nullptr,
        reach_system_hud_capsule_needs_frame,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        reach_system_hud_capsule_surface_geometry,
    };
    return &ops;
}

reach_result reach_system_hud_create(reach_system_hud **out_hud)
{
    if (out_hud == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_system_hud *hud = new (std::nothrow) reach_system_hud();
    if (hud == nullptr)
    {
        return REACH_ERROR;
    }
    reach_feature_transition_init(&hud->transition, REACH_FEATURE_TRANSITION_FROM_BELOW);
    reach_system_hud_reset(hud);
    *out_hud = hud;
    return REACH_OK;
}

void reach_system_hud_destroy(reach_system_hud *hud)
{
    delete hud;
}

int32_t reach_system_hud_arrange(reach_system_hud *hud, const reach_system_hud_arrange_context *ctx)
{
    if (hud == nullptr || ctx == nullptr || ctx->theme == nullptr)
    {
        return 0;
    }

    float scale = ctx->dpi_scale > 0.0f ? ctx->dpi_scale : 1.0f;
    float base_width = hud->state.kind == REACH_SYSTEM_HUD_MEDIA
                           ? 352.0f
                           : (hud->state.kind == REACH_SYSTEM_HUD_APP_REMOVED ? 560.0f : 248.0f);
    float base_height = hud->state.kind == REACH_SYSTEM_HUD_MEDIA ? 64.0f : 48.0f;
    float base_border = reach_theme_border_thickness(ctx->theme, 1.0f);
    float width_scale = ctx->monitor_bounds.width / (base_width + 48.0f + base_border * 2.0f);
    float height_scale = ctx->monitor_bounds.height / (base_height + 48.0f + base_border * 2.0f);
    if (width_scale > 0.0f && scale > width_scale)
    {
        scale = width_scale;
    }
    if (height_scale > 0.0f && scale > height_scale)
    {
        scale = height_scale;
    }
    hud->state.content_scale = scale;
    float border = reach_theme_border_thickness(ctx->theme, scale);
    float inner_width = base_width * scale;
    float inner_height = base_height * scale;
    float width = inner_width + border * 2.0f;
    float height = inner_height + border * 2.0f;
    float gap = 12.0f * scale;
    float dock_top = ctx->dock_shown_bounds.height > 0.0f
                         ? ctx->dock_shown_bounds.y
                         : ctx->monitor_bounds.y + ctx->monitor_bounds.height - 76.0f * scale;
    reach_system_hud_layout next = {};
    next.bounds = {ctx->monitor_bounds.x + (ctx->monitor_bounds.width - width) * 0.5f,
                   dock_top - gap - height, width, height};
    float screen_margin = 24.0f * scale;
    float minimum_y = ctx->monitor_bounds.y + screen_margin;
    float maximum_y = ctx->monitor_bounds.y + ctx->monitor_bounds.height - screen_margin - height;
    if (next.bounds.y < minimum_y)
    {
        next.bounds.y = minimum_y;
    }
    if (next.bounds.y > maximum_y)
    {
        next.bounds.y = maximum_y;
    }

    float center_y = border + inner_height * 0.5f;
    float leading = border + 8.0f * scale;

    if (hud->state.kind == REACH_SYSTEM_HUD_MEDIA)
    {
        float cover_size = REACH_SYSTEM_HUD_MEDIA_COVER_SIZE * scale;
        next.media_cover = {leading, center_y - cover_size * 0.5f, cover_size, cover_size};
        float primary_size = 36.0f * scale;
        float secondary_size = 32.0f * scale;
        float button_gap = 2.0f * scale;
        float right = width - border - 10.0f * scale;
        next.media_next = {right - secondary_size, center_y - secondary_size * 0.5f, secondary_size,
                           secondary_size};
        next.media_play = {next.media_next.x - button_gap - primary_size,
                           center_y - primary_size * 0.5f, primary_size, primary_size};
        next.media_previous = {next.media_play.x - button_gap - secondary_size,
                               center_y - secondary_size * 0.5f, secondary_size, secondary_size};
        float content_x = next.media_cover.x + cover_size + 12.0f * scale;
        float content_width = next.media_previous.x - content_x - 8.0f * scale;
        float title_height = 18.0f * scale;
        float subtitle_height = 15.0f * scale;
        float text_top = center_y - (title_height + 2.0f * scale + subtitle_height) * 0.5f;
        next.title = {content_x, text_top, content_width, title_height};
        next.subtitle = {content_x, text_top + title_height + 2.0f * scale, content_width,
                         subtitle_height};
    }
    else if (hud->state.kind == REACH_SYSTEM_HUD_APP_REMOVED)
    {
        float padding = 14.0f * scale + border;
        next.title = {padding, center_y - 10.0f * scale, width - padding * 2.0f, 20.0f * scale};
    }
    else
    {
        float icon_size = 32.0f * scale;
        next.icon = {leading, center_y - icon_size * 0.5f, icon_size, icon_size};
        float value_width = 40.0f * scale;
        float value_height = 20.0f * scale;
        next.value = {width - border - 16.0f * scale - value_width, center_y - value_height * 0.5f,
                      value_width, value_height};
        float track_x = next.icon.x + icon_size + 12.0f * scale;
        float track_height = 8.0f * scale;
        next.track = {track_x, center_y - track_height * 0.5f,
                      next.value.x - 12.0f * scale - track_x, track_height};
        float level = hud->state.kind == REACH_SYSTEM_HUD_VOLUME ? hud->state.volume.level
                                                                 : hud->state.brightness.level;
        level = reach_level_clamp01(level);
        next.fill = next.track;
        next.fill.width *= level;
    }

    int32_t changed = !reach_system_hud_rect_equal(hud->state.layout.bounds, next.bounds) ||
                      !reach_system_hud_rect_equal(hud->state.layout.fill, next.fill);
    hud->state.layout = next;
    reach_feature_transition_configure(&hud->transition, ctx->theme, ctx->dpi_scale,
                                       REACH_FEATURE_TRANSITION_FROM_BELOW);
    return changed;
}
