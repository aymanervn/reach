#include "reach/features/music_widget.h"

static const uint16_t REACH_MUSIC_WIDGET_DEFAULT_TITLE[] = {'M', 'e', 'd', 'i', 'a', 0};
static const uint16_t REACH_MUSIC_WIDGET_PREVIOUS[] = {'<', 0};
static const uint16_t REACH_MUSIC_WIDGET_PLAY_PAUSE[] = {'|', '|', 0};
static const uint16_t REACH_MUSIC_WIDGET_PLAY[] = {'|', '>', 0};
static const uint16_t REACH_MUSIC_WIDGET_NEXT[] = {'>', 0};
static const uint16_t REACH_MUSIC_WIDGET_COVER[] = {'M', 0};

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

static void reach_music_widget_push_text(reach_render_command_buffer *commands,
                                         reach_rect_f32 rect, const uint16_t *text,
                                         float text_size, int32_t text_weight,
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

void reach_music_widget_model_init(reach_music_widget_model *model)
{
    if (model == nullptr)
    {
        return;
    }

    *model = {};
    reach_copy_utf16(model->title, 260, REACH_MUSIC_WIDGET_DEFAULT_TITLE);
    model->playback = REACH_MEDIA_PLAYBACK_UNKNOWN;
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
                                                            reach_rect_f32 bounds,
                                                            float dpi_scale)
{
    reach_music_widget_layout layout = {};
    if (model == nullptr || !model->visible || bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return layout;
    }

    const reach_theme *actual = theme != nullptr ? theme : reach_theme_default();
    float padding = actual->music_widget_padding * dpi_scale;
    float gap = actual->music_widget_gap * dpi_scale;
    float control_gap = actual->music_widget_control_gap * dpi_scale;
    float control_width = actual->music_widget_control_width * dpi_scale;
    float control_height = actual->music_widget_control_height * dpi_scale;

    layout.bounds = bounds;
    float cover_size = bounds.height;
    layout.cover = reach_music_widget_rect(bounds.x, bounds.y, cover_size, cover_size);

    float content_x = layout.cover.x + layout.cover.width + gap;
    float content_right = bounds.x + bounds.width - padding;
    float content_width = content_right - content_x;
    if (content_width < 0.0f)
    {
        content_width = 0.0f;
    }

    float title_height = (bounds.height - padding * 2.0f - control_height) * 0.55f;
    if (title_height < control_height)
    {
        title_height = control_height;
    }
    layout.title = reach_music_widget_rect(content_x, bounds.y + padding, content_width,
                                           title_height);

    float controls_y = bounds.y + bounds.height - padding - control_height;
    layout.previous_button = reach_music_widget_rect(content_x, controls_y, control_width,
                                                     control_height);
    layout.play_pause_button =
        reach_music_widget_rect(layout.previous_button.x + control_width + control_gap, controls_y,
                                control_width, control_height);
    layout.next_button =
        reach_music_widget_rect(layout.play_pause_button.x + control_width + control_gap,
                                controls_y, control_width, control_height);

    return layout;
}

reach_music_widget_action_type reach_music_widget_hit_test(const reach_music_widget_layout *layout,
                                                           int32_t x, int32_t y)
{
    if (layout == nullptr || layout->bounds.width <= 0.0f || layout->bounds.height <= 0.0f ||
        !reach_music_widget_rect_contains(layout->bounds, x, y))
    {
        return REACH_MUSIC_WIDGET_ACTION_NONE;
    }
    if (reach_music_widget_rect_contains(layout->previous_button, x, y))
    {
        return REACH_MUSIC_WIDGET_ACTION_PREVIOUS;
    }
    if (reach_music_widget_rect_contains(layout->play_pause_button, x, y))
    {
        return REACH_MUSIC_WIDGET_ACTION_PLAY_PAUSE;
    }
    if (reach_music_widget_rect_contains(layout->next_button, x, y))
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
    float radius =
        reach_theme_music_widget_corner_radius(theme, input->layout->bounds.height);

    reach_music_widget_push_rect(out_commands, input->layout->bounds,
                                 theme->music_widget_background, radius);
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

    reach_rect_f32 controls[3] = {input->layout->previous_button,
                                  input->layout->play_pause_button,
                                  input->layout->next_button};
    const uint16_t *play_pause_label =
        input->model->playback == REACH_MEDIA_PLAYBACK_PLAYING ? REACH_MUSIC_WIDGET_PLAY_PAUSE
                                                               : REACH_MUSIC_WIDGET_PLAY;
    const uint16_t *labels[3] = {REACH_MUSIC_WIDGET_PREVIOUS, play_pause_label,
                                 REACH_MUSIC_WIDGET_NEXT};
    for (size_t index = 0; index < 3; ++index)
    {
        reach_music_widget_push_rect(out_commands, controls[index],
                                     theme->music_widget_control_background,
                                     controls[index].height * 0.45f);
        reach_music_widget_push_text(out_commands, controls[index], labels[index],
                                     theme->music_widget_control_text_size,
                                     REACH_TEXT_WEIGHT_DEMIBOLD, input->text_alignment_center,
                                     theme->music_widget_control_text);
    }

    return REACH_OK;
}
