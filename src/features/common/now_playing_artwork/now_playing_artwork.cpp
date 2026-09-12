#include "reach/features/common/now_playing_artwork.h"

static const float REACH_NOW_PLAYING_ARTWORK_BACKGROUND_SCALE = 1.5f;
static const float REACH_NOW_PLAYING_ARTWORK_BLUR = 0.45f;
static const float REACH_NOW_PLAYING_ARTWORK_CONTRAST = 1.20f;

static reach_result reach_now_playing_artwork_push_rect(reach_render_command_buffer *commands,
                                                        reach_rect_f32 rect, reach_color color,
                                                        float radius)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = rect;
    command.color = color;
    command.radius = radius;
    return reach_render_command_buffer_push(commands, &command);
}

reach_result
reach_now_playing_artwork_build_render_commands(const reach_now_playing_artwork_render_input *input,
                                                reach_render_command_buffer *out_commands)
{
    if (input == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (input->bounds.width <= 0.0f || input->bounds.height <= 0.0f)
    {
        return REACH_OK;
    }
    if (input->cover_image_id == 0)
    {
        return reach_now_playing_artwork_push_rect(out_commands, input->bounds,
                                                   input->base_background, input->radius);
    }

    float width = input->bounds.width * REACH_NOW_PLAYING_ARTWORK_BACKGROUND_SCALE;
    float height = input->bounds.height * REACH_NOW_PLAYING_ARTWORK_BACKGROUND_SCALE;
    reach_render_command blurred = {};
    blurred.type = REACH_RENDER_COMMAND_BLURRED_IMAGE;
    blurred.rect = {input->bounds.x - (width - input->bounds.width) * 0.5f,
                    input->bounds.y - (height - input->bounds.height) * 0.5f, width, height};
    blurred.icon_id = input->cover_image_id;
    blurred.icon_crop_to_fill = 1;
    blurred.radius = input->radius;
    blurred.blur_radius = input->bounds.height * REACH_NOW_PLAYING_ARTWORK_BLUR;
    blurred.image_contrast = REACH_NOW_PLAYING_ARTWORK_CONTRAST;
    blurred.color.a = 1.0f;
    blurred.has_clip_rect = 1;
    blurred.clip_rect = input->bounds;
    blurred.clip_radius = input->radius;
    reach_result result = reach_render_command_buffer_push(out_commands, &blurred);
    if (result != REACH_OK)
    {
        return result;
    }

    if (input->cover_bounds.width > 0.0f && input->cover_bounds.height > 0.0f)
    {
        reach_render_command cover = {};
        cover.type = REACH_RENDER_COMMAND_ICON;
        cover.rect = input->cover_bounds;
        cover.icon_id = input->cover_image_id;
        cover.icon_crop_to_fill = 1;
        cover.radius = input->cover_radius;
        cover.corner_mask = input->cover_corner_mask;
        cover.icon_fade_start = input->cover_fade_start;
        cover.color.a = 1.0f;
        result = reach_render_command_buffer_push(out_commands, &cover);
        if (result != REACH_OK)
        {
            return result;
        }
    }

    return reach_now_playing_artwork_push_rect(out_commands, input->bounds, input->overlay,
                                               input->radius);
}
