#ifndef REACH_CORE_RENDER_COMMANDS_H
#define REACH_CORE_RENDER_COMMANDS_H

#include "reach/core/theme.h"
#include "reach/core/animated_rect.h"
#include "reach/core/ui_layout.h"
#include "reach/support/motion.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_MAX_RENDER_COMMANDS (REACH_MAX_DOCK_ITEMS * 2 + REACH_MAX_DOCK_RUNNING_APPS + 16)
#define REACH_MAX_RENDER_GROUPS (REACH_MAX_DOCK_ITEMS + 32)

    typedef enum reach_render_clip_edge
    {
        REACH_RENDER_CLIP_LEFT,
        REACH_RENDER_CLIP_TOP,
        REACH_RENDER_CLIP_RIGHT,
        REACH_RENDER_CLIP_BOTTOM,
        REACH_RENDER_CLIP_EDGE_COUNT
    } reach_render_clip_edge;

    typedef enum reach_render_color_channel
    {
        REACH_RENDER_COLOR_RED,
        REACH_RENDER_COLOR_GREEN,
        REACH_RENDER_COLOR_BLUE,
        REACH_RENDER_COLOR_ALPHA,
        REACH_RENDER_COLOR_CHANNEL_COUNT
    } reach_render_color_channel;

    typedef struct reach_render_group
    {
        uint64_t id;
        uint64_t parent_id;
        reach_rect_f32 bounds;
        reach_motion x;
        reach_motion y;
        reach_motion opacity;
        reach_motion scale;
        reach_motion scale_y;
        int32_t has_clip;
        reach_rect_f32 clip;
        float clip_radius;
        reach_motion clip_insets[REACH_RENDER_CLIP_EDGE_COUNT];
        int32_t has_tint;
        reach_motion tint[REACH_RENDER_COLOR_CHANNEL_COUNT];
        reach_motion tint_rgb_mix;
        reach_color tint_rgb_target;
    } reach_render_group;

    typedef struct reach_render_stretch
    {
        float left;
        float top;
        float right;
        float bottom;
        float anchor_x;
        float anchor_y;
        reach_motion width;
        reach_motion height;
        float focal_left;
        float focal_right;
        reach_motion focal_x;
    } reach_render_stretch;

    typedef enum reach_render_command_type
    {
        REACH_RENDER_COMMAND_NONE = 0,
        REACH_RENDER_COMMAND_RECT = 1,
        REACH_RENDER_COMMAND_TEXT = 2,
        REACH_RENDER_COMMAND_ICON = 3,
        REACH_RENDER_COMMAND_TRIANGLE = 7,
        REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT = 9,
        REACH_RENDER_COMMAND_VECTOR_ICON = 10,
        REACH_RENDER_COMMAND_CLIPPED_ROUNDED_RECT = 11,
        REACH_RENDER_COMMAND_ICON_TINT = 13,
        REACH_RENDER_COMMAND_BLURRED_IMAGE = 14,
        REACH_RENDER_COMMAND_TEXTBOX = 15,
        REACH_RENDER_COMMAND_ARC_STROKE = 16,
        REACH_RENDER_COMMAND_SHADOW = 17
    } reach_render_command_type;

    typedef enum reach_render_corner_mask
    {
        REACH_RENDER_CORNER_TOP_LEFT = 1,
        REACH_RENDER_CORNER_TOP_RIGHT = 2,
        REACH_RENDER_CORNER_BOTTOM_RIGHT = 4,
        REACH_RENDER_CORNER_BOTTOM_LEFT = 8,
        REACH_RENDER_CORNER_ALL = 15
    } reach_render_corner_mask;

    typedef enum reach_notch_side
    {
        REACH_NOTCH_SIDE_BOTTOM = 0,
        REACH_NOTCH_SIDE_TOP = 1
    } reach_notch_side;

    typedef enum reach_vector_icon_id
    {
        REACH_VECTOR_ICON_NONE = 0,
        REACH_VECTOR_ICON_POWER = 1,
        REACH_VECTOR_ICON_LOCK = 2,
        REACH_VECTOR_ICON_SLEEP = 3,
        REACH_VECTOR_ICON_RESTART = 4,
        REACH_VECTOR_ICON_SHUTDOWN = 5,
        REACH_VECTOR_ICON_SIGN_OUT = 6,
        REACH_VECTOR_ICON_ARROW_UP = 7,
        REACH_VECTOR_ICON_QUICK_SETTINGS = 8,
        REACH_VECTOR_ICON_VOLUME_ZERO = 9,
        REACH_VECTOR_ICON_VOLUME_LOW = 10,
        REACH_VECTOR_ICON_VOLUME_HIGH = 11,
        REACH_VECTOR_ICON_ARROW_DOWN = 12,
        REACH_VECTOR_ICON_CHECK = 13,
        REACH_VECTOR_ICON_ETHERNET = 14,
        REACH_VECTOR_ICON_WIFI_LOW = 15,
        REACH_VECTOR_ICON_WIFI_MEDIUM = 16,
        REACH_VECTOR_ICON_WIFI_HIGH = 17,
        REACH_VECTOR_ICON_PROJECT = 19,
        REACH_VECTOR_ICON_BRIGHTNESS = 20,
        REACH_VECTOR_ICON_BLUETOOTH_ON = 21,
        REACH_VECTOR_ICON_BLUETOOTH_OFF = 22,
        REACH_VECTOR_ICON_NO_INTERNET = 23,
        REACH_VECTOR_ICON_FOLDER = 24,
        REACH_VECTOR_ICON_PHOTO = 25,
        REACH_VECTOR_ICON_VIDEO = 26,
        REACH_VECTOR_ICON_MUSIC = 27,
        REACH_VECTOR_ICON_DOCUMENT = 28,
        REACH_VECTOR_ICON_FILE = 29,
        REACH_VECTOR_ICON_PLAY = 30,
        REACH_VECTOR_ICON_PAUSE = 31,
        REACH_VECTOR_ICON_PREVIOUS = 32,
        REACH_VECTOR_ICON_NEXT = 33,
        REACH_VECTOR_ICON_MINIMIZE = 34,
        REACH_VECTOR_ICON_CLOSE = 35,
        REACH_VECTOR_ICON_RESIZE = 36,
        REACH_VECTOR_ICON_SETTINGS = 37,
        REACH_VECTOR_ICON_SEARCH = 38,
        REACH_VECTOR_ICON_EXECUTABLE = 39,
        REACH_VECTOR_ICON_MENU = 40,
        REACH_VECTOR_ICON_MUSIC_NOTE = 41
    } reach_vector_icon_id;

    typedef enum reach_text_weight
    {
        REACH_TEXT_WEIGHT_EXTRALIGHT = 100,
        REACH_TEXT_WEIGHT_LIGHT = 200,
        REACH_TEXT_WEIGHT_NORMAL = 400,
        REACH_TEXT_WEIGHT_SEMIBOLD = 500,
        REACH_TEXT_WEIGHT_DEMIBOLD = 600,
        REACH_TEXT_WEIGHT_BOLD = 700,
        REACH_TEXT_WEIGHT_EXTRABOLD = 800,
        REACH_TEXT_WEIGHT_BLACK = 900
    } reach_text_weight;

    typedef enum reach_text_alignment
    {
        REACH_TEXT_ALIGNMENT_LEADING = 0,
        REACH_TEXT_ALIGNMENT_CENTER = 1,
        REACH_TEXT_ALIGNMENT_TRAILING = 2
    } reach_text_alignment;

    typedef struct reach_render_command
    {
        reach_render_command_type type;
        uint64_t group_id;
        reach_rect_f32 rect;
        reach_rect_f32 clip_rect;
        reach_rect_f32 scissor_rect;
        int32_t has_scissor;
        reach_color color;
        float radius;
        float blur_radius;
        float shadow_offset_x;
        float shadow_offset_y;
        float image_contrast;
        float icon_fade_start;
        int32_t has_clip_rect;
        float clip_radius;
        float stroke_width;
        float arc_sweep;
        uint64_t icon_id;
        int32_t icon_crop_to_fill;
        int32_t corner_mask;
        int32_t text_weight;
        float text_size;
        float notch_center_x;
        float notch_width;
        float notch_height;
        int32_t notch_side;
        int32_t text_alignment;
        int32_t text_ellipsis;
        uint16_t text[260];

        reach_color text_color;
        reach_color placeholder_color;
        reach_color selection_color;
        int32_t caret_index;
        int32_t caret_visible;
        int32_t selection_start;
        int32_t selection_end;
        uint16_t placeholder[128];
    } reach_render_command;

    typedef struct reach_render_command_buffer
    {
        reach_render_command commands[REACH_MAX_RENDER_COMMANDS];
        size_t count;
        reach_rect_f32 scissor_rect;
        int32_t has_scissor;
        reach_rect_f32 content_rect;
        reach_transform_f32 content_transform;
        reach_render_group groups[REACH_MAX_RENDER_GROUPS];
        size_t group_count;
        uint64_t current_group_id;
        int32_t has_presentation;
        reach_render_group presentation;
        int32_t requires_content_sampling;
    } reach_render_command_buffer;

    void reach_render_command_buffer_clear(reach_render_command_buffer *buffer);
    int32_t reach_render_command_buffer_motion_active(const reach_render_command_buffer *buffer);
    reach_result
    reach_render_command_buffer_validate_groups(const reach_render_command_buffer *buffer);
    const reach_render_group *
    reach_render_command_buffer_group(const reach_render_command_buffer *buffer, uint64_t id);
    reach_transform_f32
    reach_render_command_buffer_group_transform_at(const reach_render_command_buffer *buffer,
                                                   uint64_t id, double now_seconds);
    float reach_render_command_buffer_group_opacity_at(const reach_render_command_buffer *buffer,
                                                       uint64_t id, double now_seconds);
    int32_t reach_render_command_buffer_group_clip_at(const reach_render_command_buffer *buffer,
                                                      uint64_t id, double now_seconds,
                                                      reach_rect_f32 *out_clip);
    int32_t
    reach_render_command_buffer_group_rectangular_clip_at(const reach_render_command_buffer *buffer,
                                                          uint64_t id, double now_seconds,
                                                          reach_rect_f32 *out_clip);
    reach_rect_f32 reach_render_group_clip_at(const reach_render_group *group, double now_seconds);
    reach_result reach_render_group_clip_motion(const reach_render_group *group,
                                                reach_rect_motion *out_clip);
    int32_t reach_render_group_equal(const reach_render_group *first,
                                     const reach_render_group *second);
    reach_color reach_render_group_tint_at(const reach_render_group *group, double now_seconds);
    reach_motion reach_render_group_y_scale(const reach_render_group *group);
    reach_point_f32 reach_render_group_scale_at(const reach_render_group *group,
                                                double now_seconds);
    size_t reach_render_stretch_patch_count(const reach_render_stretch *stretch);
    reach_result reach_render_stretch_patch(const reach_render_group *source,
                                            const reach_render_stretch *stretch, size_t patch_index,
                                            reach_render_group *out_patch);
    reach_result reach_render_push_stretched_chrome(reach_render_command_buffer *buffer,
                                                    const reach_render_group *source,
                                                    const reach_render_stretch *stretch,
                                                    const reach_render_command *commands,
                                                    size_t command_count);
    int32_t reach_render_retained_command_bounds(const reach_render_command *command,
                                                 reach_rect_f32 *out_bounds);
    reach_result reach_render_retain_commands(reach_render_command_buffer *buffer,
                                              size_t first_command, uint64_t first_group_id,
                                              uint64_t parent_id, const reach_motion *opacity,
                                              const reach_motion *y);
    reach_result reach_render_command_buffer_begin_group(reach_render_command_buffer *buffer,
                                                         const reach_render_group *group);
    reach_result reach_render_command_buffer_check_group(const reach_render_command_buffer *buffer,
                                                         const reach_render_group *group);
    void reach_render_command_buffer_end_group(reach_render_command_buffer *buffer);
    reach_result reach_render_command_buffer_push(reach_render_command_buffer *buffer,
                                                  const reach_render_command *command);
    void reach_render_command_buffer_set_scissor(reach_render_command_buffer *buffer,
                                                 reach_rect_f32 scissor_rect);
    void reach_render_command_buffer_set_content_rect(reach_render_command_buffer *buffer,
                                                      reach_rect_f32 content_rect);
    void reach_render_command_buffer_set_content_transform(reach_render_command_buffer *buffer,
                                                           reach_rect_f32 content_rect,
                                                           reach_transform_f32 content_transform);
    reach_result reach_render_push_shadow(reach_render_command_buffer *buffer,
                                          const reach_render_command *shape,
                                          const reach_shadow *shadow, float dpi_scale);
    size_t reach_render_bordered_background_shapes(const reach_render_command *shape,
                                                   reach_color background_color,
                                                   reach_color border_color, float border_thickness,
                                                   reach_render_command out_shapes[2]);
    reach_result reach_render_push_bordered_background(reach_render_command_buffer *buffer,
                                                       const reach_render_command *shape,
                                                       reach_color background_color,
                                                       reach_color border_color,
                                                       float border_thickness,
                                                       const reach_shadow *shadow, float dpi_scale);
    void reach_render_command_buffer_clear_scissor(reach_render_command_buffer *buffer);

#ifdef __cplusplus
}
#endif

#endif
