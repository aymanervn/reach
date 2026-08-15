#include "reach/features/stage.h"

#include "stage_common.h"

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

    float hover = state->close_hover_index == index ? state->close_hover : 0.0f;

    reach_render_command backing = {};
    backing.type = REACH_RENDER_COMMAND_RECT;
    backing.rect = button;
    backing.radius = button.height * 0.5f;
    const reach_theme *theme = ctx->theme;
    backing.color = reach_theme_color_mix(theme->stage_close_background,
                                          theme->stage_close_hover_background, hover);
    backing.color.a *= alpha;
    reach_result result = reach_render_command_buffer_push(out_commands, &backing);
    if (result != REACH_OK)
    {
        return result;
    }

    float inset = button.width * 0.28f;
    reach_render_command glyph = {};
    glyph.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    glyph.icon_id = REACH_VECTOR_ICON_CLOSE;
    glyph.rect.x = button.x + inset;
    glyph.rect.y = button.y + inset;
    glyph.rect.width = button.width - inset * 2.0f;
    glyph.rect.height = button.height - inset * 2.0f;
    glyph.color =
        reach_theme_color_mix(theme->stage_close_glyph, theme->stage_close_hover_glyph, hover);
    glyph.color.a *= alpha;
    return reach_render_command_buffer_push(out_commands, &glyph);
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
    if (!state->open || state->tile_count == 0)
    {
        return REACH_OK;
    }

    float eased = state->progress;

    reach_render_command backdrop = {};
    backdrop.type = REACH_RENDER_COMMAND_RECT;
    backdrop.rect.x = 0.0f;
    backdrop.rect.y = 0.0f;
    backdrop.rect.width = ctx->bounds.width;
    backdrop.rect.height = ctx->bounds.height;
    backdrop.color = ctx->theme->stage_backdrop;
    backdrop.color.a *= eased;
    reach_result result = reach_render_command_buffer_push(out_commands, &backdrop);
    if (result != REACH_OK)
    {
        return result;
    }

    float border = reach_stage_tile_border(state);
    float label_height = reach_stage_scaled(ctx, 22.0f);
    float label_gap = reach_stage_scaled(ctx, 6.0f);
    float label_size = reach_stage_scaled(ctx, 12.0f);

    for (size_t index = 0; index < state->tile_count; ++index)
    {
        const reach_stage_tile *tile = &state->tiles[index];
        if (state->closing && state->has_selection && index != state->selected_index)
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

        result = reach_stage_push_close_button(stage, ctx, index, alpha, out_commands);
        if (result != REACH_OK)
        {
            return result;
        }

        if ((tile->minimized || tile->departing) && !tile->desktop)
        {
            result = reach_stage_push_tile_placeholder(ctx, tile, rect, alpha, out_commands);
            if (result != REACH_OK)
            {
                return result;
            }
        }

        if (tile->desktop && tile->icon_id != 0)
        {
            reach_render_command wallpaper = {};
            wallpaper.type = REACH_RENDER_COMMAND_ICON;
            wallpaper.icon_id = tile->icon_id;
            wallpaper.rect = rect;
            wallpaper.icon_crop_to_fill = 1;
            wallpaper.color = reach_stage_rgba(1.0f, 1.0f, 1.0f, alpha);
            result = reach_render_command_buffer_push(out_commands, &wallpaper);
            if (result != REACH_OK)
            {
                return result;
            }
        }

        if (state->has_hover && state->hover_index == index)
        {
            reach_render_command highlight = {};
            highlight.type = REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE;
            highlight.rect.x = rect.x - border;
            highlight.rect.y = rect.y - border;
            highlight.rect.width = rect.width + border * 2.0f;
            highlight.rect.height = rect.height + border * 2.0f;
            highlight.stroke_width = border;
            highlight.color = ctx->theme->stage_tile_highlight;
            highlight.color.a *= alpha;
            result = reach_render_command_buffer_push(out_commands, &highlight);
            if (result != REACH_OK)
            {
                return result;
            }
        }

        if (tile->label[0] != 0 &&
            rect.y + rect.height + label_gap + label_height <= ctx->bounds.height)
        {
            reach_render_command label = {};
            label.type = REACH_RENDER_COMMAND_TEXT;
            label.rect.x = rect.x;
            label.rect.y = rect.y + rect.height + label_gap;
            label.rect.width = rect.width;
            label.rect.height = label_height;
            label.text_size = label_size;
            label.text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
            label.text_alignment = REACH_TEXT_ALIGNMENT_CENTER;
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
    }

    return REACH_OK;
}
