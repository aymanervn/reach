#include "reach/features/common/text_edit.h"

static size_t reach_text_edit_effective_max(const reach_text_edit *edit)
{
    size_t cap = REACH_TEXT_EDIT_CAPACITY - 1;
    if (edit->max_length > 0 && edit->max_length < cap)
    {
        return edit->max_length;
    }
    return cap;
}

static int32_t reach_text_edit_clamp_index(const reach_text_edit *edit, int32_t index)
{
    if (index < 0)
    {
        return 0;
    }
    if (index > (int32_t)edit->length)
    {
        return (int32_t)edit->length;
    }
    return index;
}

void reach_text_edit_init(reach_text_edit *edit, size_t max_length)
{
    if (edit == nullptr)
    {
        return;
    }
    for (size_t i = 0; i < REACH_TEXT_EDIT_CAPACITY; ++i)
    {
        edit->text[i] = 0;
    }
    edit->length = 0;
    edit->caret = 0;
    edit->selection_anchor = -1;
    edit->max_length = max_length;
}

void reach_text_edit_set_text(reach_text_edit *edit, const uint16_t *text)
{
    if (edit == nullptr)
    {
        return;
    }

    size_t max = reach_text_edit_effective_max(edit);
    size_t count = 0;
    if (text != nullptr)
    {
        while (text[count] != 0 && count < max)
        {
            edit->text[count] = text[count];
            ++count;
        }
    }
    edit->text[count] = 0;
    edit->length = count;
    edit->caret = (int32_t)count;
    edit->selection_anchor = -1;
}

void reach_text_edit_clear(reach_text_edit *edit)
{
    if (edit == nullptr)
    {
        return;
    }
    edit->text[0] = 0;
    edit->length = 0;
    edit->caret = 0;
    edit->selection_anchor = -1;
}

void reach_text_edit_selection_range(const reach_text_edit *edit, int32_t *out_start,
                                     int32_t *out_end)
{
    int32_t start = 0;
    int32_t end = 0;
    if (edit != nullptr && edit->selection_anchor >= 0 && edit->selection_anchor != edit->caret)
    {
        if (edit->selection_anchor < edit->caret)
        {
            start = edit->selection_anchor;
            end = edit->caret;
        }
        else
        {
            start = edit->caret;
            end = edit->selection_anchor;
        }
    }
    if (out_start != nullptr)
    {
        *out_start = start;
    }
    if (out_end != nullptr)
    {
        *out_end = end;
    }
}

int32_t reach_text_edit_has_selection(const reach_text_edit *edit)
{
    int32_t start = 0;
    int32_t end = 0;
    reach_text_edit_selection_range(edit, &start, &end);
    return end > start;
}

void reach_text_edit_select_all(reach_text_edit *edit)
{
    if (edit == nullptr || edit->length == 0)
    {
        return;
    }
    edit->selection_anchor = 0;
    edit->caret = (int32_t)edit->length;
}

size_t reach_text_edit_copy_selection(const reach_text_edit *edit, uint16_t *out_text,
                                      size_t capacity)
{
    if (out_text == nullptr || capacity == 0)
    {
        return 0;
    }
    out_text[0] = 0;

    int32_t start = 0;
    int32_t end = 0;
    reach_text_edit_selection_range(edit, &start, &end);
    if (end <= start)
    {
        return 0;
    }

    size_t count = 0;
    for (int32_t i = start; i < end && count + 1 < capacity; ++i)
    {
        out_text[count++] = edit->text[i];
    }
    out_text[count] = 0;
    return count;
}

static void reach_text_edit_remove_range(reach_text_edit *edit, int32_t start, int32_t end)
{
    if (end <= start)
    {
        return;
    }
    int32_t removed = end - start;
    for (int32_t i = end; i <= (int32_t)edit->length; ++i)
    {
        edit->text[i - removed] = edit->text[i];
    }
    edit->length -= (size_t)removed;
    edit->caret = start;
    edit->selection_anchor = -1;
}

int32_t reach_text_edit_delete_selection(reach_text_edit *edit)
{
    if (edit == nullptr)
    {
        return 0;
    }
    int32_t start = 0;
    int32_t end = 0;
    reach_text_edit_selection_range(edit, &start, &end);
    if (end <= start)
    {
        return 0;
    }
    reach_text_edit_remove_range(edit, start, end);
    return 1;
}

reach_text_edit_event reach_text_edit_insert_char(reach_text_edit *edit, uint16_t ch)
{
    if (edit == nullptr || ch == 0)
    {
        return REACH_TEXT_EDIT_EVENT_NONE;
    }

    (void)reach_text_edit_delete_selection(edit);

    size_t max = reach_text_edit_effective_max(edit);
    if (edit->length >= max)
    {
        return REACH_TEXT_EDIT_EVENT_NONE;
    }

    int32_t at = reach_text_edit_clamp_index(edit, edit->caret);
    for (int32_t i = (int32_t)edit->length; i > at; --i)
    {
        edit->text[i] = edit->text[i - 1];
    }
    edit->text[at] = ch;
    edit->length += 1;
    edit->text[edit->length] = 0;
    edit->caret = at + 1;
    edit->selection_anchor = -1;
    return REACH_TEXT_EDIT_EVENT_TEXT_CHANGED;
}

reach_text_edit_event reach_text_edit_insert_text(reach_text_edit *edit, const uint16_t *text)
{
    if (edit == nullptr || text == nullptr || text[0] == 0)
    {
        return REACH_TEXT_EDIT_EVENT_NONE;
    }

    int32_t changed = reach_text_edit_delete_selection(edit);
    size_t max = reach_text_edit_effective_max(edit);
    size_t index = 0;
    while (text[index] != 0 && edit->length < max)
    {
        uint16_t ch = text[index++];

        if (ch == (uint16_t)'\r' || ch == (uint16_t)'\n' || ch == (uint16_t)'\t')
        {
            continue;
        }
        int32_t at = reach_text_edit_clamp_index(edit, edit->caret);
        for (int32_t i = (int32_t)edit->length; i > at; --i)
        {
            edit->text[i] = edit->text[i - 1];
        }
        edit->text[at] = ch;
        edit->length += 1;
        edit->text[edit->length] = 0;
        edit->caret = at + 1;
        changed = 1;
    }
    edit->selection_anchor = -1;
    return changed ? REACH_TEXT_EDIT_EVENT_TEXT_CHANGED : REACH_TEXT_EDIT_EVENT_NONE;
}

static void reach_text_edit_move_caret(reach_text_edit *edit, int32_t target, int32_t shift)
{
    target = reach_text_edit_clamp_index(edit, target);
    if (shift)
    {
        if (edit->selection_anchor < 0)
        {
            edit->selection_anchor = edit->caret;
        }
        edit->caret = target;
        if (edit->selection_anchor == edit->caret)
        {
            edit->selection_anchor = -1;
        }
    }
    else
    {
        edit->caret = target;
        edit->selection_anchor = -1;
    }
}

reach_text_edit_event reach_text_edit_handle_key(reach_text_edit *edit, reach_text_edit_key key,
                                                 reach_text_edit_modifiers modifiers)
{
    if (edit == nullptr)
    {
        return REACH_TEXT_EDIT_EVENT_NONE;
    }

    switch (key)
    {
    case REACH_TEXT_EDIT_KEY_ENTER:
        return REACH_TEXT_EDIT_EVENT_SUBMIT;
    case REACH_TEXT_EDIT_KEY_ESCAPE:
        return REACH_TEXT_EDIT_EVENT_CANCEL;
    case REACH_TEXT_EDIT_KEY_UP:
        return REACH_TEXT_EDIT_EVENT_NAVIGATE_UP;
    case REACH_TEXT_EDIT_KEY_DOWN:
        return REACH_TEXT_EDIT_EVENT_NAVIGATE_DOWN;
    case REACH_TEXT_EDIT_KEY_BACKSPACE:
        if (reach_text_edit_delete_selection(edit))
        {
            return REACH_TEXT_EDIT_EVENT_TEXT_CHANGED;
        }
        if (edit->caret > 0)
        {
            reach_text_edit_remove_range(edit, edit->caret - 1, edit->caret);
            return REACH_TEXT_EDIT_EVENT_TEXT_CHANGED;
        }
        return REACH_TEXT_EDIT_EVENT_NONE;
    case REACH_TEXT_EDIT_KEY_DELETE:
        if (reach_text_edit_delete_selection(edit))
        {
            return REACH_TEXT_EDIT_EVENT_TEXT_CHANGED;
        }
        if (edit->caret < (int32_t)edit->length)
        {
            reach_text_edit_remove_range(edit, edit->caret, edit->caret + 1);
            return REACH_TEXT_EDIT_EVENT_TEXT_CHANGED;
        }
        return REACH_TEXT_EDIT_EVENT_NONE;
    case REACH_TEXT_EDIT_KEY_LEFT:

        if (!modifiers.shift && reach_text_edit_has_selection(edit))
        {
            int32_t start = 0;
            int32_t end = 0;
            reach_text_edit_selection_range(edit, &start, &end);
            edit->caret = start;
            edit->selection_anchor = -1;
        }
        else
        {
            reach_text_edit_move_caret(edit, edit->caret - 1, modifiers.shift);
        }
        return REACH_TEXT_EDIT_EVENT_NONE;
    case REACH_TEXT_EDIT_KEY_RIGHT:
        if (!modifiers.shift && reach_text_edit_has_selection(edit))
        {
            int32_t start = 0;
            int32_t end = 0;
            reach_text_edit_selection_range(edit, &start, &end);
            edit->caret = end;
            edit->selection_anchor = -1;
        }
        else
        {
            reach_text_edit_move_caret(edit, edit->caret + 1, modifiers.shift);
        }
        return REACH_TEXT_EDIT_EVENT_NONE;
    case REACH_TEXT_EDIT_KEY_HOME:
        reach_text_edit_move_caret(edit, 0, modifiers.shift);
        return REACH_TEXT_EDIT_EVENT_NONE;
    case REACH_TEXT_EDIT_KEY_END:
        reach_text_edit_move_caret(edit, (int32_t)edit->length, modifiers.shift);
        return REACH_TEXT_EDIT_EVENT_NONE;
    case REACH_TEXT_EDIT_KEY_NONE:
    default:
        return REACH_TEXT_EDIT_EVENT_NONE;
    }
}
