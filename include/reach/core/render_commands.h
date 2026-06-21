#ifndef REACH_CORE_RENDER_COMMANDS_H
#define REACH_CORE_RENDER_COMMANDS_H

#include "reach/core/ui_layout.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_MAX_RENDER_COMMANDS 256

    typedef enum reach_render_command_type
    {
        REACH_RENDER_COMMAND_NONE = 0,
        REACH_RENDER_COMMAND_RECT = 1,
        REACH_RENDER_COMMAND_TEXT = 2,
        REACH_RENDER_COMMAND_ICON = 3,
        REACH_RENDER_COMMAND_BLUR_BACKGROUND = 4,
        REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE = 5,
        REACH_RENDER_COMMAND_RESERVED_6 = 6,
        REACH_RENDER_COMMAND_TRIANGLE = 7,
        REACH_RENDER_COMMAND_NOTCH_STROKE = 8,
        REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT = 9,
        REACH_RENDER_COMMAND_VECTOR_ICON = 10,
        REACH_RENDER_COMMAND_CLIPPED_ROUNDED_RECT = 11,
        REACH_RENDER_COMMAND_ICON_TINT = 13,
        REACH_RENDER_COMMAND_BLURRED_IMAGE = 14
    } reach_render_command_type;

    typedef enum reach_render_corner_mask
    {
        REACH_RENDER_CORNER_TOP_LEFT = 1,
        REACH_RENDER_CORNER_TOP_RIGHT = 2,
        REACH_RENDER_CORNER_BOTTOM_RIGHT = 4,
        REACH_RENDER_CORNER_BOTTOM_LEFT = 8,
        REACH_RENDER_CORNER_ALL = 15
    } reach_render_corner_mask;

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
        REACH_VECTOR_ICON_BATTERY_SAVER = 18,
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
        REACH_VECTOR_ICON_SETTINGS = 37
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
        REACH_TEXT_ALIGNMENT_CENTER = 1
    } reach_text_alignment;

    typedef struct reach_render_command
    {
        reach_render_command_type type;
        reach_rect_f32 rect;
        reach_rect_f32 clip_rect;
        reach_color color;
        float radius;
        float blur_radius;
        float image_contrast;
        int32_t has_clip_rect;
        float clip_radius;
        float stroke_width;
        uint64_t icon_id;
        int32_t icon_crop_to_fill;
        int32_t corner_mask;
        int32_t text_weight;
        float text_size;
        float notch_center_x;
        float notch_width;
        float notch_height;
        int32_t text_alignment;
        int32_t text_ellipsis;
        uint16_t text[260];
    } reach_render_command;

    typedef struct reach_render_command_buffer
    {
        reach_render_command commands[REACH_MAX_RENDER_COMMANDS];
        size_t count;
    } reach_render_command_buffer;

    void reach_render_command_buffer_clear(reach_render_command_buffer *buffer);
    reach_result reach_render_command_buffer_push(reach_render_command_buffer *buffer,
                                                  const reach_render_command *command);
    reach_result reach_ui_build_render_commands(const reach_ui_state *state,
                                                const reach_ui_layout *layout,
                                                reach_render_command_buffer *buffer);

#ifdef __cplusplus
}
#endif

#endif
