#ifndef REACH_PORTS_TEXTBOX_H
#define REACH_PORTS_TEXTBOX_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/geometry.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_TEXTBOX_TEXT_CAPACITY 256
#define REACH_TEXTBOX_PLACEHOLDER_CAPACITY 128

    typedef struct reach_textbox reach_textbox;

    typedef enum reach_textbox_event_type
    {
        REACH_TEXTBOX_EVENT_NONE = 0,
        REACH_TEXTBOX_EVENT_TEXT_CHANGED = 1,
        REACH_TEXTBOX_EVENT_SUBMIT = 2,
        REACH_TEXTBOX_EVENT_CANCEL = 3,
        REACH_TEXTBOX_EVENT_NAVIGATE_UP = 4,
        REACH_TEXTBOX_EVENT_NAVIGATE_DOWN = 5
    } reach_textbox_event_type;

    typedef struct reach_textbox_event
    {
        reach_textbox_event_type type;
        uint16_t text[REACH_TEXTBOX_TEXT_CAPACITY];
        size_t text_length;
    } reach_textbox_event;

    typedef struct reach_textbox_style
    {
        float font_size;
        int32_t font_weight;
        reach_color text_color;
        /* Native Win32 EDIT controls are opaque; adapters may ignore alpha. */
        reach_color background_color;
        uint16_t placeholder[REACH_TEXTBOX_PLACEHOLDER_CAPACITY];
        size_t max_length;
    } reach_textbox_style;

    typedef void (*reach_textbox_event_callback)(void *user, const reach_textbox_event *event);

    typedef struct reach_textbox_ops
    {
        /* Bounds are in the owning platform window's client coordinate space. */
        reach_result (*set_bounds)(reach_textbox *textbox, reach_rect_f32 bounds);
        reach_result (*set_style)(reach_textbox *textbox, const reach_textbox_style *style);
        reach_result (*set_text)(reach_textbox *textbox, const uint16_t *text);
        reach_result (*get_text)(const reach_textbox *textbox, uint16_t *out_text,
                                 size_t text_capacity);
        reach_result (*show)(reach_textbox *textbox);
        reach_result (*hide)(reach_textbox *textbox);
        reach_result (*set_focused)(reach_textbox *textbox, int32_t focused);
        reach_result (*set_enabled)(reach_textbox *textbox, int32_t enabled);
        reach_result (*set_event_callback)(reach_textbox *textbox,
                                           reach_textbox_event_callback callback, void *user);
        int32_t (*has_pending_events)(const reach_textbox *textbox);
        reach_result (*dispatch_events)(reach_textbox *textbox);
        void (*destroy)(reach_textbox *textbox);
    } reach_textbox_ops;

    typedef struct reach_textbox_port
    {
        reach_textbox *textbox;
        reach_textbox_ops ops;
    } reach_textbox_port;

#ifdef __cplusplus
}
#endif

#endif
