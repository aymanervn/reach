#include "reach/features/stage.h"

#include "stage_common.h"

#include "reach/core/typography.h"
#include "reach/features/common/ui_controls.h"

#include <math.h>

#define REACH_STAGE_TILE_GROUP_TAG UINT64_C(0x1000000000000000)
#define REACH_STAGE_MOVING_GROUP_COUNT 20
#define REACH_STAGE_RASTER_GROUP_COUNT 4
#define REACH_STAGE_SNAP_MARGIN 1.0f

enum
{
    REACH_STAGE_GROUP_TILE = 0,
    REACH_STAGE_GROUP_CLIP = 1,
    REACH_STAGE_GROUP_OUTER = 2,
    REACH_STAGE_GROUP_INNER = 4,
    REACH_STAGE_GROUP_STRIP = 6,
    REACH_STAGE_GROUP_HEADER_CLIP = 8,
    REACH_STAGE_GROUP_HEADER = 10,
    REACH_STAGE_GROUP_LABEL = 12,
    REACH_STAGE_GROUP_LABEL_VIEWPORT = 13,
    REACH_STAGE_GROUP_CLOSE = 14,
    REACH_STAGE_GROUP_GLYPH = 16,
    REACH_STAGE_GROUP_PLATE = 18,
    REACH_STAGE_GROUP_PLATE_ICON = 20,
    REACH_STAGE_GROUP_CLOSE_COMMANDS = 22,
    REACH_STAGE_GROUP_PLACEHOLDER_COMMANDS = 24
};

static uint64_t reach_stage_group_base(uintptr_t window)
{
    return REACH_STAGE_TILE_GROUP_TAG | ((uint64_t)window << 6);
}

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

static float reach_stage_header_radius(const reach_stage_render_context *ctx, reach_rect_f32 header)
{
    float radius = reach_stage_scaled(ctx, ctx->theme->radius_small);
    float limit = header.height * 0.5f;
    return radius < limit ? radius : limit;
}

static void reach_stage_begin_group(reach_render_command_buffer *commands, uint64_t id)
{
    reach_render_group group = {};
    group.id = id;
    (void)reach_render_command_buffer_begin_group(commands, &group);
}

static void reach_stage_end_group(reach_render_command_buffer *commands)
{
    reach_render_command_buffer_end_group(commands);
}

static reach_color reach_stage_tile_border(const reach_stage_render_context *ctx, float hover,
                                           float alpha);

static reach_rect_f32 reach_stage_tile_outer_rect(reach_rect_f32 rect, float bar, float border)
{
    reach_rect_f32 item = rect;
    item.y -= bar;
    item.height += bar;
    return {item.x - border, item.y - border, item.width + border * 2.0f,
            item.height + border * 2.0f};
}

static reach_result reach_stage_push_tile_background(const reach_stage_render_context *ctx,
                                                     reach_render_command_buffer *commands,
                                                     reach_rect_f32 rect, float bar,
                                                     float header_radius, float width, float hover,
                                                     float alpha)
{
    if (ctx == nullptr || commands == nullptr || width <= 0.0f)
    {
        return REACH_OK;
    }

    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_RECT;
    shape.rect = reach_stage_tile_outer_rect(rect, bar, width);
    shape.radius = header_radius + width;
    shape.corner_mask = REACH_RENDER_CORNER_TOP_LEFT | REACH_RENDER_CORNER_TOP_RIGHT;

    reach_color background = ctx->theme->stage_backdrop;
    background.a = 1.0f;
    reach_color border = reach_stage_tile_border(ctx, hover, alpha);
    return reach_render_push_bordered_background(commands, &shape, background, border, width,
                                                 nullptr, ctx->dpi_scale);
}

static reach_color reach_stage_tile_border(const reach_stage_render_context *ctx, float hover,
                                           float alpha)
{
    reach_color background = ctx->theme->stage_backdrop;
    background.a = 1.0f;
    reach_color border = ctx->theme->bar_border;
    border.a = 1.0f;
    reach_color highlight = ctx->theme->stage_tile_highlight;
    float highlight_opacity = highlight.a * hover;
    highlight.a = 1.0f;
    border = reach_theme_color_mix(border, highlight, highlight_opacity);
    return reach_theme_color_mix(background, border, alpha);
}

static reach_result reach_stage_push_tile_outline(const reach_stage_render_context *ctx,
                                                  reach_render_command_buffer *commands,
                                                  reach_rect_f32 rect, float width, float alpha)
{
    reach_rect_f32 strips[4] = {
        {rect.x - width, rect.y - width, rect.width + width * 2.0f, width},
        {rect.x - width, rect.y + rect.height, rect.width + width * 2.0f, width},
        {rect.x - width, rect.y, width, rect.height},
        {rect.x + rect.width, rect.y, width, rect.height},
    };
    for (size_t index = 0; index < 4; ++index)
    {
        reach_render_command strip = {};
        strip.type = REACH_RENDER_COMMAND_RECT;
        strip.rect = strips[index];
        strip.color = reach_stage_tile_border(ctx, 0.0f, alpha);
        reach_result result = reach_render_command_buffer_push(commands, &strip);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    return REACH_OK;
}

static reach_result reach_stage_push_tile_placeholder(const reach_stage_render_context *ctx,
                                                      const reach_stage_tile *tile,
                                                      reach_rect_f32 rect, float alpha,
                                                      reach_render_command_buffer *out_commands)
{
    reach_render_command plate = {};
    plate.type = REACH_RENDER_COMMAND_RECT;
    plate.rect = rect;
    plate.color = reach_theme_color_alpha(ctx->theme->stage_tile_placeholder, alpha);
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

static const reach_motion *reach_stage_close_hover(const reach_stage *stage, size_t index)
{
    const reach_stage_state *state = &stage->state;
    return state->close_hover_index == index &&
                   (stage->close_hover.to > 0.0f || reach_motion_active(&stage->close_hover))
               ? &stage->close_hover
               : nullptr;
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

    const reach_motion *hover = reach_stage_close_hover(stage, index);
    if (hover == nullptr)
    {
        reach_stage_begin_group(out_commands, reach_stage_group_base(tile->window) +
                                                  REACH_STAGE_GROUP_CLOSE_COMMANDS);
    }
    const reach_theme *theme = ctx->theme;
    reach_render_command backing = {};
    backing.type = REACH_RENDER_COMMAND_RECT;
    backing.rect = button;
    backing.radius = button.width * 0.5f;
    reach_color background = theme->stage_close_background;
    reach_color hover_background = theme->stage_close_hover_background;
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
        reach_stage_end_group(out_commands);
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
    reach_color hover_foreground = theme->stage_close_hover_glyph;
    foreground.a *= alpha;
    hover_foreground.a *= alpha;
    retained =
        reach_ui_color_feedback_begin(out_commands, tile->window | UINT64_C(0x4000000000000000),
                                      button, foreground, hover_foreground, hover);
    glyph.color = retained ? reach_color{1, 1, 1, 1} : foreground;
    result = reach_render_command_buffer_push(out_commands, &glyph);
    if (retained)
        reach_render_command_buffer_end_group(out_commands);
    reach_stage_end_group(out_commands);
    return result;
}

static reach_render_command reach_stage_label_command(const reach_stage_render_context *ctx,
                                                      const reach_stage_tile *tile,
                                                      reach_rect_f32 rect)
{
    reach_render_command label = {};
    label.type = REACH_RENDER_COMMAND_TEXT;
    label.rect = rect;
    label.text_size = reach_stage_scaled(ctx, REACH_TEXT_SIZE_MEDIUM);
    label.text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
    label.text_alignment = REACH_TEXT_ALIGNMENT_LEADING;
    label.text_ellipsis = 1;
    label.color = ctx->theme->stage_tile_label;
    label.text_color = label.color;
    (void)reach_copy_utf16(label.text, 260, tile->label);
    return label;
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
    background.radius = reach_stage_header_radius(ctx, header);
    background.corner_mask = REACH_RENDER_CORNER_TOP_LEFT | REACH_RENDER_CORNER_TOP_RIGHT;
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
        reach_render_command label = reach_stage_label_command(
            ctx, tile, {header.x + padding, header.y, label_width, header.height});
        label.color.a *= alpha;
        label.text_color = label.color;
        result = reach_render_command_buffer_push(out_commands, &label);
    }
    return result;
}

static reach_result reach_stage_push_raster_tile(reach_stage *stage,
                                                 const reach_stage_render_context *ctx,
                                                 size_t index, float border,
                                                 reach_render_command_buffer *out_commands)
{
    const reach_stage_state *state = &stage->state;
    const reach_stage_tile *tile = &state->tiles[index];
    uint64_t base = reach_stage_group_base(tile->window);

    reach_rect_f32 rect = tile->current_rect;
    float alpha = state->progress * tile->presence;
    rect.x -= ctx->bounds.x;
    rect.y -= ctx->bounds.y;

    if (tile->desktop)
    {
        reach_stage_begin_group(out_commands, base + REACH_STAGE_GROUP_TILE);
        reach_result result = reach_stage_push_tile_outline(ctx, out_commands, rect, border, alpha);
        reach_stage_end_group(out_commands);
        return result;
    }

    float hover = state->has_hover && state->hover_index == index ? 1.0f : 0.0f;
    reach_stage_begin_group(out_commands, base + REACH_STAGE_GROUP_TILE);
    reach_result result = reach_stage_push_tile_background(
        ctx, out_commands, rect, tile->current_bar.height,
        reach_stage_header_radius(ctx, tile->current_bar), border, hover, alpha);
    if (result == REACH_OK)
    {
        result = reach_stage_push_tile_header(stage, ctx, index, alpha, out_commands);
    }
    reach_stage_end_group(out_commands);
    if (result != REACH_OK)
    {
        return result;
    }

    result = reach_stage_push_close_button(stage, ctx, index, alpha, out_commands);
    if (result != REACH_OK)
    {
        return result;
    }

    if (tile->minimized || tile->departing)
    {
        reach_stage_begin_group(out_commands, base + REACH_STAGE_GROUP_PLACEHOLDER_COMMANDS);
        result = reach_stage_push_tile_placeholder(ctx, tile, rect, alpha, out_commands);
        reach_stage_end_group(out_commands);
    }
    return result;
}

static int32_t reach_stage_motion_varies(const reach_motion &motion)
{
    float minimum = 0.0f;
    float maximum = 0.0f;
    return reach_motion_range(&motion, &minimum, &maximum) != REACH_OK || minimum != maximum;
}

static int32_t reach_stage_tile_animated(const reach_stage_tile *tile)
{
    return reach_stage_motion_varies(tile->rect_motion.x) ||
           reach_stage_motion_varies(tile->rect_motion.y) ||
           reach_stage_motion_varies(tile->rect_motion.width) ||
           reach_stage_motion_varies(tile->rect_motion.height) ||
           reach_stage_motion_varies(tile->bar_motion) ||
           reach_stage_motion_varies(tile->fade_motion);
}

static float reach_stage_motion_maximum(const reach_motion &motion)
{
    float minimum = 0.0f;
    float maximum = 0.0f;
    return reach_motion_range(&motion, &minimum, &maximum) == REACH_OK ? maximum : 0.0f;
}

static reach_motion reach_stage_constant_motion(float value)
{
    reach_motion motion = {};
    reach_motion_set(&motion, value);
    return motion;
}

struct reach_stage_fill_tint
{
    reach_color from;
    reach_color to;
    const reach_motion *progress;
};

static reach_result reach_stage_push_fill(reach_render_command_buffer *commands, uint64_t id,
                                          uint64_t parent, const reach_rect_motion &rect,
                                          float radius, reach_color color,
                                          const reach_motion *opacity,
                                          const reach_stage_fill_tint *tint,
                                          const reach_motion *radius_motion = nullptr)
{
    reach_render_group source = {};
    source.id = id;
    source.parent_id = parent;
    source.bounds = {0.0f, 0.0f, fmaxf(1.0f, ceilf(reach_stage_motion_maximum(rect.width))),
                     fmaxf(1.0f, ceilf(reach_stage_motion_maximum(rect.height)))};
    if (opacity != nullptr)
    {
        source.opacity = *opacity;
    }
    if (radius_motion != nullptr)
    {
        source.clip_radius_motion = *radius_motion;
        radius = reach_stage_motion_maximum(*radius_motion);
    }
    if (tint != nullptr)
    {
        reach_ui_color_feedback_apply(&source, tint->from, tint->to, tint->progress);
        color = reach_stage_rgba(1.0f, 1.0f, 1.0f, 1.0f);
    }
    return reach_render_push_rect_motion(commands, &source, &rect, radius, color);
}

static reach_rect_motion reach_stage_local_rect(const reach_motion &x, const reach_motion &y,
                                                const reach_motion &width,
                                                const reach_motion &height)
{
    return {x, y, width, height};
}

static reach_result reach_stage_push_group(reach_render_command_buffer *commands,
                                           const reach_render_group &group)
{
    reach_result result = reach_render_command_buffer_begin_group(commands, &group);
    reach_render_command_buffer_end_group(commands);
    return result;
}

static reach_result reach_stage_push_moving_close(const reach_stage *stage,
                                                  const reach_stage_render_context *ctx,
                                                  size_t index, uint64_t base, uint64_t parent,
                                                  float border, const reach_motion &width,
                                                  const reach_motion &bar, const reach_motion &fade,
                                                  reach_render_command_buffer *commands)
{
    const reach_theme *theme = ctx->theme;
    float size_maximum = reach_stage_motion_maximum(bar) * 0.5f;
    if (size_maximum <= 0.0f)
    {
        return REACH_OK;
    }

    const reach_motion *hover = reach_stage_close_hover(stage, index);
    reach_motion size = reach_motion_affine(&bar, 0.5f, 0.0f);
    reach_motion x = {};
    if (reach_motion_combine(&width, 1.0f, &bar, -0.75f, border, &x) != REACH_OK)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_motion y = reach_motion_affine(&bar, 0.25f, border);

    reach_stage_fill_tint tint = {theme->stage_close_background,
                                  theme->stage_close_hover_background, hover};
    reach_result result = reach_stage_push_fill(commands, base + REACH_STAGE_GROUP_CLOSE, parent,
                                                reach_stage_local_rect(x, y, size, size),
                                                size_maximum * 0.5f, theme->stage_close_background,
                                                &fade, hover != nullptr ? &tint : nullptr);
    if (result != REACH_OK)
    {
        return result;
    }

    float glyph_maximum = size_maximum * 0.52f;
    reach_render_group glyph = {};
    glyph.id = base + REACH_STAGE_GROUP_GLYPH;
    glyph.parent_id = parent;
    glyph.bounds = {0.0f, 0.0f, glyph_maximum, glyph_maximum};
    glyph.opacity = fade;
    glyph.scale = reach_motion_affine(&bar, 0.26f / glyph_maximum, 0.0f);
    if (reach_motion_combine(&width, 1.0f, &bar, -0.63f, border, &glyph.x) != REACH_OK)
    {
        return REACH_INVALID_ARGUMENT;
    }
    glyph.y = reach_motion_affine(&bar, 0.37f, border);
    reach_color foreground = theme->stage_close_glyph;
    if (hover != nullptr)
    {
        reach_ui_color_feedback_apply(&glyph, foreground, theme->stage_close_hover_glyph, hover);
        foreground = reach_stage_rgba(1.0f, 1.0f, 1.0f, 1.0f);
    }
    result = reach_render_command_buffer_begin_group(commands, &glyph);
    reach_render_command icon = {};
    icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    icon.icon_id = REACH_VECTOR_ICON_CLOSE;
    icon.rect = glyph.bounds;
    icon.color = foreground;
    if (result == REACH_OK)
    {
        result = reach_render_command_buffer_push(commands, &icon);
    }
    reach_render_command_buffer_end_group(commands);
    return result;
}

static reach_result reach_stage_push_moving_label(const reach_stage_render_context *ctx,
                                                  const reach_stage_tile *tile, uint64_t base,
                                                  uint64_t parent, float border,
                                                  const reach_motion &width,
                                                  const reach_motion &bar, const reach_motion &fade,
                                                  reach_render_command_buffer *commands)
{
    float padding = reach_stage_scaled(ctx, 8.0f);
    reach_motion label_width = {};
    if (reach_motion_combine(&width, 1.0f, &bar, -0.5f, -padding * 2.0f, &label_width) != REACH_OK)
    {
        return REACH_INVALID_ARGUMENT;
    }
    float maximum = reach_stage_motion_maximum(label_width);
    float bar_maximum = reach_stage_motion_maximum(bar);
    if (maximum <= 0.0f || bar_maximum <= 0.0f)
    {
        return REACH_OK;
    }
    label_width = reach_motion_clamp(&label_width, 0.0f, maximum);

    reach_render_group position = {};
    position.id = base + REACH_STAGE_GROUP_LABEL;
    position.parent_id = parent;
    position.bounds = {0.0f, 0.0f, maximum, bar_maximum};
    reach_motion_set(&position.x, border + padding);
    position.y = reach_motion_affine(&bar, 0.5f, border - bar_maximum * 0.5f);
    position.opacity = fade;
    reach_result result = reach_stage_push_group(commands, position);
    if (result != REACH_OK)
    {
        return result;
    }

    reach_render_group viewport = {};
    viewport.id = base + REACH_STAGE_GROUP_LABEL_VIEWPORT;
    viewport.parent_id = position.id;
    viewport.bounds = viewport.clip = position.bounds;
    viewport.has_clip = 1;
    viewport.clip_insets[REACH_RENDER_CLIP_RIGHT] =
        reach_motion_affine(&label_width, -1.0f, maximum);
    reach_render_command label = reach_stage_label_command(ctx, tile, position.bounds);
    return reach_render_push_width_text(commands, &label, &viewport);
}

static reach_result reach_stage_push_moving_placeholder(const reach_stage_render_context *ctx,
                                                        const reach_stage_tile *tile, uint64_t base,
                                                        uint64_t parent, float border,
                                                        const reach_motion &bar,
                                                        const reach_motion &fade,
                                                        reach_render_command_buffer *commands)
{
    const reach_rect_motion &rect = tile->rect_motion;
    reach_motion plate_y = reach_motion_affine(&bar, 1.0f, border);
    reach_motion plate_x = reach_stage_constant_motion(border);
    reach_result result =
        reach_stage_push_fill(commands, base + REACH_STAGE_GROUP_PLATE, parent,
                              reach_stage_local_rect(plate_x, plate_y, rect.width, rect.height),
                              0.0f, ctx->theme->stage_tile_placeholder, &fade, nullptr);
    if (result != REACH_OK || tile->icon_id == 0)
    {
        return result;
    }

    float cap = reach_stage_scaled(ctx, 48.0f);
    float maximum = fminf(cap, fminf(reach_stage_motion_maximum(rect.width) * 0.5f,
                                     reach_stage_motion_maximum(rect.height) * 0.5f));
    if (maximum <= 0.0f)
    {
        return REACH_OK;
    }
    reach_motion half_width = reach_motion_affine(&rect.width, 0.5f, 0.0f);
    reach_motion half_height = reach_motion_affine(&rect.height, 0.5f, 0.0f);
    reach_motion size = {};
    reach_motion row = {};
    reach_render_group icon_group = {};
    icon_group.id = base + REACH_STAGE_GROUP_PLATE_ICON;
    icon_group.parent_id = parent;
    icon_group.bounds = {0.0f, 0.0f, maximum, maximum};
    icon_group.opacity = fade;
    if (reach_motion_min(&half_width, &half_height, &size) != REACH_OK)
    {
        return REACH_INVALID_ARGUMENT;
    }
    size = reach_motion_clamp(&size, 0.0f, cap);
    icon_group.scale = reach_motion_affine(&size, 1.0f / maximum, 0.0f);
    if (reach_motion_combine(&rect.width, 0.5f, &size, -0.5f, border, &icon_group.x) != REACH_OK ||
        reach_motion_combine(&bar, 1.0f, &rect.height, 0.5f, border, &row) != REACH_OK ||
        reach_motion_combine(&row, 1.0f, &size, -0.5f, 0.0f, &icon_group.y) != REACH_OK)
    {
        return REACH_INVALID_ARGUMENT;
    }
    result = reach_render_command_buffer_begin_group(commands, &icon_group);
    reach_render_command icon = {};
    icon.type = REACH_RENDER_COMMAND_ICON;
    icon.icon_id = tile->icon_id;
    icon.rect = icon_group.bounds;
    icon.color = reach_stage_rgba(1.0f, 1.0f, 1.0f, 1.0f);
    if (result == REACH_OK)
    {
        result = reach_render_command_buffer_push(commands, &icon);
    }
    reach_render_command_buffer_end_group(commands);
    return result;
}

static reach_rect_f32 reach_stage_snapped_outer_at(const reach_stage_render_context *ctx,
                                                   const reach_stage_tile *tile, float border,
                                                   double now)
{
    reach_rect_f32 rect = reach_rect_motion_value_at(&tile->rect_motion, now);
    float bar = reach_motion_value_at(&tile->bar_motion, now);
    rect.x -= ctx->bounds.x;
    rect.y -= ctx->bounds.y;
    return reach_render_pixel_aligned_rect(reach_stage_tile_outer_rect(rect, bar, border));
}

static reach_motion reach_stage_snapped_edge(const reach_motion &edge, float snapped_first,
                                             float snapped_last)
{
    float minimum = 0.0f;
    float maximum = 0.0f;
    float first = reach_motion_value_at(&edge, -HUGE_VAL);
    float last = reach_motion_value_at(&edge, HUGE_VAL);
    float scale = first != last ? (snapped_last - snapped_first) / (last - first) : 1.0f;
    if (reach_motion_range(&edge, &minimum, &maximum) != REACH_OK || minimum == maximum ||
        scale == 0.0f)
    {
        return reach_stage_constant_motion(snapped_last);
    }
    return reach_motion_affine(&edge, scale, snapped_last - scale * last);
}

static reach_result reach_stage_snapped_frame(const reach_stage_render_context *ctx,
                                              const reach_stage_tile *tile, float border,
                                              const reach_motion &origin_x,
                                              const reach_motion &origin_y,
                                              reach_rect_motion *out_frame)
{
    const reach_rect_motion &rect = tile->rect_motion;
    reach_motion left = reach_motion_affine(&rect.x, 1.0f, -ctx->bounds.x - border);
    reach_motion right = {};
    reach_motion top = {};
    reach_motion bottom = {};
    if (reach_motion_combine(&rect.x, 1.0f, &rect.width, 1.0f, border - ctx->bounds.x, &right) !=
            REACH_OK ||
        reach_motion_combine(&rect.y, 1.0f, &tile->bar_motion, -1.0f, -ctx->bounds.y - border,
                             &top) != REACH_OK ||
        reach_motion_combine(&rect.y, 1.0f, &rect.height, 1.0f, border - ctx->bounds.y, &bottom) !=
            REACH_OK)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_rect_f32 first = reach_stage_snapped_outer_at(ctx, tile, border, -HUGE_VAL);
    reach_rect_f32 last = reach_stage_snapped_outer_at(ctx, tile, border, HUGE_VAL);
    left = reach_stage_snapped_edge(left, first.x, last.x);
    right = reach_stage_snapped_edge(right, first.x + first.width, last.x + last.width);
    top = reach_stage_snapped_edge(top, first.y, last.y);
    bottom = reach_stage_snapped_edge(bottom, first.y + first.height, last.y + last.height);
    if (reach_motion_combine(&left, 1.0f, &origin_x, -1.0f, 0.0f, &out_frame->x) != REACH_OK ||
        reach_motion_combine(&top, 1.0f, &origin_y, -1.0f, 0.0f, &out_frame->y) != REACH_OK ||
        reach_motion_combine(&right, 1.0f, &left, -1.0f, 0.0f, &out_frame->width) != REACH_OK ||
        reach_motion_combine(&bottom, 1.0f, &top, -1.0f, 0.0f, &out_frame->height) != REACH_OK)
    {
        return REACH_INVALID_ARGUMENT;
    }
    return REACH_OK;
}

static reach_result reach_stage_push_moving_tile(reach_stage *stage,
                                                 const reach_stage_render_context *ctx,
                                                 size_t index, float border,
                                                 reach_render_command_buffer *commands)
{
    const reach_stage_state *state = &stage->state;
    const reach_stage_tile *tile = &state->tiles[index];
    const reach_theme *theme = ctx->theme;
    const reach_rect_motion &rect = tile->rect_motion;
    const reach_motion &bar = tile->bar_motion;
    const reach_motion &fade = tile->fade_motion;
    uint64_t base = reach_stage_group_base(tile->window);
    float hover = !tile->desktop && state->has_hover && state->hover_index == index ? 1.0f : 0.0f;
    float margin = tile->desktop ? 0.0f : REACH_STAGE_SNAP_MARGIN;

    reach_motion outer_width = reach_motion_affine(&rect.width, 1.0f, border * 2.0f);
    reach_motion outer_height = {};
    if (reach_motion_combine(&rect.height, 1.0f, &bar, tile->desktop ? 0.0f : 1.0f, border * 2.0f,
                             &outer_height) != REACH_OK)
    {
        return REACH_INVALID_ARGUMENT;
    }
    float tile_width = fmaxf(1.0f, ceilf(reach_stage_motion_maximum(outer_width))) + margin * 2.0f;
    float tile_height =
        fmaxf(1.0f, ceilf(reach_stage_motion_maximum(outer_height))) + margin * 2.0f;

    reach_render_group tile_group = {};
    tile_group.id = base + REACH_STAGE_GROUP_TILE;
    tile_group.bounds = {-margin, -margin, tile_width, tile_height};
    tile_group.x = reach_motion_affine(&rect.x, 1.0f, -border - ctx->bounds.x);
    if (tile->desktop)
    {
        tile_group.y = reach_motion_affine(&rect.y, 1.0f, -border - ctx->bounds.y);
    }
    else if (reach_motion_combine(&rect.y, 1.0f, &bar, -1.0f, -border - ctx->bounds.y,
                                  &tile_group.y) != REACH_OK)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_result result = reach_stage_push_group(commands, tile_group);
    if (result != REACH_OK)
    {
        return result;
    }

    reach_rect_motion frame = {reach_stage_constant_motion(0.0f), reach_stage_constant_motion(0.0f),
                               outer_width, outer_height};
    if (!tile->desktop && reach_stage_snapped_frame(ctx, tile, border, tile_group.x, tile_group.y,
                                                    &frame) != REACH_OK)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_render_group clip = {};
    clip.id = base + REACH_STAGE_GROUP_CLIP;
    clip.parent_id = tile_group.id;
    clip.bounds = clip.clip = tile_group.bounds;
    clip.has_clip = 1;
    clip.clip_insets[REACH_RENDER_CLIP_LEFT] = reach_motion_affine(&frame.x, 1.0f, margin);
    clip.clip_insets[REACH_RENDER_CLIP_TOP] = reach_motion_affine(&frame.y, 1.0f, margin);
    if (reach_motion_combine(&frame.x, -1.0f, &frame.width, -1.0f, tile_width - margin,
                             &clip.clip_insets[REACH_RENDER_CLIP_RIGHT]) != REACH_OK ||
        reach_motion_combine(&frame.y, -1.0f, &frame.height, -1.0f, tile_height - margin,
                             &clip.clip_insets[REACH_RENDER_CLIP_BOTTOM]) != REACH_OK)
    {
        return REACH_INVALID_ARGUMENT;
    }
    result = reach_stage_push_group(commands, clip);
    if (result != REACH_OK)
    {
        return result;
    }

    reach_color background = theme->stage_backdrop;
    background.a = 1.0f;
    reach_color border_color = theme->bar_border;
    border_color.a = 1.0f;
    reach_color highlight = theme->stage_tile_highlight;
    float highlight_opacity = highlight.a * hover;
    highlight.a = 1.0f;
    border_color = reach_theme_color_mix(border_color, highlight, highlight_opacity);
    reach_stage_fill_tint border_tint = {background, border_color, &fade};
    reach_motion zero = reach_stage_constant_motion(0.0f);
    reach_motion inset = reach_stage_constant_motion(border);

    if (tile->desktop)
    {
        reach_motion bottom_y = reach_motion_affine(&outer_height, 1.0f, -border);
        reach_motion side_height = reach_motion_affine(&outer_height, 1.0f, -border * 2.0f);
        reach_motion right_x = reach_motion_affine(&outer_width, 1.0f, -border);
        reach_motion thickness = reach_stage_constant_motion(border);
        reach_rect_motion strips[4] = {
            reach_stage_local_rect(zero, zero, outer_width, thickness),
            reach_stage_local_rect(zero, bottom_y, outer_width, thickness),
            reach_stage_local_rect(zero, inset, thickness, side_height),
            reach_stage_local_rect(right_x, inset, thickness, side_height),
        };
        for (size_t strip = 0; strip < 4 && border > 0.0f; ++strip)
        {
            result = reach_stage_push_fill(
                commands, base + REACH_STAGE_GROUP_OUTER + (uint64_t)strip * 2, clip.id,
                strips[strip], 0.0f, background, nullptr, &border_tint);
            if (result != REACH_OK)
            {
                return result;
            }
        }
        return REACH_OK;
    }

    reach_motion half_bar = reach_motion_affine(&bar, 0.5f, 0.0f);
    reach_motion radius =
        reach_motion_clamp(&half_bar, 0.0f, reach_stage_scaled(ctx, theme->radius_small));
    reach_motion outer_radius = reach_motion_affine(&radius, 1.0f, border);
    float radius_maximum = reach_stage_motion_maximum(radius);

    if (border > 0.0f)
    {
        reach_motion outer_extent =
            reach_motion_affine(&frame.height, 1.0f, radius_maximum + border);
        result = reach_stage_push_fill(
            commands, base + REACH_STAGE_GROUP_OUTER, clip.id,
            reach_stage_local_rect(frame.x, frame.y, frame.width, outer_extent),
            radius_maximum + border, background, nullptr, &border_tint, &outer_radius);
        if (result != REACH_OK)
        {
            return result;
        }

        reach_motion inner_x = reach_motion_affine(&frame.x, 1.0f, border);
        reach_motion inner_y = reach_motion_affine(&frame.y, 1.0f, border);
        reach_motion inner_width = reach_motion_affine(&frame.width, 1.0f, -border * 2.0f);
        reach_motion inner_extent =
            reach_motion_affine(&frame.height, 1.0f, radius_maximum - border * 2.0f);
        result = reach_stage_push_fill(
            commands, base + REACH_STAGE_GROUP_INNER, clip.id,
            reach_stage_local_rect(inner_x, inner_y, inner_width, inner_extent), radius_maximum,
            background, nullptr, nullptr, &radius);
        if (result != REACH_OK)
        {
            return result;
        }

        reach_motion strip_y = {};
        if (reach_motion_combine(&frame.y, 1.0f, &frame.height, 1.0f, -border, &strip_y) !=
            REACH_OK)
        {
            return REACH_INVALID_ARGUMENT;
        }
        result = reach_stage_push_fill(commands, base + REACH_STAGE_GROUP_STRIP, clip.id,
                                       reach_stage_local_rect(frame.x, strip_y, frame.width,
                                                              reach_stage_constant_motion(border)),
                                       0.0f, background, nullptr, &border_tint);
        if (result != REACH_OK)
        {
            return result;
        }
    }

    if (reach_stage_motion_maximum(bar) > 0.0f)
    {
        reach_render_group header_clip = {};
        header_clip.id = base + REACH_STAGE_GROUP_HEADER_CLIP;
        header_clip.parent_id = clip.id;
        header_clip.bounds = header_clip.clip = tile_group.bounds;
        header_clip.has_clip = 1;
        reach_motion_set(&header_clip.clip_insets[REACH_RENDER_CLIP_LEFT], border + margin);
        reach_motion_set(&header_clip.clip_insets[REACH_RENDER_CLIP_TOP], border + margin);
        header_clip.clip_insets[REACH_RENDER_CLIP_RIGHT] =
            reach_motion_affine(&rect.width, -1.0f, tile_width - margin - border);
        header_clip.clip_insets[REACH_RENDER_CLIP_BOTTOM] =
            reach_motion_affine(&bar, -1.0f, tile_height - margin - border);
        result = reach_stage_push_group(commands, header_clip);
        if (result != REACH_OK)
        {
            return result;
        }
        reach_motion header_extent = reach_motion_affine(&bar, 1.0f, radius_maximum);
        reach_color header_color = theme->stage_tile_header_background;
        result =
            reach_stage_push_fill(commands, base + REACH_STAGE_GROUP_HEADER, header_clip.id,
                                  reach_stage_local_rect(inset, inset, rect.width, header_extent),
                                  radius_maximum, header_color, &fade, nullptr, &radius);
        if (result != REACH_OK)
        {
            return result;
        }
    }

    if (tile->label[0] != 0)
    {
        result = reach_stage_push_moving_label(ctx, tile, base, clip.id, border, rect.width, bar,
                                               fade, commands);
        if (result != REACH_OK)
        {
            return result;
        }
    }

    if (!tile->departing)
    {
        result = reach_stage_push_moving_close(stage, ctx, index, base, clip.id, border, rect.width,
                                               bar, fade, commands);
        if (result != REACH_OK)
        {
            return result;
        }
    }

    if (tile->minimized || tile->departing)
    {
        result = reach_stage_push_moving_placeholder(ctx, tile, base, clip.id, border, bar, fade,
                                                     commands);
    }
    return result;
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

    const reach_stage_state *state = &stage->state;
    if (!state->open || state->close_failed || state->tile_count == 0)
    {
        return REACH_OK;
    }

    float border = reach_theme_border_thickness(ctx->theme, ctx->dpi_scale);
    size_t visited = 0;
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
            size_t reserved = REACH_STAGE_RASTER_GROUP_COUNT * (state->tile_count - visited++);

            int32_t animated = reach_stage_tile_animated(tile);
            if (animated)
            {
                if (reach_stage_motion_maximum(tile->fade_motion) <= 0.0f ||
                    reach_stage_motion_maximum(tile->rect_motion.width) <= 0.0f ||
                    reach_stage_motion_maximum(tile->rect_motion.height) <= 0.0f)
                {
                    continue;
                }
            }
            else if (tile->current_rect.width <= 0.0f || tile->current_rect.height <= 0.0f ||
                     state->progress * tile->presence <= 0.0f)
            {
                continue;
            }

            if (animated && REACH_MAX_RENDER_GROUPS - out_commands->group_count >=
                                REACH_STAGE_MOVING_GROUP_COUNT + reserved)
            {
                size_t saved_commands = out_commands->count;
                size_t saved_groups = out_commands->group_count;
                if (reach_stage_push_moving_tile(stage, ctx, index, border, out_commands) ==
                    REACH_OK)
                {
                    continue;
                }
                out_commands->count = saved_commands;
                out_commands->group_count = saved_groups;
                out_commands->current_group_id = 0;
            }

            reach_result result =
                reach_stage_push_raster_tile(stage, ctx, index, border, out_commands);
            if (result != REACH_OK)
            {
                return result;
            }
        }
    }

    return REACH_OK;
}
