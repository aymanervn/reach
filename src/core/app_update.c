#include "reach/core/app_update.h"

#include <assert.h>

static int32_t reach_app_version_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

int32_t reach_app_version_parse(const char *text, int32_t out_components[3])
{
    assert(out_components != NULL);
    if (out_components == NULL)
    {
        return 0;
    }
    out_components[0] = 0;
    out_components[1] = 0;
    out_components[2] = 0;
    if (text == NULL)
    {
        return 0;
    }
    while (*text != 0 && !reach_app_version_is_digit(*text))
    {
        ++text;
    }
    int32_t component = 0;
    int32_t seen = 0;
    while (*text != 0 && component < 3)
    {
        if (reach_app_version_is_digit(*text))
        {
            out_components[component] = out_components[component] * 10 + (*text - '0');
            seen = 1;
            ++text;
        }
        else if (*text == '.')
        {
            ++component;
            ++text;
        }
        else
        {
            break;
        }
    }
    return seen;
}

int32_t reach_app_version_compare(const char *left, const char *right)
{
    int32_t left_components[3];
    int32_t right_components[3];
    reach_app_version_parse(left, left_components);
    reach_app_version_parse(right, right_components);
    for (int32_t index = 0; index < 3; ++index)
    {
        if (left_components[index] < right_components[index])
        {
            return -1;
        }
        if (left_components[index] > right_components[index])
        {
            return 1;
        }
    }
    return 0;
}
