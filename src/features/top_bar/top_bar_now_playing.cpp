#include "top_bar_now_playing.h"

#include "top_bar_metrics.h"
#include "reach/core/typography.h"

#include <math.h>
#include <new>

static const float REACH_TOP_BAR_NOW_PLAYING_COVER_WIDTH = 0.70f;
static const float REACH_TOP_BAR_NOW_PLAYING_COVER_FADE_START = 0.65f;
static const float REACH_TOP_BAR_NOW_PLAYING_BG_BLUR = 0.45f;
static const float REACH_TOP_BAR_NOW_PLAYING_BG_CONTRAST = 1.20f;

struct reach_top_bar_now_playing
{
    reach_top_bar_now_playing_model model;
    reach_top_bar_now_playing_layout layout;
    reach_marquee_state marquee;
    float text_offset_x;
    uint64_t observed_generation;
};

static reach_rect_f32 reach_top_bar_now_playing_rect(float x, float y, float width, float height)
{
    return {x, y, width, height};
}

static int32_t reach_top_bar_now_playing_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return (float)x >= rect.x && (float)x <= rect.x + rect.width && (float)y >= rect.y &&
           (float)y <= rect.y + rect.height;
}

static void reach_top_bar_now_playing_push_rect(reach_render_command_buffer *commands,
                                                reach_rect_f32 rect, reach_color color,
                                                float radius)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = rect;
    command.color = color;
    command.radius = radius;
    (void)reach_render_command_buffer_push(commands, &command);
}

static void reach_top_bar_now_playing_push_text(reach_render_command_buffer *commands,
                                                reach_rect_f32 rect, const uint16_t *value,
                                                float size, int32_t weight, int32_t alignment,
                                                reach_color color, reach_rect_f32 clip)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = rect;
    command.color = color;
    command.text_size = size;
    command.text_weight = weight;
    command.text_alignment = alignment;
    command.has_scissor = 1;
    command.scissor_rect = clip;
    reach_copy_utf16(command.text, 260, value);
    (void)reach_render_command_buffer_push(commands, &command);
}

static float reach_top_bar_now_playing_text_advance(const reach_text_measure_port *measure,
                                                    const uint16_t *text, float text_size)
{
    return reach_text_width_or_estimate(measure, text, text_size, REACH_TEXT_WEIGHT_BOLD,
                                        reach_top_bar_metrics_values.glyph_advance_ratio);
}

// The scissor clips flush against the text slot, so the first glyph is drawn hard on the edge and
// reads as clipped until it has scrolled clear. A leading space gives it that clearance up front.
static void reach_top_bar_now_playing_compose_line(reach_top_bar_now_playing_model *model)
{
    static const uint16_t separator[] = {' ', 0x2022, ' ', 0};

    size_t length = 0;
    if (model->title[0] == 0 && model->artist[0] == 0)
    {
        model->line[0] = 0;
        return;
    }

    model->line[length++] = ' ';
    for (size_t index = 0; model->title[index] != 0 && length + 1 < 260; ++index)
    {
        model->line[length++] = model->title[index];
    }
    if (model->title[0] != 0 && model->artist[0] != 0)
    {
        for (size_t index = 0; separator[index] != 0 && length + 1 < 260; ++index)
        {
            model->line[length++] = separator[index];
        }
    }
    for (size_t index = 0; model->artist[index] != 0 && length + 1 < 260; ++index)
    {
        model->line[length++] = model->artist[index];
    }
    model->line[length] = 0;
}

void reach_top_bar_now_playing_model_init(reach_top_bar_now_playing_model *model)
{
    if (model != nullptr)
    {
        *model = {};
        model->playback = REACH_MEDIA_PLAYBACK_UNKNOWN;
    }
}

float reach_top_bar_now_playing_model_desired_width(const reach_top_bar_now_playing_model *model,
                                                    const reach_theme *theme, float dpi_scale)
{
    if (model == nullptr || !model->visible)
    {
        return reach_top_bar_metrics_values.now_playing_collapsed_width * dpi_scale;
    }
    const reach_theme *actual = theme != nullptr ? theme : reach_theme_default();
    return actual->now_playing_width * dpi_scale;
}

reach_top_bar_now_playing_layout reach_top_bar_now_playing_compute_layout(
    const reach_top_bar_now_playing_model *model, const reach_theme *theme, reach_rect_f32 bounds,
    float dpi_scale, const reach_text_measure_port *text_measure)
{
    reach_top_bar_now_playing_layout layout = {};
    if (model == nullptr || bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return layout;
    }

    if (!model->visible)
    {
        layout.bounds = bounds;
        float glyph =
            bounds.height * reach_top_bar_metrics_values.now_playing_collapsed_glyph_scale;
        layout.cover =
            reach_top_bar_now_playing_rect(bounds.x + (bounds.width - glyph) * 0.5f,
                                           bounds.y + (bounds.height - glyph) * 0.5f, glyph, glyph);
        return layout;
    }

    const reach_theme *actual = theme != nullptr ? theme : reach_theme_default();
    float padding = actual->now_playing_padding * dpi_scale;
    float cover_gap = actual->now_playing_gap * dpi_scale;
    float button_gap = actual->now_playing_control_gap * dpi_scale;
    float play_size = actual->now_playing_play_button_width * dpi_scale;
    float skip_size = actual->now_playing_prev_next_button_width * dpi_scale;

    layout.bounds = bounds;
    layout.cover = bounds;

    float controls_width = play_size + button_gap + skip_size;
    float previous_x = bounds.x + padding;
    float text_x = previous_x + skip_size + cover_gap * 2.0f;
    float controls_x = bounds.x + bounds.width - padding - controls_width;
    float text_width = controls_x - cover_gap - text_x;
    if (text_width <= 0.0f)
    {
        return layout;
    }
    layout.cover = reach_top_bar_now_playing_rect(
        bounds.x, bounds.y, bounds.width * REACH_TOP_BAR_NOW_PLAYING_COVER_WIDTH, bounds.height);

    float text_size = REACH_TEXT_SIZE_MEDIUM * dpi_scale;
    layout.text = reach_top_bar_now_playing_rect(text_x, bounds.y, text_width, bounds.height);
    layout.text_advance =
        reach_top_bar_now_playing_text_advance(text_measure, model->line, text_size);
    layout.previous_button = reach_top_bar_now_playing_rect(
        previous_x, bounds.y + (bounds.height - skip_size) * 0.5f, skip_size, skip_size);
    layout.play_pause_button = reach_top_bar_now_playing_rect(
        controls_x, bounds.y + (bounds.height - play_size) * 0.5f, play_size, play_size);
    layout.next_button = reach_top_bar_now_playing_rect(
        layout.play_pause_button.x + play_size + button_gap,
        bounds.y + (bounds.height - skip_size) * 0.5f, skip_size, skip_size);
    return layout;
}

reach_now_playing_action
reach_top_bar_now_playing_hit_test(const reach_top_bar_now_playing_model *model,
                                   const reach_top_bar_now_playing_layout *layout, int32_t x,
                                   int32_t y)
{
    if (model == nullptr || layout == nullptr || !model->visible ||
        !reach_top_bar_now_playing_contains(layout->bounds, x, y))
    {
        return REACH_NOW_PLAYING_ACTION_NONE;
    }
    if (model->previous_enabled &&
        reach_top_bar_now_playing_contains(layout->previous_button, x, y))
    {
        return REACH_NOW_PLAYING_ACTION_PREVIOUS;
    }
    if (model->play_pause_enabled &&
        reach_top_bar_now_playing_contains(layout->play_pause_button, x, y))
    {
        return REACH_NOW_PLAYING_ACTION_PLAY_PAUSE;
    }
    if (model->next_enabled && reach_top_bar_now_playing_contains(layout->next_button, x, y))
    {
        return REACH_NOW_PLAYING_ACTION_NEXT;
    }
    return REACH_NOW_PLAYING_ACTION_NONE;
}

reach_result
reach_top_bar_now_playing_build_render_commands(const reach_top_bar_now_playing_render_input *input,
                                                reach_render_command_buffer *out_commands)
{
    if (input == nullptr || input->theme == nullptr || input->model == nullptr ||
        input->layout == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (input->layout->bounds.width <= 0.0f || input->layout->bounds.height <= 0.0f)
    {
        return REACH_OK;
    }

    const reach_theme *theme = input->theme;
    float radius = input->layout->bounds.height * 0.5f;

    if (!input->model->visible)
    {
        reach_top_bar_now_playing_push_rect(out_commands, input->layout->bounds,
                                            theme->bar_button_background, radius);
        reach_render_command glyph = {};
        glyph.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        glyph.rect = input->layout->cover;
        glyph.icon_id = REACH_VECTOR_ICON_MUSIC_NOTE;
        glyph.color = theme->system_glyph;
        return reach_render_command_buffer_push(out_commands, &glyph);
    }

    if (input->model->cover_image_id != 0)
    {
        float width = input->layout->bounds.width * 1.5f;
        float height = input->layout->bounds.height * 1.5f;
        reach_rect_f32 background = {
            input->layout->bounds.x - (width - input->layout->bounds.width) * 0.5f,
            input->layout->bounds.y - (height - input->layout->bounds.height) * 0.5f, width,
            height};
        reach_render_command blurred = {};
        blurred.type = REACH_RENDER_COMMAND_BLURRED_IMAGE;
        blurred.rect = background;
        blurred.icon_id = input->model->cover_image_id;
        blurred.icon_crop_to_fill = 1;
        blurred.radius = radius;
        blurred.blur_radius = input->layout->bounds.height * REACH_TOP_BAR_NOW_PLAYING_BG_BLUR;
        blurred.image_contrast = REACH_TOP_BAR_NOW_PLAYING_BG_CONTRAST;
        blurred.color.a = 1.0f;
        blurred.has_clip_rect = 1;
        blurred.clip_rect = input->layout->bounds;
        blurred.clip_radius = radius;
        (void)reach_render_command_buffer_push(out_commands, &blurred);

        reach_render_command cover = {};
        cover.type = REACH_RENDER_COMMAND_ICON;
        cover.rect = input->layout->cover;
        cover.icon_id = input->model->cover_image_id;
        cover.icon_crop_to_fill = 1;
        cover.radius = radius;
        cover.corner_mask = REACH_RENDER_CORNER_TOP_LEFT | REACH_RENDER_CORNER_BOTTOM_LEFT;
        cover.icon_fade_start = REACH_TOP_BAR_NOW_PLAYING_COVER_FADE_START;
        cover.color.a = 1.0f;
        (void)reach_render_command_buffer_push(out_commands, &cover);

        reach_top_bar_now_playing_push_rect(out_commands, input->layout->bounds,
                                            theme->now_playing_background, radius);
    }
    else
    {
        reach_top_bar_now_playing_push_rect(out_commands, input->layout->bounds,
                                            theme->bar_button_background, radius);
    }

    if (input->layout->text.width <= 0.0f || input->layout->play_pause_button.width <= 0.0f)
    {
        return REACH_OK;
    }

    float dpi_scale = input->dpi_scale > 0.0f ? input->dpi_scale : 1.0f;
    reach_rect_f32 text = input->layout->text;
    text.x += input->text_offset_x;
    text.width =
        input->layout->text_advance > text.width ? input->layout->text_advance : text.width;
    reach_top_bar_now_playing_push_text(out_commands, text, input->model->line,
                                        REACH_TEXT_SIZE_MEDIUM * dpi_scale, REACH_TEXT_WEIGHT_BOLD,
                                        REACH_TEXT_ALIGNMENT_LEADING, theme->now_playing_title,
                                        input->layout->text);

    reach_vector_icon_id icons[3] = {REACH_VECTOR_ICON_PREVIOUS,
                                     input->model->playback == REACH_MEDIA_PLAYBACK_PLAYING
                                         ? REACH_VECTOR_ICON_PAUSE
                                         : REACH_VECTOR_ICON_PLAY,
                                     REACH_VECTOR_ICON_NEXT};
    reach_rect_f32 buttons[3] = {input->layout->previous_button, input->layout->play_pause_button,
                                 input->layout->next_button};
    int32_t enabled[3] = {input->model->previous_enabled, input->model->play_pause_enabled,
                          input->model->next_enabled};
    for (size_t index = 0; index < 3; ++index)
    {
        reach_rect_f32 button = buttons[index];
        reach_rect_f32 background = button;
        if (index == 1)
        {
            float size = button.height * 1.25f;
            background = reach_top_bar_now_playing_rect(button.x + (button.width - size) * 0.5f,
                                                        button.y + (button.height - size) * 0.5f,
                                                        size, size);
        }
        if (index == 1 && input->model->cover_accent.a > 0.0f)
        {
            reach_color button_color = input->model->cover_accent;
            if (button_color.a < 0.78f)
            {
                button_color.a = 0.78f;
            }
            reach_top_bar_now_playing_push_rect(out_commands, background, button_color,
                                                background.height * 0.5f);
        }

        float icon_size = button.height * (index == 1 ? 0.58f : 0.70f);
        reach_render_command icon = {};
        icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        icon.rect = reach_top_bar_now_playing_rect(button.x + (button.width - icon_size) * 0.5f,
                                                   button.y + (button.height - icon_size) * 0.5f,
                                                   icon_size, icon_size);
        if (index == 1 && input->model->playback != REACH_MEDIA_PLAYBACK_PLAYING)
        {
            icon.rect.x += icon_size * 0.06f;
        }
        icon.icon_id = icons[index];
        icon.color = theme->now_playing_control_text;
        if (!enabled[index])
        {
            icon.color.a *= 0.30f;
        }
        (void)reach_render_command_buffer_push(out_commands, &icon);
    }

    return REACH_OK;
}

reach_result reach_top_bar_now_playing_create(reach_top_bar_now_playing **out_now_playing)
{
    if (out_now_playing == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_top_bar_now_playing *now_playing = new (std::nothrow) reach_top_bar_now_playing();
    if (now_playing == nullptr)
    {
        return REACH_ERROR;
    }
    reach_top_bar_now_playing_reset(now_playing);
    *out_now_playing = now_playing;
    return REACH_OK;
}

void reach_top_bar_now_playing_destroy(reach_top_bar_now_playing *now_playing)
{
    delete now_playing;
}

void reach_top_bar_now_playing_reset(reach_top_bar_now_playing *now_playing)
{
    if (now_playing == nullptr)
    {
        return;
    }
    reach_top_bar_now_playing_model_init(&now_playing->model);
    now_playing->layout = {};
    reach_marquee_reset(&now_playing->marquee);
    now_playing->text_offset_x = 0.0f;
    now_playing->observed_generation = 0;
}

void reach_top_bar_now_playing_sync(reach_top_bar_now_playing *now_playing,
                                    reach_now_playing_service *service,
                                    reach_top_bar_now_playing_update_result *out)
{
    if (out != nullptr)
    {
        *out = {};
    }
    if (now_playing == nullptr || service == nullptr || out == nullptr)
    {
        return;
    }
    reach_now_playing_snapshot snapshot = {};
    reach_now_playing_service_snapshot(service, &snapshot);
    if (snapshot.generation == now_playing->observed_generation)
    {
        return;
    }

    int32_t was_visible = now_playing->model.visible;
    reach_top_bar_now_playing_model next = {};
    next.visible = snapshot.has_session;
    reach_copy_utf16(next.title, 260, snapshot.title);
    reach_copy_utf16(next.artist, 260, snapshot.artist);
    reach_top_bar_now_playing_compose_line(&next);
    next.cover_image_id = snapshot.cover_image_id;
    next.cover_accent = snapshot.cover_accent;
    next.playback = snapshot.playback;
    next.previous_enabled = snapshot.previous_enabled;
    next.play_pause_enabled = snapshot.play_pause_enabled;
    next.next_enabled = snapshot.next_enabled;
    if (!reach_utf16_equal(now_playing->model.line, next.line))
    {
        reach_marquee_reset(&now_playing->marquee);
        now_playing->text_offset_x = 0.0f;
    }
    now_playing->model = next;
    now_playing->observed_generation = snapshot.generation;
    out->changed = 1;
    out->visibility_changed = was_visible != next.visible;
}

int32_t reach_top_bar_now_playing_tick(reach_top_bar_now_playing *now_playing, double delta_seconds)
{
    if (now_playing == nullptr || !now_playing->model.visible)
    {
        return 0;
    }

    reach_marquee_request request = {};
    request.content_width = now_playing->layout.text_advance;
    request.viewport_width = now_playing->layout.text.width;
    request.delta_seconds = delta_seconds;

    float offset = reach_marquee_advance(&now_playing->marquee, &request);
    if (offset == now_playing->text_offset_x)
    {
        return 0;
    }
    now_playing->text_offset_x = offset;
    return 1;
}

int32_t reach_top_bar_now_playing_scrolling(const reach_top_bar_now_playing *now_playing)
{
    if (now_playing == nullptr || !now_playing->model.visible)
    {
        return 0;
    }

    reach_marquee_request request = {};
    request.content_width = now_playing->layout.text_advance;
    request.viewport_width = now_playing->layout.text.width;
    return reach_marquee_scrolls(&request);
}

float reach_top_bar_now_playing_desired_width(const reach_top_bar_now_playing *now_playing,
                                              const reach_theme *theme, float dpi_scale)
{
    if (now_playing == nullptr)
    {
        return 0.0f;
    }
    return reach_top_bar_now_playing_model_desired_width(&now_playing->model, theme, dpi_scale);
}

void reach_top_bar_now_playing_relayout(reach_top_bar_now_playing *now_playing,
                                        const reach_theme *theme, reach_rect_f32 bounds,
                                        float dpi_scale,
                                        const reach_text_measure_port *text_measure)
{
    if (now_playing == nullptr)
    {
        return;
    }
    reach_top_bar_now_playing_model model = now_playing->model;
    now_playing->layout =
        reach_top_bar_now_playing_compute_layout(&model, theme, bounds, dpi_scale, text_measure);
}

reach_now_playing_action
reach_top_bar_now_playing_action_at(const reach_top_bar_now_playing *now_playing, int32_t x,
                                    int32_t y)
{
    if (now_playing == nullptr)
    {
        return REACH_NOW_PLAYING_ACTION_NONE;
    }
    return reach_top_bar_now_playing_hit_test(&now_playing->model, &now_playing->layout, x, y);
}

reach_result reach_top_bar_now_playing_append_render_commands(
    reach_top_bar_now_playing *now_playing, const reach_top_bar_now_playing_render_context *ctx,
    reach_render_command_buffer *out_commands)
{
    if (now_playing == nullptr || ctx == nullptr || ctx->theme == nullptr ||
        out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_top_bar_now_playing_model model = now_playing->model;
    reach_top_bar_now_playing_render_input input = {};
    input.theme = ctx->theme;
    input.model = &model;
    input.layout = &now_playing->layout;
    input.text_offset_x = now_playing->text_offset_x;
    input.dpi_scale = ctx->dpi_scale;
    return reach_top_bar_now_playing_build_render_commands(&input, out_commands);
}
