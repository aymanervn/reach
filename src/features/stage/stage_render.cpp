#include "reach/features/stage.h"

#include "stage_common.h"

#include "reach/core/typography.h"
#include "reach/features/common/ui_controls.h"

#include <math.h>

static reach_color reach_stage_rgba(float r, float g, float b, float a)
{
    reach_color color = {};
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = a;
    return color;
}

static float reach_stage_scaled(const reach_stage_render_context *ctx, float value)
{
    float scale = ctx != nullptr && ctx->dpi_scale > 0.0f ? ctx->dpi_scale : 1.0f;
    return value * scale;
}

static reach_result reach_stage_push_tile_background(const reach_stage_render_context *ctx,
                                                     reach_render_command_buffer *commands,
                                                     reach_rect_f32 item, float alpha)
{
    if (ctx == nullptr || commands == nullptr)
    {
        return REACH_OK;
    }

    reach_color background = ctx->theme->stage_background;
    background.a = 1.0f;
    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_RECT;
    shape.rect = reach_render_pixel_aligned_rect(item);
    shape.color = reach_theme_color_alpha(background, alpha);
    return reach_render_command_buffer_push(commands, &shape);
}

static reach_result reach_stage_push_tile_placeholder(const reach_stage_render_context *ctx,
                                                      const reach_stage_tile *tile,
                                                      reach_rect_f32 rect, float alpha,
                                                      reach_render_command_buffer *out_commands)
{
    reach_render_command plate = {};
    plate.type = REACH_RENDER_COMMAND_RECT;
    plate.rect = rect;
    plate.color = reach_theme_color_alpha(ctx->theme->surface.item, alpha);
    reach_result result = reach_render_command_buffer_push(out_commands, &plate);
    if (result != REACH_OK || tile->icon_id == 0)
    {
        return result;
    }

    float icon_size = reach_stage_scaled(ctx, 48.0f);
    if (icon_size > rect.width * 0.5f)
    {
        icon_size = rect.width * 0.5f;
    }
    if (icon_size > rect.height * 0.5f)
    {
        icon_size = rect.height * 0.5f;
    }

    reach_render_command icon = {};
    icon.type = REACH_RENDER_COMMAND_ICON;
    icon.icon_id = tile->icon_id;
    icon.rect.x = rect.x + (rect.width - icon_size) * 0.5f;
    icon.rect.y = rect.y + (rect.height - icon_size) * 0.5f;
    icon.rect.width = icon_size;
    icon.rect.height = icon_size;
    icon.color = reach_stage_rgba(1.0f, 1.0f, 1.0f, alpha);
    return reach_render_command_buffer_push(out_commands, &icon);
}

static reach_result reach_stage_push_close_button(reach_stage *stage,
                                                  const reach_stage_render_context *ctx,
                                                  size_t index, float alpha,
                                                  reach_render_command_buffer *out_commands)
{
    const reach_stage_state *state = &stage->state;
    const reach_stage_tile *tile = &state->tiles[index];
    if (tile->desktop || tile->departing)
    {
        return REACH_OK;
    }

    reach_rect_f32 button = reach_stage_tile_close_button_rect(stage, index);
    if (button.width <= 0.0f || button.height <= 0.0f)
    {
        return REACH_OK;
    }

    button.x -= ctx->bounds.x;
    button.y -= ctx->bounds.y;

    const reach_motion *hover =
        state->close_hover_index == index &&
                (stage->close_hover.to > 0.0f || reach_motion_active(&stage->close_hover))
            ? &stage->close_hover
            : nullptr;
    const reach_theme *theme = ctx->theme;
    reach_render_command backing = {};
    backing.type = REACH_RENDER_COMMAND_RECT;
    backing.rect = button;
    backing.radius = button.width * 0.5f;
    reach_color background = theme->text.secondary;
    reach_color hover_background = theme->danger;
    background.a *= alpha;
    hover_background.a *= alpha;
    int32_t retained = reach_ui_color_feedback_begin(out_commands, tile->window, button, background,
                                                     hover_background, hover);
    backing.color = retained ? reach_color{1, 1, 1, 1} : background;
    reach_result result = reach_render_command_buffer_push(out_commands, &backing);
    if (retained)
        reach_render_command_buffer_end_group(out_commands);
    if (result != REACH_OK)
    {
        return result;
    }

    float inset = button.width * 0.24f;
    reach_render_command glyph = {};
    glyph.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    glyph.icon_id = REACH_VECTOR_ICON_CLOSE;
    glyph.rect.x = button.x + inset;
    glyph.rect.y = button.y + inset;
    glyph.rect.width = button.width - inset * 2.0f;
    glyph.rect.height = button.height - inset * 2.0f;
    reach_color foreground = theme->stage_close_glyph;
    reach_color hover_foreground = theme->on_color;
    foreground.a *= alpha;
    hover_foreground.a *= alpha;
    retained =
        reach_ui_color_feedback_begin(out_commands, tile->window | UINT64_C(0x4000000000000000),
                                      button, foreground, hover_foreground, hover);
    glyph.color = retained ? reach_color{1, 1, 1, 1} : foreground;
    result = reach_render_command_buffer_push(out_commands, &glyph);
    if (retained)
        reach_render_command_buffer_end_group(out_commands);
    return result;
}

static reach_result reach_stage_push_tile_header(reach_stage *stage,
                                                 const reach_stage_render_context *ctx,
                                                 size_t index, float alpha,
                                                 reach_render_command_buffer *out_commands)
{
    const reach_stage_tile *tile = &stage->state.tiles[index];
    if (tile->desktop)
    {
        return REACH_OK;
    }
    reach_rect_f32 header = tile->current_bar;
    if (header.width <= 0.0f || header.height <= 0.0f)
    {
        return REACH_OK;
    }

    header.x -= ctx->bounds.x;
    header.y -= ctx->bounds.y;

    reach_render_command background = {};
    background.type = REACH_RENDER_COMMAND_RECT;
    background.rect = header;
    background.color = ctx->theme->stage_tile_header_background;
    background.color.a *= alpha;
    reach_result result = reach_render_command_buffer_push(out_commands, &background);
    if (result != REACH_OK)
    {
        return result;
    }

    reach_rect_f32 button = reach_stage_tile_close_button_rect(stage, index);
    float padding = reach_stage_scaled(ctx, 8.0f);
    float trailing = button.width > 0.0f ? button.width + padding : padding;
    float label_width = header.width - padding - trailing;
    if (tile->label[0] != 0 && label_width > 0.0f)
    {
        reach_render_command label = {};
        label.type = REACH_RENDER_COMMAND_TEXT;
        label.rect.x = header.x + padding;
        label.rect.y = header.y;
        label.rect.width = label_width;
        label.rect.height = header.height;
        label.text_size = reach_stage_scaled(ctx, REACH_TEXT_SIZE_MEDIUM);
        label.text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
        label.text_alignment = REACH_TEXT_ALIGNMENT_LEADING;
        label.text_ellipsis = 1;
        label.color = ctx->theme->text.primary;
        label.color.a *= alpha;
        label.text_color = label.color;
        (void)reach_copy_utf16(label.text, 260, tile->label);
        result = reach_render_command_buffer_push(out_commands, &label);
        if (result != REACH_OK)
        {
            return result;
        }
    }

    return reach_stage_push_close_button(stage, ctx, index, alpha, out_commands);
}

enum
{
    REACH_STAGE_GROUP_BACKDROP = 1,
    REACH_STAGE_GROUP_BACKGROUND,
    REACH_STAGE_GROUP_HEADER,
    REACH_STAGE_GROUP_LABEL,
    REACH_STAGE_GROUP_CLOSE,
    REACH_STAGE_GROUP_CLOSE_BACKING,
    REACH_STAGE_GROUP_CLOSE_GLYPH,
    REACH_STAGE_GROUP_PLACEHOLDER,
    REACH_STAGE_GROUP_PLACEHOLDER_ICON,
    REACH_STAGE_GROUP_THUMBNAIL,
    REACH_STAGE_GROUP_HOVER_PILL
};

// Worst case one tile adds on the compositor path: a closing tile draws background, header, label,
// placeholder plate, placeholder icon and thumbnail. The hovered tile's close button adds two tint
// groups, and the hover pill two groups and one command; the desktop tile's backdrop and thumbnail
// fit in its share.
#define REACH_STAGE_COMPOSITOR_TILE_GROUPS 6
#define REACH_STAGE_COMPOSITOR_TILE_COMMANDS 6
static_assert(REACH_STAGE_MAX_TILES * REACH_STAGE_COMPOSITOR_TILE_GROUPS + 4 <=
                  REACH_MAX_RENDER_GROUPS,
              "every tracked window fits the render group budget on the compositor path");
static_assert(REACH_STAGE_MAX_TILES * REACH_STAGE_COMPOSITOR_TILE_COMMANDS + 1 <=
                  REACH_MAX_RENDER_COMMANDS,
              "every tracked window fits the render command budget on the compositor path");

static uint64_t reach_stage_group_id(uint32_t role, uintptr_t window)
{
    return ((uint64_t)role << 52) | ((uint64_t)window & UINT64_C(0xfffffffffffff));
}

// Solid fills are rasterized tiny and stretched by the compositor: a solid colour scales exactly
// and keeps sharp edges, while a surface the size of every animated tile would exhaust GPU memory
// with many windows.
static const reach_rect_f32 reach_stage_fill_base = {0.0f, 0.0f, 8.0f, 8.0f};

static float reach_stage_motion_extent(const reach_motion *motion, double now)
{
    float rest = reach_motion_value_at(motion, reach_motion_deadline(motion));
    return fmaxf(1.0f, ceilf(fmaxf(rest, reach_motion_value_at(motion, now))));
}

static int32_t reach_stage_motion_visible(const reach_motion *motion)
{
    float minimum = 0.0f, maximum = 0.0f;
    return reach_motion_range(motion, &minimum, &maximum) != REACH_OK || maximum > 0.0f;
}

static reach_rect_motion reach_stage_local_rect(const reach_rect_motion *rect,
                                                reach_rect_f32 bounds)
{
    reach_rect_motion local = *rect;
    local.x = reach_motion_affine(&rect->x, 1.0f, -bounds.x);
    local.y = reach_motion_affine(&rect->y, 1.0f, -bounds.y);
    return local;
}

// A shape drawn once at a fixed size with its bounds at the origin, so the group scale maps the
// base size onto the animated rect.
static reach_render_group reach_stage_scaled_group(uint64_t id, const reach_rect_motion *rect,
                                                   reach_rect_f32 base, const reach_motion *opacity)
{
    reach_render_group group = {};
    group.id = id;
    group.bounds = base;
    group.x = rect->x;
    group.y = rect->y;
    group.scale = reach_motion_affine(&rect->width, 1.0f / base.width, 0.0f);
    group.scale_y = reach_motion_affine(&rect->height, 1.0f / base.height, 0.0f);
    group.opacity = *opacity;
    return group;
}

static reach_result reach_stage_push_grouped(reach_render_command_buffer *commands,
                                             const reach_render_group *group,
                                             const reach_render_command *command)
{
    reach_result result = reach_render_command_buffer_begin_group(commands, group);
    if (result != REACH_OK)
        return result;
    result = reach_render_command_buffer_push(commands, command);
    reach_render_command_buffer_end_group(commands);
    return result;
}

static reach_result reach_stage_push_compositor_close(reach_stage *stage,
                                                      const reach_stage_render_context *ctx,
                                                      size_t index, const reach_rect_motion *rect,
                                                      const reach_stage_tile_motion *motion,
                                                      float bar_rest,
                                                      reach_render_command_buffer *commands)
{
    // reach_stage_tile_close_button_rect: half the bar, inset a quarter of it from the header's
    // top and trailing edges.
    const reach_stage_state *state = &stage->state;
    const reach_stage_tile *tile = &state->tiles[index];
    const reach_theme *theme = ctx->theme;
    float button = bar_rest * 0.5f;
    reach_render_group position = {};
    position.id = reach_stage_group_id(REACH_STAGE_GROUP_CLOSE, tile->window);
    position.bounds = {0.0f, 0.0f, button, button};
    reach_motion trailing = {};
    if (reach_motion_combine(&rect->x, 1.0f, &rect->width, 1.0f, 0.0f, &trailing) != REACH_OK ||
        reach_motion_combine(&trailing, 1.0f, &motion->bar, -0.75f, 0.0f, &position.x) !=
            REACH_OK ||
        reach_motion_combine(&rect->y, 1.0f, &motion->bar, -0.75f, 0.0f, &position.y) != REACH_OK)
        return REACH_ERROR;
    position.scale = reach_motion_affine(&motion->bar, 1.0f / bar_rest, 0.0f);
    position.scale_y = position.scale;
    position.opacity = motion->alpha;

    float inset = button * 0.24f;
    reach_render_command backing = {};
    backing.type = REACH_RENDER_COMMAND_RECT;
    backing.rect = position.bounds;
    backing.radius = button * 0.5f;
    backing.color = theme->text.secondary;
    reach_render_command glyph = {};
    glyph.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    glyph.icon_id = REACH_VECTOR_ICON_CLOSE;
    glyph.rect = {inset, inset, button - inset * 2.0f, button - inset * 2.0f};
    glyph.color = theme->stage_close_glyph;

    // Only the button whose hover colour is animating needs tinted groups; every other button
    // draws its resting colours straight into the position group.
    int32_t hovered = state->close_hover_index == index &&
                      (stage->close_hover.to > 0.0f || reach_motion_active(&stage->close_hover));
    reach_result result = reach_render_command_buffer_begin_group(commands, &position);
    if (result != REACH_OK)
        return result;
    if (!hovered)
    {
        result = reach_render_command_buffer_push(commands, &backing);
        if (result == REACH_OK)
            result = reach_render_command_buffer_push(commands, &glyph);
        reach_render_command_buffer_end_group(commands);
        return result;
    }
    reach_render_command_buffer_end_group(commands);

    reach_render_group backing_group = {};
    backing_group.id = reach_stage_group_id(REACH_STAGE_GROUP_CLOSE_BACKING, tile->window);
    backing_group.parent_id = position.id;
    backing_group.bounds = position.bounds;
    reach_ui_color_feedback_apply(&backing_group, theme->text.secondary, theme->danger,
                                  &stage->close_hover);
    backing.color = reach_stage_rgba(1.0f, 1.0f, 1.0f, 1.0f);
    result = reach_stage_push_grouped(commands, &backing_group, &backing);
    if (result != REACH_OK)
        return result;

    reach_render_group glyph_group = backing_group;
    glyph_group.id = reach_stage_group_id(REACH_STAGE_GROUP_CLOSE_GLYPH, tile->window);
    reach_ui_color_feedback_apply(&glyph_group, theme->stage_close_glyph, theme->on_color,
                                  &stage->close_hover);
    glyph.color = reach_stage_rgba(1.0f, 1.0f, 1.0f, 1.0f);
    return reach_stage_push_grouped(commands, &glyph_group, &glyph);
}

static reach_result reach_stage_push_compositor_placeholder(const reach_stage_render_context *ctx,
                                                            const reach_stage_tile *tile,
                                                            const reach_rect_motion *rect,
                                                            const reach_stage_tile_motion *motion,
                                                            double now,
                                                            reach_render_command_buffer *commands)
{
    reach_rect_f32 base = reach_stage_fill_base;
    reach_render_command plate = {};
    plate.type = REACH_RENDER_COMMAND_RECT;
    plate.rect = base;
    plate.color = ctx->theme->surface.item;
    reach_render_group group =
        reach_stage_scaled_group(reach_stage_group_id(REACH_STAGE_GROUP_PLACEHOLDER, tile->window),
                                 rect, base, &motion->alpha);
    reach_result result = reach_stage_push_grouped(commands, &group, &plate);
    if (result != REACH_OK || tile->icon_id == 0)
        return result;

    reach_rect_f32 sampled = reach_rect_motion_value_at(rect, now);
    float icon_size =
        fminf(reach_stage_scaled(ctx, 48.0f), fminf(sampled.width, sampled.height) * 0.5f);
    if (icon_size <= 0.0f)
        return REACH_OK;
    reach_render_group icon_group = {};
    icon_group.id = reach_stage_group_id(REACH_STAGE_GROUP_PLACEHOLDER_ICON, tile->window);
    icon_group.bounds = {0.0f, 0.0f, icon_size, icon_size};
    if (reach_motion_combine(&rect->x, 1.0f, &rect->width, 0.5f, -icon_size * 0.5f,
                             &icon_group.x) != REACH_OK ||
        reach_motion_combine(&rect->y, 1.0f, &rect->height, 0.5f, -icon_size * 0.5f,
                             &icon_group.y) != REACH_OK)
        return REACH_ERROR;
    icon_group.opacity = motion->alpha;
    reach_render_command icon = {};
    icon.type = REACH_RENDER_COMMAND_ICON;
    icon.icon_id = tile->icon_id;
    icon.rect = icon_group.bounds;
    icon.color = reach_stage_rgba(1.0f, 1.0f, 1.0f, 1.0f);
    return reach_stage_push_grouped(commands, &icon_group, &icon);
}

static reach_result
reach_stage_push_compositor_chrome(reach_stage *stage, const reach_stage_render_context *ctx,
                                   size_t index, const reach_stage_tile_motion *motion, double now,
                                   reach_render_command_buffer *commands)
{
    const reach_stage_state *state = &stage->state;
    const reach_stage_tile *tile = &state->tiles[index];
    const reach_theme *theme = ctx->theme;
    reach_rect_motion rect = reach_stage_local_rect(&motion->rect, ctx->bounds);
    reach_rect_motion item = rect;
    if (reach_motion_combine(&rect.y, 1.0f, &motion->bar, -1.0f, 0.0f, &item.y) != REACH_OK ||
        reach_motion_combine(&rect.height, 1.0f, &motion->bar, 1.0f, 0.0f, &item.height) !=
            REACH_OK)
        return REACH_ERROR;

    reach_color background = theme->stage_background;
    background.a = 1.0f;
    reach_rect_f32 base = {0.0f, 0.0f, reach_stage_motion_extent(&item.width, now),
                           reach_stage_motion_extent(&item.height, now)};
    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_RECT;
    shape.rect = base;
    shape.color = background;
    shape.rect = reach_stage_fill_base;
    reach_render_group background_group =
        reach_stage_scaled_group(reach_stage_group_id(REACH_STAGE_GROUP_BACKGROUND, tile->window),
                                 &item, reach_stage_fill_base, &motion->alpha);
    reach_result result = reach_stage_push_grouped(commands, &background_group, &shape);
    if (result != REACH_OK)
        return result;

    float bar_rest = reach_stage_motion_extent(&motion->bar, now);
    if (reach_stage_motion_visible(&motion->bar))
    {
        reach_rect_motion header = {rect.x, item.y, rect.width, motion->bar};
        reach_rect_f32 header_base = reach_stage_fill_base;
        shape = {};
        shape.type = REACH_RENDER_COMMAND_RECT;
        shape.rect = header_base;
        shape.color = theme->stage_tile_header_background;
        reach_render_group group =
            reach_stage_scaled_group(reach_stage_group_id(REACH_STAGE_GROUP_HEADER, tile->window),
                                     &header, header_base, &motion->alpha);
        result = reach_stage_push_grouped(commands, &group, &shape);
        if (result != REACH_OK)
            return result;

        // The label keeps its text size like the CPU path, centred on the growing header.
        float width_rest = reach_motion_value_at(&rect.width, reach_motion_deadline(&rect.width));
        float padding = reach_stage_scaled(ctx, 8.0f);
        float button = tile->departing ? 0.0f : bar_rest * 0.5f;
        float label_width = width_rest - padding - (button > 0.0f ? button + padding : padding);
        if (tile->label[0] != 0 && label_width > 0.0f)
        {
            reach_render_group label_group = {};
            label_group.id = reach_stage_group_id(REACH_STAGE_GROUP_LABEL, tile->window);
            label_group.bounds = {0.0f, 0.0f, label_width, bar_rest};
            label_group.x = reach_motion_affine(&rect.x, 1.0f, padding);
            if (reach_motion_combine(&rect.y, 1.0f, &motion->bar, -0.5f, -0.5f * bar_rest,
                                     &label_group.y) != REACH_OK)
                return REACH_ERROR;
            label_group.opacity = motion->alpha;
            reach_render_command label = {};
            label.type = REACH_RENDER_COMMAND_TEXT;
            label.rect = label_group.bounds;
            label.text_size = reach_stage_scaled(ctx, REACH_TEXT_SIZE_MEDIUM);
            label.text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
            label.text_alignment = REACH_TEXT_ALIGNMENT_LEADING;
            label.text_ellipsis = 1;
            label.color = theme->text.primary;
            label.text_color = label.color;
            (void)reach_copy_utf16(label.text, 260, tile->label);
            result = reach_stage_push_grouped(commands, &label_group, &label);
            if (result != REACH_OK)
                return result;
        }
        if (button > 0.0f)
        {
            result = reach_stage_push_compositor_close(stage, ctx, index, &rect, motion, bar_rest,
                                                       commands);
            if (result != REACH_OK)
                return result;
        }
    }

    if (!tile->departing)
        return REACH_OK;
    return reach_stage_push_compositor_placeholder(ctx, tile, &rect, motion, now, commands);
}

static reach_result reach_stage_push_compositor_thumbnail(const reach_stage_render_context *ctx,
                                                          const reach_stage_tile *tile,
                                                          const reach_stage_tile_motion *motion,
                                                          reach_render_command_buffer *commands)
{
    if (!reach_stage_motion_visible(&motion->presence) || tile->source_rect.width <= 0.0f ||
        tile->source_rect.height <= 0.0f)
        return REACH_OK;
    reach_rect_motion rect = reach_stage_local_rect(&motion->rect, ctx->bounds);
    reach_rect_f32 base = {0.0f, 0.0f, ceilf(tile->source_rect.width),
                           ceilf(tile->source_rect.height)};
    reach_motion opacity = reach_motion_clamp(&motion->presence, 0.0f, 1.0f);
    reach_render_group group = reach_stage_scaled_group(
        reach_stage_group_id(REACH_STAGE_GROUP_THUMBNAIL, tile->window), &rect, base, &opacity);
    reach_render_command thumbnail = {};
    thumbnail.type = REACH_RENDER_COMMAND_WINDOW_THUMBNAIL;
    thumbnail.rect = base;
    thumbnail.window_id = tile->window;
    if (tile->desktop)
        thumbnail.source_screen = tile->source_rect;
    return reach_stage_push_grouped(commands, &group, &thumbnail);
}

int32_t reach_stage_compositor_active(const reach_stage *stage)
{
    return stage != nullptr && stage->compositor_motion;
}

// Retargets the hover pill at the hovered tile's resting rect, in surface coordinates. The pill
// shows only while Stage rests open, so it never trails tiles that open or close. Returns 0 when
// it has nothing to draw.
static int32_t reach_stage_update_hover_pill(reach_stage *stage,
                                             const reach_stage_render_context *ctx, double now)
{
    const reach_stage_state *state = &stage->state;
    if (state->closing)
    {
        reach_motion_set(&stage->hover_pill_opacity, 0.0f);
        return 0;
    }

    int32_t resting = !reach_stage_track_active(stage, REACH_STAGE_ANIMATION_PROGRESS) &&
                      !reach_stage_track_active(stage, REACH_STAGE_ANIMATION_DESKTOP_PROGRESS);
    const reach_stage_tile *tile =
        resting && state->has_hover && state->hover_index < state->tile_count
            ? &state->tiles[state->hover_index]
            : nullptr;
    if (tile != nullptr && (tile->desktop || tile->departing))
        tile = nullptr;

    if (tile != nullptr)
    {
        // The pill surrounds the tile's header and thumbnail with an even ring: the margin the
        // layout keeps around every tile plus the shared pill inset.
        float inset = (REACH_STAGE_TILE_MARGIN + REACH_SELECTION_PILL_INSET) * ctx->dpi_scale;
        reach_rect_f32 target = {tile->target_rect.x - ctx->bounds.x - inset,
                                 tile->target_rect.y - tile->bar_height - ctx->bounds.y - inset,
                                 tile->target_rect.width + inset * 2.0f,
                                 tile->target_rect.height + tile->bar_height + inset * 2.0f};
        // The pointer moves continuously, so a newly hovered tile is always a neighbour: step to
        // it while the pill shows, and appear in place when it was hidden.
        int32_t shown = stage->hover_pill_opacity.to > 0.0f ||
                        reach_motion_value_at(&stage->hover_pill_opacity, now) > 0.0f;
        reach_selection_pill_retarget(&stage->hover_pill, ctx->theme, target, state->hover_index,
                                      shown ? REACH_SELECTION_PILL_STEP : REACH_SELECTION_PILL_SET,
                                      now);
        if (stage->hover_pill_opacity.to != 1.0f)
            reach_motion_animate_spec_to_at(&stage->hover_pill_opacity, 1.0f,
                                            reach_stage_motion_preset(REACH_MOTION_PRESET_FADE_IN),
                                            now);
    }
    else if (stage->hover_pill_opacity.to != 0.0f)
    {
        reach_motion_animate_spec_to_at(&stage->hover_pill_opacity, 0.0f,
                                        reach_stage_motion_preset(REACH_MOTION_PRESET_FADE_OUT),
                                        now);
    }
    return stage->hover_pill_opacity.to > 0.0f ||
           reach_motion_value_at(&stage->hover_pill_opacity, now) > 0.0f;
}

static float reach_stage_hover_pill_radius(const reach_stage *stage,
                                           const reach_stage_render_context *ctx)
{
    float width = 0.0f, height = 0.0f, unused = 0.0f;
    (void)reach_motion_range(&stage->hover_pill.rect.width, &width, &unused);
    (void)reach_motion_range(&stage->hover_pill.rect.height, &height, &unused);
    return fminf(ctx->theme->radius_small * ctx->dpi_scale,
                 fmaxf(0.0f, fminf(width, height) * 0.5f));
}

static reach_result reach_stage_push_compositor_hover_pill(reach_stage *stage,
                                                           const reach_stage_render_context *ctx,
                                                           double now,
                                                           reach_render_command_buffer *commands)
{
    if (!reach_stage_update_hover_pill(stage, ctx, now))
        return REACH_OK;
    float width = 0.0f, height = 0.0f, unused = 0.0f;
    if (reach_motion_range(&stage->hover_pill.rect.width, &unused, &width) != REACH_OK ||
        reach_motion_range(&stage->hover_pill.rect.height, &unused, &height) != REACH_OK)
        return REACH_OK;
    reach_render_group pill = {};
    pill.id = reach_stage_group_id(REACH_STAGE_GROUP_HOVER_PILL, 0);
    pill.bounds = {0.0f, 0.0f, fmaxf(1.0f, ceilf(width)), fmaxf(1.0f, ceilf(height))};
    pill.opacity = reach_motion_clamp(&stage->hover_pill_opacity, 0.0f, 1.0f);
    return reach_render_push_rect_motion(commands, &pill, &stage->hover_pill.rect,
                                         reach_stage_hover_pill_radius(stage, ctx),
                                         ctx->theme->accent.fill);
}

// Without compositor motion the pill is drawn at its current sample, and Stage keeps frames
// running while it moves (reach_stage_hover_pill_moving).
static reach_result reach_stage_push_sampled_hover_pill(reach_stage *stage,
                                                        const reach_stage_render_context *ctx,
                                                        reach_render_command_buffer *commands)
{
    double now = reach_motion_now_seconds();
    if (!reach_stage_update_hover_pill(stage, ctx, now))
        return REACH_OK;
    float opacity =
        fminf(1.0f, fmaxf(0.0f, reach_motion_value_at(&stage->hover_pill_opacity, now)));
    reach_render_command pill = {};
    pill.type = REACH_RENDER_COMMAND_RECT;
    pill.rect = reach_rect_motion_value_at(&stage->hover_pill.rect, now);
    pill.radius = reach_stage_hover_pill_radius(stage, ctx);
    pill.color =
        reach_theme_color_alpha(ctx->theme->accent.fill, ctx->theme->accent.fill.a * opacity);
    return opacity > 0.0f ? reach_render_command_buffer_push(commands, &pill) : REACH_OK;
}

int32_t reach_stage_hover_pill_moving(const reach_stage *stage)
{
    const reach_rect_motion *rect = &stage->hover_pill.rect;
    return reach_motion_active(&stage->hover_pill_opacity) || reach_motion_active(&rect->x) ||
           reach_motion_active(&rect->y) || reach_motion_active(&rect->width) ||
           reach_motion_active(&rect->height);
}

// Every tile at this moment's sample, as constant motions: the CPU state presented as it is.
static void reach_stage_sampled_motions(const reach_stage *stage, reach_stage_tile_motion *motions)
{
    const reach_stage_state *state = &stage->state;
    for (size_t index = 0; index < state->tile_count; ++index)
    {
        const reach_stage_tile *tile = &state->tiles[index];
        reach_stage_tile_motion *motion = &motions[index];
        *motion = {};
        reach_motion_set(&motion->rect.x, tile->current_rect.x);
        reach_motion_set(&motion->rect.y, tile->current_rect.y);
        reach_motion_set(&motion->rect.width, tile->current_rect.width);
        reach_motion_set(&motion->rect.height, tile->current_rect.height);
        reach_motion_set(&motion->bar, tile->current_bar.height);
        reach_motion_set(&motion->alpha, state->progress * tile->presence);
        reach_motion_set(&motion->presence, tile->presence);
    }
}

static reach_result reach_stage_push_compositor_scene(reach_stage *stage,
                                                      const reach_stage_render_context *ctx,
                                                      const reach_stage_tile_motion *motions,
                                                      double now,
                                                      reach_render_command_buffer *commands)
{
    const reach_stage_state *state = &stage->state;
    reach_result result = REACH_OK;
    for (size_t index = 0; index < state->tile_count && result == REACH_OK; ++index)
    {
        const reach_stage_tile *tile = &state->tiles[index];
        if (!tile->desktop)
            continue;
        // The desktop tile sits on the Stage backdrop, below every tile's chrome.
        reach_render_command backdrop = {};
        backdrop.type = REACH_RENDER_COMMAND_RECT;
        backdrop.rect = reach_stage_fill_base;
        backdrop.color = ctx->theme->stage_background;
        backdrop.color.a = state->backdrop_opacity;
        reach_render_group group = {};
        group.id = reach_stage_group_id(REACH_STAGE_GROUP_BACKDROP, tile->window);
        group.bounds = backdrop.rect;
        reach_motion_set(&group.scale, ctx->bounds.width / reach_stage_fill_base.width);
        reach_motion_set(&group.scale_y, ctx->bounds.height / reach_stage_fill_base.height);
        result = reach_stage_push_grouped(commands, &group, &backdrop);
        if (result == REACH_OK)
            result = reach_stage_push_compositor_thumbnail(ctx, tile, &motions[index], commands);
    }
    // The pill sits behind every tile's chrome, so only its inset ring shows around the tile.
    if (result == REACH_OK)
        result = reach_stage_push_compositor_hover_pill(stage, ctx, now, commands);
    for (size_t pass = 0; pass < 4 && result == REACH_OK; ++pass)
    {
        // Passes 0-1 draw tile chrome and 2-3 the thumbnails above all of it, each with the
        // preferred window last, matching thumbnails stacked above the surface content.
        for (size_t at = 0; at < state->tile_count && result == REACH_OK; ++at)
        {
            // Tiles run front to back; thumbnails stack the front window last, on top, as the
            // native overlays did by registering in reverse.
            size_t index = pass >= 2 ? state->tile_count - 1 - at : at;
            const reach_stage_tile *tile = &state->tiles[index];
            int32_t preferred =
                ctx->preferred_front_window != 0 && tile->window == ctx->preferred_front_window;
            if (tile->desktop || preferred != (int32_t)(pass & 1))
                continue;
            if (pass >= 2)
                result =
                    reach_stage_push_compositor_thumbnail(ctx, tile, &motions[index], commands);
            else if (reach_stage_motion_visible(&motions[index].alpha))
                result = reach_stage_push_compositor_chrome(stage, ctx, index, &motions[index], now,
                                                            commands);
        }
    }
    return result;
}

static reach_result reach_stage_append_compositor_commands(reach_stage *stage,
                                                           const reach_stage_render_context *ctx,
                                                           reach_render_command_buffer *commands)
{
    reach_stage_state *state = &stage->state;
    stage->compositor_sampled = 0;
    stage->compositor_event_seconds = 0.0;
    if (!state->open || state->close_failed || state->tile_count == 0)
        return REACH_OK;

    double now = reach_motion_now_seconds();
    reach_stage_sample_at(stage, now);
    static reach_stage_tile_motion motions[REACH_STAGE_MAX_TILES];
    int32_t sampled = 0;
    for (size_t index = 0; index < state->tile_count && !sampled; ++index)
        sampled = !reach_stage_tile_motion_at(stage, index, now, &motions[index]);
    if (!sampled)
    {
        if (reach_stage_push_compositor_scene(stage, ctx, motions, now, commands) == REACH_OK)
        {
            double event = reach_stage_motion_event_seconds(stage, motions, now);
            // Never schedule into the past: a prediction the tick does not confirm retries at a
            // bounded rate instead of spinning.
            stage->compositor_event_seconds = event > 0.0 ? fmax(event, now + 1.0 / 120.0) : 0.0;
            return REACH_OK;
        }
        reach_render_command_buffer_clear(commands);
    }
    // A tile needs two moving tracks at once, or its motion could not be drawn: present this
    // sample and redraw every frame until the tracks settle.
    reach_stage_sampled_motions(stage, motions);
    stage->compositor_sampled = 1;
    return reach_stage_push_compositor_scene(stage, ctx, motions, now, commands);
}

reach_result reach_stage_append_render_commands(reach_stage *stage,
                                                const reach_stage_render_context *ctx,
                                                reach_render_command_buffer *out_commands)
{
    REACH_ASSERT(out_commands != nullptr);
    if (stage == nullptr || ctx == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_render_command_buffer_clear(out_commands);
    if (reach_stage_compositor_active(stage))
        return reach_stage_append_compositor_commands(stage, ctx, out_commands);

    const reach_stage_state *state = &stage->state;
    if (!state->open || state->close_failed || state->tile_count == 0)
    {
        return REACH_OK;
    }

    float eased = state->progress;

    reach_result pill_result = reach_stage_push_sampled_hover_pill(stage, ctx, out_commands);
    if (pill_result != REACH_OK)
    {
        return pill_result;
    }
    for (size_t pass = 0; pass < 2; ++pass)
    {
        for (size_t index = 0; index < state->tile_count; ++index)
        {
            const reach_stage_tile *tile = &state->tiles[index];
            int32_t preferred =
                ctx->preferred_front_window != 0 && tile->window == ctx->preferred_front_window;
            if (preferred != (pass == 1))
            {
                continue;
            }

            reach_rect_f32 rect = tile->current_rect;
            if (rect.width <= 0.0f || rect.height <= 0.0f)
            {
                continue;
            }

            float alpha = eased * tile->presence;
            if (alpha <= 0.0f)
            {
                continue;
            }

            if (tile->desktop)
            {
                continue;
            }

            rect.x -= ctx->bounds.x;
            rect.y -= ctx->bounds.y;

            reach_rect_f32 item = rect;
            item.y -= tile->current_bar.height;
            item.height += tile->current_bar.height;
            reach_result result = reach_stage_push_tile_background(ctx, out_commands, item, alpha);
            if (result != REACH_OK)
            {
                return result;
            }

            result = reach_stage_push_tile_header(stage, ctx, index, alpha, out_commands);
            if (result != REACH_OK)
            {
                return result;
            }

            if (tile->departing && !tile->desktop)
            {
                result = reach_stage_push_tile_placeholder(ctx, tile, rect, alpha, out_commands);
                if (result != REACH_OK)
                {
                    return result;
                }
            }
        }
    }

    return REACH_OK;
}
