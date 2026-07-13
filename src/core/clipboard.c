#include "reach/core/clipboard.h"

void reach_clipboard_build_text_preview(const uint16_t *text, uint16_t *out_preview,
                                        size_t out_capacity)
{
    if (out_preview == 0 || out_capacity == 0)
    {
        return;
    }
    out_preview[0] = 0;
    if (text == 0)
    {
        return;
    }

    size_t output = 0;
    int32_t truncated = 0;
    for (size_t input = 0; text[input] != 0; ++input)
    {
        uint16_t value = text[input];
        if (value == '\r')
        {
            if (text[input + 1] == '\n')
            {
                continue;
            }
            value = '\n';
        }
        if (output + 1 >= out_capacity)
        {
            truncated = 1;
            break;
        }
        out_preview[output++] = value;
    }

    if (truncated && out_capacity >= 2)
    {
        if (output + 1 >= out_capacity)
        {
            output = out_capacity - 2;
        }
        out_preview[output++] = 0x2026;
    }
    out_preview[output] = 0;
}
