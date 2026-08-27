#include "reach/support/util.h"

#include <stdio.h>

void reach_log_info(const char *message)
{
    if (message == nullptr)
    {
        return;
    }

    fputs("[reach] ", stderr);
    fputs(message, stderr);
    fputc('\n', stderr);
}

void reach_log_error(const char *message)
{
    if (message == nullptr)
    {
        return;
    }

    fputs("[reach:error] ", stderr);
    fputs(message, stderr);
    fputc('\n', stderr);
}

size_t reach_strlen_utf16(const uint16_t *text)
{
    if (text == nullptr)
    {
        return 0;
    }

    size_t length = 0;
    while (text[length] != 0)
    {
        ++length;
    }
    return length;
}

int32_t reach_utf16_equal(const uint16_t *a, const uint16_t *b)
{
    if (a == nullptr || b == nullptr)
    {
        return a == b;
    }

    size_t index = 0;
    while (a[index] != 0 || b[index] != 0)
    {
        if (a[index] != b[index])
        {
            return 0;
        }
        ++index;
    }
    return 1;
}

static uint16_t reach_ascii_lower(uint16_t value)
{
    return value >= 'A' && value <= 'Z' ? (uint16_t)(value + ('a' - 'A')) : value;
}

int32_t reach_utf16_equal_ascii_case_insensitive(const uint16_t *a, const uint16_t *b)
{
    if (a == nullptr || b == nullptr)
    {
        return a == b;
    }

    size_t index = 0;
    while (a[index] != 0 && b[index] != 0)
    {
        if (reach_ascii_lower(a[index]) != reach_ascii_lower(b[index]))
        {
            return 0;
        }
        ++index;
    }
    return a[index] == b[index];
}

int32_t reach_path_equals(const uint16_t *a, const uint16_t *b)
{
    if (a == nullptr || b == nullptr)
    {
        return a == b;
    }

    size_t index = 0;
    while (a[index] != 0 && b[index] != 0)
    {
        uint16_t left = a[index] == '/' ? '\\' : reach_ascii_lower(a[index]);
        uint16_t right = b[index] == '/' ? '\\' : reach_ascii_lower(b[index]);
        if (left != right)
        {
            return 0;
        }
        ++index;
    }
    return a[index] == b[index];
}

reach_result reach_copy_utf16(uint16_t *dst, size_t dst_count, const uint16_t *src)
{
    if (dst == nullptr || dst_count == 0 || src == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    size_t index = 0;
    while (index + 1 < dst_count && src[index] != 0)
    {
        dst[index] = src[index];
        ++index;
    }

    dst[index] = 0;
    return src[index] == 0 ? REACH_OK : REACH_ERROR;
}

reach_result reach_copy_ascii_to_utf16(uint16_t *dst, size_t dst_count, const char *src)
{
    if (dst == nullptr || dst_count == 0 || src == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    size_t index = 0;
    while (index + 1 < dst_count && src[index] != 0)
    {
        dst[index] = (uint16_t)(unsigned char)src[index];
        ++index;
    }

    dst[index] = 0;
    return src[index] == 0 ? REACH_OK : REACH_ERROR;
}

void reach_copy_path_stem_utf16(uint16_t *dst, size_t dst_count, const uint16_t *path)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }

    dst[0] = 0;
    if (path == nullptr)
    {
        return;
    }

    const uint16_t *name = path;
    for (const uint16_t *cursor = path; *cursor != 0; ++cursor)
    {
        if (*cursor == '\\' || *cursor == '/')
        {
            name = cursor + 1;
        }
    }

    size_t name_length = reach_strlen_utf16(name);
    size_t end = name_length;
    for (size_t index = name_length; index > 0; --index)
    {
        if (name[index - 1] == '.')
        {
            end = index - 1;
            break;
        }
    }
    if (end == 0)
    {
        end = name_length;
    }

    size_t write = 0;
    while (write + 1 < dst_count && write < end)
    {
        dst[write] = name[write];
        ++write;
    }
    dst[write] = 0;
}
