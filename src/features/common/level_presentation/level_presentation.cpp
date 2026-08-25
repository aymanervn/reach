#include "reach/features/common/level_presentation.h"

float reach_level_clamp01(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    return value > 1.0f ? 1.0f : value;
}

reach_vector_icon_id reach_volume_level_icon(float level, int32_t muted)
{
    level = reach_level_clamp01(level);
    if (muted || level <= 0.0f)
    {
        return REACH_VECTOR_ICON_VOLUME_ZERO;
    }
    return level < 0.5f ? REACH_VECTOR_ICON_VOLUME_LOW : REACH_VECTOR_ICON_VOLUME_HIGH;
}

reach_vector_icon_id reach_wifi_signal_icon(int32_t signal_strength)
{
    if (signal_strength < 34)
    {
        return REACH_VECTOR_ICON_WIFI_LOW;
    }
    return signal_strength < 67 ? REACH_VECTOR_ICON_WIFI_MEDIUM : REACH_VECTOR_ICON_WIFI_HIGH;
}

void reach_level_format_percent(uint16_t *dst, size_t dst_count, float level)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }

    int32_t percent = (int32_t)(reach_level_clamp01(level) * 100.0f + 0.5f);
    uint16_t digits[3] = {};
    size_t digit_count = 0;
    if (percent == 0)
    {
        digits[digit_count++] = '0';
    }
    while (percent > 0 && digit_count < 3)
    {
        digits[digit_count++] = (uint16_t)('0' + percent % 10);
        percent /= 10;
    }

    size_t length = 0;
    while (digit_count > 0 && length + 2 < dst_count)
    {
        dst[length++] = digits[--digit_count];
    }
    if (length + 1 < dst_count)
    {
        dst[length++] = '%';
    }
    dst[length] = 0;
}
