#include "reach/support/util.h"

#include <windows.h>

void reach_log_info(const char *message)
{
    if (message == nullptr) {
        return;
    }

    OutputDebugStringA("[reach] ");
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
}

void reach_log_error(const char *message)
{
    if (message == nullptr) {
        return;
    }

    OutputDebugStringA("[reach:error] ");
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
}

size_t reach_strlen_utf16(const uint16_t *text)
{
    if (text == nullptr) {
        return 0;
    }

    size_t length = 0;
    while (text[length] != 0) {
        ++length;
    }
    return length;
}

reach_result reach_copy_utf16(uint16_t *dst, size_t dst_count, const uint16_t *src)
{
    if (dst == nullptr || dst_count == 0 || src == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    size_t index = 0;
    while (index + 1 < dst_count && src[index] != 0) {
        dst[index] = src[index];
        ++index;
    }

    dst[index] = 0;
    return src[index] == 0 ? REACH_OK : REACH_ERROR;
}
