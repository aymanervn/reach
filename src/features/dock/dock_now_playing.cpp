#include "dock_now_playing.h"

#include <math.h>
#include <new>

static const float REACH_DOCK_NOW_PLAYING_COVER_WIDTH = 0.70f;
static const float REACH_DOCK_NOW_PLAYING_COVER_FADE_START = 0.65f;
static const float REACH_DOCK_NOW_PLAYING_BG_BLUR = 0.45f;
static const float REACH_DOCK_NOW_PLAYING_BG_CONTRAST = 1.20f;

struct reach_dock_now_playing
{
    reach_dock_now_playing_model model;
    reach_dock_now_playing_layout layout;
    reach_now_playing_action pressed_action;
    uint64_t observed_generation;
};

static float reach_dock_now_playing_max(float a, float b)
{
    return a > b ? a : b;
}

static float reach_dock_now_playing_min(float a, float b)
{
    return a < b ? a : b;
}

static reach_rect_f32 reach_dock_now_playing_rect(float x, float y, float width, float height)
{
    return {x, y, width, height};
}

static int32_t reach_dock_now_playing_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return (float)x >= rect.x && (float)x <= rect.x + rect.width && (float)y >= rect.y &&
           (float)y <= rect.y + rect.height;
}

static reach_rect_f32 reach_dock_now_playing_intersection(reach_rect_f32 a, reach_rect_f32 b)
{
    float left = reach_dock_now_playing_max(a.x, b.x);
    float top = reach_dock_now_playing_max(a.y, b.y);
    float right = reach_dock_now_playing_min(a.x + a.width, b.x + b.width);
    float bottom = reach_dock_now_playing_min(a.y + a.height, b.y + b.height);
    return {left, top, right > left ? right - left : 0.0f, bottom > top ? bottom - top : 0.0f};
}

static void reach_dock_now_playing_clip(reach_render_command *command, reach_rect_f32 reveal,
                                        float radius)
{
    if (command == nullptr)
    {
        return;
    }
    reach_rect_f32 clip = command->has_clip_rect
                              ? reach_dock_now_playing_intersection(command->clip_rect, reveal)
                              : reveal;
    command->has_clip_rect = 1;
    command->clip_rect = clip;
    command->clip_radius = radius;
    if (clip.width <= 0.0f || clip.height <= 0.0f)
    {
        command->color.a = 0.0f;
    }
}

static void reach_dock_now_playing_push_rect(reach_render_command_buffer *commands,
                                             reach_rect_f32 rect, reach_color color, float radius)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = rect;
    command.color = color;
    command.radius = radius;
    (void)reach_render_command_buffer_push(commands, &command);
}

static void reach_dock_now_playing_push_text(reach_render_command_buffer *commands,
                                             reach_rect_f32 rect, const uint16_t *value, float size,
                                             int32_t weight, int32_t alignment, reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = rect;
    command.color = color;
    command.text_size = size;
    command.text_weight = weight;
    command.text_alignment = alignment;
    command.text_ellipsis = 1;
    reach_copy_utf16(command.text, 260, value);
    (void)reach_render_command_buffer_push(commands, &command);
}

void reach_dock_now_playing_model_init(reach_dock_now_playing_model *model)
{
    if (model != nullptr)
    {
        *model = {};
        model->playback = REACH_MEDIA_PLAYBACK_UNKNOWN;
    }
}

float reach_dock_now_playing_model_desired_width(const reach_dock_now_playing_model *model,
                                                 const reach_theme *theme, float dpi_scale)
{
    if (model == nullptr || !model->visible)
    {
        return 0.0f;
    }
    const reach_theme *actual = theme != nullptr ? theme : reach_theme_default();
    return actual->now_playing_width * dpi_scale;
}

reach_dock_now_playing_layout
reach_dock_now_playing_compute_layout(const reach_dock_now_playing_model *model,
                                      const reach_theme *theme, reach_rect_f32 bounds,
                                      float dpi_scale)
{
    reach_dock_now_playing_layout layout = {};
    if (model == nullptr || !model->visible || bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return layout;
    }

    const reach_theme *actual = theme != nullptr ? theme : reach_theme_default();
    float padding = actual->now_playing_padding * dpi_scale;
    float cover_gap = actual->now_playing_gap * dpi_scale;
    float text_gap = actual->now_playing_text_gap * dpi_scale;
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
    layout.cover = reach_dock_now_playing_rect(
        bounds.x, bounds.y, bounds.width * REACH_DOCK_NOW_PLAYING_COVER_WIDTH, bounds.height);

    float title_height = actual->now_playing_title_text_size * dpi_scale;
    float artist_height = actual->now_playing_artist_text_size * dpi_scale;
    float text_y = bounds.y + (bounds.height - title_height - artist_height - text_gap) * 0.5f;
    if (text_y < bounds.y + padding)
    {
        text_y = bounds.y + padding;
    }
    layout.title = reach_dock_now_playing_rect(text_x, text_y, text_width, title_height);
    layout.artist = reach_dock_now_playing_rect(text_x, text_y + title_height + text_gap,
                                                text_width, artist_height);
    layout.previous_button = reach_dock_now_playing_rect(
        previous_x, bounds.y + (bounds.height - skip_size) * 0.5f, skip_size, skip_size);
    layout.play_pause_button = reach_dock_now_playing_rect(
        controls_x, bounds.y + (bounds.height - play_size) * 0.5f, play_size, play_size);
    layout.next_button = reach_dock_now_playing_rect(
        layout.play_pause_button.x + play_size + button_gap,
        bounds.y + (bounds.height - skip_size) * 0.5f, skip_size, skip_size);
    return layout;
}

reach_now_playing_action
reach_dock_now_playing_hit_test(const reach_dock_now_playing_model *model,
                                const reach_dock_now_playing_layout *layout, int32_t x, int32_t y)
{
    if (model == nullptr || layout == nullptr || !model->visible ||
        !reach_dock_now_playing_contains(layout->bounds, x, y))
    {
        return REACH_NOW_PLAYING_ACTION_NONE;
    }
    if (model->previous_enabled && reach_dock_now_playing_contains(layout->previous_button, x, y))
    {
        return REACH_NOW_PLAYING_ACTION_PREVIOUS;
    }
    if (model->play_pause_enabled &&
        reach_dock_now_playing_contains(layout->play_pause_button, x, y))
    {
        return REACH_NOW_PLAYING_ACTION_PLAY_PAUSE;
    }
    if (model->next_enabled && reach_dock_now_playing_contains(layout->next_button, x, y))
    {
        return REACH_NOW_PLAYING_ACTION_NEXT;
    }
    return REACH_NOW_PLAYING_ACTION_NONE;
}

reach_result
reach_dock_now_playing_build_render_commands(const reach_dock_now_playing_render_input *input,
                                             reach_render_command_buffer *out_commands)
{
    if (input == nullptr || input->theme == nullptr || input->model == nullptr ||
        input->layout == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (!input->model->visible || input->layout->bounds.width <= 0.0f ||
        input->layout->bounds.height <= 0.0f)
    {
        return REACH_OK;
    }

    float reveal_width =
        reach_dock_now_playing_min(input->reveal_width, input->layout->bounds.width);
    if (reveal_width <= 0.001f)
    {
        return REACH_OK;
    }
    reach_rect_f32 reveal = input->layout->bounds;
    reveal.width = reveal_width;
    size_t first_command = out_commands->count;
    const reach_theme *theme = input->theme;
    float radius = reach_theme_now_playing_corner_radius(theme, input->layout->bounds.height);

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
        blurred.blur_radius = input->layout->bounds.height * REACH_DOCK_NOW_PLAYING_BG_BLUR;
        blurred.image_contrast = REACH_DOCK_NOW_PLAYING_BG_CONTRAST;
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
        cover.icon_fade_start = REACH_DOCK_NOW_PLAYING_COVER_FADE_START;
        cover.color.a = 1.0f;
        (void)reach_render_command_buffer_push(out_commands, &cover);

        reach_color overlay = theme->now_playing_background;
        overlay.r *= 0.34f;
        overlay.g *= 0.34f;
        overlay.b *= 0.34f;
        if (overlay.a < 0.44f)
        {
            overlay.a = 0.44f;
        }
        else if (overlay.a > 0.50f)
        {
            overlay.a = 0.50f;
        }
        reach_dock_now_playing_push_rect(out_commands, input->layout->bounds, overlay, radius);
    }
    else
    {
        reach_dock_now_playing_push_rect(out_commands, input->layout->bounds,
                                         theme->dock_button_background, radius);
    }

    if (input->layout->title.height <= 0.0f || input->layout->play_pause_button.width <= 0.0f)
    {
        for (size_t index = first_command; index < out_commands->count; ++index)
        {
            reach_dock_now_playing_clip(&out_commands->commands[index], reveal, radius);
        }
        return REACH_OK;
    }

    reach_dock_now_playing_push_text(out_commands, input->layout->title, input->model->title,
                                     theme->now_playing_title_text_size, REACH_TEXT_WEIGHT_DEMIBOLD,
                                     REACH_TEXT_ALIGNMENT_LEADING, theme->now_playing_title);
    reach_dock_now_playing_push_text(out_commands, input->layout->artist, input->model->artist,
                                     theme->now_playing_artist_text_size, REACH_TEXT_WEIGHT_NORMAL,
                                     REACH_TEXT_ALIGNMENT_LEADING, theme->now_playing_artist_text);

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
            float size = button.height * 1.1f;
            background =
                reach_dock_now_playing_rect(button.x + (button.width - size) * 0.5f,
                                            button.y + (button.height - size) * 0.5f, size, size);
        }
        reach_color button_color = theme->now_playing_control_background;
        if (index == 1 && input->model->cover_accent.a > 0.0f)
        {
            button_color = input->model->cover_accent;
            if (button_color.a < 0.78f)
            {
                button_color.a = 0.78f;
            }
        }
        reach_dock_now_playing_push_rect(out_commands, background, button_color,
                                         index == 1 ? background.height * 0.5f
                                                    : background.height * 0.4f);

        float icon_size = button.height * 0.70f;
        reach_render_command icon = {};
        icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        icon.rect = reach_dock_now_playing_rect(button.x + (button.width - icon_size) * 0.5f,
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

    for (size_t index = first_command; index < out_commands->count; ++index)
    {
        reach_dock_now_playing_clip(&out_commands->commands[index], reveal, radius);
    }
    return REACH_OK;
}

reach_result reach_dock_now_playing_create(reach_dock_now_playing **out_now_playing)
{
    if (out_now_playing == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_dock_now_playing *now_playing = new (std::nothrow) reach_dock_now_playing();
    if (now_playing == nullptr)
    {
        return REACH_ERROR;
    }
    reach_dock_now_playing_reset(now_playing);
    *out_now_playing = now_playing;
    return REACH_OK;
}

void reach_dock_now_playing_destroy(reach_dock_now_playing *now_playing)
{
    delete now_playing;
}

void reach_dock_now_playing_reset(reach_dock_now_playing *now_playing)
{
    if (now_playing == nullptr)
    {
        return;
    }
    reach_dock_now_playing_model_init(&now_playing->model);
    now_playing->layout = {};
    now_playing->pressed_action = REACH_NOW_PLAYING_ACTION_NONE;
    now_playing->observed_generation = 0;
}

void reach_dock_now_playing_sync(reach_dock_now_playing *now_playing,
                                 reach_now_playing_service *service,
                                 reach_dock_now_playing_update_result *out)
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
    reach_dock_now_playing_model next = {};
    next.visible = snapshot.has_session;
    reach_copy_utf16(next.title, 260, snapshot.title);
    reach_copy_utf16(next.artist, 260, snapshot.artist);
    next.cover_image_id = snapshot.cover_image_id;
    next.cover_accent = snapshot.cover_accent;
    next.playback = snapshot.playback;
    next.previous_enabled = snapshot.previous_enabled;
    next.play_pause_enabled = snapshot.play_pause_enabled;
    next.next_enabled = snapshot.next_enabled;
    now_playing->model = next;
    now_playing->observed_generation = snapshot.generation;
    if (now_playing->pressed_action != REACH_NOW_PLAYING_ACTION_NONE && snapshot.transport_pending)
    {
        now_playing->pressed_action = REACH_NOW_PLAYING_ACTION_NONE;
    }
    out->changed = 1;
    out->visibility_changed = was_visible != next.visible;
}

int32_t reach_dock_now_playing_visible(const reach_dock_now_playing *now_playing)
{
    return now_playing != nullptr && now_playing->model.visible;
}

float reach_dock_now_playing_desired_width(const reach_dock_now_playing *now_playing,
                                           const reach_theme *theme, float dpi_scale)
{
    if (now_playing == nullptr)
    {
        return 0.0f;
    }
    return reach_dock_now_playing_model_desired_width(&now_playing->model, theme, dpi_scale);
}

void reach_dock_now_playing_relayout(reach_dock_now_playing *now_playing, const reach_theme *theme,
                                     reach_rect_f32 bounds, float dpi_scale)
{
    if (now_playing == nullptr)
    {
        return;
    }
    reach_dock_now_playing_model model = now_playing->model;
    now_playing->layout = reach_dock_now_playing_compute_layout(&model, theme, bounds, dpi_scale);
}

int32_t reach_dock_now_playing_pointer_down(reach_dock_now_playing *now_playing, int32_t x,
                                            int32_t y)
{
    if (now_playing == nullptr)
    {
        return 0;
    }
    reach_now_playing_action action =
        reach_dock_now_playing_hit_test(&now_playing->model, &now_playing->layout, x, y);
    if (action == REACH_NOW_PLAYING_ACTION_NONE)
    {
        return now_playing->model.visible &&
               (reach_dock_now_playing_contains(now_playing->layout.previous_button, x, y) ||
                reach_dock_now_playing_contains(now_playing->layout.play_pause_button, x, y) ||
                reach_dock_now_playing_contains(now_playing->layout.next_button, x, y));
    }
    now_playing->pressed_action = action;
    return 1;
}

int32_t reach_dock_now_playing_pointer_up(reach_dock_now_playing *now_playing, int32_t x, int32_t y,
                                          reach_now_playing_action *out_action)
{
    if (out_action != nullptr)
    {
        *out_action = REACH_NOW_PLAYING_ACTION_NONE;
    }
    if (now_playing == nullptr)
    {
        return 0;
    }
    reach_now_playing_action pressed = now_playing->pressed_action;
    now_playing->pressed_action = REACH_NOW_PLAYING_ACTION_NONE;
    if (pressed == REACH_NOW_PLAYING_ACTION_NONE)
    {
        return now_playing->model.visible &&
               (reach_dock_now_playing_contains(now_playing->layout.previous_button, x, y) ||
                reach_dock_now_playing_contains(now_playing->layout.play_pause_button, x, y) ||
                reach_dock_now_playing_contains(now_playing->layout.next_button, x, y));
    }
    reach_now_playing_action released =
        reach_dock_now_playing_hit_test(&now_playing->model, &now_playing->layout, x, y);
    if (pressed == released && out_action != nullptr)
    {
        *out_action = released;
    }
    return 1;
}

int32_t reach_dock_now_playing_pointer_cancel(reach_dock_now_playing *now_playing)
{
    if (now_playing == nullptr || now_playing->pressed_action == REACH_NOW_PLAYING_ACTION_NONE)
    {
        return 0;
    }
    now_playing->pressed_action = REACH_NOW_PLAYING_ACTION_NONE;
    return 1;
}

reach_result
reach_dock_now_playing_append_render_commands(reach_dock_now_playing *now_playing,
                                              const reach_dock_now_playing_render_context *ctx,
                                              reach_render_command_buffer *out_commands)
{
    if (now_playing == nullptr || ctx == nullptr || ctx->theme == nullptr ||
        out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    float reveal_width = ctx->reveal_width;
    if (reveal_width < 0.0f)
    {
        reveal_width = 0.0f;
    }
    if (reveal_width > now_playing->layout.bounds.width)
    {
        reveal_width = now_playing->layout.bounds.width;
    }
    reach_dock_now_playing_model model = now_playing->model;
    reach_dock_now_playing_render_input input = {};
    input.theme = ctx->theme;
    input.model = &model;
    input.layout = &now_playing->layout;
    input.reveal_width = reveal_width;
    return reach_dock_now_playing_build_render_commands(&input, out_commands);
}
