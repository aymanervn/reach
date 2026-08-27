#include "system_hud_common.h"

#include "reach/features/common/level_presentation.h"

#include <math.h>
#include <new>

static const double REACH_SYSTEM_HUD_VISIBLE_SECONDS = 1.5;
static const double REACH_SYSTEM_HUD_DEFAULT_OPEN_SECONDS = 0.16;
static const double REACH_SYSTEM_HUD_DEFAULT_CLOSE_SECONDS = 0.12;

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
    hud->state.visible_seconds = 0.0;
    reach_animation_manager_animate_to(&hud->animations, REACH_SYSTEM_HUD_ANIMATION_OPACITY, 1.0f,
                                       hud->open_seconds, REACH_EASING_EASE_OUT);
}

void reach_system_hud_begin_close(reach_system_hud *hud)
{
    if (hud == nullptr || !hud->state.open || hud->state.hovered ||
        reach_animation_manager_target(&hud->animations, REACH_SYSTEM_HUD_ANIMATION_OPACITY) <=
            0.0f)
    {
        return;
    }
    reach_animation_manager_animate_to(&hud->animations, REACH_SYSTEM_HUD_ANIMATION_OPACITY, 0.0f,
                                       hud->close_seconds, REACH_EASING_EASE_IN);
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

void reach_system_hud_hide(reach_system_hud *hud)
{
    if (hud == nullptr)
    {
        return;
    }
    hud->state.hovered = 0;
    reach_system_hud_begin_close(hud);
}

float reach_system_hud_opacity(const reach_system_hud *hud)
{
    return hud != nullptr
               ? reach_animation_manager_value(&hud->animations, REACH_SYSTEM_HUD_ANIMATION_OPACITY)
               : 0.0f;
}

void reach_system_hud_force_close(reach_system_hud *hud)
{
    if (hud == nullptr)
    {
        return;
    }
    hud->state.open = 0;
    hud->state.hovered = 0;
    hud->state.kind = REACH_SYSTEM_HUD_NONE;
    hud->state.visible_seconds = 0.0;
    reach_animation_manager_set(&hud->animations, REACH_SYSTEM_HUD_ANIMATION_OPACITY, 0.0f);
}

void reach_system_hud_reset(reach_system_hud *hud)
{
    if (hud == nullptr)
    {
        return;
    }
    hud->state = {};
    hud->open_seconds = REACH_SYSTEM_HUD_DEFAULT_OPEN_SECONDS;
    hud->close_seconds = REACH_SYSTEM_HUD_DEFAULT_CLOSE_SECONDS;
    reach_animation_manager_set(&hud->animations, REACH_SYSTEM_HUD_ANIMATION_OPACITY, 0.0f);
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

    if (delta_seconds < 0.0)
    {
        delta_seconds = 0.0;
    }
    int32_t was_animating = reach_animation_manager_any_active(&hud->animations);
    reach_animation_manager_tick(&hud->animations, delta_seconds);
    int32_t animating = reach_animation_manager_any_active(&hud->animations);
    if (was_animating || animating)
    {
        out->redraw = 1;
    }

    if (!hud->state.hovered)
    {
        hud->state.visible_seconds += delta_seconds;
        if (hud->state.visible_seconds >= REACH_SYSTEM_HUD_VISIBLE_SECONDS &&
            reach_animation_manager_target(&hud->animations, REACH_SYSTEM_HUD_ANIMATION_OPACITY) >
                0.0f)
        {
            reach_system_hud_begin_close(hud);
            out->redraw = 1;
        }
    }

    if (!animating && reach_system_hud_opacity(hud) <= 0.001f &&
        reach_animation_manager_target(&hud->animations, REACH_SYSTEM_HUD_ANIMATION_OPACITY) <=
            0.0f)
    {
        reach_system_hud_force_close(hud);
        out->redraw = 1;
        return;
    }
    out->request_update = 1;
}

static int32_t reach_system_hud_capsule_is_open(const void *capsule)
{
    const reach_system_hud *hud = static_cast<const reach_system_hud *>(capsule);
    return hud != nullptr && hud->state.open;
}

static int32_t reach_system_hud_capsule_needs_frame(const void *capsule)
{
    return reach_system_hud_capsule_is_open(capsule);
}

static int32_t reach_system_hud_capsule_wants_pointer_move(const void *capsule)
{
    return reach_system_hud_capsule_is_open(capsule);
}

static void reach_system_hud_capsule_handle_pointer(void *capsule, const reach_pointer_event *event,
                                                    reach_capsule_pointer_result *out)
{
    if (out == nullptr)
    {
        return;
    }
    *out = {};
    reach_system_hud *hud = static_cast<reach_system_hud *>(capsule);
    if (hud == nullptr || event == nullptr || !hud->state.open)
    {
        return;
    }

    out->handled = 1;
    if (event->kind == REACH_POINTER_EVENT_MOVE && !hud->state.hovered)
    {
        hud->state.hovered = 1;
        reach_animation_manager_animate_to(&hud->animations, REACH_SYSTEM_HUD_ANIMATION_OPACITY,
                                           1.0f, hud->open_seconds, REACH_EASING_EASE_OUT);
        out->redraw = 1;
    }
    else if (event->kind == REACH_POINTER_EVENT_LEAVE && hud->state.hovered)
    {
        hud->state.hovered = 0;
        if (hud->state.visible_seconds >= REACH_SYSTEM_HUD_VISIBLE_SECONDS)
        {
            reach_system_hud_begin_close(hud);
        }
        out->redraw = 1;
    }
}

static size_t reach_system_hud_capsule_input_regions(const void *capsule,
                                                     reach_rect_f32 *out_regions,
                                                     size_t max_regions)
{
    const reach_system_hud *hud = static_cast<const reach_system_hud *>(capsule);
    if (hud == nullptr || !hud->state.open || out_regions == nullptr || max_regions == 0)
    {
        return 0;
    }
    out_regions[0] = {0.0f, 0.0f, hud->state.layout.bounds.width, hud->state.layout.bounds.height};
    return 1;
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
}

const reach_feature_capsule_ops *reach_system_hud_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {
        reach_system_hud_capsule_reset,          reach_system_hud_capsule_tick,
        reach_system_hud_capsule_is_open,        nullptr,
        reach_system_hud_capsule_needs_frame,    reach_system_hud_capsule_wants_pointer_move,
        reach_system_hud_capsule_handle_pointer, nullptr,
        reach_system_hud_capsule_input_regions,  reach_system_hud_capsule_surface_geometry,
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
    reach_animation_manager_init(&hud->animations, hud->animation_tracks,
                                 REACH_SYSTEM_HUD_ANIMATION_COUNT);
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
    float border = reach_theme_border_thickness(ctx->theme, scale);
    float inner_width = (hud->state.kind == REACH_SYSTEM_HUD_MEDIA ? 340.0f : 272.0f) * scale;
    float inner_height = (hud->state.kind == REACH_SYSTEM_HUD_MEDIA ? 84.0f : 76.0f) * scale;
    float width = inner_width + border * 2.0f;
    float height = inner_height + border * 2.0f;
    float gap = 12.0f * scale;
    float dock_top = ctx->dock_shown_bounds.height > 0.0f
                         ? ctx->dock_shown_bounds.y
                         : ctx->monitor_bounds.y + ctx->monitor_bounds.height - 76.0f * scale;

    reach_system_hud_layout next = {};
    next.bounds = {ctx->monitor_bounds.x + (ctx->monitor_bounds.width - width) * 0.5f,
                   dock_top - gap - height, width, height};

    float padding = 14.0f * scale + border;
    float icon_size = (hud->state.kind == REACH_SYSTEM_HUD_MEDIA ? 56.0f : 40.0f) * scale;
    next.icon = {padding, border + (inner_height - icon_size) * 0.5f, icon_size, icon_size};
    next.media_cover = next.icon;

    float content_x = next.icon.x + next.icon.width + 14.0f * scale;
    float trailing = 14.0f * scale + border;
    float content_width = width - content_x - trailing;

    if (hud->state.kind == REACH_SYSTEM_HUD_MEDIA)
    {
        float action_size = 28.0f * scale;
        next.media_action = {width - trailing - action_size,
                             border + (inner_height - action_size) * 0.5f, action_size,
                             action_size};
        content_width = next.media_action.x - content_x - 12.0f * scale;
        next.title = {content_x, border + 18.0f * scale, content_width, 22.0f * scale};
        next.subtitle = {content_x, next.title.y + 24.0f * scale, content_width, 18.0f * scale};
    }
    else
    {
        next.title = {content_x, border + 13.0f * scale, content_width - 54.0f * scale,
                      20.0f * scale};
        next.value = {content_x + content_width - 54.0f * scale, next.title.y, 54.0f * scale,
                      next.title.height};
        next.track = {content_x, border + 48.0f * scale, content_width, 6.0f * scale};
        float level = hud->state.kind == REACH_SYSTEM_HUD_VOLUME ? hud->state.volume.level
                                                                 : hud->state.brightness.level;
        level = reach_level_clamp01(level);
        next.fill = next.track;
        next.fill.width *= level;
    }

    int32_t changed = !reach_system_hud_rect_equal(hud->state.layout.bounds, next.bounds) ||
                      !reach_system_hud_rect_equal(hud->state.layout.fill, next.fill);
    hud->state.layout = next;
    hud->open_seconds = ctx->theme->surface_open_seconds;
    hud->close_seconds = ctx->theme->surface_close_seconds;
    return changed;
}
