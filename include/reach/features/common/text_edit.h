#ifndef REACH_FEATURES_COMMON_TEXT_EDIT_H
#define REACH_FEATURES_COMMON_TEXT_EDIT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_TEXT_EDIT_CAPACITY 256

    /*
     * A neutral, OS-free single-line text edit model: a UTF-16 buffer, a caret,
     * and a selection, with key/char handling. It is the in-process replacement
     * for the native Win32 EDIT control that used to back the launcher search box
     * (the reach_textbox port, now retired). The model owns no OS resources;
     * clipboard cut/copy/paste are driven from outside by handing it text
     * (reach_text_edit_insert_text) or reading its selection
     * (reach_text_edit_copy_selection) and routing through the clipboard port.
     *
     * Rendering is done via REACH_RENDER_COMMAND_TEXTBOX, fed from this model.
     */
    typedef struct reach_text_edit
    {
        uint16_t text[REACH_TEXT_EDIT_CAPACITY];
        size_t length;
        int32_t caret;           /* 0..length */
        int32_t selection_anchor; /* -1 = no selection; else the fixed end of the range */
        size_t max_length;       /* 0 => REACH_TEXT_EDIT_CAPACITY - 1 */
    } reach_text_edit;

    /* Mirrors the events the retired textbox port emitted, so launcher handling
     * barely changes. */
    typedef enum reach_text_edit_event
    {
        REACH_TEXT_EDIT_EVENT_NONE = 0,
        REACH_TEXT_EDIT_EVENT_TEXT_CHANGED = 1,
        REACH_TEXT_EDIT_EVENT_SUBMIT = 2,
        REACH_TEXT_EDIT_EVENT_CANCEL = 3,
        REACH_TEXT_EDIT_EVENT_NAVIGATE_UP = 4,
        REACH_TEXT_EDIT_EVENT_NAVIGATE_DOWN = 5
    } reach_text_edit_event;

    /* Platform-neutral key set the model understands (the composition input layer
     * maps OS virtual keys onto these). Character insertion goes through
     * reach_text_edit_insert_char, not here. */
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
    /* Replaces the buffer and moves the caret to the end; clears selection. Does
     * NOT emit an event (it is an external set, not a user edit). */
    void reach_text_edit_set_text(reach_text_edit *edit, const uint16_t *text);
    void reach_text_edit_clear(reach_text_edit *edit);

    /* User edits. Each returns the event produced (TEXT_CHANGED when the buffer
     * changed, or a navigation/submit/cancel signal). */
    reach_text_edit_event reach_text_edit_insert_char(reach_text_edit *edit, uint16_t ch);
    reach_text_edit_event reach_text_edit_insert_text(reach_text_edit *edit, const uint16_t *text);
    reach_text_edit_event reach_text_edit_handle_key(reach_text_edit *edit, reach_text_edit_key key,
                                                     reach_text_edit_modifiers modifiers);

    /* Selection helpers (used by clipboard cut/copy and select-all). */
    void reach_text_edit_select_all(reach_text_edit *edit);
    int32_t reach_text_edit_has_selection(const reach_text_edit *edit);
    void reach_text_edit_selection_range(const reach_text_edit *edit, int32_t *out_start,
                                         int32_t *out_end);
    size_t reach_text_edit_copy_selection(const reach_text_edit *edit, uint16_t *out_text,
                                          size_t capacity);
    /* Deletes the current selection if any; returns 1 if the buffer changed. */
    int32_t reach_text_edit_delete_selection(reach_text_edit *edit);

#ifdef __cplusplus
}
#endif

#endif
