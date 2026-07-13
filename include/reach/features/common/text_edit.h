#ifndef REACH_FEATURES_COMMON_TEXT_EDIT_H
#define REACH_FEATURES_COMMON_TEXT_EDIT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_TEXT_EDIT_CAPACITY 256

    typedef struct reach_text_edit
    {
        uint16_t text[REACH_TEXT_EDIT_CAPACITY];
        size_t length;
        int32_t caret;
        int32_t selection_anchor;
        size_t max_length;
    } reach_text_edit;

    typedef enum reach_text_edit_event
    {
        REACH_TEXT_EDIT_EVENT_NONE = 0,
        REACH_TEXT_EDIT_EVENT_TEXT_CHANGED = 1,
        REACH_TEXT_EDIT_EVENT_SUBMIT = 2,
        REACH_TEXT_EDIT_EVENT_CANCEL = 3,
        REACH_TEXT_EDIT_EVENT_NAVIGATE_UP = 4,
        REACH_TEXT_EDIT_EVENT_NAVIGATE_DOWN = 5
    } reach_text_edit_event;

    typedef enum reach_text_edit_key
    {
        REACH_TEXT_EDIT_KEY_NONE = 0,
        REACH_TEXT_EDIT_KEY_BACKSPACE,
        REACH_TEXT_EDIT_KEY_DELETE,
        REACH_TEXT_EDIT_KEY_LEFT,
        REACH_TEXT_EDIT_KEY_RIGHT,
        REACH_TEXT_EDIT_KEY_HOME,
        REACH_TEXT_EDIT_KEY_END,
        REACH_TEXT_EDIT_KEY_ENTER,
        REACH_TEXT_EDIT_KEY_ESCAPE,
        REACH_TEXT_EDIT_KEY_UP,
        REACH_TEXT_EDIT_KEY_DOWN
    } reach_text_edit_key;

    typedef struct reach_text_edit_modifiers
    {
        int32_t shift;
        int32_t ctrl;
    } reach_text_edit_modifiers;

    void reach_text_edit_init(reach_text_edit *edit, size_t max_length);

    void reach_text_edit_set_text(reach_text_edit *edit, const uint16_t *text);
    void reach_text_edit_clear(reach_text_edit *edit);

    reach_text_edit_event reach_text_edit_insert_char(reach_text_edit *edit, uint16_t ch);
    reach_text_edit_event reach_text_edit_insert_text(reach_text_edit *edit, const uint16_t *text);
    reach_text_edit_event reach_text_edit_handle_key(reach_text_edit *edit, reach_text_edit_key key,
                                                     reach_text_edit_modifiers modifiers);

    void reach_text_edit_select_all(reach_text_edit *edit);
    int32_t reach_text_edit_has_selection(const reach_text_edit *edit);
    void reach_text_edit_selection_range(const reach_text_edit *edit, int32_t *out_start,
                                         int32_t *out_end);
    size_t reach_text_edit_copy_selection(const reach_text_edit *edit, uint16_t *out_text,
                                          size_t capacity);

    int32_t reach_text_edit_delete_selection(reach_text_edit *edit);

#ifdef __cplusplus
}
#endif

#endif
