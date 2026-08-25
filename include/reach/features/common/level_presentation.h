#ifndef REACH_FEATURES_COMMON_LEVEL_PRESENTATION_H
#define REACH_FEATURES_COMMON_LEVEL_PRESENTATION_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/render_commands.h"

#ifdef __cplusplus
extern "C"
{
#endif

    float reach_level_clamp01(float value);
    reach_vector_icon_id reach_volume_level_icon(float level, int32_t muted);
    void reach_level_format_percent(uint16_t *dst, size_t dst_count, float level);

#ifdef __cplusplus
}
#endif

#endif
