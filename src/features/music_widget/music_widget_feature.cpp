#include "reach/features/music_widget.h"
#include "reach/support/animation.h"

#include <math.h>

static const uint16_t REACH_MUSIC_WIDGET_DEFAULT_TITLE[] = {'M', 'e', 'd', 'i', 'a', 0};
static const uint16_t REACH_MUSIC_WIDGET_COVER[] = {'M', 0};

static const float REACH_MUSIC_WIDGET_BG_CONTRAST = 1.20f;

static reach_rect_f32 reach_music_widget_rect(float x, float y, float width, float height)
{
    reach_rect_f32 rect = {};
    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;
    return rect;
}

static int32_t reach_music_widget_rect_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return (float)x >= rect.x && (float)x <= rect.x + rect.width && (float)y >= rect.y &&
           (float)y <= rect.y + rect.height;
}

static void reach_music_widget_push_rect(reach_render_command_buffer *commands, reach_rect_f32 rect,
                                         reach_color color, float radius)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = rect;
    command.color = color;
    command.radius = radius;
    reach_render_command_buffer_push(commands, &command);
}

static void reach_music_widget_push_blurred_image(reach_render_command_buffer *commands,
                                                  const reach_rect_f32 *rect, uint64_t icon_id,
                                                  float radius, float blur_radius,
                                                  const reach_rect_f32 *clip_rect,
                                                  float clip_radius, float image_contrast)
{
    if (icon_id == 0)
    {
        return;
    }

    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_BLURRED_IMAGE;
    command.rect = *rect;
    command.icon_id = icon_id;
    command.icon_crop_to_fill = 1;
    command.radius = radius;
    command.blur_radius = blur_radius;
    command.image_contrast = image_contrast;
    command.color.a = 1.0f;

    if (clip_rect != nullptr && clip_rect->width > 0.0f && clip_rect->height > 0.0f)
    {
        command.has_clip_rect = 1;
        command.clip_rect = *clip_rect;
        command.clip_radius = clip_radius;
    }

    reach_render_command_buffer_push(commands, &command);
}

static void reach_music_widget_push_text(reach_render_command_buffer *commands, reach_rect_f32 rect,
                                         const uint16_t *text, float text_size, int32_t text_weight,
                                         int32_t text_alignment, reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = rect;
    command.color = color;
    command.text_size = text_size;
    command.text_weight = text_weight;
    command.text_alignment = text_alignment;
    command.text_ellipsis = 1;
    reach_copy_utf16(command.text, 260, text != nullptr ? text : REACH_MUSIC_WIDGET_DEFAULT_TITLE);
    reach_render_command_buffer_push(commands, &command);
}

static reach_color reach_music_widget_play_pause_background(const reach_music_widget_model *model,
                                                            const reach_theme *theme)
{
    if (model != nullptr && model->cover_accent.a > 0.0f)
    {
        reach_color color = model->cover_accent;
        color.a = color.a < 0.78f ? 0.78f : color.a;
        return color;
    }

    reach_color fallback = {};
    return theme != nullptr ? theme->music_widget_control_background : fallback;
}

static reach_color reach_music_widget_background_overlay(const reach_theme *theme)
{
    reach_color color = {};
    if (theme != nullptr)
    {
        color = theme->music_widget_background;
    }
    color.r *= 0.34f;
    color.g *= 0.34f;
    color.b *= 0.34f;
    if (color.a < 0.48f)
    {
        color.a = 0.48f;
    }
    return color;
}

static reach_rect_f32 reach_music_widget_centered_circle_bounds(reach_rect_f32 rect)
{
    float size = rect.height * 1.15f;
    if (size > rect.width)
    {
        size = rect.width;
    }
    return reach_music_widget_rect(rect.x + (rect.width - size) * 0.5f,
                                   rect.y + (rect.height - size) * 0.5f, size, size);
}

void reach_music_widget_model_init(reach_music_widget_model *model)
{
    if (model == nullptr)
    {
        return;
    }

    *model = {};
    reach_copy_utf16(model->title, 260, REACH_MUSIC_WIDGET_DEFAULT_TITLE);
    model->artist[0] = 0;
    model->playback = REACH_MEDIA_PLAYBACK_UNKNOWN;
    model->previous_enabled = 1;
    model->next_enabled = 1;
}

float reach_music_widget_desired_width(const reach_music_widget_model *model,
                                       const reach_theme *theme, float dpi_scale)
{
    if (model == nullptr || !model->visible)
    {
        return 0.0f;
    }

    const reach_theme *actual = theme != nullptr ? theme : reach_theme_default();
    return actual->music_widget_width * dpi_scale;
}

reach_music_widget_layout reach_music_widget_compute_layout(const reach_music_widget_model *model,
                                                            const reach_theme *theme,
                                                            reach_rect_f32 bounds, float dpi_scale)
{
    reach_music_widget_layout layout = {};
    if (model == nullptr || !model->visible || bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return layout;
    }

    const reach_theme *actual = theme != nullptr ? theme : reach_theme_default();
    float padding = actual->music_widget_padding * dpi_scale;
    float cover_gap = actual->music_widget_gap * dpi_scale;
    float text_gap = actual->music_widget_text_gap * dpi_scale;
    float button_gap = actual->music_widget_control_gap * dpi_scale;
    float play_button_size = actual->music_widget_play_button_width * dpi_scale;
    float prev_next_size = actual->music_widget_prev_next_button_width * dpi_scale;

    layout.bounds = bounds;
    float cover_size = bounds.height;
    if (cover_size > bounds.width * 0.42f)
    {
        cover_size = bounds.width * 0.42f;
    }
    layout.cover = reach_music_widget_rect(bounds.x, bounds.y, cover_size, cover_size);

    float total_button_row_width =
        prev_next_size + button_gap + play_button_size + button_gap + prev_next_size;
    float min_required_width = cover_size + cover_gap + total_button_row_width + padding;
    if (bounds.width < min_required_width)
    {
        layout.previous_button = {};
        layout.play_pause_button = {};
        layout.next_button = {};
        layout.title = {};
        layout.artist = {};
        return layout;
    }

    float button_start_x = bounds.x + bounds.width - padding - total_button_row_width;
    float text_end_x = button_start_x;
    float text_start_x = layout.cover.x + layout.cover.width + cover_gap;
    float text_width = text_end_x - text_start_x;
    if (text_width < 0.0f)
    {
        text_width = 0.0f;
    }

    float title_height = actual->music_widget_title_text_size * dpi_scale;
    float artist_height = actual->music_widget_artist_text_size * dpi_scale;
    float total_text_height = title_height + artist_height + text_gap;

    float text_x = text_start_x;
    float text_y = bounds.y + (bounds.height - total_text_height) * 0.5f;
    if (text_y < bounds.y + padding)
    {
        text_y = bounds.y + padding;
    }
    layout.title = reach_music_widget_rect(text_x, text_y, text_width, title_height);
    layout.artist = reach_music_widget_rect(text_x, text_y + title_height + text_gap, text_width,
                                            artist_height);

    layout.previous_button =
        reach_music_widget_rect(button_start_x, bounds.y + (bounds.height - prev_next_size) * 0.5f,
                                prev_next_size, prev_next_size);
    layout.play_pause_button = reach_music_widget_rect(
        layout.previous_button.x + prev_next_size + button_gap,
        bounds.y + (bounds.height - play_button_size) * 0.5f, play_button_size, play_button_size);
    layout.next_button = reach_music_widget_rect(
        layout.play_pause_button.x + play_button_size + button_gap,
        bounds.y + (bounds.height - prev_next_size) * 0.5f, prev_next_size, prev_next_size);

    return layout;
}

reach_music_widget_action_type reach_music_widget_hit_test(const reach_music_widget_model *model,
                                                           const reach_music_widget_layout *layout,
                                                           int32_t x, int32_t y)
{
    if (model == nullptr || layout == nullptr || layout->bounds.width <= 0.0f ||
        layout->bounds.height <= 0.0f || !reach_music_widget_rect_contains(layout->bounds, x, y))
    {
        return REACH_MUSIC_WIDGET_ACTION_NONE;
    }
    if (model->previous_enabled && reach_music_widget_rect_contains(layout->previous_button, x, y))
    {
        return REACH_MUSIC_WIDGET_ACTION_PREVIOUS;
    }
    if (reach_music_widget_rect_contains(layout->play_pause_button, x, y))
    {
        return REACH_MUSIC_WIDGET_ACTION_PLAY_PAUSE;
    }
    if (model->next_enabled && reach_music_widget_rect_contains(layout->next_button, x, y))
    {
        return REACH_MUSIC_WIDGET_ACTION_NEXT;
    }
    return REACH_MUSIC_WIDGET_ACTION_NONE;
}

reach_result reach_music_widget_build_render_commands(const reach_music_widget_render_input *input,
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

    const reach_theme *theme = input->theme;
    float radius = reach_theme_music_widget_corner_radius(theme, input->layout->bounds.height);

    const reach_music_widget_bg_animation *animation = input->animation;
    if (animation != nullptr && animation->active)
    {
        float bg_width = input->layout->bounds.width * 1.5f;
        float bg_height = input->layout->bounds.height * 1.5f;
        float offset_x = (bg_width - input->layout->bounds.width) * 0.5f;
        float offset_y = (bg_height - input->layout->bounds.height) * 0.5f;

        reach_rect_f32 inflated_bg = {};
        inflated_bg.x = input->layout->bounds.x - offset_x + animation->offset.value.x;
        inflated_bg.y = input->layout->bounds.y - offset_y + animation->offset.value.y;
        inflated_bg.width = bg_width;
        inflated_bg.height = bg_height;

        reach_music_widget_push_blurred_image(
            out_commands, &inflated_bg, input->model->cover_icon_id, radius,
            input->layout->bounds.height * 0.28f, &input->layout->bounds, radius,
            REACH_MUSIC_WIDGET_BG_CONTRAST);
    }
    else
    {
        reach_music_widget_push_blurred_image(
            out_commands, &input->layout->bounds, input->model->cover_icon_id, radius,
            input->layout->bounds.height * 0.28f, nullptr, 0.0f, 0.0f);
    }

    reach_music_widget_push_rect(out_commands, input->layout->bounds,
                                 reach_music_widget_background_overlay(theme), radius);
    reach_music_widget_push_rect(out_commands, input->layout->cover,
                                 theme->music_widget_cover_background, radius);

    if (input->model->cover_icon_id != 0)
    {
        reach_render_command icon = {};
        icon.type = REACH_RENDER_COMMAND_ICON;
        icon.rect = input->layout->cover;
        icon.icon_id = input->model->cover_icon_id;
        icon.icon_crop_to_fill = 1;
        icon.radius = radius;
        icon.corner_mask = REACH_RENDER_CORNER_TOP_LEFT | REACH_RENDER_CORNER_BOTTOM_LEFT;
        icon.color.a = 1.0f;
        reach_render_command_buffer_push(out_commands, &icon);
    }
    else
    {
        reach_music_widget_push_text(out_commands, input->layout->cover, REACH_MUSIC_WIDGET_COVER,
                                     theme->music_widget_title_text_size * 1.2f,
                                     REACH_TEXT_WEIGHT_DEMIBOLD, input->text_alignment_center,
                                     theme->music_widget_title);
    }

    reach_music_widget_push_text(out_commands, input->layout->title, input->model->title,
                                 theme->music_widget_title_text_size, REACH_TEXT_WEIGHT_DEMIBOLD,
                                 input->text_alignment_leading, theme->music_widget_title);
    reach_music_widget_push_text(out_commands, input->layout->artist, input->model->artist,
                                 theme->music_widget_artist_text_size, REACH_TEXT_WEIGHT_NORMAL,
                                 input->text_alignment_leading, theme->music_widget_artist_text);

    reach_vector_icon_id icon_ids[3] = {REACH_VECTOR_ICON_PREVIOUS,
                                        input->model->playback == REACH_MEDIA_PLAYBACK_PLAYING
                                            ? REACH_VECTOR_ICON_PAUSE
                                            : REACH_VECTOR_ICON_PLAY,
                                        REACH_VECTOR_ICON_NEXT};
    reach_rect_f32 play_pause_background_rect =
        reach_music_widget_centered_circle_bounds(input->layout->play_pause_button);
    reach_color play_pause_background =
        reach_music_widget_play_pause_background(input->model, theme);
    reach_color icon_color = theme->music_widget_control_text;
    float icon_scale = 0.70f;

    for (size_t index = 0; index < 3; ++index)
    {
        reach_rect_f32 button = index == 0 ? input->layout->previous_button
                                           : (index == 1 ? input->layout->play_pause_button
                                                         : input->layout->next_button);
        float icon_size = button.height * icon_scale;
        float icon_x = button.x + (button.width - icon_size) * 0.5f;
        float icon_y = button.y + (button.height - icon_size) * 0.5f;
        reach_rect_f32 icon_rect = reach_music_widget_rect(icon_x, icon_y, icon_size, icon_size);
        reach_rect_f32 background_rect = index == 1 ? play_pause_background_rect : button;
        float background_radius_ratio = index == 1 ? 0.5f : (index == 0 ? 0.35f : 0.45f);
        reach_color background =
            index == 1 ? play_pause_background : theme->music_widget_control_background;
        reach_music_widget_push_rect(out_commands, background_rect, background,
                                     background_rect.height * background_radius_ratio);

        reach_render_command icon = {};
        icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        icon.rect = icon_rect;
        icon.icon_id = icon_ids[index];
        icon.color = icon_color;
        if ((index == 0 && !input->model->previous_enabled) ||
            (index == 2 && !input->model->next_enabled))
        {
            icon.color.a *= 0.30f;
        }
        reach_render_command_buffer_push(out_commands, &icon);
    }

    return REACH_OK;
}

static int32_t reach_music_widget_vec2_animation_active(const reach_vec2_animation *animation)
{
    return animation != nullptr && animation->elapsed_seconds < animation->duration_seconds;
}

static const int32_t REACH_MUSIC_WIDGET_BG_SEGMENT_COUNT = 6;

static const reach_vec2 REACH_MUSIC_WIDGET_BG_TARGETS[REACH_MUSIC_WIDGET_BG_SEGMENT_COUNT] = {
    {-0.80f, -0.30f}, {0.40f, -0.70f}, {0.90f, 0.15f},
    {-0.20f, 0.75f},  {-0.95f, 0.25f}, {0.35f, 0.90f},
};

static void reach_music_widget_clamp_offset(reach_vec2 *out, float max_x, float max_y)
{
    if (out->x > max_x)
        out->x = max_x;
    else if (out->x < -max_x)
        out->x = -max_x;
    if (out->y > max_y)
        out->y = max_y;
    else if (out->y < -max_y)
        out->y = -max_y;
}

static void reach_music_widget_compute_segment_target(reach_vec2 *out, int32_t segment_index,
                                                      float max_offset_x, float max_offset_y)
{
    reach_vec2 target = REACH_MUSIC_WIDGET_BG_TARGETS[segment_index];
    out->x = target.x * max_offset_x;
    out->y = target.y * max_offset_y;
    reach_music_widget_clamp_offset(out, max_offset_x, max_offset_y);
}

int32_t reach_music_widget_bg_animation_update(reach_music_widget_bg_animation *animation,
                                               const reach_music_widget_model *model,
                                               reach_rect_f32 bounds, double delta_seconds)
{
    if (animation == nullptr || model == nullptr)
    {
        return 0;
    }

    if (!model->visible || bounds.width <= 0.0f || bounds.height <= 0.0f ||
        model->cover_icon_id == 0)
    {
        int32_t was_active = animation->active;
        animation->active = 0;
        return was_active;
    }

    animation->active = 1;

    static const double REACH_MUSIC_WIDGET_BG_PLAYING_SECONDS = 3.0;
    static const double REACH_MUSIC_WIDGET_BG_PAUSED_SECONDS =
        REACH_MUSIC_WIDGET_BG_PLAYING_SECONDS * 1.5;

    double duration = model->playback == REACH_MEDIA_PLAYBACK_PLAYING
                          ? REACH_MUSIC_WIDGET_BG_PLAYING_SECONDS
                          : REACH_MUSIC_WIDGET_BG_PAUSED_SECONDS;

    int32_t cover_changed =
        !animation->initialized ||
        (animation->current_cover_id != 0 && animation->current_cover_id != model->cover_icon_id);

    float bg_width = bounds.width * 1.5f;
    float bg_height = bounds.height * 1.5f;
    float max_offset_x = (bg_width - bounds.width) * 0.5f;
    float max_offset_y = (bg_height - bounds.height) * 0.5f;

    if (cover_changed)
    {
        animation->current_cover_id = model->cover_icon_id;
        animation->segment_index = 0;
        animation->initialized = 1;
        animation->widget_width = bounds.width;
        animation->widget_height = bounds.height;

        reach_vec2_animation *anim = &animation->offset;
        reach_vec2 start = {0.0f, 0.0f};
        reach_vec2 end = {};
        reach_music_widget_compute_segment_target(&end, 0, max_offset_x, max_offset_y);

        anim->from = start;
        anim->to = end;
        anim->value = start;
        anim->elapsed_seconds = 0.0;
        anim->duration_seconds = duration;
        anim->easing = REACH_EASING_EASE_IN_OUT;

        return 1;
    }

    float old_width = animation->widget_width;
    float old_height = animation->widget_height;

    int32_t bounds_changed =
        animation->initialized && (old_width != bounds.width || old_height != bounds.height);

    if (bounds_changed)
    {
        reach_vec2_animation *anim = &animation->offset;
        float current_x = anim->value.x;
        float current_y = anim->value.y;

        float prev_max_x = (old_width * 1.5f - old_width) * 0.5f;
        float prev_max_y = (old_height * 1.5f - old_height) * 0.5f;

        float normalized_x = prev_max_x > 0.0f ? current_x / prev_max_x : 0.0f;
        float normalized_y = prev_max_y > 0.0f ? current_y / prev_max_y : 0.0f;

        reach_vec2 new_from = {normalized_x * max_offset_x, normalized_y * max_offset_y};
        reach_music_widget_clamp_offset(&new_from, max_offset_x, max_offset_y);

        anim->from = new_from;
        anim->to = new_from;
        anim->value = new_from;
        anim->elapsed_seconds = 0.0;
        anim->duration_seconds = duration;
        anim->easing = REACH_EASING_EASE_IN_OUT;

        animation->widget_width = bounds.width;
        animation->widget_height = bounds.height;
    }

    animation->widget_width = bounds.width;
    animation->widget_height = bounds.height;

    reach_vec2_animation *anim = &animation->offset;

    reach_vec2_animation_update(anim, delta_seconds);

    if (reach_music_widget_vec2_animation_active(anim))
    {
        return 1;
    }

    animation->segment_index = (animation->segment_index + 1) % REACH_MUSIC_WIDGET_BG_SEGMENT_COUNT;

    reach_vec2 new_target = {};
    reach_music_widget_compute_segment_target(&new_target, animation->segment_index, max_offset_x,
                                              max_offset_y);

    float prev_duration = anim->duration_seconds;
    reach_vec2_animation_start(anim, anim->value, new_target, duration);

    if (fabsf(prev_duration - duration) >= 0.01f)
    {
        anim->from = anim->value;
        anim->to = new_target;
        anim->value = anim->value;
        anim->elapsed_seconds = 0.0;
        anim->duration_seconds = duration;
        anim->easing = REACH_EASING_EASE_IN_OUT;
    }

    return 1;
}
