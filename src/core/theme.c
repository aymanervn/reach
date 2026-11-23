#include "reach/core/theme.h"
#include "reach/core/config.h"
#include "reach/support/util.h"
#include "theme_palette.h"

#include <math.h>

#define REACH_THEME_STAGE_BACKDROP_ALPHA 1.0f
#define REACH_THEME_ACCENT_CONTRAST 4.5f
#define REACH_THEME_GLASS_PRIMARY_CONTRAST 4.5f
#define REACH_THEME_GLASS_SECONDARY_CONTRAST 3.0f

#define REACH_SHADOW_AMBIENT(blur_px)                                                              \
    {.offset_x = 0.0f, .offset_y = 0.0f, .blur = (blur_px), .color = REACH_PURE_BLACK(0.3f)}

#define REACH_THEME_METRICS                                                                        \
    .radius_small = 12.0f, .radius_large = 20.0f, .button_pressed_darken = 0.65f,                  \
    .dock_corner_radius_ratio = 0.5f, .border_thickness = 1.0f, .icon_box_height_ratio = 0.60f,    \
    .icon_max_box_ratio = 0.76f, .icon_box_corner_radius_ratio = 0.28f,                            \
    .icon_box_corner_radius_max = 14.0f, .tray_slot_size_ratio = 0.605f,                           \
    .bar_separator_dot_size = 4.0f, .bar_separator_dot_gap = 9.0f, .now_playing_width = 220.0f,    \
    .now_playing_padding = 9.0f, .now_playing_gap = 6.0f, .now_playing_control_gap = 8.0f,         \
    .now_playing_play_button_width = 20.0f, .now_playing_prev_next_button_width = 13.2f,           \
    .stage_animation_seconds = 0.30f, .popup_rise_distance = 10.0f, .popup_rise_scale = 0.97f

#define REACH_THEME_SPRING(response, damping) {REACH_EASING_SPRING, (response), (damping), 0.0}
#define REACH_THEME_FADE(seconds) {REACH_EASING_EASE_OUT, 0.0f, 0.0f, (seconds)}

#define REACH_THEME_MOTION                                                                         \
    .motion_presets =                                                                              \
        {                                                                                          \
            [REACH_MOTION_PRESET_SNAPPY] = REACH_THEME_SPRING(0.24f, 1.0f),                        \
            [REACH_MOTION_PRESET_SMOOTH] = REACH_THEME_SPRING(0.38f, 1.0f),                        \
            [REACH_MOTION_PRESET_BOUNCY] = REACH_THEME_SPRING(0.34f, 0.78f),                       \
            [REACH_MOTION_PRESET_PRESS] = REACH_THEME_SPRING(0.16f, 1.0f),                         \
            [REACH_MOTION_PRESET_RELEASE] = REACH_THEME_SPRING(0.30f, 0.72f),                      \
            [REACH_MOTION_PRESET_FADE_IN] = REACH_THEME_FADE(0.14),                                \
            [REACH_MOTION_PRESET_FADE_OUT] = REACH_THEME_FADE(0.11),                               \
            [REACH_MOTION_PRESET_CARD_FADE_IN] = REACH_THEME_FADE(0.05),                           \
            [REACH_MOTION_PRESET_CARD_FADE_OUT] = REACH_THEME_FADE(0.18),                          \
            [REACH_MOTION_PRESET_BAR_REVEAL] = REACH_THEME_SPRING(0.28f, 1.0f),                    \
            [REACH_MOTION_PRESET_POPUP_RISE] = REACH_THEME_SPRING(0.30f, 1.0f),                    \
            [REACH_MOTION_PRESET_POPUP_FADE_IN] = REACH_THEME_FADE(0.10),                          \
        },                                                                                         \
    .stage_motion_reference_seconds = 0.38f

static const reach_theme reach_theme_dark = {
    .mode = REACH_THEME_MODE_DARK,
    .glass = {.intensity = REACH_CONFIG_GLASS_INTENSITY_DEFAULT, .floor = 0.0f, .ceiling = 1.0f},

    .text = {.primary = REACH_DARK_TEXT(REACH_TEXT_PRIMARY),
             .secondary = REACH_DARK_TEXT(REACH_TEXT_SECONDARY),
             .tertiary = REACH_DARK_TEXT(REACH_TEXT_TERTIARY),
             .quaternary = REACH_DARK_TEXT(REACH_TEXT_QUATERNARY),
             .inverse = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY)},
    .surface = {.base = REACH_DARK_SURFACE_BASE(1.0f),
                .raised = REACH_DARK_SURFACE_RAISED(1.0f),
                .card = REACH_DARK_SURFACE_ELEVATED(1.0f),
                .item = REACH_DARK_SURFACE_ELEVATED(1.0f),
                .well = REACH_DARK_SURFACE_ELEVATED(1.0f)},
    .fill = {.primary = REACH_FILL(REACH_DARK_FILL_PRIMARY),
             .secondary = REACH_FILL(REACH_DARK_FILL_SECONDARY),
             .tertiary = REACH_FILL(REACH_DARK_FILL_TERTIARY)},
    .status = {.success = REACH_GREEN_BRIGHT(1.0f), .error = REACH_RED_BRIGHT(1.0f)},
    .accent = {.fill = REACH_BLUE_DEEP(1.0f),
               .text = REACH_BLUE_BRIGHT(1.0f),
               .tint = REACH_BLUE_DEEP(REACH_ACCENT_TINT),
               .selection = REACH_BLUE_DEEP(REACH_ACCENT_SELECTION),
               .faint = REACH_BLUE_DEEP(REACH_ACCENT_FAINT),
               .text_tint = REACH_BLUE_BRIGHT(REACH_ACCENT_TINT),
               .text_muted = REACH_BLUE_BRIGHT(REACH_ACCENT_MUTED),
               .tint_amount = REACH_ACCENT_TINT},
    .border = REACH_DARK_BORDER(1.0f),
    .divider = REACH_DARK_DIVIDER(1.0f),
    .on_color = REACH_PURE_WHITE(1.0f),
    .danger = REACH_RED_DEEP(1.0f),
    .pressed_overlay = REACH_PURE_BLACK(0.12f),
    .artwork_scrim = REACH_DARK_SURFACE_BASE(0.35f),
    .shadow = REACH_SHADOW_AMBIENT(5.0f),

    .top_bar_scrim = REACH_PURE_BLACK(0.65f),
    .top_bar_battery_fill = REACH_GREEN_DEEP(1.0f),
    .top_bar_battery_low_fill = REACH_RED_DEEP(1.0f),
    .top_bar_battery_saver_fill = REACH_YELLOW_DEEP(1.0f),
    .top_bar_battery_charging_fill = REACH_BLUE_DEEP(1.0f),
    .top_bar_cpu_text = REACH_CYAN_BRIGHT(1.0f),
    .top_bar_memory_text = REACH_LAVENDER_BRIGHT(1.0f),
    .top_bar_download_text = REACH_GREEN_BRIGHT(1.0f),
    .top_bar_upload_text = REACH_ORANGE_BRIGHT(1.0f),
    .dock_fallback_icon_background = REACH_PURE_WHITE(0.80f),
    .bar_fallback_icon_text = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),
    .widgets_artwork_overlay = REACH_DARK_SURFACE_RAISED(0.48f),
    // Opaque accent blends exist only once resolved; unresolved tables keep the plain surface.
    .widgets_accent_background = REACH_DARK_SURFACE_RAISED(1.0f),
    .stage_background = REACH_DARK_SURFACE_BASE(REACH_THEME_STAGE_BACKDROP_ALPHA),
    .stage_tile_header_background = REACH_DARK_SURFACE_ELEVATED(0.45f),
    .stage_close_glyph = REACH_PURE_WHITE(0.80f),
    .screenshot_scrim = REACH_PURE_BLACK(0.45f),
    .screenshot_label_background = REACH_DARK_SURFACE_BASE(0.92f),
    .settings_background = REACH_DARK_SURFACE_BASE(1.0f),
    .settings_window_minimize_hover_background = REACH_YELLOW_BRIGHT(1.0f),
    .settings_avatar_border = REACH_DARK_SURFACE_ELEVATED(1.0f),

    REACH_THEME_METRICS,
    REACH_THEME_MOTION,
};

static const reach_theme reach_theme_light = {
    .mode = REACH_THEME_MODE_LIGHT,
    .glass = {.intensity = REACH_CONFIG_GLASS_INTENSITY_DEFAULT, .floor = 0.0f, .ceiling = 1.0f},

    .text = {.primary = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),
             .secondary = REACH_LIGHT_TEXT(REACH_TEXT_SECONDARY),
             .tertiary = REACH_LIGHT_TEXT(REACH_TEXT_TERTIARY),
             .quaternary = REACH_LIGHT_TEXT(REACH_TEXT_QUATERNARY),
             .inverse = REACH_DARK_TEXT(REACH_TEXT_PRIMARY)},
    .surface = {.base = REACH_LIGHT_SURFACE_BASE(1.0f),
                .raised = REACH_LIGHT_SURFACE_BASE(1.0f),
                .card = REACH_LIGHT_SURFACE_BASE(1.0f),
                .item = REACH_LIGHT_SURFACE_RAISED(1.0f),
                .well = REACH_LIGHT_SURFACE_ELEVATED(1.0f)},
    .fill = {.primary = REACH_FILL(REACH_LIGHT_FILL_PRIMARY),
             .secondary = REACH_FILL(REACH_LIGHT_FILL_SECONDARY),
             .tertiary = REACH_FILL(REACH_LIGHT_FILL_TERTIARY)},
    .status = {.success = REACH_GREEN_INK(1.0f), .error = REACH_RED_INK(1.0f)},
    .accent = {.fill = REACH_BLUE_BRIGHT(1.0f),
               .text = REACH_BLUE_DEEP(1.0f),
               .tint = REACH_BLUE_BRIGHT(REACH_ACCENT_TINT),
               .selection = REACH_BLUE_BRIGHT(REACH_ACCENT_SELECTION),
               .faint = REACH_BLUE_BRIGHT(REACH_ACCENT_FAINT),
               .text_tint = REACH_BLUE_DEEP(REACH_ACCENT_TINT),
               .text_muted = REACH_BLUE_DEEP(REACH_ACCENT_MUTED),
               .tint_amount = REACH_ACCENT_TINT},
    .border = REACH_LIGHT_BORDER(1.0f),
    .divider = REACH_LIGHT_DIVIDER(1.0f),
    .on_color = REACH_PURE_WHITE(1.0f),
    .danger = REACH_RED_DEEP(1.0f),
    .pressed_overlay = REACH_PURE_BLACK(0.12f),
    .artwork_scrim = REACH_LIGHT_SURFACE_RAISED(0.35f),
    .shadow = REACH_SHADOW_AMBIENT(5.0f),

    .top_bar_scrim = REACH_PURE_WHITE(0.65f),
    .top_bar_battery_fill = REACH_GREEN_BRIGHT(1.0f),
    .top_bar_battery_low_fill = REACH_RED_BRIGHT(1.0f),
    .top_bar_battery_saver_fill = REACH_YELLOW_BRIGHT(1.0f),
    .top_bar_battery_charging_fill = REACH_BLUE_BRIGHT(1.0f),
    .top_bar_cpu_text = REACH_CYAN_INK(1.0f),
    .top_bar_memory_text = REACH_LAVENDER_INK(1.0f),
    .top_bar_download_text = REACH_GREEN_INK(1.0f),
    .top_bar_upload_text = REACH_ORANGE_INK(1.0f),
    .dock_fallback_icon_background = REACH_LIGHT_SURFACE_RAISED(0.80f),
    .bar_fallback_icon_text = REACH_LIGHT_TEXT(REACH_TEXT_PRIMARY),
    .widgets_artwork_overlay = REACH_LIGHT_SURFACE_BASE(0.48f),
    // Opaque accent blends exist only once resolved; unresolved tables keep the plain surface.
    .widgets_accent_background = REACH_LIGHT_SURFACE_BASE(1.0f),
    .stage_background = REACH_LIGHT_SURFACE_BASE(REACH_THEME_STAGE_BACKDROP_ALPHA),
    .stage_tile_header_background = REACH_LIGHT_SURFACE_ELEVATED(1.0f),
    .stage_close_glyph = REACH_PURE_WHITE(0.80f),
    .screenshot_scrim = REACH_PURE_BLACK(0.45f),
    .screenshot_label_background = REACH_LIGHT_SURFACE_BASE(0.92f),
    .settings_background = REACH_LIGHT_SURFACE_RAISED(1.0f),
    .settings_window_minimize_hover_background = REACH_YELLOW_DEEP(1.0f),
    .settings_avatar_border = REACH_LIGHT_SURFACE_BASE(1.0f),

    REACH_THEME_METRICS,
    REACH_THEME_MOTION,
};

static float reach_theme_min(float a, float b)
{
    return a < b ? a : b;
}

const reach_theme *reach_theme_default(void)
{
    return &reach_theme_dark;
}

const reach_theme *reach_theme_for_mode(reach_theme_mode mode)
{
    return mode == REACH_THEME_MODE_LIGHT ? &reach_theme_light : &reach_theme_dark;
}

static const reach_accent_pair reach_accent_presets[REACH_ACCENT_PRESET_COUNT] = {
    [REACH_ACCENT_PRESET_BLUE] = {REACH_BLUE_DEEP(1.0f), REACH_BLUE_BRIGHT(1.0f)},
    [REACH_ACCENT_PRESET_CYAN] = {REACH_CYAN_DEEP(1.0f), REACH_CYAN_BRIGHT(1.0f)},
    [REACH_ACCENT_PRESET_TEAL] = {REACH_TEAL_DEEP(1.0f), REACH_TEAL_BRIGHT(1.0f)},
    [REACH_ACCENT_PRESET_GREEN] = {REACH_GREEN_DEEP(1.0f), REACH_GREEN_BRIGHT(1.0f)},
    [REACH_ACCENT_PRESET_PURPLE] = {REACH_PURPLE_DEEP(1.0f), REACH_PURPLE_BRIGHT(1.0f)},
    [REACH_ACCENT_PRESET_LAVENDER] = {REACH_LAVENDER_DEEP(1.0f), REACH_LAVENDER_BRIGHT(1.0f)},
    [REACH_ACCENT_PRESET_ORANGE] = {REACH_ORANGE_DEEP(1.0f), REACH_ORANGE_BRIGHT(1.0f)},
    [REACH_ACCENT_PRESET_RED] = {REACH_RED_DEEP(1.0f), REACH_RED_BRIGHT(1.0f)},
};

reach_accent_preset reach_accent_preset_clamp(int32_t preset)
{
    return preset >= 0 && preset < REACH_ACCENT_PRESET_COUNT ? (reach_accent_preset)preset
                                                             : REACH_ACCENT_PRESET_BLUE;
}

reach_accent_pair reach_accent_preset_pair(reach_accent_preset preset)
{
    return reach_accent_presets[reach_accent_preset_clamp((int32_t)preset)];
}

static float reach_theme_linear_channel(float channel)
{
    return channel <= 0.04045f ? channel / 12.92f : powf((channel + 0.055f) / 1.055f, 2.4f);
}

static float reach_theme_luminance(reach_color color)
{
    return 0.2126f * reach_theme_linear_channel(color.r) +
           0.7152f * reach_theme_linear_channel(color.g) +
           0.0722f * reach_theme_linear_channel(color.b);
}

static float reach_theme_contrast(reach_color a, reach_color b)
{
    float first = reach_theme_luminance(a) + 0.05f;
    float second = reach_theme_luminance(b) + 0.05f;
    return first > second ? first / second : second / first;
}

// Translucent references are judged as drawn over the candidate, like text on an accent fill.
static int32_t reach_theme_contrast_met(reach_color color, reach_color reference)
{
    return reach_theme_contrast(color, reach_theme_color_mix(color, reference, reference.a)) >=
           REACH_THEME_ACCENT_CONTRAST;
}

static reach_color reach_theme_meet_contrast(reach_color color, reach_color reference,
                                             reach_color toward)
{
    color.a = 1.0f;
    if (reach_theme_contrast_met(color, reference))
    {
        return color;
    }
    float low = 0.0f;
    float high = 1.0f;
    for (int32_t step = 0; step < 24; ++step)
    {
        float middle = (low + high) * 0.5f;
        if (reach_theme_contrast_met(reach_theme_color_mix(color, toward, middle), reference))
        {
            high = middle;
        }
        else
        {
            low = middle;
        }
    }
    return reach_theme_color_mix(color, toward, high);
}

static int32_t reach_theme_glass_surface_readable(const reach_theme *theme, float level)
{
    const reach_color surface = {level, level, level, 1.0f};
    const reach_color primary =
        reach_theme_color_mix(surface, theme->text.primary, theme->text.primary.a);
    const reach_color secondary =
        reach_theme_color_mix(surface, theme->text.secondary, theme->text.secondary.a);
    return reach_theme_contrast(primary, surface) >= REACH_THEME_GLASS_PRIMARY_CONTRAST &&
           reach_theme_contrast(secondary, surface) >= REACH_THEME_GLASS_SECONDARY_CONTRAST;
}

// The furthest gray level from the theme's own surfaces that keeps primary and secondary text
// readable. Dark themes bound how light glass gets, light themes how dark.
static float reach_theme_glass_limit(const reach_theme *theme, int32_t light)
{
    float readable = light ? 1.0f : 0.0f;
    float unreadable = light ? 0.0f : 1.0f;
    if (reach_theme_glass_surface_readable(theme, unreadable))
        return unreadable;
    for (int32_t step = 0; step < 24; ++step)
    {
        float middle = (readable + unreadable) * 0.5f;
        if (reach_theme_glass_surface_readable(theme, middle))
            readable = middle;
        else
            unreadable = middle;
    }
    return readable;
}

static int32_t reach_theme_color_close(reach_color a, reach_color b)
{
    const float tolerance = 0.5f / 255.0f;
    return fabsf(a.r - b.r) <= tolerance && fabsf(a.g - b.g) <= tolerance &&
           fabsf(a.b - b.b) <= tolerance && fabsf(a.a - b.a) <= tolerance;
}

void reach_theme_resolve(reach_theme_mode mode, const reach_accent_pair *accent,
                         reach_theme *out_theme)
{
    if (out_theme == 0)
    {
        return;
    }
    *out_theme = *reach_theme_for_mode(mode);
    reach_accent_pair pair =
        accent != 0 ? *accent : reach_accent_preset_pair(REACH_ACCENT_PRESET_BLUE);
    const reach_color black = REACH_PURE_BLACK(1.0f);
    const reach_color white = REACH_PURE_WHITE(1.0f);
    const int32_t light = mode == REACH_THEME_MODE_LIGHT;
    const reach_color surface = light ? (reach_color)REACH_LIGHT_SURFACE_ELEVATED(1.0f)
                                      : (reach_color)REACH_DARK_SURFACE_ELEVATED(1.0f);
    reach_color fill = reach_theme_meet_contrast(light ? pair.bright : pair.deep,
                                                 out_theme->text.primary, light ? white : black);
    reach_color text =
        reach_theme_meet_contrast(light ? pair.deep : pair.bright, surface, light ? black : white);
    reach_theme_accent *resolved = &out_theme->accent;
    resolved->fill = fill;
    resolved->text = text;
    resolved->tint = reach_theme_color_alpha(fill, REACH_ACCENT_TINT);
    resolved->selection = reach_theme_color_alpha(fill, REACH_ACCENT_SELECTION);
    resolved->faint = reach_theme_color_alpha(fill, REACH_ACCENT_FAINT);
    resolved->text_tint = reach_theme_color_alpha(text, REACH_ACCENT_TINT);
    resolved->text_muted = reach_theme_color_alpha(text, REACH_ACCENT_MUTED);
    out_theme->widgets_accent_background =
        reach_theme_color_mix(out_theme->surface.raised, fill, REACH_ACCENT_FAINT);
    out_theme->settings_avatar_border =
        reach_theme_color_mix(out_theme->surface.card, text, REACH_ACCENT_MUTED);
    // The static tables hold the blue preset already resolved, so an unresolved theme matches.
    const reach_accent_pair blue = reach_accent_preset_pair(REACH_ACCENT_PRESET_BLUE);
    const reach_theme_accent *base = &reach_theme_for_mode(mode)->accent;
    REACH_ASSERT(!reach_theme_color_close(pair.deep, blue.deep) ||
                 !reach_theme_color_close(pair.bright, blue.bright) ||
                 (reach_theme_color_close(resolved->fill, base->fill) &&
                  reach_theme_color_close(resolved->text, base->text) &&
                  reach_theme_color_close(resolved->tint, base->tint) &&
                  reach_theme_color_close(resolved->selection, base->selection) &&
                  reach_theme_color_close(resolved->faint, base->faint) &&
                  reach_theme_color_close(resolved->text_tint, base->text_tint) &&
                  reach_theme_color_close(resolved->text_muted, base->text_muted)));
    // Readable text alone would let glass drift into the other mode's brightness; the band
    // keeps it on its own side, whichever bound is stricter.
    float limit = reach_theme_glass_limit(out_theme, light);
    out_theme->glass.floor = light ? fmaxf(limit, REACH_LIGHT_GLASS_FLOOR) : 0.0f;
    out_theme->glass.ceiling = light ? 1.0f : fminf(limit, REACH_DARK_GLASS_CEILING);
}

reach_color reach_theme_color_alpha(reach_color color, float alpha)
{
    color.a = alpha;
    return color;
}

reach_color reach_theme_color_mix(reach_color from, reach_color to, float t)
{
    reach_color color;
    color.r = from.r + (to.r - from.r) * t;
    color.g = from.g + (to.g - from.g) * t;
    color.b = from.b + (to.b - from.b) * t;
    color.a = from.a + (to.a - from.a) * t;
    return color;
}

float reach_theme_border_thickness(const reach_theme *theme, float dpi_scale)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    return reach_line_thickness(actual->border_thickness * (dpi_scale > 0.0f ? dpi_scale : 1.0f));
}

reach_rect_f32 reach_theme_border_content_rect(const reach_theme *theme, float dpi_scale,
                                               reach_rect_f32 bounds)
{
    float inset = reach_theme_border_thickness(theme, dpi_scale);
    inset = inset > 0.0f ? inset : 0.0f;
    float width = bounds.width > 0.0f ? bounds.width : 0.0f;
    float height = bounds.height > 0.0f ? bounds.height : 0.0f;
    float horizontal = inset < width * 0.5f ? inset : width * 0.5f;
    float vertical = inset < height * 0.5f ? inset : height * 0.5f;
    bounds.x += horizontal;
    bounds.y += vertical;
    bounds.width = width - horizontal * 2.0f;
    bounds.height = height - vertical * 2.0f;
    return bounds;
}

static float reach_shadow_ceil(float value)
{
    float truncated = (float)(int)value;
    return value > truncated ? truncated + 1.0f : truncated;
}

static float reach_shadow_edge(float extent, float offset)
{
    float edge = extent - offset;
    return edge > 0.0f ? reach_shadow_ceil(edge) : 0.0f;
}

float reach_theme_shadow_extent(const reach_shadow *shadow, float dpi_scale)
{
    float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    float sigma = (shadow != 0 ? shadow->blur : 0.0f) * scale * 0.5f;
    return sigma * 3.0f;
}

float reach_theme_shadow_visible_extent(const reach_shadow *shadow, float dpi_scale)
{
    float extent = reach_theme_shadow_extent(shadow, dpi_scale);
    if (shadow == 0 || extent <= 0.0f || shadow->color.a * 0.5f <= 1.0f / 255.0f)
    {
        return 0.0f;
    }
    float spread = extent / 3.0f * 1.41421356f;
    float visible = 0.0f;
    float hidden = extent;
    for (int32_t step = 0; step < 24; ++step)
    {
        float distance = (visible + hidden) * 0.5f;
        if (shadow->color.a * 0.5f * erfcf(distance / spread) > 1.0f / 255.0f)
        {
            visible = distance;
        }
        else
        {
            hidden = distance;
        }
    }
    return hidden;
}

reach_shadow_pad reach_theme_shadow_pad(const reach_shadow *shadow, float dpi_scale)
{
    reach_shadow_pad pad = {0.0f, 0.0f, 0.0f, 0.0f};
    if (shadow == 0 || shadow->color.a <= 0.0f)
    {
        return pad;
    }

    float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    float extent = reach_theme_shadow_extent(shadow, dpi_scale);
    float offset_x = shadow->offset_x * scale;
    float offset_y = shadow->offset_y * scale;

    pad.left = reach_shadow_edge(extent, offset_x);
    pad.right = reach_shadow_edge(extent, -offset_x);
    pad.top = reach_shadow_edge(extent, offset_y);
    pad.bottom = reach_shadow_edge(extent, -offset_y);
    return pad;
}

float reach_theme_dock_corner_radius(const reach_theme *theme, float dock_height)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    return dock_height * actual->dock_corner_radius_ratio;
}

float reach_theme_icon_box_size(const reach_theme *theme, float dock_height)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    return dock_height * actual->icon_box_height_ratio;
}

float reach_theme_icon_size(const reach_theme *theme, float icon_box_size)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    return icon_box_size * actual->icon_max_box_ratio;
}

float reach_theme_icon_box_corner_radius(const reach_theme *theme, float icon_box_size)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    return reach_theme_min(icon_box_size * actual->icon_box_corner_radius_ratio,
                           actual->icon_box_corner_radius_max);
}

float reach_theme_tray_slot_size(const reach_theme *theme, float dock_height)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    return dock_height * actual->tray_slot_size_ratio;
}

reach_motion_spec reach_theme_motion_preset(const reach_theme *theme, reach_motion_preset preset)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    if (preset < 0 || preset >= REACH_MOTION_PRESET_COUNT)
    {
        preset = REACH_MOTION_PRESET_SMOOTH;
    }
    return actual->motion_presets[preset];
}

float reach_theme_stage_motion_scale(const reach_theme *theme, float stage_seconds)
{
    const reach_theme *actual = theme != 0 ? theme : reach_theme_default();
    float reference = actual->stage_motion_reference_seconds;
    float scale = stage_seconds / reference;
    return reference > 0.0f && stage_seconds > 0.0f && scale < 1e30f ? scale : 1.0f;
}
