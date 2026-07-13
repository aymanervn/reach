#include "reach/features/common/text_edit.h"

#include <stdio.h>

static int failures;

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        ++failures;
        fprintf(stderr, "FAILED: %s\n", message);
    }
}

static void copy_ascii(uint16_t *dst, size_t dst_count, const char *src)
{
    size_t index = 0;
    while (src != nullptr && src[index] != 0 && index + 1 < dst_count)
    {
        dst[index] = (uint16_t)(unsigned char)src[index];
        ++index;
    }
    dst[index] = 0;
}

static int text_equals_ascii(const uint16_t *text, const char *expected)
{
    size_t index = 0;
    while (expected[index] != 0)
    {
        if (text[index] != (uint16_t)(unsigned char)expected[index])
        {
            return 0;
        }
        ++index;
    }
    return text[index] == 0;
}

static reach_text_edit_modifiers none()
{
    reach_text_edit_modifiers m = {};
    return m;
}

static reach_text_edit_modifiers shift()
{
    reach_text_edit_modifiers m = {};
    m.shift = 1;
    return m;
}

static void test_insert_and_caret()
{
    reach_text_edit edit;
    reach_text_edit_init(&edit, 0);

    expect_true(reach_text_edit_insert_char(&edit, 'h') == REACH_TEXT_EDIT_EVENT_TEXT_CHANGED,
                "insert emits TEXT_CHANGED");
    reach_text_edit_insert_char(&edit, 'i');
    expect_true(edit.length == 2 && edit.caret == 2, "two chars, caret at end");
    expect_true(text_equals_ascii(edit.text, "hi"), "buffer is 'hi'");

    reach_text_edit_handle_key(&edit, REACH_TEXT_EDIT_KEY_LEFT, none());
    expect_true(edit.caret == 1, "left moves caret to 1");
    reach_text_edit_insert_char(&edit, 'X');
    expect_true(text_equals_ascii(edit.text, "hXi"), "insert in middle -> 'hXi'");
}

static void test_backspace_delete_home_end()
{
    reach_text_edit edit;
    reach_text_edit_init(&edit, 0);
    uint16_t buf[REACH_TEXT_EDIT_CAPACITY];
    copy_ascii(buf, REACH_TEXT_EDIT_CAPACITY, "abc");
    reach_text_edit_set_text(&edit, buf);
    expect_true(edit.caret == 3, "set_text puts caret at end");

    expect_true(reach_text_edit_handle_key(&edit, REACH_TEXT_EDIT_KEY_BACKSPACE, none()) ==
                    REACH_TEXT_EDIT_EVENT_TEXT_CHANGED,
                "backspace changes text");
    expect_true(text_equals_ascii(edit.text, "ab"), "backspace -> 'ab'");

    reach_text_edit_handle_key(&edit, REACH_TEXT_EDIT_KEY_HOME, none());
    expect_true(edit.caret == 0, "home -> caret 0");
    expect_true(reach_text_edit_handle_key(&edit, REACH_TEXT_EDIT_KEY_DELETE, none()) ==
                    REACH_TEXT_EDIT_EVENT_TEXT_CHANGED,
                "delete at home changes text");
    expect_true(text_equals_ascii(edit.text, "b"), "delete -> 'b'");

    reach_text_edit_handle_key(&edit, REACH_TEXT_EDIT_KEY_END, none());
    expect_true(edit.caret == 1, "end -> caret at length");

    reach_text_edit_clear(&edit);
    expect_true(reach_text_edit_handle_key(&edit, REACH_TEXT_EDIT_KEY_BACKSPACE, none()) ==
                    REACH_TEXT_EDIT_EVENT_NONE,
                "backspace on empty is NONE");
}

static void test_selection_and_copy()
{
    reach_text_edit edit;
    reach_text_edit_init(&edit, 0);
    uint16_t buf[REACH_TEXT_EDIT_CAPACITY];
    copy_ascii(buf, REACH_TEXT_EDIT_CAPACITY, "hello");
    reach_text_edit_set_text(&edit, buf);

    reach_text_edit_select_all(&edit);
    expect_true(reach_text_edit_has_selection(&edit), "select_all selects");
    int32_t start = -1;
    int32_t end = -1;
    reach_text_edit_selection_range(&edit, &start, &end);
    expect_true(start == 0 && end == 5, "selection covers whole buffer");

    uint16_t out[REACH_TEXT_EDIT_CAPACITY];
    size_t copied = reach_text_edit_copy_selection(&edit, out, REACH_TEXT_EDIT_CAPACITY);
    expect_true(copied == 5 && text_equals_ascii(out, "hello"), "copy selection -> 'hello'");

    reach_text_edit_insert_char(&edit, 'Z');
    expect_true(text_equals_ascii(edit.text, "Z"), "insert over selection replaces all");

    copy_ascii(buf, REACH_TEXT_EDIT_CAPACITY, "abcd");
    reach_text_edit_set_text(&edit, buf);
    reach_text_edit_handle_key(&edit, REACH_TEXT_EDIT_KEY_HOME, none());
    reach_text_edit_handle_key(&edit, REACH_TEXT_EDIT_KEY_RIGHT, shift());
    reach_text_edit_handle_key(&edit, REACH_TEXT_EDIT_KEY_RIGHT, shift());
    reach_text_edit_selection_range(&edit, &start, &end);
    expect_true(start == 0 && end == 2, "shift+right twice selects first two");
    expect_true(reach_text_edit_delete_selection(&edit) == 1 && text_equals_ascii(edit.text, "cd"),
                "delete selection -> 'cd'");
}

static void test_paste_and_events()
{
    reach_text_edit edit;
    reach_text_edit_init(&edit, 0);
    uint16_t buf[REACH_TEXT_EDIT_CAPACITY];
    copy_ascii(buf, REACH_TEXT_EDIT_CAPACITY, "a\tb\nc");
    expect_true(reach_text_edit_insert_text(&edit, buf) == REACH_TEXT_EDIT_EVENT_TEXT_CHANGED,
                "paste emits TEXT_CHANGED");
    expect_true(text_equals_ascii(edit.text, "abc"), "paste strips tab/newline -> 'abc'");

    expect_true(reach_text_edit_handle_key(&edit, REACH_TEXT_EDIT_KEY_ENTER, none()) ==
                    REACH_TEXT_EDIT_EVENT_SUBMIT,
                "enter -> SUBMIT");
    expect_true(reach_text_edit_handle_key(&edit, REACH_TEXT_EDIT_KEY_ESCAPE, none()) ==
                    REACH_TEXT_EDIT_EVENT_CANCEL,
                "escape -> CANCEL");
    expect_true(reach_text_edit_handle_key(&edit, REACH_TEXT_EDIT_KEY_UP, none()) ==
                    REACH_TEXT_EDIT_EVENT_NAVIGATE_UP,
                "up -> NAVIGATE_UP");
    expect_true(reach_text_edit_handle_key(&edit, REACH_TEXT_EDIT_KEY_DOWN, none()) ==
                    REACH_TEXT_EDIT_EVENT_NAVIGATE_DOWN,
                "down -> NAVIGATE_DOWN");
}

static void test_max_length()
{
    reach_text_edit edit;
    reach_text_edit_init(&edit, 3);
    for (int i = 0; i < 10; ++i)
    {
        reach_text_edit_insert_char(&edit, 'x');
    }
    expect_true(edit.length == 3, "max_length caps the buffer");
}

int main()
{
    test_insert_and_caret();
    test_backspace_delete_home_end();
    test_selection_and_copy();
    test_paste_and_events();
    test_max_length();
    return failures == 0 ? 0 : 1;
}
