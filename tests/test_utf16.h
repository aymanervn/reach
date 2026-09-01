#ifndef REACH_TEST_UTF16_H
#define REACH_TEST_UTF16_H

#include <stddef.h>
#include <stdint.h>

static inline int32_t reach_test_utf16_equals_ascii(const uint16_t *text, const char *ascii)
{
    if (text == nullptr || ascii == nullptr)
    {
        return text == nullptr && ascii == nullptr;
    }

    size_t index = 0;
    while (text[index] != 0 && ascii[index] != 0)
    {
        if (text[index] != (uint16_t)(unsigned char)ascii[index])
        {
            return 0;
        }
        ++index;
    }
    return text[index] == 0 && ascii[index] == 0;
}

#endif
