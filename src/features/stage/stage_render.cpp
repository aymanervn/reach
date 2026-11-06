#include "reach/features/stage.h"

#include "stage_common.h"

#include "reach/core/typography.h"
#include "reach/features/common/ui_controls.h"

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

static reach_color reach_stage_tile_border(const reach_stage_render_context *ctx, float hover,
                                           float alpha);

static size_t reach_stage_tile_shapes(const reach_stage_render_context *ctx, reach_rect_f32 item,
                                      float header_radius, float width, float hover, float alpha,
                                      reach_render_command out_shapes[2])
{
    if (ctx == nullptr || width <= 0.0f)
    {
        return 0;
    }

    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_RECT;
    shape.rect = {item.x - width, item.y - width, item.width + width * 2.0f,
                  item.height + width * 2.0f};
    shape.radius = header_radius + width;
    shape.corner_mask = REACH_RENDER_CORNER_TOP_LEFT | REACH_RENDER_CORNER_TOP_RIGHT;

    reach_color background = ctx->theme->stage_backdrop;
    background.a = 1.0f;
    reach_color border = reach_stage_tile_border(ctx, hover, alpha);
    return reach_render_bordered_background_shapes(&shape, background, border, width, out_shapes);
}

static reach_result reach_stage_push_tile_background(const reach_stage_render_context *ctx,
                                                     reach_render_command_buffer *commands,
                                                     reach_rect_f32 item, float header_radius,
                                                     float width, float hover, float alpha)
{
    if (commands == nullptr)
    {
        return REACH_OK;
    }

    reach_render_command shapes[2];
    size_t count = reach_stage_tile_shapes(ctx, item, header_radius, width, hover, alpha, shapes);
    for (size_t index = 0; index < count; ++index)
    {
        reach_result result = reach_render_command_buffer_push(commands, &shapes[index]);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    return REACH_OK;
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

static reach_rect_f32 reach_stage_placeholder_icon_rect(const reach_stage_render_context *ctx,
                                                        reach_rect_f32 rect)
{
    float icon_size = reach_stage_scaled(ctx, 48.0f);
    if (icon_size > rect.width * 0.5f)
    {
        icon_size = rect.width * 0.5f;
    }
    if (icon_size > rect.height * 0.5f)
    {
        icon_size = rect.height * 0.5f;
    }
    return {rect.x + (rect.width - icon_size) * 0.5f, rect.y + (rect.height - icon_size) * 0.5f,
            icon_size, icon_size};
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

    reach_render_command icon = {};
    icon.type = REACH_RENDER_COMMAND_ICON;
    icon.icon_id = tile->icon_id;
    icon.rect = reach_stage_placeholder_icon_rect(ctx, rect);
    icon.color = reach_stage_rgba(1.0f, 1.0f, 1.0f, alpha);
    return reach_render_command_buffer_push(out_commands, &icon);
}

static const reach_motion *reach_stage_close_hover_motion(const reach_stage *stage, size_t index)
{
    return stage->state.close_hover_index == index &&
                   (stage->close_hover.to > 0.0f || reach_motion_active(&stage->close_hover))
               ? &stage->close_hover
               : nullptr;
}

static reach_result reach_stage_push_close_button(const reach_stage *stage,
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

    const reach_motion *hover = reach_stage_close_hover_motion(stage, index);
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
    return result;
}

static int32_t reach_stage_header_visible(const reach_stage_tile *tile)
{
    return !tile->desktop && tile->current_bar.width > 0.0f && tile->current_bar.height > 0.0f;
}

static reach_result reach_stage_push_tile_header_content(const reach_stage *stage,
                                                         const reach_stage_render_context *ctx,
                                                         size_t index, float alpha,
                                                         reach_render_command_buffer *out_commands)
{
    const reach_stage_tile *tile = &stage->state.tiles[index];
    reach_rect_f32 header = tile->current_bar;

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
        label.color = ctx->theme->stage_tile_label;
        label.color.a *= alpha;
        label.text_color = label.color;
        (void)reach_copy_utf16(label.text, 260, tile->label);
        result = reach_render_command_buffer_push(out_commands, &label);
        if (result != REACH_OK)
        {
            return result;
        }
    }

    return REACH_OK;
}

static reach_result reach_stage_push_tile_header(const reach_stage *stage,
                                                 const reach_stage_render_context *ctx,
                                                 size_t index, float alpha,
                                                 reach_render_command_buffer *out_commands)
{
    if (!reach_stage_header_visible(&stage->state.tiles[index]))
    {
        return REACH_OK;
    }
    reach_result result =
        reach_stage_push_tile_header_content(stage, ctx, index, alpha, out_commands);
    if (result != REACH_OK)
    {
        return result;
    }
    return reach_stage_push_close_button(stage, ctx, index, alpha, out_commands);
}

static reach_result reach_stage_push_tile_legacy(const reach_stage *stage,
                                                 const reach_stage_render_context *ctx,
                                                 size_t index, reach_rect_f32 rect, float alpha,
                                                 float border,
                                                 reach_render_command_buffer *out_commands)
{
    const reach_stage_state *state = &stage->state;
    const reach_stage_tile *tile = &state->tiles[index];
    if (tile->desktop)
    {
        return reach_stage_push_tile_outline(ctx, out_commands, rect, border, alpha);
    }

    reach_rect_f32 item = rect;
    item.y -= tile->current_bar.height;
    item.height += tile->current_bar.height;
    float hover = state->has_hover && state->hover_index == index ? 1.0f : 0.0f;
    reach_result result = reach_stage_push_tile_background(
        ctx, out_commands, item, reach_stage_header_radius(ctx, tile->current_bar), border, hover,
        alpha);
    if (result != REACH_OK)
    {
        return result;
    }

    result = reach_stage_push_tile_header(stage, ctx, index, alpha, out_commands);
    if (result != REACH_OK)
    {
        return result;
    }

    if (tile->minimized || tile->departing)
    {
        return reach_stage_push_tile_placeholder(ctx, tile, rect, alpha, out_commands);
    }
    return REACH_OK;
}

// Workaround: temporary Stage v3 A/B switch (stage-v3); delete with the losing path.
enum
{
    REACH_STAGE_SKELETON_HEAD = 1,
    REACH_STAGE_SKELETON_BORDER,
    REACH_STAGE_SKELETON_FILL,
    REACH_STAGE_SKELETON_PLATE,
    REACH_STAGE_SKELETON_ICON,
    REACH_STAGE_SKELETON_STRIP
};

// Workaround: temporary Stage v3 A/B switch (stage-v3); delete with the losing path.
static int32_t reach_stage_tile_motion_live(const reach_stage *stage)
{
    return reach_animation_manager_active(&stage->animations, REACH_STAGE_ANIMATION_PROGRESS) ||
           reach_animation_manager_active(&stage->animations,
                                          REACH_STAGE_ANIMATION_DESKTOP_PROGRESS) ||
           reach_animation_manager_active(&stage->animations, REACH_STAGE_ANIMATION_REFLOW) ||
           reach_animation_manager_active(&stage->animations, REACH_STAGE_ANIMATION_DEPARTURE) ||
           reach_animation_manager_active(&stage->animations, REACH_STAGE_ANIMATION_RETARGET);
}

// Workaround: temporary Stage v3 A/B switch (stage-v3); delete with the losing path.
static reach_result reach_stage_skeleton_begin(reach_render_command_buffer *commands,
                                               uintptr_t window, uint64_t role,
                                               reach_rect_f32 bounds)
{
    reach_render_group group = {};
    group.id = UINT64_C(0x2000000000000000) | (role << 52) |
               ((uint64_t)window & UINT64_C(0xfffffffffffff));
    group.bounds = bounds;
    reach_motion_set(&group.opacity, 1.0f);
    reach_motion_set(&group.scale, 1.0f);
    return reach_render_command_buffer_begin_group(commands, &group);
}

// Workaround: temporary Stage v3 A/B switch (stage-v3); delete with the losing path.
static reach_result reach_stage_skeleton_solid(reach_render_command_buffer *commands,
                                               uintptr_t window, uint64_t role, reach_rect_f32 rect,
                                               reach_color color)
{
    if (rect.width <= 0.0f || rect.height <= 0.0f)
    {
        return REACH_OK;
    }
    reach_result result = reach_stage_skeleton_begin(commands, window, role, rect);
    if (result != REACH_OK)
    {
        return result;
    }
    reach_render_command fill = {};
    fill.type = REACH_RENDER_COMMAND_RECT;
    fill.rect = rect;
    fill.color = color;
    result = reach_render_command_buffer_push(commands, &fill);
    reach_render_command_buffer_end_group(commands);
    return result;
}

// Workaround: temporary Stage v3 A/B switch (stage-v3); delete with the losing path.
static reach_result reach_stage_skeleton_head(const reach_stage *stage,
                                              const reach_stage_render_context *ctx, size_t index,
                                              float alpha, reach_rect_f32 head,
                                              const reach_render_command *shapes,
                                              size_t shape_count,
                                              reach_render_command_buffer *out_commands)
{
    const reach_stage_tile *tile = &stage->state.tiles[index];
    if (head.width <= 0.0f || head.height <= 0.0f)
    {
        return REACH_OK;
    }
    reach_result result =
        reach_stage_skeleton_begin(out_commands, tile->window, REACH_STAGE_SKELETON_HEAD, head);
    if (result != REACH_OK)
    {
        return result;
    }
    for (size_t shape = 0; shape < shape_count && result == REACH_OK; ++shape)
    {
        reach_render_command clipped = shapes[shape];
        clipped.has_scissor = 1;
        clipped.scissor_rect = head;
        result = reach_render_command_buffer_push(out_commands, &clipped);
    }
    int32_t header = reach_stage_header_visible(tile);
    if (result == REACH_OK && header)
    {
        result = reach_stage_push_tile_header_content(stage, ctx, index, alpha, out_commands);
    }
    int32_t button_in_head = reach_stage_close_hover_motion(stage, index) == nullptr;
    if (result == REACH_OK && header && button_in_head)
    {
        result = reach_stage_push_close_button(stage, ctx, index, alpha, out_commands);
    }
    reach_render_command_buffer_end_group(out_commands);
    if (result == REACH_OK && header && !button_in_head)
    {
        result = reach_stage_push_close_button(stage, ctx, index, alpha, out_commands);
    }
    return result;
}

// Workaround: temporary Stage v3 A/B switch (stage-v3); delete with the losing path.
static reach_result reach_stage_skeleton_placeholder(const reach_stage_render_context *ctx,
                                                     const reach_stage_tile *tile,
                                                     reach_rect_f32 rect, float alpha,
                                                     reach_render_command_buffer *out_commands)
{
    reach_result result = reach_stage_skeleton_solid(
        out_commands, tile->window, REACH_STAGE_SKELETON_PLATE,
        reach_render_pixel_aligned_rect(rect),
        reach_theme_color_alpha(ctx->theme->stage_tile_placeholder, alpha));
    if (result != REACH_OK || tile->icon_id == 0)
    {
        return result;
    }

    reach_rect_f32 bounds = reach_stage_placeholder_icon_rect(ctx, rect);
    bounds.x = roundf(bounds.x);
    bounds.y = roundf(bounds.y);
    if (bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return REACH_OK;
    }
    result =
        reach_stage_skeleton_begin(out_commands, tile->window, REACH_STAGE_SKELETON_ICON, bounds);
    if (result != REACH_OK)
    {
        return result;
    }
    reach_render_command icon = {};
    icon.type = REACH_RENDER_COMMAND_ICON;
    icon.icon_id = tile->icon_id;
    icon.rect = bounds;
    icon.color = reach_stage_rgba(1.0f, 1.0f, 1.0f, alpha);
    result = reach_render_command_buffer_push(out_commands, &icon);
    reach_render_command_buffer_end_group(out_commands);
    return result;
}

// Workaround: temporary Stage v3 A/B switch (stage-v3); delete with the losing path.
static reach_result reach_stage_skeleton_outline(const reach_stage_render_context *ctx,
                                                 const reach_stage_tile *tile, reach_rect_f32 rect,
                                                 float width, float alpha,
                                                 reach_render_command_buffer *out_commands)
{
    reach_rect_f32 strips[4] = {
        {rect.x - width, rect.y - width, rect.width + width * 2.0f, width},
        {rect.x - width, rect.y + rect.height, rect.width + width * 2.0f, width},
        {rect.x - width, rect.y, width, rect.height},
        {rect.x + rect.width, rect.y, width, rect.height},
    };
    reach_color color = reach_stage_tile_border(ctx, 0.0f, alpha);
    for (size_t strip = 0; strip < 4; ++strip)
    {
        reach_result result = reach_stage_skeleton_solid(
            out_commands, tile->window, REACH_STAGE_SKELETON_STRIP + strip,
            reach_render_pixel_aligned_rect(strips[strip]), color);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    return REACH_OK;
}

// Workaround: temporary Stage v3 A/B switch (stage-v3); delete with the losing path.
static reach_result reach_stage_push_tile_skeleton(const reach_stage *stage,
                                                   const reach_stage_render_context *ctx,
                                                   size_t index, reach_rect_f32 rect, float alpha,
                                                   float border,
                                                   reach_render_command_buffer *out_commands)
{
    const reach_stage_state *state = &stage->state;
    const reach_stage_tile *tile = &state->tiles[index];
    if (tile->desktop)
    {
        return reach_stage_skeleton_outline(ctx, tile, rect, border, alpha, out_commands);
    }

    reach_rect_f32 item = rect;
    item.y -= tile->current_bar.height;
    item.height += tile->current_bar.height;
    float hover = state->has_hover && state->hover_index == index ? 1.0f : 0.0f;
    float radius = reach_stage_header_radius(ctx, tile->current_bar);
    reach_render_command shapes[2];
    size_t shape_count = reach_stage_tile_shapes(ctx, item, radius, border, hover, alpha, shapes);

    reach_rect_f32 head = {};
    float body_top = 0.0f;
    float body_bottom = 0.0f;
    if (shape_count != 0)
    {
        reach_rect_f32 outer = shapes[0].rect;
        body_bottom = outer.y + outer.height;
        body_top = fminf(ceilf(item.y + fmaxf(tile->current_bar.height, radius)), body_bottom);
        head = {outer.x, outer.y, outer.width, body_top - outer.y};
    }
    else if (reach_stage_header_visible(tile))
    {
        reach_rect_f32 header = tile->current_bar;
        header.x -= ctx->bounds.x;
        header.y -= ctx->bounds.y;
        float left = floorf(header.x);
        float top = floorf(header.y);
        head = {left, top, ceilf(header.x + header.width) - left,
                ceilf(header.y + header.height) - top};
    }

    reach_result result = REACH_OK;
    if (shape_count != 0)
    {
        reach_rect_f32 outer = shapes[0].rect;
        result = reach_stage_skeleton_solid(
            out_commands, tile->window, REACH_STAGE_SKELETON_BORDER,
            {outer.x, body_top, outer.width, body_bottom - body_top}, shapes[0].color);
        if (result == REACH_OK && shape_count == 2)
        {
            float left = roundf(shapes[1].rect.x);
            float right = roundf(shapes[1].rect.x + shapes[1].rect.width);
            float bottom = roundf(shapes[1].rect.y + shapes[1].rect.height);
            result = reach_stage_skeleton_solid(
                out_commands, tile->window, REACH_STAGE_SKELETON_FILL,
                {left, body_top, right - left, bottom - body_top}, shapes[1].color);
        }
    }
    if (result == REACH_OK)
    {
        result = reach_stage_skeleton_head(stage, ctx, index, alpha, head, shapes, shape_count,
                                           out_commands);
    }
    if (result == REACH_OK && (tile->minimized || tile->departing))
    {
        result = reach_stage_skeleton_placeholder(ctx, tile, rect, alpha, out_commands);
    }
    return result;
}

static reach_result reach_stage_append_tiles(const reach_stage *stage,
                                             const reach_stage_render_context *ctx,
                                             int32_t skeleton,
                                             reach_render_command_buffer *out_commands)
{
    const reach_stage_state *state = &stage->state;
    float eased = state->progress;
    float border = reach_theme_border_thickness(ctx->theme, ctx->dpi_scale);
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

            rect.x -= ctx->bounds.x;
            rect.y -= ctx->bounds.y;
            reach_result result = skeleton
                                      ? reach_stage_push_tile_skeleton(stage, ctx, index, rect,
                                                                       alpha, border, out_commands)
                                      : reach_stage_push_tile_legacy(stage, ctx, index, rect, alpha,
                                                                     border, out_commands);
            if (result != REACH_OK)
            {
                return result;
            }
        }
    }
    return REACH_OK;
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

    // Workaround: temporary Stage v3 A/B switch (stage-v3); delete with the losing path.
    if (stage->motion_layers == 2 && ctx->retained_supported && ctx->solid_fills_composed &&
        reach_stage_tile_motion_live(stage))
    {
        if (reach_stage_append_tiles(stage, ctx, 1, out_commands) == REACH_OK)
        {
            return REACH_OK;
        }
        reach_render_command_buffer_clear(out_commands);
    }

    return reach_stage_append_tiles(stage, ctx, 0, out_commands);
}
